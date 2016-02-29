/*
 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/media/base/videobroadcaster.h"

#include <limits>

#include "webrtc/base/checks.h"

namespace rtc {

VideoBroadcaster::VideoBroadcaster() {
  thread_checker_.DetachFromThread();
}

void VideoBroadcaster::AddOrUpdateSink(
    VideoSinkInterface<cricket::VideoFrame>* sink,
    const VideoSinkWants& wants) {
  RTC_DCHECK(thread_checker_.CalledOnValidThread());
  RTC_DCHECK(sink != nullptr);

  SinkPair* sink_pair = FindSinkPair(sink);
  if (!sink_pair) {
    sinks_.push_back(SinkPair(sink, wants));
  } else {
    sink_pair->wants = wants;
  }
  UpdateWants();
}

void VideoBroadcaster::RemoveSink(
    VideoSinkInterface<cricket::VideoFrame>* sink) {
  RTC_DCHECK(thread_checker_.CalledOnValidThread());
  RTC_DCHECK(sink != nullptr);
  RTC_DCHECK(FindSinkPair(sink));

  sinks_.erase(std::remove_if(sinks_.begin(), sinks_.end(),
                              [sink](const SinkPair& sink_pair) {
                                return sink_pair.sink == sink;
                              }),
               sinks_.end());
  UpdateWants();
}

bool VideoBroadcaster::frame_wanted() const {
  RTC_DCHECK(thread_checker_.CalledOnValidThread());
  return !sinks_.empty();
}

VideoSinkWants VideoBroadcaster::wants() const {
  RTC_DCHECK(thread_checker_.CalledOnValidThread());
  return current_wants_;
}

void VideoBroadcaster::OnFrame(const cricket::VideoFrame& frame) {
  RTC_DCHECK(thread_checker_.CalledOnValidThread());
  for (auto& sink_pair : sinks_) {
    sink_pair.sink->OnFrame(frame);
  }
}

VideoBroadcaster::SinkPair* VideoBroadcaster::FindSinkPair(
    const VideoSinkInterface<cricket::VideoFrame>* sink) {
  auto sink_pair_it = std::find_if(
      sinks_.begin(), sinks_.end(),
      [sink](const SinkPair& sink_pair) { return sink_pair.sink == sink; });
  if (sink_pair_it != sinks_.end()) {
    return &*sink_pair_it;
  }
  return nullptr;
}

void VideoBroadcaster::UpdateWants() {
  RTC_DCHECK(thread_checker_.CalledOnValidThread());

  VideoSinkWants wants;
  wants.rotation_applied = false;
  for (auto& sink : sinks_) {
    // wants.rotation_applied == ANY(sink.wants.rotation_applied)
    if (sink.wants.rotation_applied) {
      wants.rotation_applied = true;
    }
    // wants.max_pixel_count == MIN(sink.wants.max_pixel_count)
    if (sink.wants.max_pixel_count &&
        (!wants.max_pixel_count ||
         (*sink.wants.max_pixel_count < *wants.max_pixel_count))) {
      wants.max_pixel_count = sink.wants.max_pixel_count;
    }
    // wants.max_pixel_count_step_up == MIN(sink.wants.max_pixel_count_step_up)
    if (sink.wants.max_pixel_count_step_up &&
        (!wants.max_pixel_count_step_up ||
         (*sink.wants.max_pixel_count_step_up <
          *wants.max_pixel_count_step_up))) {
      wants.max_pixel_count_step_up = sink.wants.max_pixel_count_step_up;
    }
  }

  if (wants.max_pixel_count && wants.max_pixel_count_step_up &&
      *wants.max_pixel_count_step_up >= *wants.max_pixel_count) {
    wants.max_pixel_count_step_up = Optional<int>();
  }
  current_wants_ = wants;
}

}  // namespace rtc
