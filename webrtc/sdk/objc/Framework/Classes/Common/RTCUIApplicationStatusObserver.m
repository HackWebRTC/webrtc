/*
 *  Copyright 2016 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "RTCUIApplicationStatusObserver.h"

#if defined(WEBRTC_IOS)

#import <UIKit/UIKit.h>

#include "webrtc/rtc_base/checks.h"

@implementation RTCUIApplicationStatusObserver {
  BOOL _initialized;
  dispatch_block_t _initializeBlock;
  UIApplicationState _state;
}

+ (instancetype)sharedInstance {
  static id sharedInstance;
  static dispatch_once_t onceToken;
  dispatch_once(&onceToken, ^{
    sharedInstance = [[self alloc] init];
  });

  return sharedInstance;
}

- (id)init {
  if (self = [super init]) {
    NSNotificationCenter *center = [NSNotificationCenter defaultCenter];
    [center addObserverForName:UIApplicationDidBecomeActiveNotification
                        object:nil
                         queue:[NSOperationQueue mainQueue]
                    usingBlock:^(NSNotification *note) {
                      _state = [UIApplication sharedApplication].applicationState;
                    }];

    [center addObserverForName:UIApplicationDidEnterBackgroundNotification
                        object:nil
                         queue:[NSOperationQueue mainQueue]
                    usingBlock:^(NSNotification *note) {
                      _state = [UIApplication sharedApplication].applicationState;
                    }];

    _initialized = NO;
    _initializeBlock = dispatch_block_create(DISPATCH_BLOCK_INHERIT_QOS_CLASS, ^{
      _state = [UIApplication sharedApplication].applicationState;
      _initialized = YES;
    });

    dispatch_async(dispatch_get_main_queue(), _initializeBlock);
  }

  return self;
}

- (BOOL)isApplicationActive {
  if (!_initialized) {
    long ret = dispatch_block_wait(_initializeBlock,
                                   dispatch_time(DISPATCH_TIME_NOW, 10.0 * NSEC_PER_SEC));
    RTC_DCHECK_EQ(ret, 0);
  }
  return _state == UIApplicationStateActive;
}

@end

#endif  // WEBRTC_IOS
