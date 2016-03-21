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

@class RTCAudioSessionConfiguration;

@interface RTCAudioSession ()

/** Number of times setActive:YES has succeeded without a balanced call to
 *  setActive:NO.
 */
@property(nonatomic, readonly) int activationCount;

/** The number of times |beginWebRTCSession| was called without a balanced call
 *  to |endWebRTCSession|.
 */
@property(nonatomic, readonly) int webRTCSessionCount;

/** The configuration of the audio session before configureWebRTCSession
 *  was first called.
 */
@property(nonatomic, strong, nullable)
    RTCAudioSessionConfiguration *savedConfiguration;

- (BOOL)checkLock:(NSError **)outError;

/** Adds the delegate to the list of delegates, and places it at the front of
 *  the list. This delegate will be notified before other delegates of
 *  audio events.
 */
- (void)pushDelegate:(id<RTCAudioSessionDelegate>)delegate;

/** Signals RTCAudioSession that a WebRTC session is about to begin and
 *  audio configuration is needed. Will configure the audio session for WebRTC
 *  if not already configured and if configuration is not delayed.
 *  Successful calls must be balanced by a call to endWebRTCSession.
 */
- (BOOL)beginWebRTCSession:(NSError **)outError;

/** Signals RTCAudioSession that a WebRTC session is about to end and audio
 *  unconfiguration is needed. Will unconfigure the audio session for WebRTC
 *  if this is the last unmatched call and if configuration is not delayed.
 */
- (BOOL)endWebRTCSession:(NSError **)outError;

/** Returns a configuration error with the given description. */
- (NSError *)configurationErrorWithDescription:(NSString *)description;

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
- (void)notifyShouldConfigure;
- (void)notifyShouldUnconfigure;
- (void)notifyDidConfigure;
- (void)notifyDidUnconfigure;

@end

NS_ASSUME_NONNULL_END
