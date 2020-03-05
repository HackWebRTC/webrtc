/*
 *  Copyright 2019 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "video/overuse_frame_detector_resource_adaptation_module.h"

#include <algorithm>
#include <limits>
#include <memory>
#include <string>
#include <utility>

#include "absl/algorithm/container.h"
#include "absl/base/macros.h"
#include "api/task_queue/task_queue_base.h"
#include "api/video/video_source_interface.h"
#include "call/adaptation/resource.h"
#include "call/adaptation/video_source_restrictions.h"
#include "rtc_base/logging.h"
#include "rtc_base/numerics/safe_conversions.h"
#include "rtc_base/strings/string_builder.h"
#include "rtc_base/time_utils.h"
#include "video/video_stream_encoder.h"

namespace webrtc {

namespace {

const int kMinFramerateFps = 2;

bool IsResolutionScalingEnabled(DegradationPreference degradation_preference) {
  return degradation_preference == DegradationPreference::MAINTAIN_FRAMERATE ||
         degradation_preference == DegradationPreference::BALANCED;
}

bool IsFramerateScalingEnabled(DegradationPreference degradation_preference) {
  return degradation_preference == DegradationPreference::MAINTAIN_RESOLUTION ||
         degradation_preference == DegradationPreference::BALANCED;
}

// Returns modified restrictions where any constraints that don't apply to the
// degradation preference are cleared.
VideoSourceRestrictions ApplyDegradationPreference(
    VideoSourceRestrictions source_restrictions,
    DegradationPreference degradation_preference) {
  switch (degradation_preference) {
    case DegradationPreference::BALANCED:
      break;
    case DegradationPreference::MAINTAIN_FRAMERATE:
      source_restrictions.set_max_frame_rate(absl::nullopt);
      break;
    case DegradationPreference::MAINTAIN_RESOLUTION:
      source_restrictions.set_max_pixels_per_frame(absl::nullopt);
      source_restrictions.set_target_pixels_per_frame(absl::nullopt);
      break;
    case DegradationPreference::DISABLED:
      source_restrictions.set_max_pixels_per_frame(absl::nullopt);
      source_restrictions.set_target_pixels_per_frame(absl::nullopt);
      source_restrictions.set_max_frame_rate(absl::nullopt);
  }
  return source_restrictions;
}

// Returns AdaptationCounters where constraints that don't apply to the
// degradation preference are cleared. This behaviour must reflect that of
// ApplyDegradationPreference for SourceRestrictions. Any changed to that
// method must also change this one.
AdaptationCounters ApplyDegradationPreference(
    AdaptationCounters counters,
    DegradationPreference degradation_preference) {
  switch (degradation_preference) {
    case DegradationPreference::BALANCED:
      break;
    case DegradationPreference::MAINTAIN_FRAMERATE:
      counters.fps_adaptations = 0;
      break;
    case DegradationPreference::MAINTAIN_RESOLUTION:
      counters.resolution_adaptations = 0;
      break;
    case DegradationPreference::DISABLED:
      counters.resolution_adaptations = 0;
      counters.fps_adaptations = 0;
      break;
    default:
      RTC_NOTREACHED();
  }
  return counters;
}

}  // namespace

bool AdaptationCounters::operator==(const AdaptationCounters& rhs) const {
  return fps_adaptations == rhs.fps_adaptations &&
         resolution_adaptations == rhs.resolution_adaptations;
}

bool AdaptationCounters::operator!=(const AdaptationCounters& rhs) const {
  return !(rhs == *this);
}

AdaptationCounters AdaptationCounters::operator+(
    const AdaptationCounters& other) const {
  return AdaptationCounters(
      resolution_adaptations + other.resolution_adaptations,
      fps_adaptations + other.fps_adaptations);
}

AdaptationCounters AdaptationCounters::operator-(
    const AdaptationCounters& other) const {
  return AdaptationCounters(
      resolution_adaptations - other.resolution_adaptations,
      fps_adaptations - other.fps_adaptations);
}

// VideoSourceRestrictor is responsible for keeping track of current
// VideoSourceRestrictions and how to modify them in response to adapting up or
// down. It is not reponsible for determining when we should adapt up or down -
// for that, see
// OveruseFrameDetectorResourceAdaptationModule::OnResourceUnderuse() and
// OnResourceOveruse() - only how to modify the source/sink restrictions when
// this happens. Note that it is also not responsible for reconfigruring the
// source/sink, it is only a keeper of desired restrictions.
class OveruseFrameDetectorResourceAdaptationModule::VideoSourceRestrictor {
 public:
  // For frame rate, the steps we take are 2/3 (down) and 3/2 (up).
  static int GetLowerFrameRateThan(int fps) {
    RTC_DCHECK(fps != std::numeric_limits<int>::max());
    return (fps * 2) / 3;
  }
  // TODO(hbos): Use absl::optional<> instead?
  static int GetHigherFrameRateThan(int fps) {
    return fps != std::numeric_limits<int>::max()
               ? (fps * 3) / 2
               : std::numeric_limits<int>::max();
  }

  // For resolution, the steps we take are 3/5 (down) and 5/3 (up).
  // Notice the asymmetry of which restriction property is set depending on if
  // we are adapting up or down:
  // - DecreaseResolution() sets the max_pixels_per_frame() to the desired
  //   target and target_pixels_per_frame() to null.
  // - IncreaseResolutionTo() sets the target_pixels_per_frame() to the desired
  //   target, and max_pixels_per_frame() is set according to
  //   GetIncreasedMaxPixelsWanted().
  static int GetLowerResolutionThan(int pixel_count) {
    RTC_DCHECK(pixel_count != std::numeric_limits<int>::max());
    return (pixel_count * 3) / 5;
  }
  // TODO(hbos): Use absl::optional<> instead?
  static int GetHigherResolutionThan(int pixel_count) {
    return pixel_count != std::numeric_limits<int>::max()
               ? (pixel_count * 5) / 3
               : std::numeric_limits<int>::max();
  }

  VideoSourceRestrictor() {}

  VideoSourceRestrictions source_restrictions() {
    return source_restrictions_;
  }
  const AdaptationCounters& adaptation_counters() const { return adaptations_; }
  void ClearRestrictions() {
    source_restrictions_ = VideoSourceRestrictions();
    adaptations_ = AdaptationCounters();
  }

  bool CanDecreaseResolutionTo(int target_pixels, int min_pixels_per_frame) {
    int max_pixels_per_frame = rtc::dchecked_cast<int>(
        source_restrictions_.max_pixels_per_frame().value_or(
            std::numeric_limits<int>::max()));
    return target_pixels < max_pixels_per_frame &&
           target_pixels >= min_pixels_per_frame;
  }
  void DecreaseResolutionTo(int target_pixels, int min_pixels_per_frame) {
    RTC_DCHECK(CanDecreaseResolutionTo(target_pixels, min_pixels_per_frame));
    RTC_LOG(LS_INFO) << "Scaling down resolution, max pixels: "
                     << target_pixels;
    source_restrictions_.set_max_pixels_per_frame(
        target_pixels != std::numeric_limits<int>::max()
            ? absl::optional<size_t>(target_pixels)
            : absl::nullopt);
    source_restrictions_.set_target_pixels_per_frame(absl::nullopt);
    ++adaptations_.resolution_adaptations;
  }

  bool CanIncreaseResolutionTo(int target_pixels) {
    int max_pixels_wanted = GetIncreasedMaxPixelsWanted(target_pixels);
    int max_pixels_per_frame = rtc::dchecked_cast<int>(
        source_restrictions_.max_pixels_per_frame().value_or(
            std::numeric_limits<int>::max()));
    return max_pixels_wanted > max_pixels_per_frame;
  }
  void IncreaseResolutionTo(int target_pixels) {
    RTC_DCHECK(CanIncreaseResolutionTo(target_pixels));
    int max_pixels_wanted = GetIncreasedMaxPixelsWanted(target_pixels);
    RTC_LOG(LS_INFO) << "Scaling up resolution, max pixels: "
                     << max_pixels_wanted;
    source_restrictions_.set_max_pixels_per_frame(
        max_pixels_wanted != std::numeric_limits<int>::max()
            ? absl::optional<size_t>(max_pixels_wanted)
            : absl::nullopt);
    source_restrictions_.set_target_pixels_per_frame(
        max_pixels_wanted != std::numeric_limits<int>::max()
            ? absl::optional<size_t>(target_pixels)
            : absl::nullopt);
    --adaptations_.resolution_adaptations;
    RTC_DCHECK_GE(adaptations_.resolution_adaptations, 0);
  }

  bool CanDecreaseFrameRateTo(int max_frame_rate) {
    const int fps_wanted = std::max(kMinFramerateFps, max_frame_rate);
    return fps_wanted < rtc::dchecked_cast<int>(
                            source_restrictions_.max_frame_rate().value_or(
                                std::numeric_limits<int>::max()));
  }
  void DecreaseFrameRateTo(int max_frame_rate) {
    RTC_DCHECK(CanDecreaseFrameRateTo(max_frame_rate));
    max_frame_rate = std::max(kMinFramerateFps, max_frame_rate);
    RTC_LOG(LS_INFO) << "Scaling down framerate: " << max_frame_rate;
    source_restrictions_.set_max_frame_rate(
        max_frame_rate != std::numeric_limits<int>::max()
            ? absl::optional<double>(max_frame_rate)
            : absl::nullopt);
    ++adaptations_.fps_adaptations;
  }

  bool CanIncreaseFrameRateTo(int max_frame_rate) {
    return max_frame_rate > rtc::dchecked_cast<int>(
                                source_restrictions_.max_frame_rate().value_or(
                                    std::numeric_limits<int>::max()));
  }
  void IncreaseFrameRateTo(int max_frame_rate) {
    RTC_DCHECK(CanIncreaseFrameRateTo(max_frame_rate));
    RTC_LOG(LS_INFO) << "Scaling up framerate: " << max_frame_rate;
    source_restrictions_.set_max_frame_rate(
        max_frame_rate != std::numeric_limits<int>::max()
            ? absl::optional<double>(max_frame_rate)
            : absl::nullopt);
    --adaptations_.fps_adaptations;
    RTC_DCHECK_GE(adaptations_.fps_adaptations, 0);
  }

 private:
  static int GetIncreasedMaxPixelsWanted(int target_pixels) {
    if (target_pixels == std::numeric_limits<int>::max())
      return std::numeric_limits<int>::max();
    // When we decrease resolution, we go down to at most 3/5 of current pixels.
    // Thus to increase resolution, we need 3/5 to get back to where we started.
    // When going up, the desired max_pixels_per_frame() has to be significantly
    // higher than the target because the source's native resolutions might not
    // match the target. We pick 12/5 of the target.
    //
    // (This value was historically 4 times the old target, which is (3/5)*4 of
    // the new target - or 12/5 - assuming the target is adjusted according to
    // the above steps.)
    RTC_DCHECK(target_pixels != std::numeric_limits<int>::max());
    return (target_pixels * 12) / 5;
  }

  VideoSourceRestrictions source_restrictions_;
  AdaptationCounters adaptations_;

  RTC_DISALLOW_COPY_AND_ASSIGN(VideoSourceRestrictor);
};

class OveruseFrameDetectorResourceAdaptationModule::InitialFrameDropper {
 public:
  explicit InitialFrameDropper(QualityScalerResource* quality_scaler_resource)
      : quality_scaler_resource_(quality_scaler_resource),
        quality_scaler_settings_(QualityScalerSettings::ParseFromFieldTrials()),
        has_seen_first_bwe_drop_(false),
        set_start_bitrate_(DataRate::Zero()),
        set_start_bitrate_time_ms_(0),
        initial_framedrop_(0) {
    RTC_DCHECK(quality_scaler_resource_);
  }

  // Output signal.
  bool DropInitialFrames() const {
    return initial_framedrop_ < kMaxInitialFramedrop;
  }

  // Input signals.
  void SetStartBitrate(DataRate start_bitrate, int64_t now_ms) {
    set_start_bitrate_ = start_bitrate;
    set_start_bitrate_time_ms_ = now_ms;
  }

  void SetTargetBitrate(DataRate target_bitrate, int64_t now_ms) {
    if (set_start_bitrate_ > DataRate::Zero() && !has_seen_first_bwe_drop_ &&
        quality_scaler_resource_->is_started() &&
        quality_scaler_settings_.InitialBitrateIntervalMs() &&
        quality_scaler_settings_.InitialBitrateFactor()) {
      int64_t diff_ms = now_ms - set_start_bitrate_time_ms_;
      if (diff_ms <
              quality_scaler_settings_.InitialBitrateIntervalMs().value() &&
          (target_bitrate <
           (set_start_bitrate_ *
            quality_scaler_settings_.InitialBitrateFactor().value()))) {
        RTC_LOG(LS_INFO) << "Reset initial_framedrop_. Start bitrate: "
                         << set_start_bitrate_.bps()
                         << ", target bitrate: " << target_bitrate.bps();
        initial_framedrop_ = 0;
        has_seen_first_bwe_drop_ = true;
      }
    }
  }

  void OnFrameDroppedDueToSize() { ++initial_framedrop_; }

  void OnMaybeEncodeFrame() { initial_framedrop_ = kMaxInitialFramedrop; }

  void OnQualityScalerSettingsUpdated() {
    if (quality_scaler_resource_->is_started()) {
      // Restart frame drops due to size.
      initial_framedrop_ = 0;
    } else {
      // Quality scaling disabled so we shouldn't drop initial frames.
      initial_framedrop_ = kMaxInitialFramedrop;
    }
  }

 private:
  // The maximum number of frames to drop at beginning of stream to try and
  // achieve desired bitrate.
  static const int kMaxInitialFramedrop = 4;

  const QualityScalerResource* quality_scaler_resource_;
  const QualityScalerSettings quality_scaler_settings_;
  bool has_seen_first_bwe_drop_;
  DataRate set_start_bitrate_;
  int64_t set_start_bitrate_time_ms_;
  // Counts how many frames we've dropped in the initial framedrop phase.
  int initial_framedrop_;
};

OveruseFrameDetectorResourceAdaptationModule::AdaptationTarget::
    AdaptationTarget(AdaptationAction action, int value)
    : action(action), value(value) {}

OveruseFrameDetectorResourceAdaptationModule::
    OveruseFrameDetectorResourceAdaptationModule(
        Clock* clock,
        bool experiment_cpu_load_estimator,
        std::unique_ptr<OveruseFrameDetector> overuse_detector,
        VideoStreamEncoderObserver* encoder_stats_observer,
        ResourceAdaptationModuleListener* adaptation_listener)
    : adaptation_listener_(adaptation_listener),
      clock_(clock),
      state_(State::kStopped),
      experiment_cpu_load_estimator_(experiment_cpu_load_estimator),
      has_input_video_(false),
      degradation_preference_(DegradationPreference::DISABLED),
      balanced_settings_(),
      last_adaptation_request_(absl::nullopt),
      source_restrictor_(std::make_unique<VideoSourceRestrictor>()),
      encode_usage_resource_(
          std::make_unique<EncodeUsageResource>(std::move(overuse_detector))),
      quality_scaler_resource_(std::make_unique<QualityScalerResource>()),
      initial_frame_dropper_(std::make_unique<InitialFrameDropper>(
          quality_scaler_resource_.get())),
      quality_scaling_experiment_enabled_(QualityScalingExperiment::Enabled()),
      last_input_frame_size_(absl::nullopt),
      target_frame_rate_(absl::nullopt),
      encoder_target_bitrate_bps_(absl::nullopt),
      quality_rampup_done_(false),
      quality_rampup_experiment_(QualityRampupExperiment::ParseSettings()),
      encoder_settings_(absl::nullopt),
      encoder_stats_observer_(encoder_stats_observer),
      active_counts_() {
  RTC_DCHECK(adaptation_listener_);
  RTC_DCHECK(encoder_stats_observer_);
  AddResource(encode_usage_resource_.get(),
              AdaptationObserverInterface::AdaptReason::kCpu);
  AddResource(quality_scaler_resource_.get(),
              AdaptationObserverInterface::AdaptReason::kQuality);
}

OveruseFrameDetectorResourceAdaptationModule::
    ~OveruseFrameDetectorResourceAdaptationModule() {
  RTC_DCHECK_EQ(state_, State::kStopped);
}

void OveruseFrameDetectorResourceAdaptationModule::StartResourceAdaptation(
    ResourceAdaptationModuleListener* adaptation_listener) {
  RTC_DCHECK_EQ(state_, State::kStopped);
  RTC_DCHECK(encoder_settings_.has_value());
  // TODO(https://crbug.com/webrtc/11222): Rethink when the adaptation listener
  // should be passed in and why. If resources are separated from modules then
  // those resources may be started or stopped separately from the module.
  RTC_DCHECK_EQ(adaptation_listener, adaptation_listener_);
  encode_usage_resource_->StartCheckForOveruse(GetCpuOveruseOptions());
  for (auto& resource_and_reason : resources_) {
    resource_and_reason.resource->RegisterListener(this);
  }
  state_ = State::kStarted;
}

void OveruseFrameDetectorResourceAdaptationModule::StopResourceAdaptation() {
  encode_usage_resource_->StopCheckForOveruse();
  quality_scaler_resource_->StopCheckForOveruse();
  for (auto& resource_and_reason : resources_) {
    resource_and_reason.resource->UnregisterListener(this);
  }
  state_ = State::kStopped;
}

void OveruseFrameDetectorResourceAdaptationModule::AddResource(
    Resource* resource) {
  return AddResource(resource, AdaptationObserverInterface::AdaptReason::kCpu);
}

void OveruseFrameDetectorResourceAdaptationModule::AddResource(
    Resource* resource,
    AdaptationObserverInterface::AdaptReason reason) {
  RTC_DCHECK(resource);
  RTC_DCHECK(absl::c_find_if(resources_,
                             [resource](const ResourceAndReason& r) {
                               return r.resource == resource;
                             }) == resources_.end())
      << "Resource " << resource->name() << " already was inserted";
  resources_.emplace_back(resource, reason);
}

void OveruseFrameDetectorResourceAdaptationModule::SetHasInputVideo(
    bool has_input_video) {
  // While false, OnResourceUnderuse() and OnResourceOveruse() are NO-OPS.
  has_input_video_ = has_input_video;
}

void OveruseFrameDetectorResourceAdaptationModule::SetDegradationPreference(
    DegradationPreference degradation_preference) {
  if (degradation_preference_ != degradation_preference) {
    // Reset adaptation state, so that we're not tricked into thinking there's
    // an already pending request of the same type.
    last_adaptation_request_.reset();
    if (degradation_preference == DegradationPreference::BALANCED ||
        degradation_preference_ == DegradationPreference::BALANCED) {
      source_restrictor_->ClearRestrictions();
      active_counts_.fill(AdaptationCounters());
    }
  }
  degradation_preference_ = degradation_preference;
  MaybeUpdateVideoSourceRestrictions();
}

void OveruseFrameDetectorResourceAdaptationModule::SetEncoderSettings(
    EncoderSettings encoder_settings) {
  encoder_settings_ = std::move(encoder_settings);

  quality_rampup_experiment_.SetMaxBitrate(
      LastInputFrameSizeOrDefault(),
      encoder_settings_->video_codec().maxBitrate);
  MaybeUpdateTargetFrameRate();
}

void OveruseFrameDetectorResourceAdaptationModule::SetStartBitrate(
    DataRate start_bitrate) {
  if (!start_bitrate.IsZero())
    encoder_target_bitrate_bps_ = start_bitrate.bps();
  initial_frame_dropper_->SetStartBitrate(start_bitrate,
                                          clock_->TimeInMicroseconds());
}

void OveruseFrameDetectorResourceAdaptationModule::SetTargetBitrate(
    DataRate target_bitrate) {
  if (!target_bitrate.IsZero())
    encoder_target_bitrate_bps_ = target_bitrate.bps();
  initial_frame_dropper_->SetTargetBitrate(target_bitrate,
                                           clock_->TimeInMilliseconds());
}

void OveruseFrameDetectorResourceAdaptationModule::SetEncoderRates(
    const VideoEncoder::RateControlParameters& encoder_rates) {
  encoder_rates_ = encoder_rates;
}

void OveruseFrameDetectorResourceAdaptationModule::
    ResetVideoSourceRestrictions() {
  last_adaptation_request_.reset();
  source_restrictor_->ClearRestrictions();
  active_counts_.fill(AdaptationCounters());
  MaybeUpdateVideoSourceRestrictions();
}

void OveruseFrameDetectorResourceAdaptationModule::OnFrame(
    const VideoFrame& frame) {
  last_input_frame_size_ = frame.size();
}

void OveruseFrameDetectorResourceAdaptationModule::OnFrameDroppedDueToSize() {
  AdaptationCounters counters_before =
      source_restrictor_->adaptation_counters();
  OnResourceOveruse(AdaptationObserverInterface::AdaptReason::kQuality);
  if (degradation_preference() == DegradationPreference::BALANCED &&
      source_restrictor_->adaptation_counters().fps_adaptations >
          counters_before.fps_adaptations) {
    // Adapt framerate in same step as resolution.
    OnResourceOveruse(AdaptationObserverInterface::AdaptReason::kQuality);
  }
  if (source_restrictor_->adaptation_counters().resolution_adaptations >
      counters_before.resolution_adaptations) {
    encoder_stats_observer_->OnInitialQualityResolutionAdaptDown();
  }
  initial_frame_dropper_->OnFrameDroppedDueToSize();
}

void OveruseFrameDetectorResourceAdaptationModule::OnEncodeStarted(
    const VideoFrame& cropped_frame,
    int64_t time_when_first_seen_us) {
  encode_usage_resource_->OnEncodeStarted(cropped_frame,
                                          time_when_first_seen_us);
}

void OveruseFrameDetectorResourceAdaptationModule::OnEncodeCompleted(
    const EncodedImage& encoded_image,
    int64_t time_sent_in_us,
    absl::optional<int> encode_duration_us) {
  // Inform |encode_usage_resource_| of the encode completed event.
  uint32_t timestamp = encoded_image.Timestamp();
  int64_t capture_time_us =
      encoded_image.capture_time_ms_ * rtc::kNumMicrosecsPerMillisec;
  encode_usage_resource_->OnEncodeCompleted(
      timestamp, time_sent_in_us, capture_time_us, encode_duration_us);
  // Inform |quality_scaler_resource_| of the encode completed event.
  quality_scaler_resource_->OnEncodeCompleted(encoded_image, time_sent_in_us);
}

void OveruseFrameDetectorResourceAdaptationModule::OnFrameDropped(
    EncodedImageCallback::DropReason reason) {
  quality_scaler_resource_->OnFrameDropped(reason);
}

bool OveruseFrameDetectorResourceAdaptationModule::DropInitialFrames() const {
  return initial_frame_dropper_->DropInitialFrames();
}

void OveruseFrameDetectorResourceAdaptationModule::OnMaybeEncodeFrame() {
  initial_frame_dropper_->OnMaybeEncodeFrame();
  MaybePerformQualityRampupExperiment();
}

void OveruseFrameDetectorResourceAdaptationModule::UpdateQualityScalerSettings(
    absl::optional<VideoEncoder::QpThresholds> qp_thresholds) {
  if (qp_thresholds.has_value()) {
    quality_scaler_resource_->StopCheckForOveruse();
    quality_scaler_resource_->StartCheckForOveruse(qp_thresholds.value());
  } else {
    quality_scaler_resource_->StopCheckForOveruse();
  }
  initial_frame_dropper_->OnQualityScalerSettingsUpdated();
}

void OveruseFrameDetectorResourceAdaptationModule::ConfigureQualityScaler(
    const VideoEncoder::EncoderInfo& encoder_info) {
  const auto scaling_settings = encoder_info.scaling_settings;
  const bool quality_scaling_allowed =
      IsResolutionScalingEnabled(degradation_preference_) &&
      scaling_settings.thresholds;

  // TODO(https://crbug.com/webrtc/11222): Should this move to
  // QualityScalerResource?
  if (quality_scaling_allowed) {
    if (!quality_scaler_resource_->is_started()) {
      // Quality scaler has not already been configured.

      // Use experimental thresholds if available.
      absl::optional<VideoEncoder::QpThresholds> experimental_thresholds;
      if (quality_scaling_experiment_enabled_) {
        experimental_thresholds = QualityScalingExperiment::GetQpThresholds(
            GetVideoCodecTypeOrGeneric());
      }
      UpdateQualityScalerSettings(experimental_thresholds
                                      ? *experimental_thresholds
                                      : *(scaling_settings.thresholds));
    }
  } else {
    UpdateQualityScalerSettings(absl::nullopt);
  }

  // Set the qp-thresholds to the balanced settings if balanced mode.
  if (degradation_preference_ == DegradationPreference::BALANCED &&
      quality_scaler_resource_->is_started()) {
    absl::optional<VideoEncoder::QpThresholds> thresholds =
        balanced_settings_.GetQpThresholds(GetVideoCodecTypeOrGeneric(),
                                           LastInputFrameSizeOrDefault());
    if (thresholds) {
      quality_scaler_resource_->SetQpThresholds(*thresholds);
    }
  }

  encoder_stats_observer_->OnAdaptationChanged(
      VideoStreamEncoderObserver::AdaptationReason::kNone,
      GetActiveCounts(AdaptationObserverInterface::AdaptReason::kCpu),
      GetActiveCounts(AdaptationObserverInterface::AdaptReason::kQuality));
}

ResourceListenerResponse
OveruseFrameDetectorResourceAdaptationModule::OnResourceUsageStateMeasured(
    const Resource& resource) {
  const auto& registered_resource =
      absl::c_find_if(resources_, [&resource](const ResourceAndReason& r) {
        return r.resource == &resource;
      });
  RTC_DCHECK(registered_resource != resources_.end())
      << resource.name() << " not found.";

  const AdaptationObserverInterface::AdaptReason reason =
      registered_resource->reason;
  switch (resource.usage_state()) {
    case ResourceUsageState::kOveruse:
      return OnResourceOveruse(reason);
    case ResourceUsageState::kStable:
      // Do nothing.
      //
      // This module has two resources: |encoude_usage_resource_| and
      // |quality_scaler_resource_|. A smarter adaptation module might not
      // attempt to adapt up unless ALL resources were underused, but this
      // module acts on each resource's measurement in isolation - without
      // taking the current usage of any other resource into account.
      return ResourceListenerResponse::kNothing;
    case ResourceUsageState::kUnderuse:
      OnResourceUnderuse(reason);
      return ResourceListenerResponse::kNothing;
  }
}

absl::optional<OveruseFrameDetectorResourceAdaptationModule::AdaptationTarget>
OveruseFrameDetectorResourceAdaptationModule::GetAdaptUpTarget(
    int input_pixels,
    int input_fps,
    AdaptationObserverInterface::AdaptReason reason) const {
  // Preconditions for being able to adapt up:
  if (!has_input_video_)
    return absl::nullopt;
  // 1. We can't adapt up if we're already at the highest setting.
  // Note that this only includes counts relevant to the current degradation
  // preference. e.g. we previously adapted resolution, now prefer adpating fps,
  // only count the fps adaptations and not the previous resolution adaptations.
  // TODO(https://crbug.com/webrtc/11394): Checking the counts for reason should
  // be replaced with checking the overuse state of all resources.
  int num_downgrades = ApplyDegradationPreference(active_counts_[reason],
                                                  degradation_preference_)
                           .Total();
  RTC_DCHECK_GE(num_downgrades, 0);
  if (num_downgrades == 0)
    return absl::nullopt;
  // 2. We shouldn't adapt up if we're currently waiting for a previous upgrade
  // to have an effect.
  // TODO(hbos): What about in the case of other degradation preferences?
  bool last_adaptation_was_up =
      last_adaptation_request_ &&
      last_adaptation_request_->mode_ == AdaptationRequest::Mode::kAdaptUp;
  if (last_adaptation_was_up &&
      degradation_preference_ == DegradationPreference::MAINTAIN_FRAMERATE &&
      input_pixels <= last_adaptation_request_->input_pixel_count_) {
    return absl::nullopt;
  }
  // 3. We shouldn't adapt up if BalancedSettings doesn't allow it, which is
  // only applicable if reason is kQuality and preference is BALANCED.
  if (reason == AdaptationObserverInterface::AdaptReason::kQuality &&
      EffectiveDegradationPreference() == DegradationPreference::BALANCED &&
      !balanced_settings_.CanAdaptUp(GetVideoCodecTypeOrGeneric(), input_pixels,
                                     encoder_target_bitrate_bps_.value_or(0))) {
    return absl::nullopt;
  }

  // Attempt to find an allowed adaptation target.
  switch (EffectiveDegradationPreference()) {
    case DegradationPreference::BALANCED: {
      // Attempt to increase target frame rate.
      int target_fps =
          balanced_settings_.MaxFps(GetVideoCodecTypeOrGeneric(), input_pixels);
      if (source_restrictor_->CanIncreaseFrameRateTo(target_fps)) {
        return AdaptationTarget(AdaptationAction::kIncreaseFrameRate,
                                target_fps);
      }
      // Fall-through to maybe-adapting resolution, unless |balanced_settings_|
      // forbids it based on bitrate.
      if (reason == AdaptationObserverInterface::AdaptReason::kQuality &&
          !balanced_settings_.CanAdaptUpResolution(
              GetVideoCodecTypeOrGeneric(), input_pixels,
              encoder_target_bitrate_bps_.value_or(0))) {
        return absl::nullopt;
      }
      // Scale up resolution.
      ABSL_FALLTHROUGH_INTENDED;
    }
    case DegradationPreference::MAINTAIN_FRAMERATE: {
      // Don't adapt resolution if CanAdaptUpResolution() forbids it based on
      // bitrate and limits specified by encoder capabilities.
      if (reason == AdaptationObserverInterface::AdaptReason::kQuality &&
          !CanAdaptUpResolution(input_pixels,
                                encoder_target_bitrate_bps_.value_or(0))) {
        return absl::nullopt;
      }
      // Attempt to increase pixel count.
      int target_pixels = input_pixels;
      if (source_restrictor_->adaptation_counters().resolution_adaptations ==
          1) {
        RTC_LOG(LS_INFO) << "Removing resolution down-scaling setting.";
        target_pixels = std::numeric_limits<int>::max();
      }
      target_pixels =
          VideoSourceRestrictor::GetHigherResolutionThan(target_pixels);
      if (!source_restrictor_->CanIncreaseResolutionTo(target_pixels))
        return absl::nullopt;
      return AdaptationTarget(AdaptationAction::kIncreaseResolution,
                              target_pixels);
    }
    case DegradationPreference::MAINTAIN_RESOLUTION: {
      // Scale up framerate.
      int target_fps = input_fps;
      if (source_restrictor_->adaptation_counters().fps_adaptations == 1) {
        RTC_LOG(LS_INFO) << "Removing framerate down-scaling setting.";
        target_fps = std::numeric_limits<int>::max();
      }
      target_fps = VideoSourceRestrictor::GetHigherFrameRateThan(target_fps);
      if (!source_restrictor_->CanIncreaseFrameRateTo(target_fps))
        return absl::nullopt;
      return AdaptationTarget(AdaptationAction::kIncreaseFrameRate, target_fps);
    }
    case DegradationPreference::DISABLED:
      return absl::nullopt;
  }
}

absl::optional<OveruseFrameDetectorResourceAdaptationModule::AdaptationTarget>
OveruseFrameDetectorResourceAdaptationModule::GetAdaptDownTarget(
    int input_pixels,
    int input_fps,
    int min_pixels_per_frame) const {
  // Preconditions for being able to adapt down:
  if (!has_input_video_)
    return absl::nullopt;
  // 1. We are not disabled.
  // TODO(hbos): Don't support DISABLED, it doesn't exist in the spec and it
  // causes scaling due to bandwidth constraints (QualityScalerResource) to be
  // ignored, not just CPU signals. This is not a use case we want to support;
  // remove the enum value.
  if (degradation_preference_ == DegradationPreference::DISABLED)
    return absl::nullopt;
  bool last_adaptation_was_down =
      last_adaptation_request_ &&
      last_adaptation_request_->mode_ == AdaptationRequest::Mode::kAdaptDown;
  // 2. We shouldn't adapt down if our frame rate is below the minimum or if its
  // currently unknown.
  if (EffectiveDegradationPreference() ==
      DegradationPreference::MAINTAIN_RESOLUTION) {
    // TODO(hbos): This usage of |last_adaptation_was_down| looks like a mistake
    // - delete it.
    if (input_fps <= 0 ||
        (last_adaptation_was_down && input_fps < kMinFramerateFps)) {
      return absl::nullopt;
    }
  }
  // 3. We shouldn't adapt down if we're currently waiting for a previous
  // downgrade to have an effect.
  // TODO(hbos): What about in the case of other degradation preferences?
  if (last_adaptation_was_down &&
      degradation_preference_ == DegradationPreference::MAINTAIN_FRAMERATE &&
      input_pixels >= last_adaptation_request_->input_pixel_count_) {
    return absl::nullopt;
  }

  // Attempt to find an allowed adaptation target.
  switch (EffectiveDegradationPreference()) {
    case DegradationPreference::BALANCED: {
      // Try scale down framerate, if lower.
      int target_fps =
          balanced_settings_.MinFps(GetVideoCodecTypeOrGeneric(), input_pixels);
      if (source_restrictor_->CanDecreaseFrameRateTo(target_fps)) {
        return AdaptationTarget(AdaptationAction::kDecreaseFrameRate,
                                target_fps);
      }
      // Scale down resolution.
      ABSL_FALLTHROUGH_INTENDED;
    }
    case DegradationPreference::MAINTAIN_FRAMERATE: {
      // Scale down resolution.
      int target_pixels =
          VideoSourceRestrictor::GetLowerResolutionThan(input_pixels);
      // TODO(https://crbug.com/webrtc/11222): Move this logic to
      // ApplyAdaptationTarget() or elsewhere - simply checking which adaptation
      // target is available should not have side-effects.
      if (target_pixels < min_pixels_per_frame)
        encoder_stats_observer_->OnMinPixelLimitReached();
      if (!source_restrictor_->CanDecreaseResolutionTo(target_pixels,
                                                       min_pixels_per_frame)) {
        return absl::nullopt;
      }
      return AdaptationTarget(AdaptationAction::kDecreaseResolution,
                              target_pixels);
    }
    case DegradationPreference::MAINTAIN_RESOLUTION: {
      int target_fps = VideoSourceRestrictor::GetLowerFrameRateThan(input_fps);
      if (!source_restrictor_->CanDecreaseFrameRateTo(target_fps))
        return absl::nullopt;
      return AdaptationTarget(AdaptationAction::kDecreaseFrameRate, target_fps);
    }
    case DegradationPreference::DISABLED:
      RTC_NOTREACHED();
      return absl::nullopt;
  }
}

void OveruseFrameDetectorResourceAdaptationModule::ApplyAdaptationTarget(
    const AdaptationTarget& target,
    int min_pixels_per_frame,
    AdaptationObserverInterface::AdaptReason reason) {
  switch (target.action) {
    case AdaptationAction::kIncreaseResolution:
      source_restrictor_->IncreaseResolutionTo(target.value);
      return;
    case AdaptationAction::kDecreaseResolution:
      source_restrictor_->DecreaseResolutionTo(target.value,
                                               min_pixels_per_frame);
      return;
    case AdaptationAction::kIncreaseFrameRate:
      source_restrictor_->IncreaseFrameRateTo(target.value);
      // TODO(https://crbug.com/webrtc/11222): Don't adapt in two steps.
      // GetAdaptUpTarget() should tell us the correct value, but BALANCED logic
      // in DecrementFramerate() makes it hard to predict whether this will be
      // the last step. Remove the dependency on GetConstAdaptCounter().
      if (EffectiveDegradationPreference() == DegradationPreference::BALANCED &&
          source_restrictor_->adaptation_counters().fps_adaptations == 0 &&
          target.value != std::numeric_limits<int>::max()) {
        RTC_LOG(LS_INFO) << "Removing framerate down-scaling setting.";
        source_restrictor_->IncreaseFrameRateTo(
            std::numeric_limits<int>::max());
      }
      return;
    case AdaptationAction::kDecreaseFrameRate:
      source_restrictor_->DecreaseFrameRateTo(target.value);
      return;
  }
}

void OveruseFrameDetectorResourceAdaptationModule::OnResourceUnderuse(
    AdaptationObserverInterface::AdaptReason reason) {
  int input_pixels = LastInputFrameSizeOrDefault();
  int input_fps = encoder_stats_observer_->GetInputFrameRate();
  int min_pixels_per_frame = MinPixelsPerFrame();
  // Should we adapt, if so to what target?
  absl::optional<AdaptationTarget> target =
      GetAdaptUpTarget(input_pixels, input_fps, reason);
  if (!target.has_value())
    return;
  // Apply target.
  ApplyAdaptationTarget(target.value(), min_pixels_per_frame, reason);
  last_adaptation_request_.emplace(AdaptationRequest{
      input_pixels, input_fps, AdaptationRequest::Mode::kAdaptUp});
  // Update VideoSourceRestrictions based on adaptation. This also informs the
  // |adaptation_listener_|.
  MaybeUpdateVideoSourceRestrictions();
  // Stats and logging.
  UpdateAdaptationStats(reason);
  RTC_LOG(LS_INFO) << ActiveCountsToString();
}

ResourceListenerResponse
OveruseFrameDetectorResourceAdaptationModule::OnResourceOveruse(
    AdaptationObserverInterface::AdaptReason reason) {
  if (!has_input_video_)
    return ResourceListenerResponse::kQualityScalerShouldIncreaseFrequency;
  int input_pixels = LastInputFrameSizeOrDefault();
  int input_fps = encoder_stats_observer_->GetInputFrameRate();
  int min_pixels_per_frame = MinPixelsPerFrame();
  // Should we adapt, if so to what target?
  absl::optional<AdaptationTarget> target =
      GetAdaptDownTarget(input_pixels, input_fps, min_pixels_per_frame);
  if (!target.has_value())
    return ResourceListenerResponse::kNothing;
  // Apply target.
  ApplyAdaptationTarget(target.value(), min_pixels_per_frame, reason);
  last_adaptation_request_.emplace(AdaptationRequest{
      input_pixels, input_fps, AdaptationRequest::Mode::kAdaptDown});
  // Update VideoSourceRestrictions based on adaptation. This also informs the
  // |adaptation_listener_|.
  MaybeUpdateVideoSourceRestrictions();
  // Stats and logging.
  UpdateAdaptationStats(reason);
  RTC_LOG(INFO) << ActiveCountsToString();
  // In BALANCED, if requested FPS is higher or close to input FPS to the target
  // we tell the QualityScaler to increase its frequency.
  if (EffectiveDegradationPreference() == DegradationPreference::BALANCED &&
      target->action == AdaptationAction::kDecreaseFrameRate) {
    absl::optional<int> min_diff = balanced_settings_.MinFpsDiff(input_pixels);
    if (min_diff && input_fps > 0) {
      int fps_diff = input_fps - target->value;
      if (fps_diff < min_diff.value()) {
        return ResourceListenerResponse::kQualityScalerShouldIncreaseFrequency;
      }
    }
  }
  return ResourceListenerResponse::kNothing;
}

// TODO(pbos): Lower these thresholds (to closer to 100%) when we handle
// pipelining encoders better (multiple input frames before something comes
// out). This should effectively turn off CPU adaptations for systems that
// remotely cope with the load right now.
CpuOveruseOptions
OveruseFrameDetectorResourceAdaptationModule::GetCpuOveruseOptions() const {
  // This is already ensured by the only caller of this method:
  // StartResourceAdaptation().
  RTC_DCHECK(encoder_settings_.has_value());
  CpuOveruseOptions options;
  // Hardware accelerated encoders are assumed to be pipelined; give them
  // additional overuse time.
  if (encoder_settings_->encoder_info().is_hardware_accelerated) {
    options.low_encode_usage_threshold_percent = 150;
    options.high_encode_usage_threshold_percent = 200;
  }
  if (experiment_cpu_load_estimator_) {
    options.filter_time_ms = 5 * rtc::kNumMillisecsPerSec;
  }
  return options;
}

VideoCodecType
OveruseFrameDetectorResourceAdaptationModule::GetVideoCodecTypeOrGeneric()
    const {
  return encoder_settings_.has_value()
             ? encoder_settings_->encoder_config().codec_type
             : kVideoCodecGeneric;
}

int OveruseFrameDetectorResourceAdaptationModule::LastInputFrameSizeOrDefault()
    const {
  // The dependency on this hardcoded resolution is inherited from old code,
  // which used this resolution as a stand-in for not knowing the resolution
  // yet.
  // TODO(hbos): Can we simply DCHECK has_value() before usage instead? Having a
  // DCHECK passed all the tests but adding it does change the requirements of
  // this class (= not being allowed to call OnResourceUnderuse() or
  // OnResourceOveruse() before OnFrame()) and deserves a standalone CL.
  return last_input_frame_size_.value_or(
      VideoStreamEncoder::kDefaultLastFrameInfoWidth *
      VideoStreamEncoder::kDefaultLastFrameInfoHeight);
}

int OveruseFrameDetectorResourceAdaptationModule::MinPixelsPerFrame() const {
  return encoder_settings_.has_value()
             ? encoder_settings_->encoder_info()
                   .scaling_settings.min_pixels_per_frame
             : kDefaultMinPixelsPerFrame;
}

void OveruseFrameDetectorResourceAdaptationModule::
    MaybeUpdateVideoSourceRestrictions() {
  VideoSourceRestrictions new_restrictions = ApplyDegradationPreference(
      source_restrictor_->source_restrictions(), degradation_preference_);
  if (video_source_restrictions_ != new_restrictions) {
    video_source_restrictions_ = std::move(new_restrictions);
    adaptation_listener_->OnVideoSourceRestrictionsUpdated(
        video_source_restrictions_);
    MaybeUpdateTargetFrameRate();
  }
}

void OveruseFrameDetectorResourceAdaptationModule::
    MaybeUpdateTargetFrameRate() {
  absl::optional<double> codec_max_frame_rate =
      encoder_settings_.has_value()
          ? absl::optional<double>(
                encoder_settings_->video_codec().maxFramerate)
          : absl::nullopt;
  // The current target framerate is the maximum frame rate as specified by
  // the current codec configuration or any limit imposed by the adaptation
  // module. This is used to make sure overuse detection doesn't needlessly
  // trigger in low and/or variable framerate scenarios.
  absl::optional<double> target_frame_rate =
      ApplyDegradationPreference(source_restrictor_->source_restrictions(),
                                 degradation_preference_)
          .max_frame_rate();
  if (!target_frame_rate.has_value() ||
      (codec_max_frame_rate.has_value() &&
       codec_max_frame_rate.value() < target_frame_rate.value())) {
    target_frame_rate = codec_max_frame_rate;
  }
  encode_usage_resource_->SetTargetFrameRate(target_frame_rate);
}

void OveruseFrameDetectorResourceAdaptationModule::OnAdaptationCountChanged(
    const AdaptationCounters& adaptation_count,
    AdaptationCounters* active_count,
    AdaptationCounters* other_active) {
  RTC_DCHECK(active_count);
  RTC_DCHECK(other_active);
  const int active_total = active_count->Total();
  const int other_total = other_active->Total();
  const AdaptationCounters prev_total = *active_count + *other_active;
  const AdaptationCounters delta = adaptation_count - prev_total;

  RTC_DCHECK_EQ(
      std::abs(delta.resolution_adaptations) + std::abs(delta.fps_adaptations),
      1)
      << "Adaptation took more than one step!";

  if (delta.resolution_adaptations > 0) {
    ++active_count->resolution_adaptations;
  } else if (delta.resolution_adaptations < 0) {
    if (active_count->resolution_adaptations == 0) {
      RTC_DCHECK_GT(active_count->fps_adaptations, 0) << "No downgrades left";
      RTC_DCHECK_GT(other_active->resolution_adaptations, 0)
          << "No resolution adaptation to borrow from";
      // Lend an fps adaptation to other and take one resolution adaptation.
      --active_count->fps_adaptations;
      ++other_active->fps_adaptations;
      --other_active->resolution_adaptations;
    } else {
      --active_count->resolution_adaptations;
    }
  }
  if (delta.fps_adaptations > 0) {
    ++active_count->fps_adaptations;
  } else if (delta.fps_adaptations < 0) {
    if (active_count->fps_adaptations == 0) {
      RTC_DCHECK_GT(active_count->resolution_adaptations, 0)
          << "No downgrades left";
      RTC_DCHECK_GT(other_active->fps_adaptations, 0)
          << "No fps adaptation to borrow from";
      // Lend a resolution adaptation to other and take one fps adaptation.
      --active_count->resolution_adaptations;
      ++other_active->resolution_adaptations;
      --other_active->fps_adaptations;
    } else {
      --active_count->fps_adaptations;
    }
  }

  RTC_DCHECK(*active_count + *other_active == adaptation_count);
  RTC_DCHECK_EQ(other_active->Total(), other_total);
  RTC_DCHECK_EQ(active_count->Total(), active_total + delta.Total());
  RTC_DCHECK_GE(active_count->resolution_adaptations, 0);
  RTC_DCHECK_GE(active_count->fps_adaptations, 0);
  RTC_DCHECK_GE(other_active->resolution_adaptations, 0);
  RTC_DCHECK_GE(other_active->fps_adaptations, 0);
}

// TODO(nisse): Delete, once AdaptReason and AdaptationReason are merged.
void OveruseFrameDetectorResourceAdaptationModule::UpdateAdaptationStats(
    AdaptationObserverInterface::AdaptReason reason) {
  // Update active counts
  AdaptationCounters& active_count = active_counts_[reason];
  AdaptationCounters& other_active = active_counts_[(reason + 1) % 2];
  const AdaptationCounters total_counts =
      source_restrictor_->adaptation_counters();

  OnAdaptationCountChanged(total_counts, &active_count, &other_active);

  switch (reason) {
    case AdaptationObserverInterface::AdaptReason::kCpu:
      encoder_stats_observer_->OnAdaptationChanged(
          VideoStreamEncoderObserver::AdaptationReason::kCpu,
          GetActiveCounts(AdaptationObserverInterface::AdaptReason::kCpu),
          GetActiveCounts(AdaptationObserverInterface::AdaptReason::kQuality));
      break;
    case AdaptationObserverInterface::AdaptReason::kQuality:
      encoder_stats_observer_->OnAdaptationChanged(
          VideoStreamEncoderObserver::AdaptationReason::kQuality,
          GetActiveCounts(AdaptationObserverInterface::AdaptReason::kCpu),
          GetActiveCounts(AdaptationObserverInterface::AdaptReason::kQuality));
      break;
  }
}

VideoStreamEncoderObserver::AdaptationSteps
OveruseFrameDetectorResourceAdaptationModule::GetActiveCounts(
    AdaptationObserverInterface::AdaptReason reason) {
  // TODO(https://crbug.com/webrtc/11392) Ideally this shuold be moved out of
  // this class and into the encoder_stats_observer_.
  const AdaptationCounters counters = active_counts_[reason];

  VideoStreamEncoderObserver::AdaptationSteps counts =
      VideoStreamEncoderObserver::AdaptationSteps();
  counts.num_resolution_reductions = counters.resolution_adaptations;
  counts.num_framerate_reductions = counters.fps_adaptations;
  switch (reason) {
    case AdaptationObserverInterface::AdaptReason::kCpu:
      if (!IsFramerateScalingEnabled(degradation_preference_))
        counts.num_framerate_reductions = absl::nullopt;
      if (!IsResolutionScalingEnabled(degradation_preference_))
        counts.num_resolution_reductions = absl::nullopt;
      break;
    case AdaptationObserverInterface::AdaptReason::kQuality:
      if (!IsFramerateScalingEnabled(degradation_preference_) ||
          !quality_scaler_resource_->is_started()) {
        counts.num_framerate_reductions = absl::nullopt;
      }
      if (!IsResolutionScalingEnabled(degradation_preference_) ||
          !quality_scaler_resource_->is_started()) {
        counts.num_resolution_reductions = absl::nullopt;
      }
      break;
  }
  return counts;
}

DegradationPreference
OveruseFrameDetectorResourceAdaptationModule::EffectiveDegradationPreference()
    const {
  // Balanced mode for screenshare works via automatic animation detection:
  // Resolution is capped for fullscreen animated content.
  // Adapatation is done only via framerate downgrade.
  // Thus effective degradation preference is MAINTAIN_RESOLUTION.
  return (encoder_settings_.has_value() &&
          encoder_settings_->encoder_config().content_type ==
              VideoEncoderConfig::ContentType::kScreen &&
          degradation_preference_ == DegradationPreference::BALANCED)
             ? DegradationPreference::MAINTAIN_RESOLUTION
             : degradation_preference_;
}

bool OveruseFrameDetectorResourceAdaptationModule::CanAdaptUpResolution(
    int pixels,
    uint32_t bitrate_bps) const {
  absl::optional<VideoEncoder::ResolutionBitrateLimits> bitrate_limits =
      encoder_settings_.has_value()
          ? GetEncoderBitrateLimits(
                encoder_settings_->encoder_info(),
                VideoSourceRestrictor::GetHigherResolutionThan(pixels))
          : absl::nullopt;
  if (!bitrate_limits.has_value() || bitrate_bps == 0) {
    return true;  // No limit configured or bitrate provided.
  }
  RTC_DCHECK_GE(bitrate_limits->frame_size_pixels, pixels);
  return bitrate_bps >=
         static_cast<uint32_t>(bitrate_limits->min_start_bitrate_bps);
}
void OveruseFrameDetectorResourceAdaptationModule::
    MaybePerformQualityRampupExperiment() {
  if (!quality_scaler_resource_->is_started())
    return;

  if (quality_rampup_done_)
    return;

  int64_t now_ms = clock_->TimeInMilliseconds();
  uint32_t bw_kbps = encoder_rates_.has_value()
                         ? encoder_rates_.value().bandwidth_allocation.kbps()
                         : 0;

  bool try_quality_rampup = false;
  if (quality_rampup_experiment_.BwHigh(now_ms, bw_kbps)) {
    // Verify that encoder is at max bitrate and the QP is low.
    if (encoder_settings_ &&
        encoder_target_bitrate_bps_.value_or(0) ==
            encoder_settings_->video_codec().maxBitrate * 1000 &&
        quality_scaler_resource_->QpFastFilterLow()) {
      try_quality_rampup = true;
    }
  }
  // TODO(https://crbug.com/webrtc/11392): See if we can rely on the total
  // counts or the stats, and not the active counts.
  const AdaptationCounters& qp_counts =
      std::get<AdaptationObserverInterface::kQuality>(active_counts_);
  const AdaptationCounters& cpu_counts =
      std::get<AdaptationObserverInterface::kCpu>(active_counts_);
  if (try_quality_rampup && qp_counts.resolution_adaptations > 0 &&
      cpu_counts.Total() == 0) {
    RTC_LOG(LS_INFO) << "Reset quality limitations.";
    ResetVideoSourceRestrictions();
    quality_rampup_done_ = true;
  }
}

std::string OveruseFrameDetectorResourceAdaptationModule::ActiveCountsToString()
    const {
  rtc::StringBuilder ss;

  ss << "Downgrade counts: fps: {";
  for (size_t reason = 0; reason < active_counts_.size(); ++reason) {
    ss << (reason ? " cpu" : "quality") << ":";
    ss << active_counts_[reason].fps_adaptations;
  }
  ss << "}, resolution {";
  for (size_t reason = 0; reason < active_counts_.size(); ++reason) {
    ss << (reason ? " cpu" : "quality") << ":";
    ss << active_counts_[reason].resolution_adaptations;
  }
  ss << "}";

  return ss.Release();
}
}  // namespace webrtc
