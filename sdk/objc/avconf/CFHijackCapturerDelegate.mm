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

#import "base/RTCVideoFrameBuffer.h"

#include "rtc_base/logging.h"
#include "rtc_base/time_utils.h"
#include "rtc_base/timestamp_aligner.h"

const int64_t kBlackFrameIntervalMs = 100;

@interface EmptyVideoFrameBuffer : NSObject<RTCVideoFrameBuffer>

- (instancetype)initWithWidth:(int)width height:(int)height;

@end

@implementation CFHijackCapturerDelegate {
    id<RTCVideoCapturerDelegate> _realDelegate;
    bool _muted;
    rtc::TimestampAligner* _timestampAligner;
    EmptyVideoFrameBuffer* _blackBuffer;
    RTCVideoCapturer* _dummyCapturer;
}

- (instancetype)initWithRealDelegate:(id<RTCVideoCapturerDelegate>)delegate {
    self = [super init];
    if (self) {
        _realDelegate = delegate;
        _muted = false;
        _timestampAligner = new rtc::TimestampAligner();
        _blackBuffer =
            [[EmptyVideoFrameBuffer alloc] initWithWidth:480 height:640];
        _dummyCapturer = [[RTCVideoCapturer alloc] initWithDelegate:self];
    }
    return self;
}

- (void)toggleMute:(bool)muted {
    RTC_LOG(LS_INFO) << "HijackCapturerObserver toggleMute " << muted;
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

- (void)dispose {
    RTC_LOG(LS_INFO) << "HijackCapturerObserver dispose";
    _muted = false;
    delete _timestampAligner;
    _timestampAligner = nullptr;
}

- (void)capturer:(RTCVideoCapturer*)capturer
    didCaptureVideoFrame:(RTCVideoFrame*)frame {
    if (!_muted && _timestampAligner) {
        int64_t translatedTimestampNs =
            _timestampAligner->TranslateTimestamp(
                frame.timeStampNs / rtc::kNumNanosecsPerMicrosec,
                rtc::TimeMicros()) *
            rtc::kNumNanosecsPerMicrosec;
        RTCVideoFrame* videoFrame =
            [[RTCVideoFrame alloc] initWithBuffer:frame.buffer
                                         rotation:frame.rotation
                                      timeStampNs:translatedTimestampNs];
        [_realDelegate capturer:capturer didCaptureVideoFrame:videoFrame];
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
                                  timeStampNs:rtc::TimeNanos()];
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
