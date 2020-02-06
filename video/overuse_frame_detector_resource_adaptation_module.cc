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

// The maximum number of frames to drop at beginning of stream
// to try and achieve desired bitrate.
const int kMaxInitialFramedrop = 4;

}  // namespace

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
  VideoSourceRestrictor() {}

  VideoSourceRestrictions source_restrictions() {
    return source_restrictions_;
  }

  // Updates the source_restrictions(). The source/sink has to be informed of
  // this separately.
  void ClearRestrictions() {
    source_restrictions_ = VideoSourceRestrictions();
  }

  // Updates the source_restrictions(). The source/sink has to be informed of
  // this separately.
  bool RequestResolutionLowerThan(int pixel_count,
                                  int min_pixels_per_frame,
                                  bool* min_pixels_reached) {
    // The input video frame size will have a resolution less than or equal to
    // |max_pixel_count| depending on how the source can scale the frame size.
    const int pixels_wanted = (pixel_count * 3) / 5;
    if (pixels_wanted >=
        rtc::dchecked_cast<int>(
            source_restrictions_.max_pixels_per_frame().value_or(
                std::numeric_limits<int>::max()))) {
      return false;
    }
    if (pixels_wanted < min_pixels_per_frame) {
      *min_pixels_reached = true;
      return false;
    }
    RTC_LOG(LS_INFO) << "Scaling down resolution, max pixels: "
                     << pixels_wanted;
    source_restrictions_.set_max_pixels_per_frame(
        pixels_wanted != std::numeric_limits<int>::max()
            ? absl::optional<size_t>(pixels_wanted)
            : absl::nullopt);
    source_restrictions_.set_target_pixels_per_frame(absl::nullopt);
    return true;
  }

  // Updates the source_restrictions(). The source/sink has to be informed of
  // this separately.
  int RequestFramerateLowerThan(int fps) {
    // The input video frame rate will be scaled down to 2/3, rounding down.
    int framerate_wanted = (fps * 2) / 3;
    return RestrictFramerate(framerate_wanted) ? framerate_wanted : -1;
  }

  int GetHigherResolutionThan(int pixel_count) const {
    // On step down we request at most 3/5 the pixel count of the previous
    // resolution, so in order to take "one step up" we request a resolution
    // as close as possible to 5/3 of the current resolution. The actual pixel
    // count selected depends on the capabilities of the source. In order to
    // not take a too large step up, we cap the requested pixel count to be at
    // most four time the current number of pixels.
    return (pixel_count * 5) / 3;
  }

  // Updates the source_restrictions(). The source/sink has to be informed of
  // this separately.
  bool RequestHigherResolutionThan(int pixel_count) {
    int max_pixels_wanted = pixel_count;
    if (max_pixels_wanted != std::numeric_limits<int>::max())
      max_pixels_wanted = pixel_count * 4;

    if (max_pixels_wanted <=
        rtc::dchecked_cast<int>(
            source_restrictions_.max_pixels_per_frame().value_or(
                std::numeric_limits<int>::max()))) {
      return false;
    }

    RTC_LOG(LS_INFO) << "Scaling up resolution, max pixels: "
                     << max_pixels_wanted;
    source_restrictions_.set_max_pixels_per_frame(
        max_pixels_wanted != std::numeric_limits<int>::max()
            ? absl::optional<size_t>(max_pixels_wanted)
            : absl::nullopt);
    source_restrictions_.set_target_pixels_per_frame(
        max_pixels_wanted != std::numeric_limits<int>::max()
            ? absl::optional<size_t>(GetHigherResolutionThan(pixel_count))
            : absl::nullopt);
    return true;
  }

  // Updates the source_restrictions(). The source/sink has to be informed of
  // this separately.
  // Request upgrade in framerate. Returns the new requested frame, or -1 if
  // no change requested. Note that maxint may be returned if limits due to
  // adaptation requests are removed completely. In that case, consider
  // |max_framerate_| to be the current limit (assuming the capturer complies).
  int RequestHigherFramerateThan(int fps) {
    // The input frame rate will be scaled up to the last step, with rounding.
    int framerate_wanted = fps;
    if (fps != std::numeric_limits<int>::max())
      framerate_wanted = (fps * 3) / 2;

    return IncreaseFramerate(framerate_wanted) ? framerate_wanted : -1;
  }

  // Updates the source_restrictions(). The source/sink has to be informed of
  // this separately.
  bool RestrictFramerate(int fps) {
    const int fps_wanted = std::max(kMinFramerateFps, fps);
    if (fps_wanted >=
        rtc::dchecked_cast<int>(source_restrictions_.max_frame_rate().value_or(
            std::numeric_limits<int>::max())))
      return false;

    RTC_LOG(LS_INFO) << "Scaling down framerate: " << fps_wanted;
    source_restrictions_.set_max_frame_rate(
        fps_wanted != std::numeric_limits<int>::max()
            ? absl::optional<double>(fps_wanted)
            : absl::nullopt);
    return true;
  }

  // Updates the source_restrictions(). The source/sink has to be informed of
  // this separately.
  bool IncreaseFramerate(int fps) {
    const int fps_wanted = std::max(kMinFramerateFps, fps);
    if (fps_wanted <=
        rtc::dchecked_cast<int>(source_restrictions_.max_frame_rate().value_or(
            std::numeric_limits<int>::max())))
      return false;

    RTC_LOG(LS_INFO) << "Scaling up framerate: " << fps_wanted;
    source_restrictions_.set_max_frame_rate(
        fps_wanted != std::numeric_limits<int>::max()
            ? absl::optional<double>(fps_wanted)
            : absl::nullopt);
    return true;
  }

 private:
  VideoSourceRestrictions source_restrictions_;

  RTC_DISALLOW_COPY_AND_ASSIGN(VideoSourceRestrictor);
};

