/*
 *  Copyright 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#import <Foundation/Foundation.h>

#import "RTCVideoDecoderH264.h"

RTC_OBJC_EXPORT
API_AVAILABLE(ios(11.0))
@interface RTC_OBJC_TYPE (RTCVideoDecoderH265) : RTC_OBJC_TYPE(RTCVideoDecoderH264)

+ (bool)supported;

@end
