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

#include "talk/base/criticalsection.h"
#include "webrtc/modules/video_render/include/video_render.h"

namespace cricket {
class PassthroughStream;

class WebRtcPassthroughRender : public webrtc::VideoRender {
 public:
  WebRtcPassthroughRender();
  virtual ~WebRtcPassthroughRender();

  virtual int32_t Version(int8_t* version,
                          uint32_t& remainingBufferInBytes,
                          uint32_t& position) const {
    return 0;
  }

  virtual int32_t ChangeUniqueId(const int32_t id) {
    return 0;
  }

  virtual int32_t TimeUntilNextProcess() { return 0; }

  virtual int32_t Process() { return 0; }

  virtual void* Window() {
    talk_base::CritScope cs(&render_critical_);
    return window_;
  }

  virtual int32_t ChangeWindow(void* window) {
    talk_base::CritScope cs(&render_critical_);
    window_ = window;
    return 0;
  }

  virtual webrtc::VideoRenderCallback* AddIncomingRenderStream(
      const uint32_t stream_id,
      const uint32_t zOrder,
      const float left, const float top,
      const float right, const float bottom);

  virtual int32_t DeleteIncomingRenderStream(const uint32_t stream_id);

  virtual int32_t AddExternalRenderCallback(
      const uint32_t stream_id,
      webrtc::VideoRenderCallback* render_object);

  virtual int32_t GetIncomingRenderStreamProperties(
      const uint32_t stream_id,
      uint32_t& zOrder,
      float& left, float& top,
      float& right, float& bottom) const {
    return -1;
  }

  virtual uint32_t GetIncomingFrameRate(
      const uint32_t stream_id) {
    return 0;
  }

  virtual uint32_t GetNumIncomingRenderStreams() const {
    return static_cast<uint32_t>(stream_render_map_.size());
  }

  virtual bool HasIncomingRenderStream(const uint32_t stream_id) const;

  virtual int32_t RegisterRawFrameCallback(
      const uint32_t stream_id,
      webrtc::VideoRenderCallback* callback_obj) {
    return -1;
  }

  virtual int32_t GetLastRenderedFrame(
      const uint32_t stream_id,
      webrtc::I420VideoFrame &frame) const {
    return -1;
  }

  virtual int32_t StartRender(
      const uint32_t stream_id);

  virtual int32_t StopRender(
      const uint32_t stream_id);

  virtual int32_t ResetRender() { return 0; }

  virtual webrtc::RawVideoType PreferredVideoType() const;

  virtual bool IsFullScreen() { return false; }

  virtual int32_t GetScreenResolution(uint32_t& screenWidth,
                                      uint32_t& screenHeight) const {
    return -1;
  }

  virtual uint32_t RenderFrameRate(
      const uint32_t stream_id) {
    return 0;
  }

  virtual int32_t SetStreamCropping(
      const uint32_t stream_id,
      const float left, const float top,
      const float right,
      const float bottom) {
    return -1;
  }

  virtual int32_t SetExpectedRenderDelay(uint32_t stream_id, int32_t delay_ms) {
    return -1;
  }

  virtual int32_t ConfigureRenderer(
      const uint32_t stream_id,
      const unsigned int zOrder,
      const float left, const float top,
      const float right,
      const float bottom) {
    return -1;
  }

  virtual int32_t SetTransparentBackground(const bool enable) {
    return -1;
  }

  virtual int32_t FullScreenRender(void* window, const bool enable) {
    return -1;
  }

  virtual int32_t SetBitmap(const void* bitMap,
      const uint8_t pictureId, const void* colorKey,
      const float left, const float top,
      const float right, const float bottom) {
    return -1;
  }

  virtual int32_t SetText(const uint8_t textId,
      const uint8_t* text,
      const int32_t textLength,
      const uint32_t textColorRef,
      const uint32_t backgroundColorRef,
      const float left, const float top,
      const float right, const float bottom) {
    return -1;
  }

  virtual int32_t SetStartImage(
      const uint32_t stream_id,
      const webrtc::I420VideoFrame& videoFrame) {
    return -1;
  }

  virtual int32_t SetTimeoutImage(
      const uint32_t stream_id,
      const webrtc::I420VideoFrame& videoFrame,
      const uint32_t timeout) {
    return -1;
  }

  virtual int32_t MirrorRenderStream(const int renderId,
                                     const bool enable,
                                     const bool mirrorXAxis,
                                     const bool mirrorYAxis) {
    return -1;
  }

 private:
  typedef std::map<uint32_t, PassthroughStream*> StreamMap;

  PassthroughStream* FindStream(const uint32_t stream_id) const;

  void* window_;
  StreamMap stream_render_map_;
  talk_base::CriticalSection render_critical_;
};
}  // namespace cricket

#endif  // TALK_MEDIA_WEBRTCPASSTHROUGHRENDER_H_
