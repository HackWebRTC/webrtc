/*
 *  Copyright 2016 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#import "webrtc/modules/audio_device/ios/objc/RTCAudioSession.h"

NS_ASSUME_NONNULL_BEGIN

@interface RTCAudioSession ()

/** The lock that guards access to AVAudioSession methods. */
@property(nonatomic, strong) NSRecursiveLock *lock;

/** The delegates. */
@property(nonatomic, readonly) NSSet *delegates;

/** Number of times setActive:YES has succeeded without a balanced call to
 *  setActive:NO.
 */
@property(nonatomic, readonly) NSInteger activationCount;

@end

NS_ASSUME_NONNULL_END
