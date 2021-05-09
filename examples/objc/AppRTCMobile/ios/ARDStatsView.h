/*
 *  Copyright 2015 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#import <UIKit/UIKit.h>

#if defined(BUILD_WITHOUT_NINJA)
@import WebRTC;
#else
#import "sdk/objc/base/RTCMacros.h"
#endif

@class RTC_OBJC_TYPE(RTCStatisticsReport);

@interface ARDStatsView : UIView

- (void)setStats:(RTC_OBJC_TYPE(RTCStatisticsReport) *)stats;

@end
