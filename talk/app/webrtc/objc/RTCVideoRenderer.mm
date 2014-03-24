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
#import <UIKit/UIKit.h>

#import "RTCI420Frame.h"
#import "RTCVideoRendererDelegate.h"

#import "webrtc/modules/video_render/ios/video_render_ios_impl.h"
#import "webrtc/modules/video_render/ios/video_render_ios_view.h"

#include "common_video/interface/i420_video_frame.h"
#include "talk/app/webrtc/mediastreaminterface.h"
#include "talk/media/base/videoframe.h"
#include "webrtc/modules/video_render/include/video_render_defines.h"

// An adapter presenting VideoRendererInterface's API and delegating to
// a VideoRenderCallback.  Suitable for feeding to
// VideoTrackInterface::AddRenderer().
class CallbackConverter : public webrtc::VideoRendererInterface {

 public:
  CallbackConverter(webrtc::VideoRenderCallback* callback,
                    const uint32_t streamId)
      : callback_(callback), streamId_(streamId) {}

  virtual void SetSize(int width, int height) {};
  virtual void RenderFrame(const cricket::VideoFrame* frame) {
    // Make this into an I420VideoFrame.
    size_t width = frame->GetWidth();
    size_t height = frame->GetHeight();

    size_t y_plane_size = width * height;
    size_t uv_plane_size = frame->GetChromaSize();

    webrtc::I420VideoFrame i420Frame;
    i420Frame.CreateFrame(y_plane_size,
                          frame->GetYPlane(),
                          uv_plane_size,
                          frame->GetUPlane(),
                          uv_plane_size,
                          frame->GetVPlane(),
                          width,
                          height,
                          frame->GetYPitch(),
                          frame->GetUPitch(),
                          frame->GetVPitch());

    i420Frame.set_render_time_ms(frame->GetTimeStamp() / 1000000);

    callback_->RenderFrame(streamId_, i420Frame);
  }

 private:
  webrtc::VideoRenderCallback* callback_;
  const uint32_t streamId_;
};

@implementation RTCVideoRenderer {
  CallbackConverter* _converter;
  talk_base::scoped_ptr<webrtc::VideoRenderIosImpl> _iosRenderer;
}

@synthesize delegate = _delegate;

+ (RTCVideoRenderer *)videoRenderGUIWithFrame:(CGRect)frame {
  return [[RTCVideoRenderer alloc]
      initWithRenderView:[RTCVideoRenderer newRenderViewWithFrame:frame]];
}

- (id)initWithDelegate:(id<RTCVideoRendererDelegate>)delegate {
  if ((self = [super init])) {
    _delegate = delegate;
    // TODO (hughv): Create video renderer.
  }
  return self;
}

+ (UIView*)newRenderViewWithFrame:(CGRect)frame {
  VideoRenderIosView* newView =
      [[VideoRenderIosView alloc] initWithFrame:frame];
  return newView;
}

- (id)initWithRenderView:(UIView*)view {
  NSAssert([view isKindOfClass:[VideoRenderIosView class]],
           @"The view must be of kind 'VideoRenderIosView'");
  if ((self = [super init])) {
    VideoRenderIosView* renderView = (VideoRenderIosView*)view;
    _iosRenderer.reset(
        new webrtc::VideoRenderIosImpl(0, (__bridge void*)renderView, NO));
    if (_iosRenderer->Init() != -1) {
      webrtc::VideoRenderCallback* callback =
          _iosRenderer->AddIncomingRenderStream(0, 1, 0, 0, 1, 1);
      _converter = new CallbackConverter(callback, 0);
      _iosRenderer->StartRender();
    } else {
      self = nil;
    }
  }
  return self;
}

- (void)start {
  _iosRenderer->StartRender();
}

- (void)stop {
  _iosRenderer->StopRender();
}

@end

@implementation RTCVideoRenderer (Internal)

- (webrtc::VideoRendererInterface*)videoRenderer {
  return _converter;
}

@end

#else  // TARGET_OS_IPHONE

// TODO(fischman): implement an OS/X RTCVideoRenderer (and add to
// RTCPeerConnectionTest!).

#import "RTCI420Frame.h"
#import "RTCVideoRendererDelegate.h"
@implementation RTCVideoRenderer
@synthesize delegate = _delegate;
+ (RTCVideoRenderer*)videoRenderGUIWithFrame:(CGRect)frame {
  // TODO(hughv): Implement.
  return nil;
}
- (id)initWithDelegate:(id<RTCVideoRendererDelegate>)delegate {
  if ((self = [super init])) {
    _delegate = delegate;
    // TODO(hughv): Create video renderer.
  }
  return self;
}

+ (UIView*)newRenderViewWithFrame:(CGRect)frame {
  return nil;
}
- (id)initWithRenderView:(UIView*)renderView {
  return nil;
}
- (void)start {
}
- (void)stop {
}

@end
@implementation RTCVideoRenderer (Internal)
- (id)initWithVideoRenderer:(webrtc::VideoRendererInterface *)videoRenderer {
  if ((self = [super init])) {
    // TODO(hughv): Implement.
  }
  return self;
}
- (webrtc::VideoRendererInterface *)videoRenderer {
  // TODO(hughv): Implement.
  return NULL;
}
@end

#endif  // TARGET_OS_IPHONE
