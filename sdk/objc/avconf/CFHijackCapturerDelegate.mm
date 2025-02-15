/*
 *  Copyright 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#import "CFHijackCapturerDelegate.h"

#import <UIKit/UIKit.h>

#import "base/RTCVideoFrameBuffer.h"
#import "components/video_frame_buffer/RTCCVPixelBuffer.h"

#include "rtc_base/logging.h"
#include "rtc_base/time_utils.h"
#include "rtc_base/timestamp_aligner.h"

#define TAG "HijackCapturerObserver "

const int64_t kBlackFrameIntervalMs = 100;

UIImage* rotate(UIImage* src, RTCVideoRotation rotation) {
    UIImageOrientation orientation = UIImageOrientationUp;
    switch (rotation) {
        case RTCVideoRotation_90:
            orientation = UIImageOrientationRight;
            break;
        case RTCVideoRotation_180:
            orientation = UIImageOrientationDown;
            break;
        case RTCVideoRotation_270:
            orientation = UIImageOrientationLeft;
            break;
        case RTCVideoRotation_0:
        default:
            orientation = UIImageOrientationUp;
            break;
    }
    return [[UIImage alloc] initWithCGImage:src.CGImage
                                      scale:1.0
                                orientation:orientation];
}

@interface EmptyVideoFrameBuffer : NSObject<RTCVideoFrameBuffer>

- (instancetype)initWithWidth:(int)width height:(int)height;

@end

@implementation CFHijackCapturerDelegate {
    id<RTCVideoCapturerDelegate> _realDelegate;
    rtc::TimestampAligner* _timestampAligner;
    RTCVideoCapturer* _dummyCapturer;

    bool _paused;

    bool _sendLastFrame;
    id<RTCVideoFrameBuffer> _lastBuffer;
    RTCVideoRotation _lastFrameRotation;
    NSString* _jpegPath;
    id<CFJpegFrameCallbackInternal> _jpegFrameCallback;

    id<CFYuvFrameCallbackInternal> _yuvFrameCallback;
    int64_t _yuvFrameCallbackIntervalNs;
    int64_t _lastYuvFrameCallbackTimestampNs;

    bool _muted;
    EmptyVideoFrameBuffer* _blackBuffer;
}

- (instancetype)initWithRealDelegate:(id<RTCVideoCapturerDelegate>)delegate {
    self = [super init];
    if (self) {
        _realDelegate = delegate;
        _timestampAligner = new rtc::TimestampAligner();
        _dummyCapturer = [[RTCVideoCapturer alloc] initWithDelegate:self];

        _paused = false;

        _sendLastFrame = false;
        _lastBuffer = nil;
        _lastFrameRotation = RTCVideoRotation_0;
        _jpegPath = nil;
        _jpegFrameCallback = nil;

        _yuvFrameCallback = nil;
        _yuvFrameCallbackIntervalNs = 0;
        _lastYuvFrameCallbackTimestampNs = 0;

        _muted = false;
        _blackBuffer =
            [[EmptyVideoFrameBuffer alloc] initWithWidth:480 height:640];
    }
    return self;
}

- (void)toggleMute:(bool)muted {
    RTC_LOG(LS_INFO) << TAG "toggleMute " << muted;
    _muted = muted;
    if (_muted) {
        __weak CFHijackCapturerDelegate* weakSelf = self;
        // frame interval of 25 fps
        [self dispatchAfter:40
                      block:^{
                          [weakSelf produceBlackFrame];
                      }];
    }
}

- (void)togglePause:(bool)pause {
    RTC_LOG(LS_INFO) << TAG "togglePause " << pause;
    _paused = pause;
}

- (void)setYuvFrameCallback:(int64_t)intervalMs callback:(nullable id<CFYuvFrameCallbackInternal>)callback {
    RTC_LOG(LS_INFO) << TAG "setYuvFrameCallback intervalMs " << intervalMs << ", callback " << (__bridge void*) callback;

    _yuvFrameCallbackIntervalNs = intervalMs * 1000 * 1000;
    _yuvFrameCallback = callback;
}

- (void)toggleSendLastFrame:(nullable NSString*)path callback:(nullable id<CFJpegFrameCallbackInternal>)callback {
    RTC_LOG(LS_INFO) << TAG "toggleSendLastFrame path " << [path UTF8String] << ", callback " << (__bridge void*) callback;

    _sendLastFrame = callback != nil;
    _jpegPath = path;
    _jpegFrameCallback = callback;
}

- (void)dispose {
    RTC_LOG(LS_INFO) << TAG "dispose";
    _muted = false;
    delete _timestampAligner;
    _timestampAligner = nullptr;

    _lastBuffer = nil;
}

- (void)capturer:(RTCVideoCapturer*)capturer
    didCaptureVideoFrame:(RTCVideoFrame*)frame {
    if (!_paused && !_muted && _timestampAligner) {
        if (_sendLastFrame && !_lastBuffer) {
            _lastBuffer = (id<RTCVideoFrameBuffer>) [frame.buffer toI420];
            _lastFrameRotation = frame.rotation;
        }

        if (!_sendLastFrame && _lastBuffer) {
            _lastBuffer = nil;
        }

        int64_t translatedTimestampNs =
            _timestampAligner->TranslateTimestamp(
                frame.timeStampNs / rtc::kNumNanosecsPerMicrosec,
                rtc::TimeMicros()) *
            rtc::kNumNanosecsPerMicrosec;

        RTCVideoFrame* newFrame =
            [[RTCVideoFrame alloc] initWithBuffer:_sendLastFrame ? _lastBuffer : frame.buffer
                                         rotation:_sendLastFrame ? _lastFrameRotation : frame.rotation
                                      timeStampNs:translatedTimestampNs];

        if (_jpegFrameCallback && _jpegPath && [frame.buffer isKindOfClass:[RTCCVPixelBuffer class]]) {
            RTC_LOG(LS_INFO) << TAG "callback last frame";

            // credit: https://stackoverflow.com/a/15726807
            // credit: https://stackoverflow.com/a/11592331
            CVPixelBufferRef pixelBuffer = ((RTCCVPixelBuffer*) frame.buffer).pixelBuffer;
            CIImage* ciImage = [CIImage imageWithCVPixelBuffer:pixelBuffer];
            CIContext* context = [CIContext contextWithOptions:nil];
            CGImageRef imageRef = [context createCGImage:ciImage
                                                fromRect:CGRectMake(0, 0,
                                                         CVPixelBufferGetWidth(pixelBuffer),
                                                         CVPixelBufferGetHeight(pixelBuffer))];

            UIImage* uiImage = [UIImage imageWithCGImage:imageRef];
            UIImage* rotated = rotate(uiImage, _lastFrameRotation);
            [UIImageJPEGRepresentation(rotated, 100) writeToFile:_jpegPath atomically:YES];

            [_jpegFrameCallback onJpegFrame:_jpegPath];

            _jpegFrameCallback = nil;
            _jpegPath = nil;
        } else if (_yuvFrameCallback && newFrame.timeStampNs - _lastYuvFrameCallbackTimestampNs >= _yuvFrameCallbackIntervalNs) {
            _lastYuvFrameCallbackTimestampNs = newFrame.timeStampNs;
            // TODO: convert and callback
        }

        [_realDelegate capturer:capturer didCaptureVideoFrame:newFrame];
    }
}

- (void)dispatchAfter:(int64_t)afterMs block:(dispatch_block_t)block {
    dispatch_after(
        dispatch_time(DISPATCH_TIME_NOW, (int64_t)(afterMs * NSEC_PER_MSEC)),
        dispatch_get_main_queue(), block);
}

- (void)produceBlackFrame {
    if (!_muted) {
        return;
    }

    RTCVideoFrame* videoFrame =
        [[RTCVideoFrame alloc] initWithBuffer:_blackBuffer
                                     rotation:RTCVideoRotation_0
                                  timeStampNs:rtc::TimeNanos()
                                        dummy:true
                                      transit:false];
    // capturer param won't be used
    [_realDelegate capturer:_dummyCapturer didCaptureVideoFrame:videoFrame];

    __weak CFHijackCapturerDelegate* weakSelf = self;
    [self dispatchAfter:kBlackFrameIntervalMs
                  block:^{
                      [weakSelf produceBlackFrame];
                  }];
}

@end

@implementation EmptyVideoFrameBuffer {
    int _width;
    int _height;
}

- (instancetype)initWithWidth:(int)width height:(int)height {
    self = [super init];
    if (self) {
        _width = width;
        _height = height;
    }
    return self;
}

- (int)width {
    return _width;
}

- (int)height {
    return _height;
}

- (id<RTCI420Buffer>)toI420 {
    return nil;
}

@end
