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

#import "webrtc/base/objc/RTCLogging.h"
#import "webrtc/modules/audio_device/ios/objc/RTCAudioSession+Private.h"
#import "webrtc/modules/audio_device/ios/objc/RTCAudioSessionConfiguration.h"

@implementation RTCAudioSession (Configuration)

- (BOOL)setConfiguration:(RTCAudioSessionConfiguration *)configuration
                  active:(BOOL)active
                   error:(NSError **)outError {
  if (![self checkLock:outError]) {
    return NO;
  }

  // Provide an error even if there isn't one so we can log it. We will not
  // return immediately on error in this function and instead try to set
  // everything we can.
  NSError *error = nil;

  if (self.category != configuration.category ||
      self.categoryOptions != configuration.categoryOptions) {
    NSError *categoryError = nil;
    if (![self setCategory:configuration.category
               withOptions:configuration.categoryOptions
                     error:&categoryError]) {
      RTCLogError(@"Failed to set category: %@",
                  categoryError.localizedDescription);
      error = categoryError;
    }
  }

  if (self.mode != configuration.mode) {
    NSError *modeError = nil;
    if (![self setMode:configuration.mode error:&modeError]) {
      RTCLogError(@"Failed to set mode: %@",
                  modeError.localizedDescription);
      error = modeError;
    }
  }

  if (self.sampleRate != configuration.sampleRate) {
    NSError *sampleRateError = nil;
    if (![self setPreferredSampleRate:configuration.sampleRate
                                error:&sampleRateError]) {
      RTCLogError(@"Failed to set preferred sample rate: %@",
                  sampleRateError.localizedDescription);
      error = sampleRateError;
    }
  }

  if (self.IOBufferDuration != configuration.ioBufferDuration) {
    NSError *bufferDurationError = nil;
    if (![self setPreferredIOBufferDuration:configuration.ioBufferDuration
                                      error:&bufferDurationError]) {
      RTCLogError(@"Failed to set preferred IO buffer duration: %@",
                  bufferDurationError.localizedDescription);
      error = bufferDurationError;
    }
  }

  NSError *activeError = nil;
  if (![self setActive:active error:&activeError]) {
    RTCLogError(@"Failed to setActive to %d: %@",
                active, activeError.localizedDescription);
    error = activeError;
  }

  if (self.isActive) {
    // Try to set the preferred number of hardware audio channels. These calls
    // must be done after setting the audio session’s category and mode and
    // activating the session.
    NSInteger inputNumberOfChannels = configuration.inputNumberOfChannels;
    if (self.inputNumberOfChannels != inputNumberOfChannels) {
      NSError *inputChannelsError = nil;
      if (![self setPreferredInputNumberOfChannels:inputNumberOfChannels
                                             error:&inputChannelsError]) {
       RTCLogError(@"Failed to set preferred input number of channels: %@",
                   inputChannelsError.localizedDescription);
       error = inputChannelsError;
      }
    }
    NSInteger outputNumberOfChannels = configuration.outputNumberOfChannels;
    if (self.outputNumberOfChannels != outputNumberOfChannels) {
      NSError *outputChannelsError = nil;
      if (![self setPreferredOutputNumberOfChannels:outputNumberOfChannels
                                              error:&outputChannelsError]) {
        RTCLogError(@"Failed to set preferred output number of channels: %@",
                    outputChannelsError.localizedDescription);
        error = outputChannelsError;
      }
    }
  }

  if (outError) {
    *outError = error;
  }

  return error == nil;
}

- (BOOL)configureWebRTCSession:(NSError **)outError {
  if (![self checkLock:outError]) {
    return NO;
  }
  RTCLog(@"Configuring audio session for WebRTC.");

  // Provide an error even if there isn't one so we can log it.
  BOOL hasSucceeded = YES;
  NSError *error = nil;
  RTCAudioSessionConfiguration *currentConfig =
      [RTCAudioSessionConfiguration currentConfiguration];
  RTCAudioSessionConfiguration *webRTCConfig =
      [RTCAudioSessionConfiguration webRTCConfiguration];
  if (![self setConfiguration:webRTCConfig active:YES error:&error]) {
    RTCLogError(@"Failed to set WebRTC audio configuration: %@",
                error.localizedDescription);
    // Attempt to restore previous state.
    [self setConfiguration:currentConfig active:NO error:nil];
    hasSucceeded = NO;
  } else if (![self isConfiguredForWebRTC]) {
    // Ensure that the active audio session has the correct category and mode.
    // This should never happen - this means that we succeeded earlier but
    // somehow the settings didn't apply.
    RTCLogError(@"Failed to configure audio session.");
    // Attempt to restore previous state.
    [self setConfiguration:currentConfig active:NO error:nil];
    error =
        [[NSError alloc] initWithDomain:kRTCAudioSessionErrorDomain
                                   code:kRTCAudioSessionErrorConfiguration
                               userInfo:nil];
    hasSucceeded = NO;
  }

  if (outError) {
    *outError = error;
  }

  return hasSucceeded;
}

#pragma mark - Private

- (BOOL)isConfiguredForWebRTC {
  // Ensure that the device currently supports audio input.
  if (!self.inputAvailable) {
    RTCLogError(@"No audio input path is available!");
    return NO;
  }

  // Only check a minimal list of requirements for whether we have
  // what we want.
  RTCAudioSessionConfiguration *currentConfig =
      [RTCAudioSessionConfiguration currentConfiguration];
  RTCAudioSessionConfiguration *webRTCConfig =
      [RTCAudioSessionConfiguration webRTCConfiguration];

  if (![currentConfig.category isEqualToString:webRTCConfig.category]) {
    RTCLog(@"Current category %@ does not match %@",
           currentConfig.category,
           webRTCConfig.category);
    return NO;
  }

  if (![currentConfig.mode isEqualToString:webRTCConfig.mode]) {
    RTCLog(@"Current mode %@ does not match %@",
           currentConfig.mode,
           webRTCConfig.mode);
    return NO;
  }

  return YES;
}

@end
