/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_COMMON_VIDEO_INCLUDE_INCOMING_VIDEO_STREAM_H_
#define WEBRTC_COMMON_VIDEO_INCLUDE_INCOMING_VIDEO_STREAM_H_

#include <memory>

#include "webrtc/base/criticalsection.h"
#include "webrtc/base/platform_thread.h"
#include "webrtc/base/thread_annotations.h"
#include "webrtc/common_video/video_render_frames.h"
#include "webrtc/media/base/videosinkinterface.h"

namespace webrtc {
class EventTimerWrapper;


class IncomingVideoStream : public rtc::VideoSinkInterface<VideoFrame> {
 public:
  explicit IncomingVideoStream(bool disable_prerenderer_smoothing);
  ~IncomingVideoStream();

  // Overrides VideoSinkInterface
  void OnFrame(const VideoFrame& video_frame) override;

  // Set callback to the platform dependent code.
  void SetRenderCallback(rtc::VideoSinkInterface<VideoFrame>* render_callback);

  // Callback for file recording, snapshot, ...
  void SetExternalCallback(rtc::VideoSinkInterface<VideoFrame>* render_object);

  // Start/Stop.
  int32_t Start();
  int32_t Stop();

  // Clear all buffers.
  int32_t Reset();

  // Properties.
  uint32_t IncomingRate() const;

  int32_t SetExpectedRenderDelay(int32_t delay_ms);

 protected:
  static bool IncomingVideoStreamThreadFun(void* obj);
  bool IncomingVideoStreamProcess();

 private:
  enum { kEventStartupTimeMs = 10 };
  enum { kEventMaxWaitTimeMs = 100 };
  enum { kFrameRatePeriodMs = 1000 };

  void DeliverFrame(const VideoFrame& video_frame);

  const bool disable_prerenderer_smoothing_;
  // Critsects in allowed to enter order.
  rtc::CriticalSection stream_critsect_;
  rtc::CriticalSection thread_critsect_;
  rtc::CriticalSection buffer_critsect_;
  // TODO(pbos): Make plain member and stop resetting this thread, just
  // start/stoping it is enough.
  std::unique_ptr<rtc::PlatformThread> incoming_render_thread_
      GUARDED_BY(thread_critsect_);
  std::unique_ptr<EventTimerWrapper> deliver_buffer_event_;

  bool running_ GUARDED_BY(stream_critsect_);
  rtc::VideoSinkInterface<VideoFrame>* external_callback_
      GUARDED_BY(thread_critsect_);
  rtc::VideoSinkInterface<VideoFrame>* render_callback_
      GUARDED_BY(thread_critsect_);
  const std::unique_ptr<VideoRenderFrames> render_buffers_
      GUARDED_BY(buffer_critsect_);

  uint32_t incoming_rate_ GUARDED_BY(stream_critsect_);
  int64_t last_rate_calculation_time_ms_ GUARDED_BY(stream_critsect_);
  uint16_t num_frames_since_last_calculation_ GUARDED_BY(stream_critsect_);
};

}  // namespace webrtc

#endif  // WEBRTC_COMMON_VIDEO_INCLUDE_INCOMING_VIDEO_STREAM_H_
