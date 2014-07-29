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

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

#import "RTCVideoRenderer+Internal.h"

#if TARGET_OS_IPHONE
#import "RTCEAGLVideoView+Internal.h"
#endif
#import "RTCI420Frame+Internal.h"

namespace webrtc {

class RTCVideoRendererAdapter : public VideoRendererInterface {
 public:
  RTCVideoRendererAdapter(RTCVideoRenderer* renderer) { _renderer = renderer; }

  virtual void SetSize(int width, int height) OVERRIDE {
    [_renderer.delegate renderer:_renderer
                      didSetSize:CGSizeMake(width, height)];
  }

  virtual void RenderFrame(const cricket::VideoFrame* frame) OVERRIDE {
    if (!_renderer.delegate) {
      return;
    }
    RTCI420Frame* i420Frame = [[RTCI420Frame alloc] initWithVideoFrame:frame];
    [_renderer.delegate renderer:_renderer didReceiveFrame:i420Frame];
  }

 private:
  __weak RTCVideoRenderer* _renderer;
};
}

@implementation RTCVideoRenderer {
  rtc::scoped_ptr<webrtc::RTCVideoRendererAdapter> _adapter;
#if TARGET_OS_IPHONE
  RTCEAGLVideoView* _videoView;
#endif
}

- (instancetype)initWithDelegate:(id<RTCVideoRendererDelegate>)delegate {
  if (self = [super init]) {
    _delegate = delegate;
    _adapter.reset(new webrtc::RTCVideoRendererAdapter(self));
  }
  return self;
}

#if TARGET_OS_IPHONE
// TODO(tkchin): remove shim for deprecated method.
- (instancetype)initWithView:(UIView*)view {
  if (self = [super init]) {
    _videoView = [[RTCEAGLVideoView alloc] initWithFrame:view.bounds];
    _videoView.autoresizingMask =
        UIViewAutoresizingFlexibleHeight | UIViewAutoresizingFlexibleWidth;
    _videoView.translatesAutoresizingMaskIntoConstraints = YES;
    [view addSubview:_videoView];
    self.delegate = _videoView;
    _adapter.reset(new webrtc::RTCVideoRendererAdapter(self));
  }
  return self;
}
#endif

@end

@implementation RTCVideoRenderer (Internal)

- (webrtc::VideoRendererInterface*)videoRenderer {
  return _adapter.get();
}

@end
