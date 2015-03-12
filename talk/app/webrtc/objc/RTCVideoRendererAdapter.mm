/*
 * libjingle
 * Copyright 2013 Google Inc.
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

#import "RTCVideoRendererAdapter.h"
#import "RTCI420Frame+Internal.h"

namespace webrtc {

class RTCVideoRendererNativeAdapter : public VideoRendererInterface {
 public:
  RTCVideoRendererNativeAdapter(RTCVideoRendererAdapter* adapter) {
    _adapter = adapter;
    _size = CGSizeZero;
  }

  void RenderFrame(const cricket::VideoFrame* videoFrame) override {
    const cricket::VideoFrame* frame = videoFrame->GetCopyWithRotationApplied();
    CGSize currentSize = CGSizeMake(frame->GetWidth(), frame->GetHeight());
    if (!CGSizeEqualToSize(_size, currentSize)) {
      _size = currentSize;
      [_adapter.videoRenderer setSize:_size];
    }
    RTCI420Frame* i420Frame = [[RTCI420Frame alloc] initWithVideoFrame:frame];
    [_adapter.videoRenderer renderFrame:i420Frame];
  }

 private:
  __weak RTCVideoRendererAdapter* _adapter;
  CGSize _size;
};
}

@implementation RTCVideoRendererAdapter {
  id<RTCVideoRenderer> _videoRenderer;
  rtc::scoped_ptr<webrtc::RTCVideoRendererNativeAdapter> _adapter;
}

- (instancetype)initWithVideoRenderer:(id<RTCVideoRenderer>)videoRenderer {
  NSParameterAssert(videoRenderer);
  if (self = [super init]) {
    _videoRenderer = videoRenderer;
    _adapter.reset(new webrtc::RTCVideoRendererNativeAdapter(self));
  }
  return self;
}

- (webrtc::VideoRendererInterface*)nativeVideoRenderer {
  return _adapter.get();
}

@end
