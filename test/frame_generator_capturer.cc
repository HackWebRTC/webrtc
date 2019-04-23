/*
 *  Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "test/frame_generator_capturer.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <utility>
#include <vector>

#include "rtc_base/checks.h"
#include "rtc_base/critical_section.h"
#include "rtc_base/logging.h"
#include "rtc_base/task_queue.h"
#include "rtc_base/task_utils/repeating_task.h"
#include "rtc_base/time_utils.h"
#include "system_wrappers/include/clock.h"

namespace webrtc {
namespace test {

FrameGeneratorCapturer::FrameGeneratorCapturer(
    Clock* clock,
    std::unique_ptr<FrameGenerator> frame_generator,
    int target_fps,
    TaskQueueFactory& task_queue_factory)
    : clock_(clock),
      sending_(true),
      sink_wants_observer_(nullptr),
      frame_generator_(std::move(frame_generator)),
      source_fps_(target_fps),
      target_capture_fps_(target_fps),
      first_frame_capture_time_(-1),
      task_queue_(task_queue_factory.CreateTaskQueue(
          "FrameGenCapQ",
          TaskQueueFactory::Priority::HIGH)) {
  RTC_DCHECK(frame_generator_);
  RTC_DCHECK_GT(target_fps, 0);
}

FrameGeneratorCapturer::~FrameGeneratorCapturer() {
  Stop();
}

void FrameGeneratorCapturer::SetFakeRotation(VideoRotation rotation) {
  rtc::CritScope cs(&lock_);
  fake_rotation_ = rotation;
}

void FrameGeneratorCapturer::SetFakeColorSpace(
    absl::optional<ColorSpace> color_space) {
  rtc::CritScope cs(&lock_);
  fake_color_space_ = color_space;
}

bool FrameGeneratorCapturer::Init() {
  // This check is added because frame_generator_ might be file based and should
  // not crash because a file moved.
  if (frame_generator_.get() == nullptr)
    return false;

  RepeatingTaskHandle::DelayedStart(
      task_queue_.Get(),
      TimeDelta::seconds(1) / GetCurrentConfiguredFramerate(), [this] {
        InsertFrame();
        return TimeDelta::seconds(1) / GetCurrentConfiguredFramerate();
      });
  return true;
}

void FrameGeneratorCapturer::InsertFrame() {
  rtc::CritScope cs(&lock_);
  if (sending_) {
    VideoFrame* frame = frame_generator_->NextFrame();
    // TODO(srte): Use more advanced frame rate control to allow arbritrary
    // fractions.
    int decimation =
        std::round(static_cast<double>(source_fps_) / target_capture_fps_);
    for (int i = 1; i < decimation; ++i)
      frame = frame_generator_->NextFrame();
    frame->set_timestamp_us(clock_->TimeInMicroseconds());
    frame->set_ntp_time_ms(clock_->CurrentNtpInMilliseconds());
    frame->set_rotation(fake_rotation_);
    if (fake_color_space_) {
      frame->set_color_space(fake_color_space_);
    }
    if (first_frame_capture_time_ == -1) {
      first_frame_capture_time_ = frame->ntp_time_ms();
    }

    TestVideoCapturer::OnFrame(*frame);
  }
}

void FrameGeneratorCapturer::Start() {
  rtc::CritScope cs(&lock_);
  sending_ = true;
}

void FrameGeneratorCapturer::Stop() {
  rtc::CritScope cs(&lock_);
  sending_ = false;
}

void FrameGeneratorCapturer::ChangeResolution(size_t width, size_t height) {
  rtc::CritScope cs(&lock_);
  frame_generator_->ChangeResolution(width, height);
}

void FrameGeneratorCapturer::ChangeFramerate(int target_framerate) {
  rtc::CritScope cs(&lock_);
  RTC_CHECK(target_capture_fps_ > 0);
  if (target_framerate > source_fps_)
    RTC_LOG(LS_WARNING) << "Target framerate clamped from " << target_framerate
                        << " to " << source_fps_;
  if (source_fps_ % target_capture_fps_ != 0) {
    int decimation =
        std::round(static_cast<double>(source_fps_) / target_capture_fps_);
    int effective_rate = target_capture_fps_ / decimation;
    RTC_LOG(LS_WARNING) << "Target framerate, " << target_framerate
                        << ", is an uneven fraction of the source rate, "
                        << source_fps_
                        << ". The framerate will be :" << effective_rate;
  }
  target_capture_fps_ = std::min(source_fps_, target_framerate);
}

void FrameGeneratorCapturer::SetSinkWantsObserver(SinkWantsObserver* observer) {
  rtc::CritScope cs(&lock_);
  RTC_DCHECK(!sink_wants_observer_);
  sink_wants_observer_ = observer;
}

void FrameGeneratorCapturer::AddOrUpdateSink(
    rtc::VideoSinkInterface<VideoFrame>* sink,
    const rtc::VideoSinkWants& wants) {
  TestVideoCapturer::AddOrUpdateSink(sink, wants);
  rtc::CritScope cs(&lock_);
  if (sink_wants_observer_) {
    // Tests need to observe unmodified sink wants.
    sink_wants_observer_->OnSinkWantsChanged(sink, wants);
  }
  UpdateFps(GetSinkWants().max_framerate_fps);
}

void FrameGeneratorCapturer::RemoveSink(
    rtc::VideoSinkInterface<VideoFrame>* sink) {
  TestVideoCapturer::RemoveSink(sink);

  rtc::CritScope cs(&lock_);
  UpdateFps(GetSinkWants().max_framerate_fps);
}

void FrameGeneratorCapturer::UpdateFps(int max_fps) {
  if (max_fps < target_capture_fps_) {
    wanted_fps_.emplace(max_fps);
  } else {
    wanted_fps_.reset();
  }
}

void FrameGeneratorCapturer::ForceFrame() {
  // One-time non-repeating task,
  task_queue_.PostTask([this] { InsertFrame(); });
}

int FrameGeneratorCapturer::GetCurrentConfiguredFramerate() {
  rtc::CritScope cs(&lock_);
  if (wanted_fps_ && *wanted_fps_ < target_capture_fps_)
    return *wanted_fps_;
  return target_capture_fps_;
}

}  // namespace test
}  // namespace webrtc
