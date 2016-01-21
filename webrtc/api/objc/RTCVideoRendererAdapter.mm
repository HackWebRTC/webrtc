/*
 *  Copyright 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#import "RTCVideoRendererAdapter.h"

#import "webrtc/api/objc/RTCVideoFrame+Private.h"
#import "webrtc/api/objc/RTCVideoRendererAdapter+Private.h"

namespace webrtc {

class VideoRendererAdapter : public VideoRendererInterface {
 public:
  VideoRendererAdapter(RTCVideoRendererAdapter* adapter) {
    adapter_ = adapter;
    size_ = CGSizeZero;
  }

  void RenderFrame(const cricket::VideoFrame *nativeVideoFrame) override {
    const cricket::VideoFrame *frame =
        nativeVideoFrame->GetCopyWithRotationApplied();
    CGSize current_size = CGSizeMake(frame->GetWidth(), frame->GetHeight());
    if (!CGSizeEqualToSize(size_, current_size)) {
      size_ = current_size;
      [adapter_.videoRenderer setSize:size_];
    }
    RTCVideoFrame *videoFrame =
        [[RTCVideoFrame alloc] initWithNativeFrame:frame];
    [adapter_.videoRenderer renderFrame:videoFrame];
  }

 private:
  __weak RTCVideoRendererAdapter *adapter_;
  CGSize size_;
};
}

@implementation RTCVideoRendererAdapter {
  rtc::scoped_ptr<webrtc::VideoRendererAdapter> _adapter;
}

@synthesize videoRenderer = _videoRenderer;

- (instancetype)initWithNativeRenderer:(id<RTCVideoRenderer>)videoRenderer {
  NSParameterAssert(videoRenderer);
  if (self = [super init]) {
    _videoRenderer = videoRenderer;
    _adapter.reset(new webrtc::VideoRendererAdapter(self));
  }
  return self;
}

- (webrtc::VideoRendererInterface *)nativeVideoRenderer {
  return _adapter.get();
}

@end