class OveruseFrameDetectorResourceAdaptationModule::AdaptCounter final {
 public:
  AdaptCounter() {
    fps_counters_.resize(AdaptationObserverInterface::kScaleReasonSize);
    resolution_counters_.resize(AdaptationObserverInterface::kScaleReasonSize);
    static_assert(AdaptationObserverInterface::kScaleReasonSize == 2,
                  "Update MoveCount.");
  }
  ~AdaptCounter() = default;

  // Get number of adaptation downscales for |reason|.
  VideoStreamEncoderObserver::AdaptationSteps Counts(int reason) const {
    VideoStreamEncoderObserver::AdaptationSteps counts;
    counts.num_framerate_reductions = fps_counters_[reason];
    counts.num_resolution_reductions = resolution_counters_[reason];
    return counts;
  }

  std::string ToString() const {
    rtc::StringBuilder ss;
    ss << "Downgrade counts: fps: {" << ToString(fps_counters_);
    ss << "}, resolution: {" << ToString(resolution_counters_) << "}";
    return ss.Release();
  }

  void IncrementFramerate(int reason) { ++(fps_counters_[reason]); }
  void IncrementResolution(int reason) { ++(resolution_counters_[reason]); }
  void DecrementFramerate(int reason) {
    if (fps_counters_[reason] == 0) {
      // Balanced mode: Adapt up is in a different order, switch reason.
      // E.g. framerate adapt down: quality (2), framerate adapt up: cpu (3).
      // 1. Down resolution (cpu):  res={quality:0,cpu:1}, fps={quality:0,cpu:0}
      // 2. Down fps (quality):     res={quality:0,cpu:1}, fps={quality:1,cpu:0}
      // 3. Up fps (cpu):           res={quality:1,cpu:0}, fps={quality:0,cpu:0}
      // 4. Up resolution (quality):res={quality:0,cpu:0}, fps={quality:0,cpu:0}
      RTC_DCHECK_GT(TotalCount(reason), 0) << "No downgrade for reason.";
      RTC_DCHECK_GT(FramerateCount(), 0) << "Framerate not downgraded.";
      MoveCount(&resolution_counters_, reason);
      MoveCount(&fps_counters_,
                (reason + 1) % AdaptationObserverInterface::kScaleReasonSize);
    }
    --(fps_counters_[reason]);
    RTC_DCHECK_GE(fps_counters_[reason], 0);
  }

