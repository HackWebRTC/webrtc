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

- (BOOL)isConfiguredForWebRTC {
  return self.savedConfiguration != nil;
}

- (BOOL)setConfiguration:(RTCAudioSessionConfiguration *)configuration
                  active:(BOOL)active
                   error:(NSError **)outError {
  NSParameterAssert(configuration);
  if (outError) {
    *outError = nil;
  }
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
    } else {
      RTCLog(@"Set category to: %@", configuration.category);
    }
  }

  if (self.mode != configuration.mode) {
    NSError *modeError = nil;
    if (![self setMode:configuration.mode error:&modeError]) {
      RTCLogError(@"Failed to set mode: %@",
                  modeError.localizedDescription);
      error = modeError;
    } else {
      RTCLog(@"Set mode to: %@", configuration.mode);
    }
  }

  // self.sampleRate is accurate only if the audio session is active.
  if (!self.isActive || self.sampleRate != configuration.sampleRate) {
    NSError *sampleRateError = nil;
    if (![self setPreferredSampleRate:configuration.sampleRate
                                error:&sampleRateError]) {
      RTCLogError(@"Failed to set preferred sample rate: %@",
                  sampleRateError.localizedDescription);
      error = sampleRateError;
    } else {
      RTCLog(@"Set preferred sample rate to: %.2f",
             configuration.sampleRate);
    }
  }

  // self.IOBufferDuration is accurate only if the audio session is active.
  if (!self.isActive ||
      self.IOBufferDuration != configuration.ioBufferDuration) {
    NSError *bufferDurationError = nil;
    if (![self setPreferredIOBufferDuration:configuration.ioBufferDuration
                                      error:&bufferDurationError]) {
      RTCLogError(@"Failed to set preferred IO buffer duration: %@",
                  bufferDurationError.localizedDescription);
      error = bufferDurationError;
    } else {
      RTCLog(@"Set preferred IO buffer duration to: %f",
             configuration.ioBufferDuration);
    }
  }

  NSError *activeError = nil;
  if (![self setActive:active error:&activeError]) {
    RTCLogError(@"Failed to setActive to %d: %@",
                active, activeError.localizedDescription);
    error = activeError;
  }

  if (self.isActive &&
      // TODO(tkchin): Figure out which category/mode numChannels is valid for.
      [self.mode isEqualToString:AVAudioSessionModeVoiceChat]) {
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
      } else {
        RTCLog(@"Set input number of channels to: %ld",
               (long)inputNumberOfChannels);
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
      } else {
        RTCLog(@"Set output number of channels to: %ld",
               (long)outputNumberOfChannels);
      }
    }
  }

  if (outError) {
    *outError = error;
  }

  return error == nil;
}

- (BOOL)configureWebRTCSession:(NSError **)outError {
  if (outError) {
    *outError = nil;
  }
  if (![self checkLock:outError]) {
    return NO;
  }
  RTCLog(@"Configuring audio session for WebRTC.");

  if (self.isConfiguredForWebRTC) {
    RTCLogError(@"Already configured.");
    if (outError) {
      *outError =
          [self configurationErrorWithDescription:@"Already configured."];
    }
    return NO;
  }

  // Configure the AVAudioSession and activate it.
  // Provide an error even if there isn't one so we can log it.
  NSError *error = nil;
  RTCAudioSessionConfiguration *currentConfig =
      [RTCAudioSessionConfiguration currentConfiguration];
  RTCAudioSessionConfiguration *webRTCConfig =
      [RTCAudioSessionConfiguration webRTCConfiguration];
  self.savedConfiguration = currentConfig;
  if (![self setConfiguration:webRTCConfig active:YES error:&error]) {
    RTCLogError(@"Failed to set WebRTC audio configuration: %@",
                error.localizedDescription);
    [self unconfigureWebRTCSession:nil];
    if (outError) {
      *outError = error;
    }
    return NO;
  }

  // Ensure that the device currently supports audio input.
  // TODO(tkchin): Figure out if this is really necessary.
  if (!self.inputAvailable) {
    RTCLogError(@"No audio input path is available!");
    [self unconfigureWebRTCSession:nil];
    if (outError) {
      *outError = [self configurationErrorWithDescription:@"No input path."];
    }
    return NO;
  }

  // Give delegates a chance to process the event. In particular, the audio
  // devices listening to this event will initialize their audio units.
  [self notifyDidConfigure];

  return YES;
}

- (BOOL)unconfigureWebRTCSession:(NSError **)outError {
  if (outError) {
    *outError = nil;
  }
  if (![self checkLock:outError]) {
    return NO;
  }
  RTCLog(@"Unconfiguring audio session for WebRTC.");

  if (!self.isConfiguredForWebRTC) {
    RTCLogError(@"Already unconfigured.");
    if (outError) {
      *outError =
          [self configurationErrorWithDescription:@"Already unconfigured."];
    }
    return NO;
  }

  [self setConfiguration:self.savedConfiguration active:NO error:outError];
  self.savedConfiguration = nil;

  [self notifyDidUnconfigure];

  return YES;
}

@end
