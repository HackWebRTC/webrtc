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

#import "ARDVideoCallView.h"

#import <AVFoundation/AVFoundation.h>
#import "UIImage+ARDUtilities.h"

static CGFloat const kButtonPadding = 16;
static CGFloat const kButtonSize = 48;
static CGFloat const kLocalVideoViewSize = 120;
static CGFloat const kLocalVideoViewPadding = 8;

@interface ARDVideoCallView () <RTCEAGLVideoViewDelegate>
@end

@implementation ARDVideoCallView {
  UIButton *_cameraSwitchButton;
  UIButton *_hangupButton;
  CGSize _localVideoSize;
  CGSize _remoteVideoSize;
  BOOL _useRearCamera;
}

@synthesize statusLabel = _statusLabel;
@synthesize localVideoView = _localVideoView;
@synthesize remoteVideoView = _remoteVideoView;
@synthesize delegate = _delegate;

- (instancetype)initWithFrame:(CGRect)frame {
  if (self = [super initWithFrame:frame]) {
    _remoteVideoView = [[RTCEAGLVideoView alloc] initWithFrame:CGRectZero];
    _remoteVideoView.delegate = self;
    [self addSubview:_remoteVideoView];

    // TODO(tkchin): replace this with a view that renders layer from
    // AVCaptureSession.
    _localVideoView = [[RTCEAGLVideoView alloc] initWithFrame:CGRectZero];
    _localVideoView.delegate = self;
    [self addSubview:_localVideoView];

    // TODO(tkchin): don't display this if we can't actually do camera switch.
    _cameraSwitchButton = [UIButton buttonWithType:UIButtonTypeCustom];
    _cameraSwitchButton.backgroundColor = [UIColor whiteColor];
    _cameraSwitchButton.layer.cornerRadius = kButtonSize / 2;
    _cameraSwitchButton.layer.masksToBounds = YES;
    UIImage *image = [UIImage imageNamed:@"ic_switch_video_black_24dp.png"];
    [_cameraSwitchButton setImage:image forState:UIControlStateNormal];
    [_cameraSwitchButton addTarget:self
                      action:@selector(onCameraSwitch:)
            forControlEvents:UIControlEventTouchUpInside];
    [self addSubview:_cameraSwitchButton];

    _hangupButton = [UIButton buttonWithType:UIButtonTypeCustom];
    _hangupButton.backgroundColor = [UIColor redColor];
    _hangupButton.layer.cornerRadius = kButtonSize / 2;
    _hangupButton.layer.masksToBounds = YES;
    image = [UIImage imageForName:@"ic_call_end_black_24dp.png"
                            color:[UIColor whiteColor]];
    [_hangupButton setImage:image forState:UIControlStateNormal];
    [_hangupButton addTarget:self
                      action:@selector(onHangup:)
            forControlEvents:UIControlEventTouchUpInside];
    [self addSubview:_hangupButton];

    _statusLabel = [[UILabel alloc] initWithFrame:CGRectZero];
    _statusLabel.font = [UIFont fontWithName:@"Roboto" size:16];
    _statusLabel.textColor = [UIColor whiteColor];
    [self addSubview:_statusLabel];
  }
  return self;
}

- (void)layoutSubviews {
  CGRect bounds = self.bounds;
  if (_remoteVideoSize.width > 0 && _remoteVideoSize.height > 0) {
    // Aspect fill remote video into bounds.
    CGRect remoteVideoFrame =
        AVMakeRectWithAspectRatioInsideRect(_remoteVideoSize, bounds);
    CGFloat scale = 1;
    if (remoteVideoFrame.size.width > remoteVideoFrame.size.height) {
      // Scale by height.
      scale = bounds.size.height / remoteVideoFrame.size.height;
    } else {
      // Scale by width.
      scale = bounds.size.width / remoteVideoFrame.size.width;
    }
    remoteVideoFrame.size.height *= scale;
    remoteVideoFrame.size.width *= scale;
    _remoteVideoView.frame = remoteVideoFrame;
    _remoteVideoView.center =
        CGPointMake(CGRectGetMidX(bounds), CGRectGetMidY(bounds));
  } else {
    _remoteVideoView.frame = bounds;
  }

  if (_localVideoSize.width && _localVideoSize.height > 0) {
    // Aspect fit local video view into a square box.
    CGRect localVideoFrame =
        CGRectMake(0, 0, kLocalVideoViewSize, kLocalVideoViewSize);
    localVideoFrame =
        AVMakeRectWithAspectRatioInsideRect(_localVideoSize, localVideoFrame);

    // Place the view in the bottom right.
    localVideoFrame.origin.x = CGRectGetMaxX(bounds)
        - localVideoFrame.size.width - kLocalVideoViewPadding;
    localVideoFrame.origin.y = CGRectGetMaxY(bounds)
        - localVideoFrame.size.height - kLocalVideoViewPadding;
    _localVideoView.frame = localVideoFrame;
  } else {
    _localVideoView.frame = bounds;
  }

  // Place hangup button in the bottom left.
  _hangupButton.frame =
      CGRectMake(CGRectGetMinX(bounds) + kButtonPadding,
                 CGRectGetMaxY(bounds) - kButtonPadding -
                     kButtonSize,
                 kButtonSize,
                 kButtonSize);

  // Place button to the right of hangup button.
  CGRect cameraSwitchFrame = _hangupButton.frame;
  cameraSwitchFrame.origin.x =
      CGRectGetMaxX(cameraSwitchFrame) + kButtonPadding;
  _cameraSwitchButton.frame = cameraSwitchFrame;

  [_statusLabel sizeToFit];
  _statusLabel.center =
      CGPointMake(CGRectGetMidX(bounds), CGRectGetMidY(bounds));
}

#pragma mark - RTCEAGLVideoViewDelegate

- (void)videoView:(RTCEAGLVideoView*)videoView didChangeVideoSize:(CGSize)size {
  if (videoView == _localVideoView) {
    _localVideoSize = size;
    _localVideoView.hidden = CGSizeEqualToSize(CGSizeZero, _localVideoSize);
  } else if (videoView == _remoteVideoView) {
    _remoteVideoSize = size;
  }
  [self setNeedsLayout];
}

#pragma mark - Private

- (void)onCameraSwitch:(id)sender {
  [_delegate videoCallViewDidSwitchCamera:self];
}

- (void)onHangup:(id)sender {
  [_delegate videoCallViewDidHangup:self];
}

@end
