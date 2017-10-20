/*
 *  Copyright 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#import "WebRTC/RTCBitrateAllocationStrategy.h"

#include "rtc_base/bitrateallocationstrategy.h"
#include "rtc_base/checks.h"

@implementation RTCBitrateAllocationStrategy

@synthesize strategy = _strategy;

- (instancetype)initWith:(rtc::BitrateAllocationStrategy*)strategy {
  RTC_DCHECK(strategy);
  if (self = [super init]) {
    _strategy = strategy;
  }
  return self;
}

@end