  void DecrementResolution(int reason) {
    if (resolution_counters_[reason] == 0) {
      // Balanced mode: Adapt up is in a different order, switch reason.
      RTC_DCHECK_GT(TotalCount(reason), 0) << "No downgrade for reason.";
      RTC_DCHECK_GT(ResolutionCount(), 0) << "Resolution not downgraded.";
      MoveCount(&fps_counters_, reason);
      MoveCount(&resolution_counters_,
                (reason + 1) % AdaptationObserverInterface::kScaleReasonSize);
    }
    --(resolution_counters_[reason]);
    RTC_DCHECK_GE(resolution_counters_[reason], 0);
  }

  void DecrementFramerate(int reason, int cur_fps) {
    DecrementFramerate(reason);
    // Reset if at max fps (i.e. in case of fewer steps up than down).
    if (cur_fps == std::numeric_limits<int>::max())
      absl::c_fill(fps_counters_, 0);
  }

  // Gets the total number of downgrades (for all adapt reasons).
  int FramerateCount() const { return Count(fps_counters_); }
  int ResolutionCount() const { return Count(resolution_counters_); }

  // Gets the total number of downgrades for |reason|.
  int FramerateCount(int reason) const { return fps_counters_[reason]; }
  int ResolutionCount(int reason) const { return resolution_counters_[reason]; }
  int TotalCount(int reason) const {
    return FramerateCount(reason) + ResolutionCount(reason);
  }

 private:
  std::string ToString(const std::vector<int>& counters) const {
    rtc::StringBuilder ss;
    for (size_t reason = 0;
         reason < AdaptationObserverInterface::kScaleReasonSize; ++reason) {
      ss << (reason ? " cpu" : "quality") << ":" << counters[reason];
    }
    return ss.Release();
  }

  int Count(const std::vector<int>& counters) const {
    return absl::c_accumulate(counters, 0);
  }

  void MoveCount(std::vector<int>* counters, int from_reason) {
    int to_reason =
        (from_reason + 1) % AdaptationObserverInterface::kScaleReasonSize;
    ++((*counters)[to_reason]);
    --((*counters)[from_reason]);
  }

  // Degradation counters holding number of framerate/resolution reductions
  // per adapt reason.
  std::vector<int> fps_counters_;
  std::vector<int> resolution_counters_;
};

OveruseFrameDetectorResourceAdaptationModule::
    OveruseFrameDetectorResourceAdaptationModule(
        Clock* clock,
        bool experiment_cpu_load_estimator,
        std::unique_ptr<OveruseFrameDetector> overuse_detector,
        VideoStreamEncoderObserver* encoder_stats_observer,
        ResourceAdaptationModuleListener* adaptation_listener)
    : adaptation_listener_(adaptation_listener),
      clock_(clock),
      experiment_cpu_load_estimator_(experiment_cpu_load_estimator),
      has_input_video_(false),
      degradation_preference_(DegradationPreference::DISABLED),
      adapt_counters_(),
      balanced_settings_(),
      last_adaptation_request_(absl::nullopt),
      source_restrictor_(std::make_unique<VideoSourceRestrictor>()),
      encode_usage_resource_(
          std::make_unique<EncodeUsageResource>(std::move(overuse_detector))),
      quality_scaler_resource_(std::make_unique<QualityScalerResource>()),
      quality_scaling_experiment_enabled_(QualityScalingExperiment::Enabled()),
      last_input_frame_size_(absl::nullopt),
      target_frame_rate_(absl::nullopt),
      encoder_target_bitrate_bps_(absl::nullopt),
      quality_scaler_settings_(QualityScalerSettings::ParseFromFieldTrials()),
      quality_rampup_done_(false),
      quality_rampup_experiment_(QualityRampupExperiment::ParseSettings()),
      encoder_settings_(absl::nullopt),
      encoder_stats_observer_(encoder_stats_observer),
      initial_framedrop_(0) {
  RTC_DCHECK(adaptation_listener_);
  RTC_DCHECK(encoder_stats_observer_);
  encode_usage_resource_->RegisterListener(this);
  quality_scaler_resource_->RegisterListener(this);
}

OveruseFrameDetectorResourceAdaptationModule::
    ~OveruseFrameDetectorResourceAdaptationModule() {}

void OveruseFrameDetectorResourceAdaptationModule::StartResourceAdaptation(
    ResourceAdaptationModuleListener* adaptation_listener) {
  RTC_DCHECK(encoder_settings_.has_value());
  // TODO(https://crbug.com/webrtc/11222): Rethink when the adaptation listener
  // should be passed in and why. If resources are separated from modules then
  // those resources may be started or stopped separately from the module.
  RTC_DCHECK_EQ(adaptation_listener, adaptation_listener_);
  encode_usage_resource_->StartCheckForOveruse(GetCpuOveruseOptions());
}

