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


#import "CFMTLVideoView.h"
#import "CFVideoFreezeDetector.h"

#if defined(__aarch64__)

@implementation CFMTLVideoView {
    CFVideoFreezeDetector* _detector;
}

- (instancetype)initWithFrame:(CGRect)frame andUid:(NSString*)uid {
    self = [super initWithFrame:frame];
    if (self) {
        _detector = [[CFVideoFreezeDetector alloc] initWithUid:uid];
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

- (void)renderFrame:(RTCVideoFrame*)frame {
    [super renderFrame:frame];
    [_detector onFrame];
}

@end

#endif
