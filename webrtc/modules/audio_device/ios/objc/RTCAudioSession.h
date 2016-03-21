/*
 *  Copyright 2016 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#import <AVFoundation/AVFoundation.h>
#import <Foundation/Foundation.h>

NS_ASSUME_NONNULL_BEGIN

extern NSString * const kRTCAudioSessionErrorDomain;
/** Method that requires lock was called without lock. */
extern NSInteger const kRTCAudioSessionErrorLockRequired;
/** Unknown configuration error occurred. */
extern NSInteger const kRTCAudioSessionErrorConfiguration;

@class RTCAudioSession;
@class RTCAudioSessionConfiguration;

// Surfaces AVAudioSession events. WebRTC will listen directly for notifications
// from AVAudioSession and handle them before calling these delegate methods,
// at which point applications can perform additional processing if required.
@protocol RTCAudioSessionDelegate <NSObject>

@optional
/** Called on a system notification thread when AVAudioSession starts an
 *  interruption event.
 */
- (void)audioSessionDidBeginInterruption:(RTCAudioSession *)session;

/** Called on a system notification thread when AVAudioSession ends an
 *  interruption event.
 */
- (void)audioSessionDidEndInterruption:(RTCAudioSession *)session
                   shouldResumeSession:(BOOL)shouldResumeSession;

/** Called on a system notification thread when AVAudioSession changes the
 *  route.
 */
- (void)audioSessionDidChangeRoute:(RTCAudioSession *)session
           reason:(AVAudioSessionRouteChangeReason)reason
    previousRoute:(AVAudioSessionRouteDescription *)previousRoute;

/** Called on a system notification thread when AVAudioSession media server
 *  terminates.
 */
- (void)audioSessionMediaServicesWereLost:(RTCAudioSession *)session;

/** Called on a system notification thread when AVAudioSession media server
 *  restarts.
 */
- (void)audioSessionMediaServicesWereReset:(RTCAudioSession *)session;

// TODO(tkchin): Maybe handle SilenceSecondaryAudioHintNotification.

/** Called on a WebRTC thread when WebRTC needs to take over audio. Applications
 *  should call -[RTCAudioSession configureWebRTCSession] to allow WebRTC to
 *  play and record audio. Will only occur if shouldDelayAudioConfiguration is
 *  set to YES.
 */
- (void)audioSessionShouldConfigure:(RTCAudioSession *)session;

/** Called on a WebRTC thread when WebRTC no longer requires audio. Applications
 *  should call -[RTCAudioSession unconfigureWebRTCSession] to restore their
 *  audio session settings. Will only occur if shouldDelayAudioConfiguration is
 *  set to YES.
 */
- (void)audioSessionShouldUnconfigure:(RTCAudioSession *)session;

/** Called on a WebRTC thread when WebRTC has configured the audio session for
 *  WebRTC audio.
 */
- (void)audioSessionDidConfigure:(RTCAudioSession *)session;

/** Called on a WebRTC thread when WebRTC has unconfigured the audio session for
 *  WebRTC audio.
 */
- (void)audioSessionDidUnconfigure:(RTCAudioSession *)session;

@end

/** Proxy class for AVAudioSession that adds a locking mechanism similar to
 *  AVCaptureDevice. This is used to that interleaving configurations between
 *  WebRTC and the application layer are avoided.
 *
 *  RTCAudioSession also coordinates activation so that the audio session is
 *  activated only once. See |setActive:error:|.
 */
@interface RTCAudioSession : NSObject

/** Convenience property to access the AVAudioSession singleton. Callers should
 *  not call setters on AVAudioSession directly, but other method invocations
 *  are fine.
 */
@property(nonatomic, readonly) AVAudioSession *session;

/** Our best guess at whether the session is active based on results of calls to
 *  AVAudioSession.
 */
@property(nonatomic, readonly) BOOL isActive;
/** Whether RTCAudioSession is currently locked for configuration. */
@property(nonatomic, readonly) BOOL isLocked;

/** If YES, WebRTC will not initialize the audio unit automatically when an
 *  audio track is ready for playout or recording. Instead, applications should
 *  listen to the delegate method |audioSessionShouldConfigure| and configure
 *  the session manually. This should be set before making WebRTC media calls
 *  and should not be changed while a call is active.
 */
@property(nonatomic, assign) BOOL shouldDelayAudioConfiguration;

// Proxy properties.
@property(readonly) NSString *category;
@property(readonly) AVAudioSessionCategoryOptions categoryOptions;
@property(readonly) NSString *mode;
@property(readonly) BOOL secondaryAudioShouldBeSilencedHint;
@property(readonly) AVAudioSessionRouteDescription *currentRoute;
@property(readonly) NSInteger maximumInputNumberOfChannels;
@property(readonly) NSInteger maximumOutputNumberOfChannels;
@property(readonly) float inputGain;
@property(readonly) BOOL inputGainSettable;
@property(readonly) BOOL inputAvailable;
@property(readonly, nullable)
    NSArray<AVAudioSessionDataSourceDescription *> * inputDataSources;