void OveruseFrameDetectorResourceAdaptationModule::StopResourceAdaptation() {
  encode_usage_resource_->StopCheckForOveruse();
  quality_scaler_resource_->StopCheckForOveruse();
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
      // TODO(asapersson): Consider removing |adapt_counters_| map and use one
      // AdaptCounter for all modes.
      source_restrictor_->ClearRestrictions();
      adapt_counters_.clear();
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
  start_bitrate_.set_start_bitrate_ = start_bitrate;
  start_bitrate_.set_start_bitrate_time_ms_ = clock_->TimeInMicroseconds();
}

void OveruseFrameDetectorResourceAdaptationModule::SetTargetBitrate(
    DataRate target_bitrate) {
  if (!target_bitrate.IsZero())
    encoder_target_bitrate_bps_ = target_bitrate.bps();

  // Check for bwe drop experiment
  // TODO(https://crbug.com/webrtc/11222): Should this move to
  // QualityScalerResource?
  if (start_bitrate_.set_start_bitrate_ > DataRate::Zero() &&
      !start_bitrate_.has_seen_first_bwe_drop_ &&
      quality_scaler_resource_->is_started() &&
      quality_scaler_settings_.InitialBitrateIntervalMs() &&
      quality_scaler_settings_.InitialBitrateFactor()) {
    int64_t diff_ms = clock_->TimeInMilliseconds() -
                      start_bitrate_.set_start_bitrate_time_ms_;
    if (diff_ms < quality_scaler_settings_.InitialBitrateIntervalMs().value() &&
        (target_bitrate <
         (start_bitrate_.set_start_bitrate_ *
          quality_scaler_settings_.InitialBitrateFactor().value()))) {
      RTC_LOG(LS_INFO) << "Reset initial_framedrop_. Start bitrate: "
                       << start_bitrate_.set_start_bitrate_.bps()
                       << ", target bitrate: " << target_bitrate.bps();
      initial_framedrop_ = 0;
      start_bitrate_.has_seen_first_bwe_drop_ = true;
    }
  }
}

void OveruseFrameDetectorResourceAdaptationModule::SetEncoderRates(
    const VideoEncoder::RateControlParameters& encoder_rates) {
  encoder_rates_ = encoder_rates;
}

void OveruseFrameDetectorResourceAdaptationModule::
    ResetVideoSourceRestrictions() {
  last_adaptation_request_.reset();
  source_restrictor_->ClearRestrictions();
  adapt_counters_.clear();
  MaybeUpdateVideoSourceRestrictions();
}

void OveruseFrameDetectorResourceAdaptationModule::OnFrame(
    const VideoFrame& frame) {
  last_input_frame_size_ = frame.size();
}

