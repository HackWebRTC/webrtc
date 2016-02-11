/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/media/base/capturerenderadapter.h"

#include "webrtc/base/logging.h"
#include "webrtc/media/base/videocapturer.h"
#include "webrtc/media/base/videorenderer.h"

namespace cricket {

CaptureRenderAdapter::CaptureRenderAdapter(VideoCapturer* video_capturer)
    : video_capturer_(video_capturer) {
}

CaptureRenderAdapter::~CaptureRenderAdapter() {
  // Since the signal we're connecting to is multi-threaded,
  // disconnect_all() will block until all calls are serviced, meaning any
  // outstanding calls to OnVideoFrame will be done when this is done, and no
  // more calls will be serviced by this.
  // We do this explicitly instead of just letting the has_slots<> destructor
  // take care of it because we need to do this *before* sinks_ is
  // cleared by the destructor; otherwise we could mess with it while
  // OnVideoFrame is running.
  // We *don't* take capture_crit_ here since it could deadlock with the lock
  // taken by the video frame signal.
  disconnect_all();
}

CaptureRenderAdapter* CaptureRenderAdapter::Create(
    VideoCapturer* video_capturer) {
  if (!video_capturer) {
    return NULL;
  }
  CaptureRenderAdapter* return_value = new CaptureRenderAdapter(video_capturer);
  return_value->Init();  // Can't fail.
  return return_value;
}

void CaptureRenderAdapter::AddSink(rtc::VideoSinkInterface<VideoFrame>* sink) {
  RTC_DCHECK(sink);

  rtc::CritScope cs(&capture_crit_);
  // This implements set semantics, the same renderer can only be
  // added once.
  // TODO(nisse): Is this really needed?
  if (std::find(sinks_.begin(), sinks_.end(), sink) == sinks_.end())
    sinks_.push_back(sink);
}

void CaptureRenderAdapter::RemoveSink(
    rtc::VideoSinkInterface<VideoFrame>* sink) {
  RTC_DCHECK(sink);

  rtc::CritScope cs(&capture_crit_);
  sinks_.erase(std::remove(sinks_.begin(), sinks_.end(), sink), sinks_.end());
}

void CaptureRenderAdapter::Init() {
  video_capturer_->SignalVideoFrame.connect(
      this,
      &CaptureRenderAdapter::OnVideoFrame);
}

void CaptureRenderAdapter::OnVideoFrame(VideoCapturer* capturer,
                                        const VideoFrame* video_frame) {
  rtc::CritScope cs(&capture_crit_);
  if (sinks_.empty()) {
    return;
  }

  for (auto* sink : sinks_)
    sink->OnFrame(*video_frame);
}

}  // namespace cricket
