/*
 *  Copyright (c) 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 *
 */

#import "RTCVideoDecoderH264.h"

#import <VideoToolbox/VideoToolbox.h>

#import "api/peerconnection/RTCEncodedImage+Private.h"
#import "base/RTCVideoFrame.h"
#import "base/RTCVideoFrameBuffer.h"
#import "components/video_frame_buffer/RTCCVPixelBuffer.h"
#import "helpers.h"
#import "helpers/scoped_cftyperef.h"

#import "RTCH264ProfileLevelId.h"

#if defined(WEBRTC_IOS)
#import "helpers/UIDevice+RTCDevice.h"
#endif

#include "common_video/h264/h264_bitstream_parser.h"
#if !defined(DISABLE_H265)
#include "common_video/h265/h265_bitstream_parser.h"
#endif
#include "modules/video_coding/include/video_error_codes.h"
#include "rtc_base/checks.h"
#include "rtc_base/logging.h"
#include "rtc_base/time_utils.h"
#include "sdk/objc/components/video_codec/nalu_rewriter.h"

// Struct that we pass to the decoder per frame to decode. We receive it again
// in the decoder callback.
struct RTCFrameDecodeParams {
  RTCFrameDecodeParams(RTCVideoDecoderCallback cb, int64_t ts, int32_t qp) : callback(cb), timestamp(ts), qp(qp) {}
  RTCVideoDecoderCallback callback;
  int64_t timestamp;
  int32_t qp;
};

@interface RTC_OBJC_TYPE (RTCVideoDecoderH264)
() - (void)setError : (OSStatus)error;
@end

// This is the callback function that VideoToolbox calls when decode is
// complete.
void decompressionOutputCallback(void *decoderRef,
                                 void *params,
                                 OSStatus status,
                                 VTDecodeInfoFlags infoFlags,
                                 CVImageBufferRef imageBuffer,
                                 CMTime timestamp,
                                 CMTime duration) {
  std::unique_ptr<RTCFrameDecodeParams> decodeParams(
      reinterpret_cast<RTCFrameDecodeParams *>(params));
  if (status != noErr) {
    RTC_OBJC_TYPE(RTCVideoDecoderH264) *decoder =
        (__bridge RTC_OBJC_TYPE(RTCVideoDecoderH264) *)decoderRef;
    [decoder setError:status];
    RTC_LOG(LS_ERROR) << "Failed to decode frame. Status: " << status;
    return;
  }
  // TODO(tkchin): Handle CVO properly.
  RTC_OBJC_TYPE(RTCCVPixelBuffer) *frameBuffer =
      [[RTC_OBJC_TYPE(RTCCVPixelBuffer) alloc] initWithPixelBuffer:imageBuffer];
  RTC_OBJC_TYPE(RTCVideoFrame) *decodedFrame = [[RTC_OBJC_TYPE(RTCVideoFrame) alloc]
      initWithBuffer:frameBuffer
            rotation:RTCVideoRotation_0
         timeStampNs:CMTimeGetSeconds(timestamp) * rtc::kNumNanosecsPerSec];
  decodedFrame.timeStamp = decodeParams->timestamp;
  decodeParams->callback(decodedFrame, decodeParams->qp);
}

// Decoder.
@implementation RTC_OBJC_TYPE (RTCVideoDecoderH264) {
  CMVideoFormatDescriptionRef _videoFormat;
  CMMemoryPoolRef _memoryPool;
  VTDecompressionSessionRef _decompressionSession;
  RTCVideoDecoderCallback _callback;
  OSStatus _error;
  webrtc::H264BitstreamParser _h264BitstreamParser;
  RTC_OBJC_TYPE(RTCVideoCodecInfo) *_codecInfo;

#if !defined(DISABLE_H265)
  webrtc::H265BitstreamParser _h265BitstreamParser;
#endif
}

- (instancetype)initWithCodecInfo:(RTC_OBJC_TYPE(RTCVideoCodecInfo) *)codecInfo {
  self = [super init];
  if (self) {
    _memoryPool = CMMemoryPoolCreate(nil);
    _codecInfo = codecInfo;
  }
  return self;
}

- (void)dealloc {
  CMMemoryPoolInvalidate(_memoryPool);
  CFRelease(_memoryPool);
  [self destroyDecompressionSession];
  [self setVideoFormat:nullptr];
}

- (NSInteger)startDecodeWithNumberOfCores:(int)numberOfCores {
  return WEBRTC_VIDEO_CODEC_OK;
}

