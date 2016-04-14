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
  rtc::CritScope cs(&sinks_and_wants_lock_);
  VideoSourceBase::AddOrUpdateSink(sink, wants);
  UpdateWants();
}

void VideoBroadcaster::RemoveSink(
    VideoSinkInterface<cricket::VideoFrame>* sink) {
  RTC_DCHECK(thread_checker_.CalledOnValidThread());
  RTC_DCHECK(sink != nullptr);
  rtc::CritScope cs(&sinks_and_wants_lock_);
  VideoSourceBase::RemoveSink(sink);
  UpdateWants();
}

bool VideoBroadcaster::frame_wanted() const {
  RTC_DCHECK(thread_checker_.CalledOnValidThread());
  rtc::CritScope cs(&sinks_and_wants_lock_);
  return !sink_pairs().empty();
}

VideoSinkWants VideoBroadcaster::wants() const {
  RTC_DCHECK(thread_checker_.CalledOnValidThread());
  rtc::CritScope cs(&sinks_and_wants_lock_);
  return current_wants_;
}

void VideoBroadcaster::OnFrame(const cricket::VideoFrame& frame) {
  rtc::CritScope cs(&sinks_and_wants_lock_);
  for (auto& sink_pair : sink_pairs()) {
    if (sink_pair.wants.black_frames) {
      sink_pair.sink->OnFrame(GetBlackFrame(frame));
    } else {
      sink_pair.sink->OnFrame(frame);
    }
  }
}

void VideoBroadcaster::UpdateWants() {
  RTC_DCHECK(thread_checker_.CalledOnValidThread());

  VideoSinkWants wants;
  wants.rotation_applied = false;
  for (auto& sink : sink_pairs()) {
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

const cricket::VideoFrame& VideoBroadcaster::GetBlackFrame(
    const cricket::VideoFrame& frame) {
  if (black_frame_ && black_frame_->width() == frame.width() &&
      black_frame_->height() == frame.height() &&
      black_frame_->rotation() == frame.rotation()) {
    black_frame_->set_timestamp_us(frame.timestamp_us());
    return *black_frame_;
  }
  black_frame_.reset(new cricket::WebRtcVideoFrame(
      new rtc::RefCountedObject<webrtc::I420Buffer>(frame.width(),
                                                    frame.height()),
      frame.rotation(), frame.timestamp_us()));
  black_frame_->SetToBlack();
  return *black_frame_;
}

}  // namespace rtc
