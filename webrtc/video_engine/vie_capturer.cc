/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/video_engine/vie_capturer.h"

#include "webrtc/base/checks.h"
#include "webrtc/common_video/libyuv/include/webrtc_libyuv.h"
#include "webrtc/modules/interface/module_common_types.h"
#include "webrtc/modules/utility/interface/process_thread.h"
#include "webrtc/modules/video_capture/include/video_capture_factory.h"
#include "webrtc/modules/video_processing/main/interface/video_processing.h"
#include "webrtc/modules/video_render/include/video_render_defines.h"
#include "webrtc/system_wrappers/interface/clock.h"
#include "webrtc/system_wrappers/interface/critical_section_wrapper.h"
#include "webrtc/system_wrappers/interface/event_wrapper.h"
#include "webrtc/system_wrappers/interface/logging.h"
#include "webrtc/system_wrappers/interface/tick_util.h"
#include "webrtc/system_wrappers/interface/trace_event.h"
#include "webrtc/video_engine/overuse_frame_detector.h"
#include "webrtc/video_engine/vie_defines.h"
#include "webrtc/video_engine/vie_encoder.h"

namespace webrtc {

const int kThreadWaitTimeMs = 100;

class RegistrableCpuOveruseMetricsObserver : public CpuOveruseMetricsObserver {
 public:
  void CpuOveruseMetricsUpdated(const CpuOveruseMetrics& metrics) override {
    rtc::CritScope lock(&crit_);
    if (observer_)
      observer_->CpuOveruseMetricsUpdated(metrics);
    metrics_ = metrics;
  }

  CpuOveruseMetrics GetCpuOveruseMetrics() const {
    rtc::CritScope lock(&crit_);
    return metrics_;
  }

  void Set(CpuOveruseMetricsObserver* observer) {
    rtc::CritScope lock(&crit_);
    observer_ = observer;
  }

 private:
  mutable rtc::CriticalSection crit_;
  CpuOveruseMetricsObserver* observer_ GUARDED_BY(crit_) = nullptr;
  CpuOveruseMetrics metrics_ GUARDED_BY(crit_);
};

ViECapturer::ViECapturer(ProcessThread* module_process_thread,
                         ViEFrameCallback* frame_callback)
    : capture_cs_(CriticalSectionWrapper::CreateCriticalSection()),
      module_process_thread_(module_process_thread),
      frame_callback_(frame_callback),
      incoming_frame_cs_(CriticalSectionWrapper::CreateCriticalSection()),
      capture_thread_(ThreadWrapper::CreateThread(ViECaptureThreadFunction,
                                                  this,
                                                  "ViECaptureThread")),
      capture_event_(*EventWrapper::Create()),
      deliver_event_(*EventWrapper::Create()),
      stop_(0),
      last_captured_timestamp_(0),
      delta_ntp_internal_ms_(
          Clock::GetRealTimeClock()->CurrentNtpInMilliseconds() -
          TickTime::MillisecondTimestamp()),
      cpu_overuse_metrics_observer_(new RegistrableCpuOveruseMetricsObserver()),
      overuse_detector_(
          new OveruseFrameDetector(Clock::GetRealTimeClock(),
                                   cpu_overuse_metrics_observer_.get())) {
  capture_thread_->Start();
  capture_thread_->SetPriority(kHighPriority);
  module_process_thread_->RegisterModule(overuse_detector_.get());
}

ViECapturer::~ViECapturer() {
  module_process_thread_->DeRegisterModule(overuse_detector_.get());

  // Stop the thread.
  rtc::AtomicOps::Increment(&stop_);
  capture_event_.Set();

  // Stop the camera input.
  capture_thread_->Stop();
  delete &capture_event_;
  delete &deliver_event_;
}

void ViECapturer::RegisterCpuOveruseObserver(CpuOveruseObserver* observer) {
  overuse_detector_->SetObserver(observer);
}

void ViECapturer::RegisterCpuOveruseMetricsObserver(
    CpuOveruseMetricsObserver* observer) {
  cpu_overuse_metrics_observer_->Set(observer);
}

void ViECapturer::IncomingFrame(const I420VideoFrame& video_frame) {
  I420VideoFrame incoming_frame = video_frame;

  if (incoming_frame.ntp_time_ms() != 0) {
    // If a NTP time stamp is set, this is the time stamp we will use.
    incoming_frame.set_render_time_ms(
        incoming_frame.ntp_time_ms() - delta_ntp_internal_ms_);
  } else {  // NTP time stamp not set.
    int64_t render_time = incoming_frame.render_time_ms() != 0 ?
        incoming_frame.render_time_ms() : TickTime::MillisecondTimestamp();

    incoming_frame.set_render_time_ms(render_time);
    incoming_frame.set_ntp_time_ms(
        render_time + delta_ntp_internal_ms_);
  }

  // Convert NTP time, in ms, to RTP timestamp.
  const int kMsToRtpTimestamp = 90;
  incoming_frame.set_timestamp(kMsToRtpTimestamp *
      static_cast<uint32_t>(incoming_frame.ntp_time_ms()));

  CriticalSectionScoped cs(capture_cs_.get());
  if (incoming_frame.ntp_time_ms() <= last_captured_timestamp_) {
    // We don't allow the same capture time for two frames, drop this one.
    LOG(LS_WARNING) << "Same/old NTP timestamp for incoming frame. Dropping.";
    return;
  }

  captured_frame_.ShallowCopy(incoming_frame);
  last_captured_timestamp_ = incoming_frame.ntp_time_ms();

  overuse_detector_->FrameCaptured(captured_frame_.width(),
                                   captured_frame_.height(),
                                   captured_frame_.render_time_ms());

  TRACE_EVENT_ASYNC_BEGIN1("webrtc", "Video", video_frame.render_time_ms(),
                           "render_time", video_frame.render_time_ms());

  capture_event_.Set();
}

bool ViECapturer::ViECaptureThreadFunction(void* obj) {
  return static_cast<ViECapturer*>(obj)->ViECaptureProcess();
}

bool ViECapturer::ViECaptureProcess() {
  int64_t capture_time = -1;
  if (capture_event_.Wait(kThreadWaitTimeMs) == kEventSignaled) {
    if (rtc::AtomicOps::Load(&stop_))
      return false;

    overuse_detector_->FrameProcessingStarted();
    int64_t encode_start_time = -1;
    I420VideoFrame deliver_frame;
    {
      CriticalSectionScoped cs(capture_cs_.get());
      if (!captured_frame_.IsZeroSize()) {
        deliver_frame = captured_frame_;
        captured_frame_.Reset();
      }
    }
    if (!deliver_frame.IsZeroSize()) {
      capture_time = deliver_frame.render_time_ms();
      encode_start_time = Clock::GetRealTimeClock()->TimeInMilliseconds();
      frame_callback_->DeliverFrame(deliver_frame);
    }
    // Update the overuse detector with the duration.
    if (encode_start_time != -1) {
      overuse_detector_->FrameEncoded(
          Clock::GetRealTimeClock()->TimeInMilliseconds() - encode_start_time);
    }
  }
  // We're done!
  if (capture_time != -1) {
    overuse_detector_->FrameSent(capture_time);
  }
  return true;
}

}  // namespace webrtc
