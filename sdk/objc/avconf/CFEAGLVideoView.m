//
/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2018 Piasy
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */
//


#import "CFEAGLVideoView.h"
#import "CFVideoFreezeDetector.h"

#import "base/RTCLogging.h"

@interface CFEAGLVideoView() <RTCVideoViewDelegate>
@end

@implementation CFEAGLVideoView {
    id<RTCVideoViewDelegate> _userDelegate;
    CFVideoFreezeDetector* _detector;
    CFVideoRenderScaleType _scaleType;
    CGSize _videoSize;
}

- (instancetype)initWithFrame:(CGRect)frame
                       andUid:(NSString*)uid
                 andScaleType:(CFVideoRenderScaleType)scaleType {
    self = [super initWithFrame:frame];
    if (self) {
        [super setDelegate:self];
        _detector = [[CFVideoFreezeDetector alloc] initWithUid:uid];
        _scaleType = scaleType;
    }
    return self;
}

- (void)layoutSubviews {
    [super layoutSubviews];

    if (self.subviews.count > 0) {
        self.subviews[0].transform = self.mirror
                                         ? CGAffineTransformMakeScale(-1.0, 1.0)
                                         : CGAffineTransformIdentity;
    }
}

- (void)setDelegate:(id<RTCVideoViewDelegate>)delegate {
    [super setDelegate:self];
    _userDelegate = delegate;
}

- (void)renderFrame:(RTCVideoFrame*)frame {
    [super renderFrame:frame];
    [_detector onFrame];
}

- (void)videoView:(nonnull id<RTCVideoRenderer>)videoView
    didChangeVideoSize:(CGSize)size {
    _videoSize = size;
    [self adjustViewSize];

    [_userDelegate videoView:videoView didChangeVideoSize:size];
}

- (void)setScaleType:(CFVideoRenderScaleType)scaleType {
    _scaleType = scaleType;
    __weak CFEAGLVideoView* weakSelf = self;
    dispatch_async(dispatch_get_main_queue(), ^{
        CFEAGLVideoView* strongSelf = weakSelf;
        if (strongSelf) {
            [strongSelf adjustViewSize];
        }
    });
}

- (void)adjustViewSize {
    // important: video content may display out bounds without this
    self.superview.clipsToBounds = YES;
    CGRect bounds = self.superview.bounds;
    RTCLogInfo("CFEAGLVideoView adjustViewSize: bounds %fx%f, _videoSize %fx%f",
               bounds.size.width, bounds.size.height, _videoSize.width,
               _videoSize.height);
    if (bounds.size.width * bounds.size.height > 0 &&
        _videoSize.width * _videoSize.height > 0) {
        // Aspect fit remote video into bounds.
        CGRect rendererFrame =
            AVMakeRectWithAspectRatioInsideRect(_videoSize, bounds);
        CGFloat scale = 1;
        if (_scaleType == CF_SCALE_TYPE_CENTER_CROP) {
            if (rendererFrame.origin.y > 0) {
                // Scale by height.
                scale = bounds.size.height / rendererFrame.size.height;
            } else {
                // Scale by width.
                scale = bounds.size.width / rendererFrame.size.width;
            }
            rendererFrame.size.height *= scale;
            rendererFrame.size.width *= scale;
        } else if (_scaleType == CF_SCALE_TYPE_CENTER_INSIDE) {
            // do nothing
        }
        self.frame = rendererFrame;
        self.center = CGPointMake(CGRectGetMidX(bounds), CGRectGetMidY(bounds));
        RTCLogInfo(
            "CFEAGLVideoView adjustViewSize: scaleType %ld, origin.x %f, "
            "origin.y %f, scale %f, width %f, height %f",
            _scaleType, rendererFrame.origin.x, rendererFrame.origin.y, scale,
            rendererFrame.size.width, rendererFrame.size.height);
    }
}

@end
