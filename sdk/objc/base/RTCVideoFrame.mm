/*
 *  Copyright 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#import "RTCVideoFrame.h"

#import "RTCI420Buffer.h"
#import "RTCVideoFrameBuffer.h"

@implementation RTC_OBJC_TYPE (RTCVideoFrame) {
  RTCVideoRotation _rotation;
  int64_t _timeStampNs;
  bool _dummy;
  bool _transit;
}

@synthesize buffer = _buffer;
@synthesize timeStamp;

- (int)width {
  return _buffer.width;
}

- (int)height {
  return _buffer.height;
}

- (RTCVideoRotation)rotation {
  return _rotation;
}

- (int64_t)timeStampNs {
  return _timeStampNs;
}

- (bool)dummy {
  return _dummy;
}

- (bool)transit {
  return _transit;
}

- (RTC_OBJC_TYPE(RTCVideoFrame) *)newI420VideoFrame {
  return [[RTC_OBJC_TYPE(RTCVideoFrame) alloc] initWithBuffer:[_buffer toI420]
                                                     rotation:_rotation
                                                  timeStampNs:_timeStampNs];
}

- (instancetype)initWithBuffer:(id<RTC_OBJC_TYPE(RTCVideoFrameBuffer)>)buffer
                      rotation:(RTCVideoRotation)rotation
                   timeStampNs:(int64_t)timeStampNs {
  self = [super init];
  if (self) {
    _buffer = buffer;
    _rotation = rotation;
    _timeStampNs = timeStampNs;
    _dummy = false;
    _transit = false;
  }
  return self;
}

- (instancetype)initWithBuffer:(id<RTCVideoFrameBuffer>)buffer
                      rotation:(RTCVideoRotation)rotation
                   timeStampNs:(int64_t)timeStampNs
                         dummy:(bool)dummy
                       transit:(bool)transit {
    if ((self = [super init])) {
      _buffer = buffer;
      _rotation = rotation;
      _timeStampNs = timeStampNs;
      _dummy = dummy;
      _transit = transit;
    }

    return self;
}

@end
