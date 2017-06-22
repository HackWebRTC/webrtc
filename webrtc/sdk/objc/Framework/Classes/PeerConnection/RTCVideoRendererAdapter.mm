/*
 *  Copyright 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#import "RTCI420Buffer+Private.h"
#import "RTCVideoRendererAdapter+Private.h"
#import "WebRTC/RTCVideoFrame.h"
#import "WebRTC/RTCVideoFrameBuffer.h"
#import "objc_frame_buffer.h"

#include <memory>

namespace webrtc {

class VideoRendererAdapter
    : public rtc::VideoSinkInterface<webrtc::VideoFrame> {
 public:
  VideoRendererAdapter(RTCVideoRendererAdapter* adapter) {
    adapter_ = adapter;
    size_ = CGSizeZero;
  }

  void OnFrame(const webrtc::VideoFrame& nativeVideoFrame) override {
    rtc::scoped_refptr<VideoFrameBuffer> video_frame_buffer = nativeVideoFrame.video_frame_buffer();
    id<RTCVideoFrameBuffer> rtc_frame_buffer;
    if (video_frame_buffer->type() == VideoFrameBuffer::Type::kNative) {
      rtc::scoped_refptr<ObjCFrameBuffer> objc_frame_buffer(
          static_cast<ObjCFrameBuffer*>(video_frame_buffer.get()));
      rtc_frame_buffer = (id<RTCVideoFrameBuffer>)objc_frame_buffer->wrapped_frame_buffer();
    } else {
      rtc_frame_buffer = [[RTCI420Buffer alloc] initWithFrameBuffer:video_frame_buffer->ToI420()];
    }
    RTCVideoFrame* videoFrame = [[RTCVideoFrame alloc]
        initWithBuffer:rtc_frame_buffer
              rotation:static_cast<RTCVideoRotation>(nativeVideoFrame.rotation())
           timeStampNs:nativeVideoFrame.timestamp_us() * rtc::kNumNanosecsPerMicrosec];

    CGSize current_size = (videoFrame.rotation % 180 == 0)
                              ? CGSizeMake(videoFrame.width, videoFrame.height)
                              : CGSizeMake(videoFrame.height, videoFrame.width);

    if (!CGSizeEqualToSize(size_, current_size)) {
      size_ = current_size;
      [adapter_.videoRenderer setSize:size_];
    }
    [adapter_.videoRenderer renderFrame:videoFrame];
  }

 private:
  __weak RTCVideoRendererAdapter *adapter_;
  CGSize size_;
};
}

@implementation RTCVideoRendererAdapter {
  std::unique_ptr<webrtc::VideoRendererAdapter> _adapter;
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

- (rtc::VideoSinkInterface<webrtc::VideoFrame> *)nativeVideoRenderer {
  return _adapter.get();
}

@end
