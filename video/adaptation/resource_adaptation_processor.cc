/*
 *  Copyright 2020 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "video/adaptation/resource_adaptation_processor.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <memory>
#include <string>
#include <utility>

#include "absl/algorithm/container.h"
#include "absl/base/macros.h"
#include "api/task_queue/task_queue_base.h"
#include "api/video/video_adaptation_reason.h"
#include "api/video/video_source_interface.h"
#include "call/adaptation/resource.h"
#include "call/adaptation/video_source_restrictions.h"
#include "rtc_base/logging.h"
#include "rtc_base/numerics/safe_conversions.h"
#include "rtc_base/strings/string_builder.h"
#include "rtc_base/time_utils.h"

namespace webrtc {

const int kDefaultInputPixelsWidth = 176;
const int kDefaultInputPixelsHeight = 144;

namespace {

bool IsResolutionScalingEnabled(DegradationPreference degradation_preference) {
  return degradation_preference == DegradationPreference::MAINTAIN_FRAMERATE ||
         degradation_preference == DegradationPreference::BALANCED;
}

bool IsFramerateScalingEnabled(DegradationPreference degradation_preference) {
  return degradation_preference == DegradationPreference::MAINTAIN_RESOLUTION ||
         degradation_preference == DegradationPreference::BALANCED;
}

std::string ToString(VideoAdaptationReason reason) {
  switch (reason) {
    case VideoAdaptationReason::kQuality:
      return "quality";
    case VideoAdaptationReason::kCpu:
      return "cpu";
  }
}

VideoAdaptationReason OtherReason(VideoAdaptationReason reason) {
  switch (reason) {
    case VideoAdaptationReason::kQuality:
      return VideoAdaptationReason::kCpu;
    case VideoAdaptationReason::kCpu:
      return VideoAdaptationReason::kQuality;
  }
}

}  // namespace

class ResourceAdaptationProcessor::InitialFrameDropper {
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

ResourceAdaptationProcessor::PreventAdaptUpDueToActiveCounts::
    PreventAdaptUpDueToActiveCounts(ResourceAdaptationProcessor* processor)
    : processor_(processor) {}

bool ResourceAdaptationProcessor::PreventAdaptUpDueToActiveCounts::
    IsAdaptationUpAllowed(const VideoStreamInputState& input_state,
                          const VideoSourceRestrictions& restrictions_before,
                          const VideoSourceRestrictions& restrictions_after,
                          const Resource& reason_resource) const {
  VideoAdaptationReason reason =
      processor_->GetReasonFromResource(reason_resource);
  // We can't adapt up if we're already at the highest setting.
  // Note that this only includes counts relevant to the current degradation
  // preference. e.g. we previously adapted resolution, now prefer adpating fps,
  // only count the fps adaptations and not the previous resolution adaptations.
  // TODO(hbos): Why would the reason matter? If a particular resource doesn't
  // want us to go up it should prevent us from doing so itself rather than to
  // have this catch-all reason- and stats-based approach.
  int num_downgrades = FilterVideoAdaptationCountersByDegradationPreference(
                           processor_->active_counts_[reason],
                           processor_->effective_degradation_preference())
                           .Total();
  RTC_DCHECK_GE(num_downgrades, 0);
  return num_downgrades > 0;
}

ResourceAdaptationProcessor::PreventIncreaseResolutionDueToBitrateResource::
    PreventIncreaseResolutionDueToBitrateResource(
        ResourceAdaptationProcessor* processor)
    : processor_(processor) {}

bool ResourceAdaptationProcessor::
    PreventIncreaseResolutionDueToBitrateResource::IsAdaptationUpAllowed(
        const VideoStreamInputState& input_state,
        const VideoSourceRestrictions& restrictions_before,
        const VideoSourceRestrictions& restrictions_after,
        const Resource& reason_resource) const {
  VideoAdaptationReason reason =
      processor_->GetReasonFromResource(reason_resource);
  // If increasing resolution due to kQuality, make sure bitrate limits are not
  // violated.
  // TODO(hbos): Why are we allowing violating bitrate constraints if adapting
  // due to CPU? Shouldn't this condition be checked regardless of reason?
  if (reason == VideoAdaptationReason::kQuality &&
      DidIncreaseResolution(restrictions_before, restrictions_after)) {
    uint32_t bitrate_bps = processor_->encoder_target_bitrate_bps_.value_or(0);
    absl::optional<VideoEncoder::ResolutionBitrateLimits> bitrate_limits =
        processor_->encoder_settings_.has_value()
            ? processor_->encoder_settings_->encoder_info()
                  .GetEncoderBitrateLimitsForResolution(
                      // Need some sort of expected resulting pixels to be used
                      // instead of unrestricted.
                      GetHigherResolutionThan(
                          input_state.frame_size_pixels().value()))
            : absl::nullopt;
    if (bitrate_limits.has_value() && bitrate_bps != 0) {
      RTC_DCHECK_GE(bitrate_limits->frame_size_pixels,
                    input_state.frame_size_pixels().value());
      return bitrate_bps >=
             static_cast<uint32_t>(bitrate_limits->min_start_bitrate_bps);
    }
  }
  return true;
}

ResourceAdaptationProcessor::PreventAdaptUpInBalancedResource::
    PreventAdaptUpInBalancedResource(ResourceAdaptationProcessor* processor)
    : processor_(processor) {}

bool ResourceAdaptationProcessor::PreventAdaptUpInBalancedResource::
    IsAdaptationUpAllowed(const VideoStreamInputState& input_state,
                          const VideoSourceRestrictions& restrictions_before,
                          const VideoSourceRestrictions& restrictions_after,
                          const Resource& reason_resource) const {
  VideoAdaptationReason reason =
      processor_->GetReasonFromResource(reason_resource);
  // Don't adapt if BalancedDegradationSettings applies and determines this will
  // exceed bitrate constraints.
  // TODO(hbos): Why are we allowing violating balanced settings if adapting due
  // CPU? Shouldn't this condition be checked regardless of reason?
  if (reason == VideoAdaptationReason::kQuality &&
      processor_->effective_degradation_preference() ==
          DegradationPreference::BALANCED &&
      !processor_->stream_adapter_->balanced_settings().CanAdaptUp(
          input_state.video_codec_type(),
          input_state.frame_size_pixels().value(),
          processor_->encoder_target_bitrate_bps_.value_or(0))) {
    return false;
  }
  if (reason == VideoAdaptationReason::kQuality &&
      DidIncreaseResolution(restrictions_before, restrictions_after) &&
      !processor_->stream_adapter_->balanced_settings().CanAdaptUpResolution(
          input_state.video_codec_type(),
          input_state.frame_size_pixels().value(),
          processor_->encoder_target_bitrate_bps_.value_or(0))) {
    return false;
  }
  return true;
}

ResourceAdaptationProcessor::ResourceAdaptationProcessor(
    VideoStreamInputStateProvider* input_state_provider,
    Clock* clock,
    bool experiment_cpu_load_estimator,
    std::unique_ptr<OveruseFrameDetector> overuse_detector,
    VideoStreamEncoderObserver* encoder_stats_observer,
    ResourceAdaptationProcessorListener* adaptation_listener)
    : prevent_adapt_up_due_to_active_counts_(this),
      prevent_increase_resolution_due_to_bitrate_resource_(this),
      prevent_adapt_up_in_balanced_resource_(this),
      encode_usage_resource_(std::move(overuse_detector)),
      quality_scaler_resource_(),
      input_state_provider_(input_state_provider),
      adaptation_listener_(adaptation_listener),
      clock_(clock),
      state_(State::kStopped),
      experiment_cpu_load_estimator_(experiment_cpu_load_estimator),
      degradation_preference_(DegradationPreference::DISABLED),
      effective_degradation_preference_(DegradationPreference::DISABLED),
      stream_adapter_(std::make_unique<VideoStreamAdapter>()),
      initial_frame_dropper_(
          std::make_unique<InitialFrameDropper>(&quality_scaler_resource_)),
      quality_scaling_experiment_enabled_(QualityScalingExperiment::Enabled()),
      encoder_target_bitrate_bps_(absl::nullopt),
      quality_rampup_done_(false),
      quality_rampup_experiment_(QualityRampupExperiment::ParseSettings()),
      encoder_settings_(absl::nullopt),
      encoder_stats_observer_(encoder_stats_observer),
      active_counts_() {
  RTC_DCHECK(adaptation_listener_);
  RTC_DCHECK(encoder_stats_observer_);
  AddResource(&prevent_adapt_up_due_to_active_counts_,
              VideoAdaptationReason::kQuality);
  AddResource(&prevent_increase_resolution_due_to_bitrate_resource_,
              VideoAdaptationReason::kQuality);
  AddResource(&prevent_adapt_up_in_balanced_resource_,
              VideoAdaptationReason::kQuality);
  AddResource(&encode_usage_resource_, VideoAdaptationReason::kCpu);
  AddResource(&quality_scaler_resource_, VideoAdaptationReason::kQuality);
}

ResourceAdaptationProcessor::~ResourceAdaptationProcessor() {
  RTC_DCHECK_EQ(state_, State::kStopped);
}

void ResourceAdaptationProcessor::StartResourceAdaptation(
    ResourceAdaptationProcessorListener* adaptation_listener) {
  RTC_DCHECK_EQ(state_, State::kStopped);
  RTC_DCHECK(encoder_settings_.has_value());
  // TODO(https://crbug.com/webrtc/11222): Rethink when the adaptation listener
  // should be passed in and why. If resources are separated from modules then
  // those resources may be started or stopped separately from the module.
  RTC_DCHECK_EQ(adaptation_listener, adaptation_listener_);
  encode_usage_resource_.StartCheckForOveruse(GetCpuOveruseOptions());
  for (auto& resource_and_reason : resources_) {
    resource_and_reason.resource->RegisterListener(this);
  }
  state_ = State::kStarted;
}

void ResourceAdaptationProcessor::StopResourceAdaptation() {
  encode_usage_resource_.StopCheckForOveruse();
  quality_scaler_resource_.StopCheckForOveruse();
  for (auto& resource_and_reason : resources_) {
    resource_and_reason.resource->UnregisterListener(this);
  }
  state_ = State::kStopped;
}

void ResourceAdaptationProcessor::AddResource(Resource* resource) {
  return AddResource(resource, VideoAdaptationReason::kCpu);
}

void ResourceAdaptationProcessor::AddResource(Resource* resource,
                                              VideoAdaptationReason reason) {
  RTC_DCHECK(resource);
  RTC_DCHECK(absl::c_find_if(resources_,
                             [resource](const ResourceAndReason& r) {
                               return r.resource == resource;
                             }) == resources_.end())
      << "Resource " << resource->name() << " already was inserted";
  resources_.emplace_back(resource, reason);
}

void ResourceAdaptationProcessor::SetDegradationPreference(
    DegradationPreference degradation_preference) {
  degradation_preference_ = degradation_preference;
  UpdateStatsAdaptationSettings();
  MaybeUpdateEffectiveDegradationPreference();
}

void ResourceAdaptationProcessor::SetEncoderSettings(
    EncoderSettings encoder_settings) {
  encoder_settings_ = std::move(encoder_settings);
  MaybeUpdateEffectiveDegradationPreference();

  quality_rampup_experiment_.SetMaxBitrate(
      LastInputFrameSizeOrDefault(),
      encoder_settings_->video_codec().maxBitrate);
  MaybeUpdateTargetFrameRate();
}

void ResourceAdaptationProcessor::SetStartBitrate(DataRate start_bitrate) {
  if (!start_bitrate.IsZero())
    encoder_target_bitrate_bps_ = start_bitrate.bps();
  initial_frame_dropper_->SetStartBitrate(start_bitrate,
                                          clock_->TimeInMicroseconds());
}

void ResourceAdaptationProcessor::SetTargetBitrate(DataRate target_bitrate) {
  if (!target_bitrate.IsZero())
    encoder_target_bitrate_bps_ = target_bitrate.bps();
  initial_frame_dropper_->SetTargetBitrate(target_bitrate,
                                           clock_->TimeInMilliseconds());
}

void ResourceAdaptationProcessor::SetEncoderRates(
    const VideoEncoder::RateControlParameters& encoder_rates) {
  encoder_rates_ = encoder_rates;
}

void ResourceAdaptationProcessor::ResetVideoSourceRestrictions() {
  stream_adapter_->ClearRestrictions();
  MaybeUpdateVideoSourceRestrictions(nullptr);
}

void ResourceAdaptationProcessor::OnFrameDroppedDueToSize() {
  VideoAdaptationCounters counters_before =
      stream_adapter_->adaptation_counters();
  OnResourceOveruse(quality_scaler_resource_);
  if (degradation_preference_ == DegradationPreference::BALANCED &&
      stream_adapter_->adaptation_counters().fps_adaptations >
          counters_before.fps_adaptations) {
    // Adapt framerate in same step as resolution.
    OnResourceOveruse(quality_scaler_resource_);
  }
  if (stream_adapter_->adaptation_counters().resolution_adaptations >
      counters_before.resolution_adaptations) {
    encoder_stats_observer_->OnInitialQualityResolutionAdaptDown();
  }
  initial_frame_dropper_->OnFrameDroppedDueToSize();
}

void ResourceAdaptationProcessor::OnEncodeStarted(
    const VideoFrame& cropped_frame,
    int64_t time_when_first_seen_us) {
  encode_usage_resource_.OnEncodeStarted(cropped_frame,
                                         time_when_first_seen_us);
}

void ResourceAdaptationProcessor::OnEncodeCompleted(
    const EncodedImage& encoded_image,
    int64_t time_sent_in_us,
    absl::optional<int> encode_duration_us) {
  // Inform |encode_usage_resource_| of the encode completed event.
  uint32_t timestamp = encoded_image.Timestamp();
  int64_t capture_time_us =
      encoded_image.capture_time_ms_ * rtc::kNumMicrosecsPerMillisec;
  encode_usage_resource_.OnEncodeCompleted(timestamp, time_sent_in_us,
                                           capture_time_us, encode_duration_us);
  // Inform |quality_scaler_resource_| of the encode completed event.
  quality_scaler_resource_.OnEncodeCompleted(encoded_image, time_sent_in_us);
}

void ResourceAdaptationProcessor::OnFrameDropped(
    EncodedImageCallback::DropReason reason) {
  quality_scaler_resource_.OnFrameDropped(reason);
}

bool ResourceAdaptationProcessor::DropInitialFrames() const {
  return initial_frame_dropper_->DropInitialFrames();
}

void ResourceAdaptationProcessor::OnMaybeEncodeFrame() {
  initial_frame_dropper_->OnMaybeEncodeFrame();
  MaybePerformQualityRampupExperiment();
}

void ResourceAdaptationProcessor::UpdateQualityScalerSettings(
    absl::optional<VideoEncoder::QpThresholds> qp_thresholds) {
  if (qp_thresholds.has_value()) {
    quality_scaler_resource_.StopCheckForOveruse();
    quality_scaler_resource_.StartCheckForOveruse(qp_thresholds.value());
  } else {
    quality_scaler_resource_.StopCheckForOveruse();
  }
  initial_frame_dropper_->OnQualityScalerSettingsUpdated();
}

void ResourceAdaptationProcessor::ConfigureQualityScaler(
    const VideoEncoder::EncoderInfo& encoder_info) {
  const auto scaling_settings = encoder_info.scaling_settings;
  const bool quality_scaling_allowed =
      IsResolutionScalingEnabled(degradation_preference_) &&
      scaling_settings.thresholds;

  // TODO(https://crbug.com/webrtc/11222): Should this move to
  // QualityScalerResource?
  if (quality_scaling_allowed) {
    if (!quality_scaler_resource_.is_started()) {
      // Quality scaler has not already been configured.

      // Use experimental thresholds if available.
      absl::optional<VideoEncoder::QpThresholds> experimental_thresholds;
      if (quality_scaling_experiment_enabled_) {
        experimental_thresholds = QualityScalingExperiment::GetQpThresholds(
            GetVideoCodecTypeOrGeneric(encoder_settings_));
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
      quality_scaler_resource_.is_started()) {
    absl::optional<VideoEncoder::QpThresholds> thresholds =
        stream_adapter_->balanced_settings().GetQpThresholds(
            GetVideoCodecTypeOrGeneric(encoder_settings_),
            LastInputFrameSizeOrDefault());
    if (thresholds) {
      quality_scaler_resource_.SetQpThresholds(*thresholds);
    }
  }
  UpdateStatsAdaptationSettings();
}

ResourceListenerResponse
ResourceAdaptationProcessor::OnResourceUsageStateMeasured(
    const Resource& resource) {
  switch (resource.usage_state()) {
    case ResourceUsageState::kOveruse:
      return OnResourceOveruse(resource);
    case ResourceUsageState::kStable:
      // Do nothing.
      // TODO(https://crbug.com/webrtc/11172): Delete kStable in favor of null.
      return ResourceListenerResponse::kNothing;
    case ResourceUsageState::kUnderuse:
      OnResourceUnderuse(resource);
      return ResourceListenerResponse::kNothing;
  }
}

bool ResourceAdaptationProcessor::HasSufficientInputForAdaptation(
    const VideoStreamInputState& input_state) const {
  return input_state.HasInputFrameSizeAndFramesPerSecond() &&
         (effective_degradation_preference_ !=
              DegradationPreference::MAINTAIN_RESOLUTION ||
          input_state.frames_per_second() >= kMinFrameRateFps);
}

VideoAdaptationReason ResourceAdaptationProcessor::GetReasonFromResource(
    const Resource& resource) const {
  const auto& registered_resource =
      absl::c_find_if(resources_, [&resource](const ResourceAndReason& r) {
        return r.resource == &resource;
      });
  RTC_DCHECK(registered_resource != resources_.end())
      << resource.name() << " not found.";
  return registered_resource->reason;
}

void ResourceAdaptationProcessor::OnResourceUnderuse(
    const Resource& reason_resource) {
  VideoStreamInputState input_state = input_state_provider_->InputState();
  if (effective_degradation_preference_ == DegradationPreference::DISABLED ||
      !HasSufficientInputForAdaptation(input_state)) {
    return;
  }
  // Update video input states and encoder settings for accurate adaptation.
  stream_adapter_->SetInput(input_state);
  // How can this stream be adapted up?
  Adaptation adaptation = stream_adapter_->GetAdaptationUp();
  if (adaptation.status() != Adaptation::Status::kValid)
    return;
  // Are all resources OK with this adaptation being applied?
  VideoSourceRestrictions restrictions_before =
      stream_adapter_->source_restrictions();
  VideoSourceRestrictions restrictions_after =
      stream_adapter_->PeekNextRestrictions(adaptation);
  if (!absl::c_all_of(resources_, [&input_state, &restrictions_before,
                                   &restrictions_after, &reason_resource](
                                      ResourceAndReason resource_and_reason) {
        return resource_and_reason.resource->IsAdaptationUpAllowed(
            input_state, restrictions_before, restrictions_after,
            reason_resource);
      })) {
    return;
  }
  // Apply adaptation.
  stream_adapter_->ApplyAdaptation(adaptation);
  // Update VideoSourceRestrictions based on adaptation. This also informs the
  // |adaptation_listener_|.
  MaybeUpdateVideoSourceRestrictions(&reason_resource);
}

ResourceListenerResponse ResourceAdaptationProcessor::OnResourceOveruse(
    const Resource& reason_resource) {
  VideoStreamInputState input_state = input_state_provider_->InputState();
  if (!input_state.has_input()) {
    return ResourceListenerResponse::kQualityScalerShouldIncreaseFrequency;
  }
  if (effective_degradation_preference_ == DegradationPreference::DISABLED ||
      !HasSufficientInputForAdaptation(input_state)) {
    return ResourceListenerResponse::kNothing;
  }
  // Update video input states and encoder settings for accurate adaptation.
  stream_adapter_->SetInput(input_state);
  // How can this stream be adapted down?
  Adaptation adaptation = stream_adapter_->GetAdaptationDown();
  if (adaptation.min_pixel_limit_reached())
    encoder_stats_observer_->OnMinPixelLimitReached();
  if (adaptation.status() != Adaptation::Status::kValid)
    return ResourceListenerResponse::kNothing;
  // Apply adaptation.
  ResourceListenerResponse response =
      stream_adapter_->ApplyAdaptation(adaptation);
  // Update VideoSourceRestrictions based on adaptation. This also informs the
  // |adaptation_listener_|.
  MaybeUpdateVideoSourceRestrictions(&reason_resource);
  return response;
}

// TODO(pbos): Lower these thresholds (to closer to 100%) when we handle
// pipelining encoders better (multiple input frames before something comes
// out). This should effectively turn off CPU adaptations for systems that
// remotely cope with the load right now.
CpuOveruseOptions ResourceAdaptationProcessor::GetCpuOveruseOptions() const {
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

int ResourceAdaptationProcessor::LastInputFrameSizeOrDefault() const {
  return input_state_provider_->InputState().frame_size_pixels().value_or(
      kDefaultInputPixelsWidth * kDefaultInputPixelsHeight);
}

void ResourceAdaptationProcessor::MaybeUpdateEffectiveDegradationPreference() {
  bool is_screenshare = encoder_settings_.has_value() &&
                        encoder_settings_->encoder_config().content_type ==
                            VideoEncoderConfig::ContentType::kScreen;
  effective_degradation_preference_ =
      (is_screenshare &&
       degradation_preference_ == DegradationPreference::BALANCED)
          ? DegradationPreference::MAINTAIN_RESOLUTION
          : degradation_preference_;
  stream_adapter_->SetDegradationPreference(effective_degradation_preference_);
  MaybeUpdateVideoSourceRestrictions(nullptr);
}

void ResourceAdaptationProcessor::MaybeUpdateVideoSourceRestrictions(
    const Resource* reason_resource) {
  VideoSourceRestrictions new_restrictions =
      FilterRestrictionsByDegradationPreference(
          stream_adapter_->source_restrictions(), degradation_preference_);
  if (video_source_restrictions_ != new_restrictions) {
    video_source_restrictions_ = std::move(new_restrictions);
    // TODO(https://crbug.com/webrtc/11172): Support multiple listeners and
    // loop through them here instead of calling two hardcoded listeners (|this|
    // and |adaptation_listener_|).
    OnVideoSourceRestrictionsUpdated(video_source_restrictions_,
                                     stream_adapter_->adaptation_counters(),
                                     reason_resource);
    adaptation_listener_->OnVideoSourceRestrictionsUpdated(
        video_source_restrictions_, stream_adapter_->adaptation_counters(),
        reason_resource);
  }
}

void ResourceAdaptationProcessor::OnVideoSourceRestrictionsUpdated(
    VideoSourceRestrictions restrictions,
    const VideoAdaptationCounters& adaptation_counters,
    const Resource* reason) {
  VideoAdaptationCounters previous_adaptation_counters =
      active_counts_[VideoAdaptationReason::kQuality] +
      active_counts_[VideoAdaptationReason::kCpu];
  int adaptation_counters_total_abs_diff = std::abs(
      adaptation_counters.Total() - previous_adaptation_counters.Total());
  if (reason) {
    // A resource signal triggered this adaptation. The adaptation counters have
    // to be updated every time the adaptation counter is incremented or
    // decremented due to a resource.
    RTC_DCHECK_EQ(adaptation_counters_total_abs_diff, 1);
    VideoAdaptationReason reason_type = GetReasonFromResource(*reason);
    UpdateAdaptationStats(adaptation_counters, reason_type);
  } else if (adaptation_counters.Total() == 0) {
    // Adaptation was manually reset - clear the per-reason counters too.
    ResetActiveCounts();
    encoder_stats_observer_->ClearAdaptationStats();
  } else {
    // If a reason did not increase or decrease the Total() by 1 and the
    // restrictions were not just reset, the adaptation counters MUST not have
    // been modified and there is nothing to do stats-wise.
    RTC_DCHECK_EQ(adaptation_counters_total_abs_diff, 0);
  }
  RTC_LOG(LS_INFO) << ActiveCountsToString();
  MaybeUpdateTargetFrameRate();
}

void ResourceAdaptationProcessor::MaybeUpdateTargetFrameRate() {
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
      video_source_restrictions_.max_frame_rate();
  if (!target_frame_rate.has_value() ||
      (codec_max_frame_rate.has_value() &&
       codec_max_frame_rate.value() < target_frame_rate.value())) {
    target_frame_rate = codec_max_frame_rate;
  }
  encode_usage_resource_.SetTargetFrameRate(target_frame_rate);
}

void ResourceAdaptationProcessor::OnAdaptationCountChanged(
    const VideoAdaptationCounters& adaptation_count,
    VideoAdaptationCounters* active_count,
    VideoAdaptationCounters* other_active) {
  RTC_DCHECK(active_count);
  RTC_DCHECK(other_active);
  const int active_total = active_count->Total();
  const int other_total = other_active->Total();
  const VideoAdaptationCounters prev_total = *active_count + *other_active;
  const int delta_resolution_adaptations =
      adaptation_count.resolution_adaptations -
      prev_total.resolution_adaptations;
  const int delta_fps_adaptations =
      adaptation_count.fps_adaptations - prev_total.fps_adaptations;

  RTC_DCHECK_EQ(
      std::abs(delta_resolution_adaptations) + std::abs(delta_fps_adaptations),
      1)
      << "Adaptation took more than one step!";

  if (delta_resolution_adaptations > 0) {
    ++active_count->resolution_adaptations;
  } else if (delta_resolution_adaptations < 0) {
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
  if (delta_fps_adaptations > 0) {
    ++active_count->fps_adaptations;
  } else if (delta_fps_adaptations < 0) {
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
  RTC_DCHECK_EQ(
      active_count->Total(),
      active_total + delta_resolution_adaptations + delta_fps_adaptations);
  RTC_DCHECK_GE(active_count->resolution_adaptations, 0);
  RTC_DCHECK_GE(active_count->fps_adaptations, 0);
  RTC_DCHECK_GE(other_active->resolution_adaptations, 0);
  RTC_DCHECK_GE(other_active->fps_adaptations, 0);
}

void ResourceAdaptationProcessor::UpdateAdaptationStats(
    const VideoAdaptationCounters& total_counts,
    VideoAdaptationReason reason) {
  // Update active counts
  VideoAdaptationCounters& active_count = active_counts_[reason];
  VideoAdaptationCounters& other_active = active_counts_[OtherReason(reason)];

  OnAdaptationCountChanged(total_counts, &active_count, &other_active);

  encoder_stats_observer_->OnAdaptationChanged(
      reason, active_counts_[VideoAdaptationReason::kCpu],
      active_counts_[VideoAdaptationReason::kQuality]);
}

void ResourceAdaptationProcessor::UpdateStatsAdaptationSettings() const {
  VideoStreamEncoderObserver::AdaptationSettings cpu_settings(
      IsResolutionScalingEnabled(degradation_preference_),
      IsFramerateScalingEnabled(degradation_preference_));

  VideoStreamEncoderObserver::AdaptationSettings quality_settings =
      quality_scaler_resource_.is_started()
          ? cpu_settings
          : VideoStreamEncoderObserver::AdaptationSettings();
  encoder_stats_observer_->UpdateAdaptationSettings(cpu_settings,
                                                    quality_settings);
}

void ResourceAdaptationProcessor::MaybePerformQualityRampupExperiment() {
  if (!quality_scaler_resource_.is_started())
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
        quality_scaler_resource_.QpFastFilterLow()) {
      try_quality_rampup = true;
    }
  }
  // TODO(https://crbug.com/webrtc/11392): See if we can rely on the total
  // counts or the stats, and not the active counts.
  const VideoAdaptationCounters& qp_counts =
      active_counts_[VideoAdaptationReason::kQuality];
  const VideoAdaptationCounters& cpu_counts =
      active_counts_[VideoAdaptationReason::kCpu];
  if (try_quality_rampup && qp_counts.resolution_adaptations > 0 &&
      cpu_counts.Total() == 0) {
    RTC_LOG(LS_INFO) << "Reset quality limitations.";
    ResetVideoSourceRestrictions();
    quality_rampup_done_ = true;
  }
}

void ResourceAdaptationProcessor::ResetActiveCounts() {
  active_counts_.clear();
  active_counts_[VideoAdaptationReason::kCpu] = VideoAdaptationCounters();
  active_counts_[VideoAdaptationReason::kQuality] = VideoAdaptationCounters();
}

std::string ResourceAdaptationProcessor::ActiveCountsToString() const {
  RTC_DCHECK_EQ(2, active_counts_.size());
  rtc::StringBuilder ss;

  ss << "Downgrade counts: fps: {";
  for (auto& reason_count : active_counts_) {
    ss << ToString(reason_count.first) << ":";
    ss << reason_count.second.fps_adaptations;
  }
  ss << "}, resolution {";
  for (auto& reason_count : active_counts_) {
    ss << ToString(reason_count.first) << ":";
    ss << reason_count.second.resolution_adaptations;
  }
  ss << "}";

  return ss.Release();
}
}  // namespace webrtc