- (NSInteger)decode:(RTC_OBJC_TYPE(RTCEncodedImage) *)inputImage
        missingFrames:(BOOL)missingFrames
    codecSpecificInfo:(nullable id<RTC_OBJC_TYPE(RTCCodecSpecificInfo)>)info
         renderTimeMs:(int64_t)renderTimeMs {
  RTC_DCHECK(inputImage.buffer);

  if (_error != noErr) {
    RTC_LOG(LS_WARNING) << "Last frame decode failed.";
    _error = noErr;
    return WEBRTC_VIDEO_CODEC_ERROR;
  }

#if !defined(DISABLE_H265)
  rtc::ScopedCFTypeRef<CMVideoFormatDescriptionRef> inputFormat(nullptr);
  if ([_codecInfo.name isEqualToString:kRTCVideoCodecH264Name]) {
    inputFormat = rtc::ScopedCF(webrtc::CreateVideoFormatDescription((uint8_t *)inputImage.buffer.bytes,
                                                                     inputImage.buffer.length));
  } else if (@available(iOS 11, *)) {
    inputFormat = rtc::ScopedCF(webrtc::CreateH265VideoFormatDescription((uint8_t*)inputImage.buffer.bytes,
                                                                         inputImage.buffer.length));
  }
#else
  rtc::ScopedCFTypeRef<CMVideoFormatDescriptionRef> inputFormat =
      rtc::ScopedCF(webrtc::CreateVideoFormatDescription((uint8_t *)inputImage.buffer.bytes,
                                                         inputImage.buffer.length));
#endif
  if (inputFormat) {
    // Check if the video format has changed, and reinitialize decoder if
    // needed.
    if (!CMFormatDescriptionEqual(inputFormat.get(), _videoFormat)) {
      [self setVideoFormat:inputFormat.get()];
      int resetDecompressionSessionError = [self resetDecompressionSession];
      if (resetDecompressionSessionError != WEBRTC_VIDEO_CODEC_OK) {
        return resetDecompressionSessionError;
      }
    }
  }
  if (!_videoFormat) {
    // We received a frame but we don't have format information so we can't
    // decode it.
    // This can happen after backgrounding. We need to wait for the next
    // sps/pps before we can resume so we request a keyframe by returning an
    // error.
    RTC_LOG(LS_WARNING) << "Missing video format. Frame with sps/pps required.";
    return WEBRTC_VIDEO_CODEC_ERROR;
  }
  CMSampleBufferRef sampleBuffer = nullptr;
#if !defined(DISABLE_H265)
  if ([_codecInfo.name isEqualToString:kRTCVideoCodecH264Name]) {
    if (!webrtc::H264AnnexBBufferToCMSampleBuffer((uint8_t *)inputImage.buffer.bytes,
                                                  inputImage.buffer.length,
                                                  _videoFormat,
                                                  &sampleBuffer,
                                                  _memoryPool)) {
      return WEBRTC_VIDEO_CODEC_ERROR;
    }
  } else if (@available(iOS 11, *)) {
    if (!webrtc::H265AnnexBBufferToCMSampleBuffer((uint8_t*)inputImage.buffer.bytes,
                                                  inputImage.buffer.length,
                                                  _videoFormat,
                                                  &sampleBuffer)) {
      return WEBRTC_VIDEO_CODEC_ERROR;
    }
  } else {
    return WEBRTC_VIDEO_CODEC_ERROR;
  }
#else
  if (!webrtc::H264AnnexBBufferToCMSampleBuffer((uint8_t *)inputImage.buffer.bytes,
                                                inputImage.buffer.length,
                                                _videoFormat,
                                                &sampleBuffer,
                                                _memoryPool)) {
    return WEBRTC_VIDEO_CODEC_ERROR;
  }
#endif
  RTC_DCHECK(sampleBuffer);
  VTDecodeFrameFlags decodeFlags = kVTDecodeFrame_EnableAsynchronousDecompression;
  std::unique_ptr<RTCFrameDecodeParams> frameDecodeParams;

  int qp = -1;
#if !defined(DISABLE_H265)
  if ([_codecInfo.name isEqualToString:kRTCVideoCodecH264Name]) {
    _h264BitstreamParser.ParseBitstream([inputImage nativeEncodedImage]);
    qp = _h264BitstreamParser.GetLastSliceQp().value_or(-1);
  } else {
    _h265BitstreamParser.ParseBitstream([inputImage nativeEncodedImage]);
    qp = _h265BitstreamParser.GetLastSliceQp().value_or(-1);
  }
#else
    _h264BitstreamParser.ParseBitstream([inputImage nativeEncodedImage]);
    qp = _h264BitstreamParser.GetLastSliceQp().value_or(-1);
#endif

  frameDecodeParams.reset(new RTCFrameDecodeParams(_callback, inputImage.timeStamp, qp));

  OSStatus status = VTDecompressionSessionDecodeFrame(
      _decompressionSession, sampleBuffer, decodeFlags, frameDecodeParams.release(), nullptr);
#if defined(WEBRTC_IOS)
  // Re-initialize the decoder if we have an invalid session while the app is
  // active or decoder malfunctions and retry the decode request.
  if ((status == kVTInvalidSessionErr || status == kVTVideoDecoderMalfunctionErr) &&
      [self resetDecompressionSession] == WEBRTC_VIDEO_CODEC_OK) {
    RTC_LOG(LS_INFO) << "Failed to decode frame with code: " << status
                     << " retrying decode after decompression session reset";
    frameDecodeParams.reset(new RTCFrameDecodeParams(_callback, inputImage.timeStamp, qp));
    status = VTDecompressionSessionDecodeFrame(
        _decompressionSession, sampleBuffer, decodeFlags, frameDecodeParams.release(), nullptr);
  }
#endif
  CFRelease(sampleBuffer);
  if (status != noErr) {
    RTC_LOG(LS_ERROR) << "Failed to decode frame with code: " << status;
    return WEBRTC_VIDEO_CODEC_ERROR;
  }
  return WEBRTC_VIDEO_CODEC_OK;
}

