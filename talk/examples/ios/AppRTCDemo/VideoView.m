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
 * This VideoView must be initialzed and added to a View to get
 * either the local or remote video stream rendered.
 * It is a view itself and it encapsulates
 * an object of VideoRenderIosView and UIActivityIndicatorView.
 * Both of the views will get resized as per the frame of their parent.
 */

#import "VideoView.h"

#import "RTCVideoRenderer.h"
#import "RTCVideoTrack.h"

@interface VideoView () {
    RTCVideoTrack *_track;
    RTCVideoRenderer *_renderer;
}

@property (nonatomic, weak) UIView *renderView;
@property (nonatomic, weak) UIActivityIndicatorView *activityView;

@end

@implementation VideoView

@synthesize videoOrientation = _videoOrientation;
@synthesize isRemote = _isRemote;
@synthesize renderView = _renderView;
@synthesize activityView = _activityView;

static void init(VideoView *self) {
    UIView *renderView = [RTCVideoRenderer newRenderViewWithFrame:
                          CGRectMake(0,
                                     0,
                                     self.bounds.size.width,
                                     self.bounds.size.height)];
    [self addSubview:renderView];
    renderView.autoresizingMask = UIViewAutoresizingFlexibleHeight |
                                    UIViewAutoresizingFlexibleWidth;
    renderView.translatesAutoresizingMaskIntoConstraints = YES;
    self.renderView = renderView;

    UIActivityIndicatorView *indicatorView =
        [[UIActivityIndicatorView alloc]
            initWithActivityIndicatorStyle:
                UIActivityIndicatorViewStyleWhiteLarge];
    indicatorView.frame = self.bounds;
    indicatorView.hidesWhenStopped = YES;
    [self addSubview:indicatorView];
    indicatorView.autoresizingMask = UIViewAutoresizingFlexibleWidth |
                                        UIViewAutoresizingFlexibleHeight;
    indicatorView.translatesAutoresizingMaskIntoConstraints = YES;
    [indicatorView startAnimating];
    self.activityView = indicatorView;
}

- (id)initWithFrame:(CGRect)frame {
    self = [super initWithFrame:frame];
    if (self) {
        init(self);
    }
    return self;
}

-(id)initWithCoder:(NSCoder *)aDecoder {
    self = [super initWithCoder:aDecoder];
    if (self) {
        init(self);
    }
    return self;
}

- (UIInterfaceOrientation)videoOrientation {
    return _videoOrientation;
}

- (void)setVideoOrientation:(UIInterfaceOrientation)videoOrientation {
    if (_videoOrientation != videoOrientation) {
        _videoOrientation = videoOrientation;

        CGFloat angle;
        switch (videoOrientation) {
            case UIInterfaceOrientationPortrait:
                angle = M_PI_2;
                break;
            case UIInterfaceOrientationPortraitUpsideDown:
                angle = -M_PI_2;
                break;
            case UIInterfaceOrientationLandscapeLeft:
                angle = M_PI;
                break;
            case UIInterfaceOrientationLandscapeRight:
                angle = 0;
                break;
        }
        // The video comes in mirrored. That is fine for the local video,
        // but the remote video should be put back to original.
        CGAffineTransform xform =
            CGAffineTransformMakeScale([self isRemote] ? -1 : 1, 1);
        xform = CGAffineTransformRotate(xform, angle);
        [[self renderView] setTransform:xform];
    }
}

- (void)renderVideoTrackInterface:(RTCVideoTrack *)videoTrack {
    [self stop];

    _track = videoTrack;

    if (_track) {
        if (!_renderer) {
            _renderer = [[RTCVideoRenderer alloc]
                         initWithRenderView:[self renderView]];
        }
        [_track addRenderer:_renderer];
        [self resume];
    }

    [self setVideoOrientation:UIInterfaceOrientationLandscapeLeft];
    [self setVideoOrientation:UIInterfaceOrientationPortrait];
    [self setVideoOrientation:UIInterfaceOrientationLandscapeLeft];
}

-(void)pause {
    [_renderer stop];
}

-(void)resume {
    [self.activityView stopAnimating];
    [self.activityView removeFromSuperview];
    self.activityView = nil;

    [_renderer start];
}

- (void)stop {
    [_track removeRenderer:_renderer];
    [_renderer stop];
}

@end
