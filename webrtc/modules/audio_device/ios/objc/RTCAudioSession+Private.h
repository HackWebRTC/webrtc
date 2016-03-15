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

#include <vector>

NS_ASSUME_NONNULL_BEGIN

@interface RTCAudioSession ()

/** Number of times setActive:YES has succeeded without a balanced call to
 *  setActive:NO.
 */
@property(nonatomic, readonly) NSInteger activationCount;

- (BOOL)checkLock:(NSError **)outError;

/** Adds the delegate to the list of delegates, and places it at the front of
 *  the list. This delegate will be notified before other delegates of
 *  audio events.
 */
- (void)pushDelegate:(id<RTCAudioSessionDelegate>)delegate;

// Properties and methods for tests.
@property(nonatomic, readonly)
    std::vector<__weak id<RTCAudioSessionDelegate> > delegates;
- (void)notifyDidBeginInterruption;
- (void)notifyDidEndInterruptionWithShouldResumeSession:
    (BOOL)shouldResumeSession;
- (void)notifyDidChangeRouteWithReason:(AVAudioSessionRouteChangeReason)reason
    previousRoute:(AVAudioSessionRouteDescription *)previousRoute;
- (void)notifyMediaServicesWereLost;
- (void)notifyMediaServicesWereReset;

@end

NS_ASSUME_NONNULL_END
