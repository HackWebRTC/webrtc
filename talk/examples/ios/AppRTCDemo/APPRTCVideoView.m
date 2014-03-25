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

/*
 * This APPRTCVideoView must be initialzed and added to a View to get
 * either the local or remote video stream rendered.
 * It is a view itself and it encapsulates
 * an object of VideoRenderIosView and UIActivityIndicatorView.
 * Both of the views will get resized as per the frame of their parent.
 */

#import "APPRTCVideoView.h"

#import "RTCVideoRenderer.h"
#import "RTCVideoTrack.h"

@interface APPRTCVideoView () {
  RTCVideoTrack* _track;
  RTCVideoRenderer* _renderer;
}

@property(nonatomic, weak) UIView* renderView;
@property(nonatomic, weak) UIActivityIndicatorView* activityView;

@end

@implementation APPRTCVideoView

@synthesize videoOrientation = _videoOrientation;

- (void)layoutSubviews {
  [super layoutSubviews];
  if (!_renderer) {
    // Left-right (mirror) flip the remote view.
    CGAffineTransform xform =
        CGAffineTransformMakeScale(self.isRemote ? -1 : 1, 1);
    // TODO(fischman): why is this rotate (vertical+horizontal flip) needed?!?
    xform = CGAffineTransformRotate(xform, M_PI);
    // TODO(fischman): ensure back-camera flip is correct in all orientations,
    // when back-camera support is added.
    [self setTransform:xform];
    _renderer = [[RTCVideoRenderer alloc] initWithView:self];
  }
}

- (void)renderVideoTrackInterface:(RTCVideoTrack*)videoTrack {
  [_track removeRenderer:_renderer];
  [_renderer stop];

  _track = videoTrack;

  if (_track) {
    [_track addRenderer:_renderer];
    [_renderer start];
  }
}

@end
