/*
 *  Copyright 2020 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef VIDEO_VIDEO_SOURCE_SINK_CONTROLLER_H_
#define VIDEO_VIDEO_SOURCE_SINK_CONTROLLER_H_

#include "absl/types/optional.h"
#include "api/rtp_parameters.h"
#include "api/video/video_frame.h"
#include "api/video/video_sink_interface.h"
#include "api/video/video_source_interface.h"
#include "call/adaptation/resource_adaptation_module_interface.h"
#include "rtc_base/critical_section.h"

namespace webrtc {

// Responsible for configuring source/sink settings, i.e. performing
// rtc::VideoSourceInterface<VideoFrame>::AddOrUpdateSink(). It does this by
// storing settings internally which are converted to rtc::VideoSinkWants when
// PushSourceSinkSettings() is performed.
class VideoSourceSinkController {
 public:
  VideoSourceSinkController(rtc::VideoSinkInterface<VideoFrame>* sink,
                            rtc::VideoSourceInterface<VideoFrame>* source);

  // TODO(https://crbug.com/webrtc/11222): Remove dependency on
  // DegradationPreference! How degradation preference affects
  // VideoSourceRestrictions should not be a responsibility of the controller,
  // but of the resource adaptation module.
  void SetSource(rtc::VideoSourceInterface<VideoFrame>* source,
                 DegradationPreference degradation_preference);
  // Must be called in order for changes to settings to have an effect. This
  // allows you to modify multiple properties in a single push to the sink.
  void PushSourceSinkSettings();

  VideoSourceRestrictions restrictions() const;
  absl::optional<size_t> pixels_per_frame_upper_limit() const;
  absl::optional<double> frame_rate_upper_limit() const;
  bool rotation_applied() const;
  int resolution_alignment() const;

  // Updates the settings stored internally. In order for these settings to be
  // applied to the sink, PushSourceSinkSettings() must subsequently be called.
  void SetRestrictions(VideoSourceRestrictions restrictions);
  void SetPixelsPerFrameUpperLimit(
      absl::optional<size_t> pixels_per_frame_upper_limit);
  void SetFrameRateUpperLimit(absl::optional<double> frame_rate_upper_limit);
  void SetRotationApplied(bool rotation_applied);
  void SetResolutionAlignment(int resolution_alignment);

  // TODO(https://crbug.com/webrtc/11222): Outside of testing, this is only used
  // by OveruseFrameDetectorResourceAdaptationModule::RefreshTargetFramerate().
  // When the DegradationPreference logic has moved outside of this class, there
  // will be no public need for this method other than testing reasons and this
  // can be renamed "ForTesting".
  rtc::VideoSinkWants CurrentSettingsToSinkWants() const;

 private:
  rtc::VideoSinkWants CurrentSettingsToSinkWantsInternal() const
      RTC_EXCLUSIVE_LOCKS_REQUIRED(crit_);

  // TODO(hbos): If everything is handled on the same sequence (i.e.
  // VideoStreamEncoder's encoder queue) then |crit_| can be replaced by
  // sequence checker. Investigate if we want to do this.
  mutable rtc::CriticalSection crit_;
  rtc::VideoSinkInterface<VideoFrame>* const sink_;
  rtc::VideoSourceInterface<VideoFrame>* source_ RTC_GUARDED_BY(&crit_);
  DegradationPreference degradation_preference_ RTC_GUARDED_BY(&crit_);
  // Pixel and frame rate restrictions.
  VideoSourceRestrictions restrictions_ RTC_GUARDED_BY(&crit_);
  // Ensures that even if we are not restricted, the sink is never configured
  // above this limit. Example: We are not CPU limited (no |restrictions_|) but
  // our encoder is capped at 30 fps (= |frame_rate_upper_limit_|).
  absl::optional<size_t> pixels_per_frame_upper_limit_ RTC_GUARDED_BY(&crit_);
  absl::optional<double> frame_rate_upper_limit_ RTC_GUARDED_BY(&crit_);
  bool rotation_applied_ RTC_GUARDED_BY(&crit_) = false;
  int resolution_alignment_ RTC_GUARDED_BY(&crit_) = 1;
};

}  // namespace webrtc

#endif  // VIDEO_VIDEO_SOURCE_SINK_CONTROLLER_H_