- (void)setCallback:(RTCVideoDecoderCallback)callback {
  _callback = callback;
}

- (void)setError:(OSStatus)error {
  _error = error;
}

- (NSInteger)releaseDecoder {
  // Need to invalidate the session so that callbacks no longer occur and it
  // is safe to null out the callback.
  [self destroyDecompressionSession];
  [self setVideoFormat:nullptr];
  _callback = nullptr;
  return WEBRTC_VIDEO_CODEC_OK;
}

#pragma mark - Private

- (int)resetDecompressionSession {
  [self destroyDecompressionSession];

  // Need to wait for the first SPS to initialize decoder.
  if (!_videoFormat) {
    return WEBRTC_VIDEO_CODEC_OK;
  }

  // Set keys for OpenGL and IOSurface compatibilty, which makes the encoder
  // create pixel buffers with GPU backed memory. The intent here is to pass
  // the pixel buffers directly so we avoid a texture upload later during
  // rendering. This currently is moot because we are converting back to an
  // I420 frame after decode, but eventually we will be able to plumb
  // CVPixelBuffers directly to the renderer.
  // TODO(tkchin): Maybe only set OpenGL/IOSurface keys if we know that that
  // we can pass CVPixelBuffers as native handles in decoder output.
  NSDictionary *attributes = @{
#if defined(WEBRTC_IOS) && (TARGET_OS_MACCATALYST || TARGET_OS_SIMULATOR)
    (NSString *)kCVPixelBufferMetalCompatibilityKey : @(YES),
#elif defined(WEBRTC_IOS)
    (NSString *)kCVPixelBufferOpenGLESCompatibilityKey : @(YES),
#elif defined(WEBRTC_MAC) && !defined(WEBRTC_ARCH_ARM64)
    (NSString *)kCVPixelBufferOpenGLCompatibilityKey : @(YES),
#endif
#if !(TARGET_OS_SIMULATOR)
    (NSString *)kCVPixelBufferIOSurfacePropertiesKey : @{},
#endif
    (NSString *)
    kCVPixelBufferPixelFormatTypeKey : @(kCVPixelFormatType_420YpCbCr8BiPlanarFullRange),
  };

  VTDecompressionOutputCallbackRecord record = {
      decompressionOutputCallback, (__bridge void *)self,
  };
  OSStatus status = VTDecompressionSessionCreate(nullptr,
                                                 _videoFormat,
                                                 nullptr,
                                                 (__bridge CFDictionaryRef)attributes,
                                                 &record,
                                                 &_decompressionSession);
  if (status != noErr) {
    RTC_LOG(LS_ERROR) << "Failed to create decompression session: " << status;
    [self destroyDecompressionSession];
    return WEBRTC_VIDEO_CODEC_ERROR;
  }
  [self configureDecompressionSession];

  return WEBRTC_VIDEO_CODEC_OK;
}

- (void)configureDecompressionSession {
  RTC_DCHECK(_decompressionSession);
#if defined(WEBRTC_IOS)
  VTSessionSetProperty(_decompressionSession, kVTDecompressionPropertyKey_RealTime, kCFBooleanTrue);
#endif
}

- (void)destroyDecompressionSession {
  if (_decompressionSession) {
#if defined(WEBRTC_IOS)
    if ([UIDevice isIOS11OrLater]) {
      VTDecompressionSessionWaitForAsynchronousFrames(_decompressionSession);
    }
#endif
    VTDecompressionSessionInvalidate(_decompressionSession);
    CFRelease(_decompressionSession);
    _decompressionSession = nullptr;
  }
}

- (void)setVideoFormat:(CMVideoFormatDescriptionRef)videoFormat {
  if (_videoFormat == videoFormat) {
    return;
  }
  if (_videoFormat) {
    CFRelease(_videoFormat);
  }
  _videoFormat = videoFormat;
  if (_videoFormat) {
    CFRetain(_videoFormat);
  }
}

- (NSString *)implementationName {
  return @"VideoToolbox";
}

@end