void OveruseFrameDetectorResourceAdaptationModule::OnFrameDroppedDueToSize() {
  int fps_count = GetConstAdaptCounter().FramerateCount(
      AdaptationObserverInterface::AdaptReason::kQuality);
  int res_count = GetConstAdaptCounter().ResolutionCount(
      AdaptationObserverInterface::AdaptReason::kQuality);
  OnResourceOveruse(AdaptationObserverInterface::AdaptReason::kQuality);
  if (degradation_preference() == DegradationPreference::BALANCED &&
      GetConstAdaptCounter().FramerateCount(
          AdaptationObserverInterface::AdaptReason::kQuality) > fps_count) {
    // Adapt framerate in same step as resolution.
    OnResourceOveruse(AdaptationObserverInterface::AdaptReason::kQuality);
  }
  if (GetConstAdaptCounter().ResolutionCount(
          AdaptationObserverInterface::AdaptReason::kQuality) > res_count) {
    encoder_stats_observer_->OnInitialQualityResolutionAdaptDown();
  }
  ++initial_framedrop_;
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

void OveruseFrameDetectorResourceAdaptationModule::OnMaybeEncodeFrame() {
  initial_framedrop_ = kMaxInitialFramedrop;
  MaybePerformQualityRampupExperiment();
}

bool OveruseFrameDetectorResourceAdaptationModule::DropInitialFrames() const {
  return initial_framedrop_ < kMaxInitialFramedrop;
}

void OveruseFrameDetectorResourceAdaptationModule::UpdateQualityScalerSettings(
    absl::optional<VideoEncoder::QpThresholds> qp_thresholds) {
  if (qp_thresholds.has_value()) {
    quality_scaler_resource_->StopCheckForOveruse();
    quality_scaler_resource_->StartCheckForOveruse(qp_thresholds.value());
    // Restart frame drops due to size.
    initial_framedrop_ = 0;
  } else {
    quality_scaler_resource_->StopCheckForOveruse();
    // Quality scaling disabled so we shouldn't drop initial frames.
    initial_framedrop_ = kMaxInitialFramedrop;
  }
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
  // If we didn't have this dependency on AdaptReason the module could be
  // listening to other types of Resources.
  RTC_DCHECK(&resource == encode_usage_resource_.get() ||
             &resource == quality_scaler_resource_.get());
  AdaptationObserverInterface::AdaptReason reason =
      &resource == encode_usage_resource_.get()
          ? AdaptationObserverInterface::AdaptReason::kCpu
          : AdaptationObserverInterface::AdaptReason::kQuality;
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

void OveruseFrameDetectorResourceAdaptationModule::OnResourceUnderuseForTesting(
    AdaptationObserverInterface::AdaptReason reason) {
  OnResourceUnderuse(reason);
}

bool OveruseFrameDetectorResourceAdaptationModule::OnResourceOveruseForTesting(
    AdaptationObserverInterface::AdaptReason reason) {
  return OnResourceOveruse(reason) !=
         ResourceListenerResponse::kQualityScalerShouldIncreaseFrequency;
}

void OveruseFrameDetectorResourceAdaptationModule::OnResourceUnderuse(
    AdaptationObserverInterface::AdaptReason reason) {
  if (!has_input_video_)
    return;
  const AdaptCounter& adapt_counter = GetConstAdaptCounter();
  int num_downgrades = adapt_counter.TotalCount(reason);
  if (num_downgrades == 0)
    return;
  RTC_DCHECK_GT(num_downgrades, 0);

  AdaptationRequest adaptation_request = {
      LastInputFrameSizeOrDefault(),
      encoder_stats_observer_->GetInputFrameRate(),
      AdaptationRequest::Mode::kAdaptUp};

  bool adapt_up_requested =
      last_adaptation_request_ &&
      last_adaptation_request_->mode_ == AdaptationRequest::Mode::kAdaptUp;

  if (EffectiveDegradataionPreference() ==
      DegradationPreference::MAINTAIN_FRAMERATE) {
    if (adapt_up_requested &&
        adaptation_request.input_pixel_count_ <=
            last_adaptation_request_->input_pixel_count_) {
      // Don't request higher resolution if the current resolution is not
      // higher than the last time we asked for the resolution to be higher.
      return;
    }
  }

  switch (EffectiveDegradataionPreference()) {
    case DegradationPreference::BALANCED: {
      // Check if quality should be increased based on bitrate.
      if (reason == AdaptationObserverInterface::AdaptReason::kQuality &&
          !balanced_settings_.CanAdaptUp(
              GetVideoCodecTypeOrGeneric(), LastInputFrameSizeOrDefault(),
              encoder_target_bitrate_bps_.value_or(0))) {
        return;
      }
      // Try scale up framerate, if higher.
      int fps = balanced_settings_.MaxFps(GetVideoCodecTypeOrGeneric(),
                                          LastInputFrameSizeOrDefault());
      if (source_restrictor_->IncreaseFramerate(fps)) {
        GetAdaptCounter().DecrementFramerate(reason, fps);
        // Reset framerate in case of fewer fps steps down than up.
        if (adapt_counter.FramerateCount() == 0 &&
            fps != std::numeric_limits<int>::max()) {
          RTC_LOG(LS_INFO) << "Removing framerate down-scaling setting.";
          source_restrictor_->IncreaseFramerate(
              std::numeric_limits<int>::max());
        }
        break;
      }
      // Check if resolution should be increased based on bitrate.
      if (reason == AdaptationObserverInterface::AdaptReason::kQuality &&
          !balanced_settings_.CanAdaptUpResolution(
              GetVideoCodecTypeOrGeneric(), LastInputFrameSizeOrDefault(),
              encoder_target_bitrate_bps_.value_or(0))) {
        return;
      }
      // Scale up resolution.
      ABSL_FALLTHROUGH_INTENDED;
    }
    case DegradationPreference::MAINTAIN_FRAMERATE: {
      // Check if resolution should be increased based on bitrate and
      // limits specified by encoder capabilities.
      if (reason == AdaptationObserverInterface::AdaptReason::kQuality &&
          !CanAdaptUpResolution(LastInputFrameSizeOrDefault(),
                                encoder_target_bitrate_bps_.value_or(0))) {
        return;
      }

      // Scale up resolution.
      int pixel_count = adaptation_request.input_pixel_count_;
      if (adapt_counter.ResolutionCount() == 1) {
        RTC_LOG(LS_INFO) << "Removing resolution down-scaling setting.";
        pixel_count = std::numeric_limits<int>::max();
      }
      if (!source_restrictor_->RequestHigherResolutionThan(pixel_count))
        return;
      GetAdaptCounter().DecrementResolution(reason);
      break;
    }
    case DegradationPreference::MAINTAIN_RESOLUTION: {
      // Scale up framerate.
      int fps = adaptation_request.framerate_fps_;
      if (adapt_counter.FramerateCount() == 1) {
        RTC_LOG(LS_INFO) << "Removing framerate down-scaling setting.";
        fps = std::numeric_limits<int>::max();
      }

      const int requested_framerate =
          source_restrictor_->RequestHigherFramerateThan(fps);
      if (requested_framerate == -1) {
        return;
      }
      GetAdaptCounter().DecrementFramerate(reason);
      break;
    }
    case DegradationPreference::DISABLED:
      return;
  }

  // Tell the adaptation listener to reconfigure the source for us according to
  // the latest adaptation.
  MaybeUpdateVideoSourceRestrictions();

  last_adaptation_request_.emplace(adaptation_request);

  UpdateAdaptationStats(reason);

  RTC_LOG(LS_INFO) << adapt_counter.ToString();
}

ResourceListenerResponse
OveruseFrameDetectorResourceAdaptationModule::OnResourceOveruse(
    AdaptationObserverInterface::AdaptReason reason) {
  if (!has_input_video_)
    return ResourceListenerResponse::kQualityScalerShouldIncreaseFrequency;
  AdaptationRequest adaptation_request = {
      LastInputFrameSizeOrDefault(),
      encoder_stats_observer_->GetInputFrameRate(),
      AdaptationRequest::Mode::kAdaptDown};

  bool downgrade_requested =
      last_adaptation_request_ &&
      last_adaptation_request_->mode_ == AdaptationRequest::Mode::kAdaptDown;

  switch (EffectiveDegradataionPreference()) {
    case DegradationPreference::BALANCED:
      break;
    case DegradationPreference::MAINTAIN_FRAMERATE:
      if (downgrade_requested &&
          adaptation_request.input_pixel_count_ >=
              last_adaptation_request_->input_pixel_count_) {
        // Don't request lower resolution if the current resolution is not
        // lower than the last time we asked for the resolution to be lowered.
        return ResourceListenerResponse::kNothing;
      }
      break;
    case DegradationPreference::MAINTAIN_RESOLUTION:
      if (adaptation_request.framerate_fps_ <= 0 ||
          (downgrade_requested &&
           adaptation_request.framerate_fps_ < kMinFramerateFps)) {
        // If no input fps estimate available, can't determine how to scale down
        // framerate. Otherwise, don't request lower framerate if we don't have
        // a valid frame rate. Since framerate, unlike resolution, is a measure
        // we have to estimate, and can fluctuate naturally over time, don't
        // make the same kind of limitations as for resolution, but trust the
        // overuse detector to not trigger too often.
        return ResourceListenerResponse::kNothing;
      }
      break;
    case DegradationPreference::DISABLED:
      return ResourceListenerResponse::kNothing;
  }

  ResourceListenerResponse response = ResourceListenerResponse::kNothing;

  switch (EffectiveDegradataionPreference()) {
    case DegradationPreference::BALANCED: {
      // Try scale down framerate, if lower.
      int fps = balanced_settings_.MinFps(GetVideoCodecTypeOrGeneric(),
                                          LastInputFrameSizeOrDefault());
      if (source_restrictor_->RestrictFramerate(fps)) {
        GetAdaptCounter().IncrementFramerate(reason);
        // Check if requested fps is higher (or close to) input fps.
        absl::optional<int> min_diff =
            balanced_settings_.MinFpsDiff(LastInputFrameSizeOrDefault());
        if (min_diff && adaptation_request.framerate_fps_ > 0) {
          int fps_diff = adaptation_request.framerate_fps_ - fps;
          if (fps_diff < min_diff.value()) {
            response =
                ResourceListenerResponse::kQualityScalerShouldIncreaseFrequency;
          }
        }
        break;
      }
      // Scale down resolution.
      ABSL_FALLTHROUGH_INTENDED;
    }
    case DegradationPreference::MAINTAIN_FRAMERATE: {
      // Scale down resolution.
      bool min_pixels_reached = false;
      if (!source_restrictor_->RequestResolutionLowerThan(
              adaptation_request.input_pixel_count_,
              encoder_settings_.has_value()
                  ? encoder_settings_->encoder_info()
                        .scaling_settings.min_pixels_per_frame
                  : kDefaultMinPixelsPerFrame,
              &min_pixels_reached)) {
        if (min_pixels_reached)
          encoder_stats_observer_->OnMinPixelLimitReached();
        return ResourceListenerResponse::kNothing;
      }
      GetAdaptCounter().IncrementResolution(reason);
      break;
    }
    case DegradationPreference::MAINTAIN_RESOLUTION: {
      // Scale down framerate.
      const int requested_framerate =
          source_restrictor_->RequestFramerateLowerThan(
              adaptation_request.framerate_fps_);
      if (requested_framerate == -1)
        return ResourceListenerResponse::kNothing;
      GetAdaptCounter().IncrementFramerate(reason);
      break;
    }
    case DegradationPreference::DISABLED:
      RTC_NOTREACHED();
  }

  // Tell the adaptation listener to reconfigure the source for us according to
  // the latest adaptation.
  MaybeUpdateVideoSourceRestrictions();

  last_adaptation_request_.emplace(adaptation_request);

  UpdateAdaptationStats(reason);

  RTC_LOG(LS_INFO) << GetConstAdaptCounter().ToString();
  return response;
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

// TODO(nisse): Delete, once AdaptReason and AdaptationReason are merged.
void OveruseFrameDetectorResourceAdaptationModule::UpdateAdaptationStats(
    AdaptationObserverInterface::AdaptReason reason) {
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
  VideoStreamEncoderObserver::AdaptationSteps counts =
      GetConstAdaptCounter().Counts(reason);
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

DegradationPreference OveruseFrameDetectorResourceAdaptationModule::
    EffectiveDegradataionPreference() {
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

OveruseFrameDetectorResourceAdaptationModule::AdaptCounter&
OveruseFrameDetectorResourceAdaptationModule::GetAdaptCounter() {
  return adapt_counters_[degradation_preference_];
}

const OveruseFrameDetectorResourceAdaptationModule::AdaptCounter&
OveruseFrameDetectorResourceAdaptationModule::GetConstAdaptCounter() {
  return adapt_counters_[degradation_preference_];
}

bool OveruseFrameDetectorResourceAdaptationModule::CanAdaptUpResolution(
    int pixels,
    uint32_t bitrate_bps) const {
  absl::optional<VideoEncoder::ResolutionBitrateLimits> bitrate_limits =
      encoder_settings_.has_value()
          ? GetEncoderBitrateLimits(
                encoder_settings_->encoder_info(),
                source_restrictor_->GetHigherResolutionThan(pixels))
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
  if (try_quality_rampup &&
      GetConstAdaptCounter().ResolutionCount(
          AdaptationObserverInterface::AdaptReason::kQuality) > 0 &&
      GetConstAdaptCounter().TotalCount(
          AdaptationObserverInterface::AdaptReason::kCpu) == 0) {
    RTC_LOG(LS_INFO) << "Reset quality limitations.";
    ResetVideoSourceRestrictions();
    quality_rampup_done_ = true;
  }
}

}  // namespace webrtc
