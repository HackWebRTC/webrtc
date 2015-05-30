/*
 * libjingle
 * Copyright 2004 Google Inc.
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

#ifndef TALK_MEDIA_WEBRTCPASSTHROUGHRENDER_H_
#define TALK_MEDIA_WEBRTCPASSTHROUGHRENDER_H_

#include <map>

#include "webrtc/base/criticalsection.h"
#include "webrtc/modules/video_render/include/video_render.h"

namespace cricket {
class PassthroughStream;

class WebRtcPassthroughRender : public webrtc::VideoRender {
 public:
  WebRtcPassthroughRender();
  virtual ~WebRtcPassthroughRender();

  int64_t TimeUntilNextProcess() override { return 0; }

  int32_t Process() override { return 0; }

  void* Window() override {
    rtc::CritScope cs(&render_critical_);
    return window_;
  }

  int32_t ChangeWindow(void* window) override {
    rtc::CritScope cs(&render_critical_);
    window_ = window;
    return 0;
  }

  webrtc::VideoRenderCallback* AddIncomingRenderStream(
      const uint32_t stream_id,
      const uint32_t zOrder,
      const float left,
      const float top,
      const float right,
      const float bottom) override;

  int32_t DeleteIncomingRenderStream(const uint32_t stream_id) override;

  int32_t AddExternalRenderCallback(
      const uint32_t stream_id,
      webrtc::VideoRenderCallback* render_object) override;

  int32_t GetIncomingRenderStreamProperties(const uint32_t stream_id,
                                            uint32_t& zOrder,
                                            float& left,
                                            float& top,
                                            float& right,
                                            float& bottom) const override {
    return -1;
  }

  uint32_t GetIncomingFrameRate(const uint32_t stream_id) override { return 0; }

  uint32_t GetNumIncomingRenderStreams() const override {
    return static_cast<uint32_t>(stream_render_map_.size());
  }

  bool HasIncomingRenderStream(const uint32_t stream_id) const override;

  int32_t RegisterRawFrameCallback(
      const uint32_t stream_id,
      webrtc::VideoRenderCallback* callback_obj) override {
    return -1;
  }

  int32_t StartRender(const uint32_t stream_id) override;

  int32_t StopRender(const uint32_t stream_id) override;

  int32_t ResetRender() override { return 0; }

  webrtc::RawVideoType PreferredVideoType() const override;

  bool IsFullScreen() override { return false; }

  int32_t GetScreenResolution(uint32_t& screenWidth,
                              uint32_t& screenHeight) const override {
    return -1;
  }

  uint32_t RenderFrameRate(const uint32_t stream_id) override { return 0; }

  int32_t SetStreamCropping(const uint32_t stream_id,
                            const float left,
                            const float top,
                            const float right,
                            const float bottom) override {
    return -1;
  }

  int32_t SetExpectedRenderDelay(uint32_t stream_id,
                                 int32_t delay_ms) override {
    return -1;
  }

  int32_t ConfigureRenderer(const uint32_t stream_id,
                            const unsigned int zOrder,
                            const float left,
                            const float top,
                            const float right,
                            const float bottom) override {
    return -1;
  }

  int32_t SetTransparentBackground(const bool enable) override { return -1; }

  int32_t FullScreenRender(void* window, const bool enable) override {
    return -1;
  }

  int32_t SetBitmap(const void* bitMap,
                    const uint8_t pictureId,
                    const void* colorKey,
                    const float left,
                    const float top,
                    const float right,
                    const float bottom) override {
    return -1;
  }

  int32_t SetText(const uint8_t textId,
                  const uint8_t* text,
                  const int32_t textLength,
                  const uint32_t textColorRef,
                  const uint32_t backgroundColorRef,
                  const float left,
                  const float top,
                  const float right,
                  const float bottom) override {
    return -1;
  }

  int32_t SetStartImage(const uint32_t stream_id,
                        const webrtc::VideoFrame& videoFrame) override {
    return -1;
  }

  int32_t SetTimeoutImage(const uint32_t stream_id,
                          const webrtc::VideoFrame& videoFrame,
                          const uint32_t timeout) override {
    return -1;
  }

 private:
  typedef std::map<uint32_t, PassthroughStream*> StreamMap;

  PassthroughStream* FindStream(const uint32_t stream_id) const;

  void* window_;
  StreamMap stream_render_map_;
  rtc::CriticalSection render_critical_;
};
}  // namespace cricket

#endif  // TALK_MEDIA_WEBRTCPASSTHROUGHRENDER_H_
