/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_VIDEO_ENGINE_VIE_CAPTURER_H_
#define WEBRTC_VIDEO_ENGINE_VIE_CAPTURER_H_

#include <vector>

#include "webrtc/base/criticalsection.h"
#include "webrtc/base/scoped_ptr.h"
#include "webrtc/base/thread_annotations.h"
#include "webrtc/common_types.h"
#include "webrtc/engine_configurations.h"
#include "webrtc/modules/video_capture/include/video_capture.h"
#include "webrtc/modules/video_coding/codecs/interface/video_codec_interface.h"
#include "webrtc/modules/video_coding/main/interface/video_coding.h"
#include "webrtc/modules/video_processing/main/interface/video_processing.h"
#include "webrtc/system_wrappers/interface/critical_section_wrapper.h"
#include "webrtc/system_wrappers/interface/thread_wrapper.h"
#include "webrtc/typedefs.h"
#include "webrtc/video_engine/vie_defines.h"
#include "webrtc/video_engine/vie_frame_provider_base.h"

namespace webrtc {

class Config;
class CpuOveruseMetricsObserver;
class CpuOveruseObserver;
class CriticalSectionWrapper;
class EventWrapper;
class OveruseFrameDetector;
class ProcessThread;
class RegistrableCpuOveruseMetricsObserver;
class ViEEffectFilter;
class ViEFrameCallback;

class ViECapturer {
 public:
  ViECapturer(ProcessThread* module_process_thread,
              ViEFrameCallback* frame_callback);
  ~ViECapturer();

  void IncomingFrame(const I420VideoFrame& frame);

  void RegisterCpuOveruseObserver(CpuOveruseObserver* observer);
  void RegisterCpuOveruseMetricsObserver(CpuOveruseMetricsObserver* observer);

 protected:
  // Help function used for keeping track of VideoImageProcesingModule.
  // Creates the module if it is needed, returns 0 on success and guarantees
  // that the image proc module exist.
  int32_t IncImageProcRefCount();
  int32_t DecImageProcRefCount();

  // Thread functions for deliver captured frames to receivers.
  static bool ViECaptureThreadFunction(void* obj);
  bool ViECaptureProcess();

 private:
  void DeliverI420Frame(I420VideoFrame* video_frame);

  rtc::scoped_ptr<CriticalSectionWrapper> capture_cs_;
  ProcessThread* const module_process_thread_;

  ViEFrameCallback* const frame_callback_;

  // Frame used in IncomingFrameI420.
  rtc::scoped_ptr<CriticalSectionWrapper> incoming_frame_cs_;
  I420VideoFrame incoming_frame_;

  // Capture thread.
  rtc::scoped_ptr<ThreadWrapper> capture_thread_;
  // TODO(pbos): scoped_ptr
  EventWrapper& capture_event_;
  EventWrapper& deliver_event_;

  volatile int stop_;

  I420VideoFrame captured_frame_ GUARDED_BY(capture_cs_.get());
  // Used to make sure incoming time stamp is increasing for every frame.
  int64_t last_captured_timestamp_;
  // Delta used for translating between NTP and internal timestamps.
  const int64_t delta_ntp_internal_ms_;

  // Must be declared before overuse_detector_ where it's registered.
  const rtc::scoped_ptr<RegistrableCpuOveruseMetricsObserver>
      cpu_overuse_metrics_observer_;
  rtc::scoped_ptr<OveruseFrameDetector> overuse_detector_;
};

}  // namespace webrtc

#endif  // WEBRTC_VIDEO_ENGINE_VIE_CAPTURER_H_
