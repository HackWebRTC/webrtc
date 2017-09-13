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

@interface RTCUIApplicationStatusObserver ()

@property(nonatomic, assign) BOOL initialized;
@property(nonatomic, assign) UIApplicationState state;

@end

@implementation RTCUIApplicationStatusObserver {
  BOOL _initialized;
  dispatch_block_t _initializeBlock;
  UIApplicationState _state;

  id<NSObject> _activeObserver;
  id<NSObject> _backgroundObserver;
}

@synthesize initialized = _initialized;
@synthesize state = _state;

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
    __weak RTCUIApplicationStatusObserver *weakSelf = self;
    _activeObserver = [center addObserverForName:UIApplicationDidBecomeActiveNotification
                                          object:nil
                                           queue:[NSOperationQueue mainQueue]
                                      usingBlock:^(NSNotification *note) {
                                        weakSelf.state =
                                            [UIApplication sharedApplication].applicationState;
                                      }];

    _backgroundObserver = [center addObserverForName:UIApplicationDidEnterBackgroundNotification
                                              object:nil
                                               queue:[NSOperationQueue mainQueue]
                                          usingBlock:^(NSNotification *note) {
                                            weakSelf.state =
                                                [UIApplication sharedApplication].applicationState;
                                          }];

    _initialized = NO;
    _initializeBlock = dispatch_block_create(DISPATCH_BLOCK_INHERIT_QOS_CLASS, ^{
      weakSelf.state = [UIApplication sharedApplication].applicationState;
      weakSelf.initialized = YES;
    });

    dispatch_async(dispatch_get_main_queue(), _initializeBlock);
  }

  return self;
}

- (void)dealloc {
  NSNotificationCenter *center = [NSNotificationCenter defaultCenter];
  [center removeObserver:_activeObserver];
  [center removeObserver:_backgroundObserver];
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
