/*
 *  Copyright 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#import <Foundation/Foundation.h>

#include "testing/gtest/include/gtest/gtest.h"

#import "webrtc/modules/audio_device/ios/objc/RTCAudioSession.h"

@interface RTCAudioSessionTest : NSObject

- (void)testLockForConfiguration;

@end

@implementation RTCAudioSessionTest

- (void)testLockForConfiguration {
  RTCAudioSession *session = [RTCAudioSession sharedInstance];

  for (size_t i = 0; i < 2; i++) {
    [session lockForConfiguration];
    EXPECT_TRUE(session.isLocked);
  }
  for (size_t i = 0; i < 2; i++) {
    EXPECT_TRUE(session.isLocked);
    [session unlockForConfiguration];
  }
  EXPECT_FALSE(session.isLocked);
}

@end

TEST(RTCAudioSessionTest, LockForConfiguration) {
  RTCAudioSessionTest *test = [[RTCAudioSessionTest alloc] init];
  [test testLockForConfiguration];
}
