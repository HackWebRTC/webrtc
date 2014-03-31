/*
 * libjingle
 * Copyright 2013, Google Inc.
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

#import "APPRTCViewController.h"

#import "APPRTCVideoView.h"

@interface APPRTCViewController ()

@property(nonatomic, assign) UIInterfaceOrientation statusBarOrientation;

@end

@implementation APPRTCViewController

- (void)viewDidLoad {
  [super viewDidLoad];
  self.statusBarOrientation =
      [UIApplication sharedApplication].statusBarOrientation;
  self.roomInput.delegate = self;
  [self.roomInput becomeFirstResponder];
}

- (void)viewDidLayoutSubviews {
  if (self.statusBarOrientation !=
      [UIApplication sharedApplication].statusBarOrientation) {
    self.statusBarOrientation =
        [UIApplication sharedApplication].statusBarOrientation;
    [[NSNotificationCenter defaultCenter]
        postNotificationName:@"StatusBarOrientationDidChange"
                      object:nil];
  }
}

- (void)displayText:(NSString*)text {
  dispatch_async(dispatch_get_main_queue(), ^(void) {
      NSString* output =
          [NSString stringWithFormat:@"%@\n%@", self.logView.text, text];
      self.logView.text = output;
      [self.logView
          scrollRangeToVisible:NSMakeRange([self.logView.text length], 0)];
  });
}

- (void)resetUI {
  [self.roomInput resignFirstResponder];
  self.roomInput.text = nil;
  self.roomInput.hidden = NO;
  self.instructionsView.hidden = NO;
  self.logView.hidden = YES;
  self.logView.text = nil;
  self.blackView.hidden = YES;

  [_remoteVideoView renderVideoTrackInterface:nil];
  [_remoteVideoView removeFromSuperview];
  self.remoteVideoView = nil;

  [_localVideoView renderVideoTrackInterface:nil];
  [_localVideoView removeFromSuperview];
  self.localVideoView = nil;
}

// TODO(fischman): Use video dimensions from the incoming video stream
// and resize the Video View accordingly w.r.t. aspect ratio.
enum {
  // Remote video view dimensions.
  kRemoteVideoWidth = 640,
  kRemoteVideoHeight = 480,
  // Padding space for local video view with its parent.
  kLocalViewPadding = 20
};

- (void)setupCaptureSession {
  self.blackView.hidden = NO;

  CGRect frame =
      CGRectMake((self.blackView.bounds.size.width - kRemoteVideoWidth) / 2,
                 (self.blackView.bounds.size.height - kRemoteVideoHeight) / 2,
                 kRemoteVideoWidth,
                 kRemoteVideoHeight);
  APPRTCVideoView* videoView = [[APPRTCVideoView alloc] initWithFrame:frame];
  videoView.isRemote = TRUE;

  [self.blackView addSubview:videoView];
  videoView.autoresizingMask = UIViewAutoresizingFlexibleLeftMargin |
                               UIViewAutoresizingFlexibleRightMargin |
                               UIViewAutoresizingFlexibleBottomMargin |
                               UIViewAutoresizingFlexibleTopMargin;
  videoView.translatesAutoresizingMaskIntoConstraints = YES;
  _remoteVideoView = videoView;

  CGSize screenSize = [[UIScreen mainScreen] bounds].size;
  CGFloat localVideoViewWidth =
      UIInterfaceOrientationIsPortrait(self.statusBarOrientation)
          ? screenSize.width / 4
          : screenSize.height / 4;
  CGFloat localVideoViewHeight =
      UIInterfaceOrientationIsPortrait(self.statusBarOrientation)
          ? screenSize.height / 4
          : screenSize.width / 4;
  frame = CGRectMake(self.blackView.bounds.size.width - localVideoViewWidth -
                         kLocalViewPadding,
                     kLocalViewPadding,
                     localVideoViewWidth,
                     localVideoViewHeight);
  videoView = [[APPRTCVideoView alloc] initWithFrame:frame];
  videoView.isRemote = FALSE;

  [self.blackView addSubview:videoView];
  videoView.autoresizingMask = UIViewAutoresizingFlexibleLeftMargin |
                               UIViewAutoresizingFlexibleBottomMargin |
                               UIViewAutoresizingFlexibleHeight |
                               UIViewAutoresizingFlexibleWidth;
  videoView.translatesAutoresizingMaskIntoConstraints = YES;
  _localVideoView = videoView;
}

#pragma mark - UITextFieldDelegate

- (void)textFieldDidEndEditing:(UITextField*)textField {
  NSString* room = textField.text;
  if ([room length] == 0) {
    return;
  }
  textField.hidden = YES;
  self.instructionsView.hidden = YES;
  self.logView.hidden = NO;
  // TODO(hughv): Instead of launching a URL with apprtc scheme, change to
  // prepopulating the textField with a valid URL missing the room.  This allows
  // the user to have the simplicity of just entering the room or the ability to
  // override to a custom appspot instance.  Remove apprtc:// when this is done.
  NSString* url =
      [NSString stringWithFormat:@"apprtc://apprtc.appspot.com/?r=%@", room];
  [[UIApplication sharedApplication] openURL:[NSURL URLWithString:url]];

  dispatch_async(dispatch_get_main_queue(), ^{ [self setupCaptureSession]; });
}

- (BOOL)textFieldShouldReturn:(UITextField*)textField {
  // There is no other control that can take focus, so manually resign focus
  // when return (Join) is pressed to trigger |textFieldDidEndEditing|.
  [textField resignFirstResponder];
  return YES;
}

@end
