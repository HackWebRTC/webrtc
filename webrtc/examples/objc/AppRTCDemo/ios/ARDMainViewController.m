/*
 *  Copyright 2015 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#import "ARDMainViewController.h"

#import <AVFoundation/AVFoundation.h>

#import "webrtc/base/objc/RTCDispatcher.h"
#import "webrtc/base/objc/RTCLogging.h"
#import "webrtc/modules/audio_device/ios/objc/RTCAudioSession.h"
#import "webrtc/modules/audio_device/ios/objc/RTCAudioSessionConfiguration.h"

#import "ARDAppClient.h"
#import "ARDMainView.h"
#import "ARDVideoCallViewController.h"

@interface ARDMainViewController () <
    ARDMainViewDelegate,
    RTCAudioSessionDelegate>
@end

@implementation ARDMainViewController {
  ARDMainView *_mainView;
  AVAudioPlayer *_audioPlayer;
  BOOL _shouldDelayAudioConfig;
}

- (void)loadView {
  _mainView = [[ARDMainView alloc] initWithFrame:CGRectZero];
  _mainView.delegate = self;
  self.view = _mainView;

  [self setupAudioSession];
  [self setupAudioPlayer];
}

#pragma mark - ARDMainViewDelegate

- (void)mainView:(ARDMainView *)mainView
              didInputRoom:(NSString *)room
                isLoopback:(BOOL)isLoopback
               isAudioOnly:(BOOL)isAudioOnly
    shouldDelayAudioConfig:(BOOL)shouldDelayAudioConfig {
  if (!room.length) {
    [self showAlertWithMessage:@"Missing room name."];
    return;
  }
  // Trim whitespaces.
  NSCharacterSet *whitespaceSet = [NSCharacterSet whitespaceCharacterSet];
  NSString *trimmedRoom = [room stringByTrimmingCharactersInSet:whitespaceSet];

  // Check that room name is valid.
  NSError *error = nil;
  NSRegularExpressionOptions options = NSRegularExpressionCaseInsensitive;
  NSRegularExpression *regex =
      [NSRegularExpression regularExpressionWithPattern:@"\\w+"
                                                options:options
                                                  error:&error];
  if (error) {
    [self showAlertWithMessage:error.localizedDescription];
    return;
  }
  NSRange matchRange =
      [regex rangeOfFirstMatchInString:trimmedRoom
                               options:0
                                 range:NSMakeRange(0, trimmedRoom.length)];
  if (matchRange.location == NSNotFound ||
      matchRange.length != trimmedRoom.length) {
    [self showAlertWithMessage:@"Invalid room name."];
    return;
  }

  _shouldDelayAudioConfig = shouldDelayAudioConfig;
  RTCAudioSession *session = [RTCAudioSession sharedInstance];
  session.shouldDelayAudioConfiguration = _shouldDelayAudioConfig;

  // Kick off the video call.
  ARDVideoCallViewController *videoCallViewController =
      [[ARDVideoCallViewController alloc] initForRoom:trimmedRoom
                                           isLoopback:isLoopback
                                          isAudioOnly:isAudioOnly];
  videoCallViewController.modalTransitionStyle =
      UIModalTransitionStyleCrossDissolve;
  [self presentViewController:videoCallViewController
                     animated:YES
                   completion:nil];
}

- (void)mainViewDidToggleAudioLoop:(ARDMainView *)mainView {
  if (mainView.isAudioLoopPlaying) {
    [_audioPlayer stop];
  } else {
    [_audioPlayer play];
  }
  mainView.isAudioLoopPlaying = _audioPlayer.playing;
}

#pragma mark - RTCAudioSessionDelegate

- (void)audioSessionShouldConfigure:(RTCAudioSession *)session {
  // Won't get called unless audio config is delayed.
  // Stop playback on main queue and then configure WebRTC.
  [RTCDispatcher dispatchAsyncOnType:RTCDispatcherTypeMain
                               block:^{
    if (_mainView.isAudioLoopPlaying) {
      RTCLog(@"Stopping audio loop due to WebRTC start.");
      [_audioPlayer stop];
    }
    // TODO(tkchin): Shouldn't lock on main queue. Figure out better way to
    // check audio loop state.
    [session lockForConfiguration];
    [session configureWebRTCSession:nil];
    [session unlockForConfiguration];
  }];
}

- (void)audioSessionShouldUnconfigure:(RTCAudioSession *)session {
  // Won't get called unless audio config is delayed.
  [session lockForConfiguration];
  [session unconfigureWebRTCSession:nil];
  [session unlockForConfiguration];
}

- (void)audioSessionDidUnconfigure:(RTCAudioSession *)session {
  // WebRTC is done with the audio session. Restart playback.
  [RTCDispatcher dispatchAsyncOnType:RTCDispatcherTypeMain
                               block:^{
    if (_mainView.isAudioLoopPlaying) {
      RTCLog(@"Starting audio loop due to WebRTC end.");
      [_audioPlayer play];
    }
  }];
}

#pragma mark - Private

- (void)setupAudioSession {
  RTCAudioSessionConfiguration *configuration =
      [[RTCAudioSessionConfiguration alloc] init];
  configuration.category = AVAudioSessionCategoryAmbient;
  configuration.categoryOptions = AVAudioSessionCategoryOptionDuckOthers;
  configuration.mode = AVAudioSessionModeDefault;

  RTCAudioSession *session = [RTCAudioSession sharedInstance];
  [session addDelegate:self];
  [session lockForConfiguration];
  NSError *error = nil;
  if (![session setConfiguration:configuration active:YES error:&error]) {
    RTCLogError(@"Error setting configuration: %@", error.localizedDescription);
  }
  [session unlockForConfiguration];
}

- (void)setupAudioPlayer {
  NSString *audioFilePath =
      [[NSBundle mainBundle] pathForResource:@"mozart" ofType:@"mp3"];
  NSURL *audioFileURL = [NSURL URLWithString:audioFilePath];
  _audioPlayer = [[AVAudioPlayer alloc] initWithContentsOfURL:audioFileURL
                                                        error:nil];
  _audioPlayer.numberOfLoops = -1;
  _audioPlayer.volume = 1.0;
  [_audioPlayer prepareToPlay];
}

- (void)showAlertWithMessage:(NSString*)message {
  UIAlertView* alertView = [[UIAlertView alloc] initWithTitle:nil
                                                      message:message
                                                     delegate:nil
                                            cancelButtonTitle:@"OK"
                                            otherButtonTitles:nil];
  [alertView show];
}

@end