@property(readonly, nullable)
  AVAudioSessionDataSourceDescription *inputDataSource;
@property(readonly, nullable)
    NSArray<AVAudioSessionDataSourceDescription *> * outputDataSources;
@property(readonly, nullable)
    AVAudioSessionDataSourceDescription *outputDataSource;
@property(readonly) double sampleRate;
@property(readonly) NSInteger inputNumberOfChannels;
@property(readonly) NSInteger outputNumberOfChannels;
@property(readonly) float outputVolume;
@property(readonly) NSTimeInterval inputLatency;
@property(readonly) NSTimeInterval outputLatency;
@property(readonly) NSTimeInterval IOBufferDuration;

/** Default constructor. */
+ (instancetype)sharedInstance;
- (instancetype)init NS_UNAVAILABLE;

/** Adds a delegate, which is held weakly. */
- (void)addDelegate:(id<RTCAudioSessionDelegate>)delegate;
/** Removes an added delegate. */
- (void)removeDelegate:(id<RTCAudioSessionDelegate>)delegate;

/** Request exclusive access to the audio session for configuration. This call
 *  will block if the lock is held by another object.
 */
- (void)lockForConfiguration;
/** Relinquishes exclusive access to the audio session. */
- (void)unlockForConfiguration;

/** If |active|, activates the audio session if it isn't already active.
 *  Successful calls must be balanced with a setActive:NO when activation is no
 *  longer required. If not |active|, deactivates the audio session if one is
 *  active and this is the last balanced call. When deactivating, the
 *  AVAudioSessionSetActiveOptionNotifyOthersOnDeactivation option is passed to
 *  AVAudioSession.
 */
- (BOOL)setActive:(BOOL)active
            error:(NSError **)outError;

// The following methods are proxies for the associated methods on
// AVAudioSession. |lockForConfiguration| must be called before using them
// otherwise they will fail with kRTCAudioSessionErrorLockRequired.

- (BOOL)setCategory:(NSString *)category
        withOptions:(AVAudioSessionCategoryOptions)options
              error:(NSError **)outError;
- (BOOL)setMode:(NSString *)mode error:(NSError **)outError;
- (BOOL)setInputGain:(float)gain error:(NSError **)outError;
- (BOOL)setPreferredSampleRate:(double)sampleRate error:(NSError **)outError;
- (BOOL)setPreferredIOBufferDuration:(NSTimeInterval)duration
                               error:(NSError **)outError;
- (BOOL)setPreferredInputNumberOfChannels:(NSInteger)count
                                    error:(NSError **)outError;
- (BOOL)setPreferredOutputNumberOfChannels:(NSInteger)count
                                     error:(NSError **)outError;
- (BOOL)overrideOutputAudioPort:(AVAudioSessionPortOverride)portOverride
                          error:(NSError **)outError;
- (BOOL)setPreferredInput:(AVAudioSessionPortDescription *)inPort
                    error:(NSError **)outError;
- (BOOL)setInputDataSource:(AVAudioSessionDataSourceDescription *)dataSource
                     error:(NSError **)outError;
- (BOOL)setOutputDataSource:(AVAudioSessionDataSourceDescription *)dataSource
                      error:(NSError **)outError;

@end

@interface RTCAudioSession (Configuration)

/** Whether or not |configureWebRTCSession| has been called without a balanced
 *  call to |unconfigureWebRTCSession|. This is not an indication of whether the
 *  audio session has the right settings.
 */
@property(readonly) BOOL isConfiguredForWebRTC;

/** Applies the configuration to the current session. Attempts to set all
 *  properties even if previous ones fail. Only the last error will be
 *  returned. Also calls setActive with |active|.
 *  |lockForConfiguration| must be called first.
 */
- (BOOL)setConfiguration:(RTCAudioSessionConfiguration *)configuration
                  active:(BOOL)active
                   error:(NSError **)outError;

/** Configure the audio session for WebRTC. This call will fail if the session
 *  is already configured. On other failures, we will attempt to restore the
 *  previously used audio session configuration.
 *  |lockForConfiguration| must be called first.
 *  Successful calls to configureWebRTCSession must be matched by calls to
 *  |unconfigureWebRTCSession|.
 */
- (BOOL)configureWebRTCSession:(NSError **)outError;

/** Unconfigures the session for WebRTC. This will attempt to restore the
 *  audio session to the settings used before |configureWebRTCSession| was
 *  called.
 *  |lockForConfiguration| must be called first.
 */
- (BOOL)unconfigureWebRTCSession:(NSError **)outError;

@end

NS_ASSUME_NONNULL_END
