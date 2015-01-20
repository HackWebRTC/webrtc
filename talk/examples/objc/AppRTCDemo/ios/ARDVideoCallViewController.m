/*
 * libjingle
 * Copyright 2015 Google Inc.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 *  1. Redistributions of source code must retain the above copyright notice,
 *     this list of conditions and the following disclaimer.
 *  2. Redistributions in binary form must reproduce the above copyright notice,
 *     this list of conditions and the following disclaimer in the documentation
 *     and/or other materials provided with the distribution.
 *  3. The name of the author may not be used to endorse or promote products
 *     derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO
 * EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#import "ARDVideoCallViewController.h"

#import "ARDAppClient.h"
#import "ARDVideoCallView.h"

@interface ARDVideoCallViewController () <ARDAppClientDelegate,
    ARDVideoCallViewDelegate>
@property(nonatomic, readonly) ARDVideoCallView *videoCallView;
@end

@implementation ARDVideoCallViewController {
  ARDAppClient *_client;
  RTCVideoTrack *_remoteVideoTrack;
  RTCVideoTrack *_localVideoTrack;
}

@synthesize videoCallView = _videoCallView;

- (instancetype)initForRoom:(NSString *)room {
  if (self = [super init]) {
    _client = [[ARDAppClient alloc] initWithDelegate:self];
    [_client connectToRoomWithId:room options:nil];
  }
  return self;
}

- (void)loadView {
  _videoCallView = [[ARDVideoCallView alloc] initWithFrame:CGRectZero];
  _videoCallView.delegate = self;
  _videoCallView.statusLabel.text =
      [self statusTextForState:RTCICEConnectionNew];
  self.view = _videoCallView;
}

#pragma mark - ARDAppClientDelegate

- (void)appClient:(ARDAppClient *)client
    didChangeState:(ARDAppClientState)state {
  switch (state) {
    case kARDAppClientStateConnected:
      NSLog(@"Client connected.");
      break;
    case kARDAppClientStateConnecting:
      NSLog(@"Client connecting.");
      break;
    case kARDAppClientStateDisconnected:
      NSLog(@"Client disconnected.");
      [self hangup];
      break;
  }
}

- (void)appClient:(ARDAppClient *)client
    didChangeConnectionState:(RTCICEConnectionState)state {
  NSLog(@"ICE state changed: %d", state);
  __weak ARDVideoCallViewController *weakSelf = self;
  dispatch_async(dispatch_get_main_queue(), ^{
    ARDVideoCallViewController *strongSelf = weakSelf;
    strongSelf.videoCallView.statusLabel.text =
        [strongSelf statusTextForState:state];
  });
}

- (void)appClient:(ARDAppClient *)client
    didReceiveLocalVideoTrack:(RTCVideoTrack *)localVideoTrack {
  if (!_localVideoTrack) {
    _localVideoTrack = localVideoTrack;
    [_localVideoTrack addRenderer:_videoCallView.localVideoView];
  }
}

- (void)appClient:(ARDAppClient *)client
    didReceiveRemoteVideoTrack:(RTCVideoTrack *)remoteVideoTrack {
  if (!_remoteVideoTrack) {
    _remoteVideoTrack = remoteVideoTrack;
    [_remoteVideoTrack addRenderer:_videoCallView.remoteVideoView];
    _videoCallView.statusLabel.hidden = YES;
  }
}

- (void)appClient:(ARDAppClient *)client
         didError:(NSError *)error {
  NSString *message =
      [NSString stringWithFormat:@"%@", error.localizedDescription];
  [self showAlertWithMessage:message];
  [self hangup];
}

#pragma mark - ARDVideoCallViewDelegate

- (void)videoCallViewDidHangup:(ARDVideoCallView *)view {
  [self hangup];
}

#pragma mark - Private

- (void)hangup {
  if (_remoteVideoTrack) {
    [_remoteVideoTrack removeRenderer:_videoCallView.remoteVideoView];
    _remoteVideoTrack = nil;
    [_videoCallView.remoteVideoView renderFrame:nil];
  }
  if (_localVideoTrack) {
    [_localVideoTrack removeRenderer:_videoCallView.localVideoView];
    _localVideoTrack = nil;
    [_videoCallView.localVideoView renderFrame:nil];
  }
  [_client disconnect];
  [self.presentingViewController dismissViewControllerAnimated:YES
                                                    completion:nil];
}

- (NSString *)statusTextForState:(RTCICEConnectionState)state {
  switch (state) {
    case RTCICEConnectionNew:
    case RTCICEConnectionChecking:
      return @"Connecting...";
    case RTCICEConnectionConnected:
    case RTCICEConnectionCompleted:
    case RTCICEConnectionFailed:
    case RTCICEConnectionDisconnected:
    case RTCICEConnectionClosed:
      return nil;
  }
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
