/*
 *  Copyright 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#import "WebRTC/RTCVideoCodec.h"

#import "RTCVideoCodec+Private.h"

@implementation RTCEncodedImage

@synthesize buffer = _buffer;
@synthesize encodedWidth = _encodedWidth;
@synthesize encodedHeight = _encodedHeight;
@synthesize timeStamp = _timeStamp;
@synthesize captureTimeMs = _captureTimeMs;
@synthesize ntpTimeMs = _ntpTimeMs;
@synthesize isTimingFrame = _isTimingFrame;
@synthesize encodeStartMs = _encodeStartMs;
@synthesize encodeFinishMs = _encodeFinishMs;
@synthesize frameType = _frameType;
@synthesize rotation = _rotation;
@synthesize completeFrame = _completeFrame;
@synthesize qp = _qp;

- (instancetype)initWithNativeEncodedImage:(webrtc::EncodedImage)encodedImage {
  if (self = [super init]) {
    // Wrap the buffer in NSData without copying, do not take ownership.
    _buffer = [NSData dataWithBytesNoCopy:encodedImage._buffer
                                   length:encodedImage._length
                             freeWhenDone:NO];
    _encodedWidth = encodedImage._encodedWidth;
    _encodedHeight = encodedImage._encodedHeight;
    _timeStamp = encodedImage._timeStamp;
    _captureTimeMs = encodedImage.capture_time_ms_;
    _ntpTimeMs = encodedImage.ntp_time_ms_;
    _isTimingFrame = encodedImage.timing_.is_timing_frame;
    _encodeStartMs = encodedImage.timing_.encode_start_ms;
    _encodeFinishMs = encodedImage.timing_.encode_finish_ms;
    _frameType = (RTCFrameType)encodedImage._frameType;
    _rotation = encodedImage.rotation_;
    _completeFrame = encodedImage._completeFrame;
    _qp = encodedImage.qp_ == -1 ? nil : @(encodedImage.qp_);
  }

  return self;
}

- (webrtc::EncodedImage)nativeEncodedImage {
  // Return the pointer without copying.
  webrtc::EncodedImage encodedImage(
      (uint8_t *)_buffer.bytes, (size_t)_buffer.length, (size_t)_buffer.length);
  encodedImage._encodedWidth = _encodedWidth;
  encodedImage._encodedHeight = _encodedHeight;
  encodedImage._timeStamp = _timeStamp;
  encodedImage.capture_time_ms_ = _captureTimeMs;
  encodedImage.ntp_time_ms_ = _ntpTimeMs;
  encodedImage.timing_.is_timing_frame = _isTimingFrame;
  encodedImage.timing_.encode_start_ms = _encodeStartMs;
  encodedImage.timing_.encode_finish_ms = _encodeFinishMs;
  encodedImage._frameType = webrtc::FrameType(_frameType);
  encodedImage.rotation_ = webrtc::VideoRotation(_rotation);
  encodedImage._completeFrame = _completeFrame;
  encodedImage.qp_ = _qp ? _qp.intValue : -1;

  return encodedImage;
}

@end
