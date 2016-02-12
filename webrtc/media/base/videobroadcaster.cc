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

  // Rotation must be applied by the source if one sink wants it.
  current_wants_.rotation_applied = false;
  for (auto& sink_pair : sinks_) {
    current_wants_.rotation_applied |= sink_pair.wants.rotation_applied;
  }
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

}  // namespace rtc
