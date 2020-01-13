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
#include "api/video/video_source_interface.h"
#include "call/adaptation/video_source_restrictions.h"
#include "rtc_base/logging.h"
#include "rtc_base/numerics/safe_conversions.h"
#include "rtc_base/strings/string_builder.h"
#include "rtc_base/system/fallthrough.h"
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

}  // namespace

// VideoSourceRestrictor is responsible for keeping track of current
// VideoSourceRestrictions and how to modify them in response to adapting up or
// down. It is not reponsible for determining when we should adapt up or down -
// for that, see OveruseFrameDetectorResourceAdaptationModule::AdaptUp() and
// AdaptDown() - only how to modify the source/sink restrictions when this
// happens. Note that it is also not responsible for reconfigruring the
// source/sink, it is only a keeper of desired restrictions.
//
// Thread safety is ensured between SetHasInputVideoAndDegradationPreference()
// calls on the worker thread and adaptation logic on the encoder task queue
// using a lock.
class OveruseFrameDetectorResourceAdaptationModule::VideoSourceRestrictor {
 public:
  explicit VideoSourceRestrictor(
      VideoSourceSinkController* video_source_sink_controller)
      : video_source_sink_controller_(video_source_sink_controller),
        has_input_video_(false),
        degradation_preference_(DegradationPreference::DISABLED) {}

  VideoSourceRestrictions source_restrictions() {
    rtc::CritScope lock(&crit_);
    return source_restrictions_;
  }

  // Inform the restrictor of new source status and degradation preference.
  // TODO(hbos): Can this be moved to the encoder queue? If so, the |crit_| lock
  // can be removed and we only need a sequence checker.
  void SetHasInputVideoAndDegradationPreference(
      bool has_input_video,
      DegradationPreference degradation_preference) {
    // Called on libjingle's worker thread.
    RTC_DCHECK_RUN_ON(&main_checker_);
    rtc::CritScope lock(&crit_);
    has_input_video_ = has_input_video;
    degradation_preference_ = degradation_preference;
  }

  // Informs the sink of the new source settings.
  // TODO(https://crbug.com/webrtc/11222): Handle all sink updates in
  // video_stream_encoder.cc. This method is only used when setting the
  // degradation preference such that it moves in or out of the "balanced"
  // state, or when clearing all counters. When moving the remaining degradation
  // preference logic inside the VideoSourceSinkController to here, stop
  // explicitly setting the controller's restrictions and instead inform the
  // VideoStreamEncoder of updated restrictions using
  // OnVideoSourceRestrictionsUpdated().
  void ResetPixelFpsCount() {
    rtc::CritScope lock(&crit_);
    // Clear all restrictions.
    source_restrictions_ = VideoSourceRestrictions();
    video_source_sink_controller_->SetRestrictions(source_restrictions_);
    video_source_sink_controller_->PushSourceSinkSettings();
  }

  // Updates the source_restrictions(). The source/sink has to be informed of
  // this separately.
  bool RequestResolutionLowerThan(int pixel_count,
                                  int min_pixels_per_frame,
                                  bool* min_pixels_reached) {
    // Called on the encoder task queue.
    rtc::CritScope lock(&crit_);
    if (!has_input_video_ ||
        !IsResolutionScalingEnabled(degradation_preference_)) {
      // This can happen since |degradation_preference_| is set on libjingle's
      // worker thread but the adaptation is done on the encoder task queue.
      return false;
    }
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
    // Called on the encoder task queue.
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
    // Called on the encoder task queue.
    rtc::CritScope lock(&crit_);
    if (!has_input_video_ ||
        !IsResolutionScalingEnabled(degradation_preference_)) {
      // This can happen since |degradation_preference_| is set on libjingle's
      // worker thread but the adaptation is done on the encoder task queue.
      return false;
    }
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
    // Called on the encoder task queue.
    // The input frame rate will be scaled up to the last step, with rounding.
    int framerate_wanted = fps;
    if (fps != std::numeric_limits<int>::max())
      framerate_wanted = (fps * 3) / 2;

    return IncreaseFramerate(framerate_wanted) ? framerate_wanted : -1;
  }

  // Updates the source_restrictions(). The source/sink has to be informed of
  // this separately.
  bool RestrictFramerate(int fps) {
    // Called on the encoder task queue.
    rtc::CritScope lock(&crit_);
    if (!has_input_video_ ||
        !IsFramerateScalingEnabled(degradation_preference_))
      return false;

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
    // Called on the encoder task queue.
    rtc::CritScope lock(&crit_);
    if (!has_input_video_ ||
        !IsFramerateScalingEnabled(degradation_preference_))
      return false;

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
  rtc::CriticalSection crit_;
  SequenceChecker main_checker_;
  VideoSourceSinkController* const video_source_sink_controller_;
  VideoSourceRestrictions source_restrictions_ RTC_GUARDED_BY(&crit_);
  bool has_input_video_ RTC_GUARDED_BY(&crit_);
  DegradationPreference degradation_preference_ RTC_GUARDED_BY(&crit_);

  RTC_DISALLOW_COPY_AND_ASSIGN(VideoSourceRestrictor);
};

// Class holding adaptation information.
OveruseFrameDetectorResourceAdaptationModule::AdaptCounter::AdaptCounter() {
  fps_counters_.resize(kScaleReasonSize);
  resolution_counters_.resize(kScaleReasonSize);
  static_assert(kScaleReasonSize == 2, "Update MoveCount.");
}

OveruseFrameDetectorResourceAdaptationModule::AdaptCounter::~AdaptCounter() {}

std::string
OveruseFrameDetectorResourceAdaptationModule::AdaptCounter::ToString() const {
  rtc::StringBuilder ss;
  ss << "Downgrade counts: fps: {" << ToString(fps_counters_);
  ss << "}, resolution: {" << ToString(resolution_counters_) << "}";
  return ss.Release();
}

VideoStreamEncoderObserver::AdaptationSteps
OveruseFrameDetectorResourceAdaptationModule::AdaptCounter::Counts(
    int reason) const {
  VideoStreamEncoderObserver::AdaptationSteps counts;
  counts.num_framerate_reductions = fps_counters_[reason];
  counts.num_resolution_reductions = resolution_counters_[reason];
  return counts;
}

void OveruseFrameDetectorResourceAdaptationModule::AdaptCounter::
    IncrementFramerate(int reason) {
  ++(fps_counters_[reason]);
}

void OveruseFrameDetectorResourceAdaptationModule::AdaptCounter::
    IncrementResolution(int reason) {
  ++(resolution_counters_[reason]);
}

void OveruseFrameDetectorResourceAdaptationModule::AdaptCounter::
    DecrementFramerate(int reason) {
  if (fps_counters_[reason] == 0) {
    // Balanced mode: Adapt up is in a different order, switch reason.
    // E.g. framerate adapt down: quality (2), framerate adapt up: cpu (3).
    // 1. Down resolution (cpu):   res={quality:0,cpu:1}, fps={quality:0,cpu:0}
    // 2. Down fps (quality):      res={quality:0,cpu:1}, fps={quality:1,cpu:0}
    // 3. Up fps (cpu):            res={quality:1,cpu:0}, fps={quality:0,cpu:0}
    // 4. Up resolution (quality): res={quality:0,cpu:0}, fps={quality:0,cpu:0}
    RTC_DCHECK_GT(TotalCount(reason), 0) << "No downgrade for reason.";
    RTC_DCHECK_GT(FramerateCount(), 0) << "Framerate not downgraded.";
    MoveCount(&resolution_counters_, reason);
    MoveCount(&fps_counters_, (reason + 1) % kScaleReasonSize);
  }
  --(fps_counters_[reason]);
  RTC_DCHECK_GE(fps_counters_[reason], 0);
}

void OveruseFrameDetectorResourceAdaptationModule::AdaptCounter::
    DecrementResolution(int reason) {
  if (resolution_counters_[reason] == 0) {
    // Balanced mode: Adapt up is in a different order, switch reason.
    RTC_DCHECK_GT(TotalCount(reason), 0) << "No downgrade for reason.";
    RTC_DCHECK_GT(ResolutionCount(), 0) << "Resolution not downgraded.";
    MoveCount(&fps_counters_, reason);
    MoveCount(&resolution_counters_, (reason + 1) % kScaleReasonSize);
  }
  --(resolution_counters_[reason]);
  RTC_DCHECK_GE(resolution_counters_[reason], 0);
}

void OveruseFrameDetectorResourceAdaptationModule::AdaptCounter::
    DecrementFramerate(int reason, int cur_fps) {
  DecrementFramerate(reason);
  // Reset if at max fps (i.e. in case of fewer steps up than down).
  if (cur_fps == std::numeric_limits<int>::max())
    absl::c_fill(fps_counters_, 0);
}

int OveruseFrameDetectorResourceAdaptationModule::AdaptCounter::FramerateCount()
    const {
  return Count(fps_counters_);
}

int OveruseFrameDetectorResourceAdaptationModule::AdaptCounter::
    ResolutionCount() const {
  return Count(resolution_counters_);
}

int OveruseFrameDetectorResourceAdaptationModule::AdaptCounter::FramerateCount(
    int reason) const {
  return fps_counters_[reason];
}

int OveruseFrameDetectorResourceAdaptationModule::AdaptCounter::ResolutionCount(
    int reason) const {
  return resolution_counters_[reason];
}

int OveruseFrameDetectorResourceAdaptationModule::AdaptCounter::TotalCount(
    int reason) const {
  return FramerateCount(reason) + ResolutionCount(reason);
}

int OveruseFrameDetectorResourceAdaptationModule::AdaptCounter::Count(
    const std::vector<int>& counters) const {
  return absl::c_accumulate(counters, 0);
}

void OveruseFrameDetectorResourceAdaptationModule::AdaptCounter::MoveCount(
    std::vector<int>* counters,
    int from_reason) {
  int to_reason = (from_reason + 1) % kScaleReasonSize;
  ++((*counters)[to_reason]);
  --((*counters)[from_reason]);
}

std::string
OveruseFrameDetectorResourceAdaptationModule::AdaptCounter::ToString(
    const std::vector<int>& counters) const {
  rtc::StringBuilder ss;
  for (size_t reason = 0; reason < kScaleReasonSize; ++reason) {
    ss << (reason ? " cpu" : "quality") << ":" << counters[reason];
  }
  return ss.Release();
}

OveruseFrameDetectorResourceAdaptationModule::
    OveruseFrameDetectorResourceAdaptationModule(
        VideoStreamEncoder* video_stream_encoder,
        VideoSourceSinkController* video_source_sink_controller,
        std::unique_ptr<OveruseFrameDetector> overuse_detector,
        VideoStreamEncoderObserver* encoder_stats_observer,
        ResourceAdaptationModuleListener* adaptation_listener)
    : encoder_queue_(nullptr),
      adaptation_listener_(adaptation_listener),
      video_stream_encoder_(video_stream_encoder),
      video_source_sink_controller_(video_source_sink_controller),
      degradation_preference_(DegradationPreference::DISABLED),
      adapt_counters_(),
      balanced_settings_(),
      last_adaptation_request_(absl::nullopt),
      last_frame_pixel_count_(absl::nullopt),
      source_restrictor_(std::make_unique<VideoSourceRestrictor>(
          video_source_sink_controller)),
      overuse_detector_(std::move(overuse_detector)),
      codec_max_framerate_(-1),
      encoder_start_bitrate_bps_(0),
      is_quality_scaler_enabled_(false),
      encoder_config_(),
      encoder_(nullptr),
      encoder_stats_observer_(encoder_stats_observer) {
  RTC_DCHECK(adaptation_listener_);
  RTC_DCHECK(video_stream_encoder_);
  RTC_DCHECK(overuse_detector_);
  RTC_DCHECK(encoder_stats_observer_);
}

OveruseFrameDetectorResourceAdaptationModule::
    ~OveruseFrameDetectorResourceAdaptationModule() {}

void OveruseFrameDetectorResourceAdaptationModule::Initialize(
    rtc::TaskQueue* encoder_queue) {
  RTC_DCHECK(!encoder_queue_);
  encoder_queue_ = encoder_queue;
  RTC_DCHECK(encoder_queue_);
}

void OveruseFrameDetectorResourceAdaptationModule::SetEncoder(
    VideoEncoder* encoder) {
  RTC_DCHECK(encoder_queue_);
  RTC_DCHECK_RUN_ON(encoder_queue_);
  encoder_ = encoder;
}

void OveruseFrameDetectorResourceAdaptationModule::StartCheckForOveruse(
    ResourceAdaptationModuleListener* adaptation_listener) {
  RTC_DCHECK(encoder_queue_);
  RTC_DCHECK_RUN_ON(encoder_queue_);
  RTC_DCHECK(encoder_);
  // TODO(hbos): When AdaptUp() and AdaptDown() are no longer invoked outside
  // the interval between StartCheckForOveruse() and StopCheckForOveruse(),
  // support configuring which |adaptation_listener_| to use on the fly. It is
  // currently hardcoded for the entire lifetime of the module in order to
  // support adaptation caused by VideoStreamEncoder or QualityScaler invoking
  // AdaptUp() and AdaptDown() even when the OveruseDetector is inactive.
  RTC_DCHECK_EQ(adaptation_listener, adaptation_listener_);
  overuse_detector_->StartCheckForOveruse(
      encoder_queue_, video_stream_encoder_->GetCpuOveruseOptions(), this);
}

void OveruseFrameDetectorResourceAdaptationModule::StopCheckForOveruse() {
  RTC_DCHECK(encoder_queue_);
  RTC_DCHECK_RUN_ON(encoder_queue_);
  overuse_detector_->StopCheckForOveruse();
}

void OveruseFrameDetectorResourceAdaptationModule::FrameCaptured(
    const VideoFrame& frame,
    int64_t time_when_first_seen_us) {
  RTC_DCHECK(encoder_queue_);
  RTC_DCHECK_RUN_ON(encoder_queue_);
  overuse_detector_->FrameCaptured(frame, time_when_first_seen_us);
}

void OveruseFrameDetectorResourceAdaptationModule::FrameSent(
    uint32_t timestamp,
    int64_t time_sent_in_us,
    int64_t capture_time_us,
    absl::optional<int> encode_duration_us) {
  RTC_DCHECK(encoder_queue_);
  RTC_DCHECK_RUN_ON(encoder_queue_);
  overuse_detector_->FrameSent(timestamp, time_sent_in_us, capture_time_us,
                               encode_duration_us);
}

void OveruseFrameDetectorResourceAdaptationModule::SetLastFramePixelCount(
    absl::optional<int> last_frame_pixel_count) {
  RTC_DCHECK(encoder_queue_);
  RTC_DCHECK_RUN_ON(encoder_queue_);
  last_frame_pixel_count_ = last_frame_pixel_count;
}

void OveruseFrameDetectorResourceAdaptationModule::SetEncoderConfig(
    VideoEncoderConfig encoder_config) {
  RTC_DCHECK(encoder_queue_);
  RTC_DCHECK_RUN_ON(encoder_queue_);
  encoder_config_ = std::move(encoder_config);
}

void OveruseFrameDetectorResourceAdaptationModule::SetCodecMaxFramerate(
    int codec_max_framerate) {
  RTC_DCHECK(encoder_queue_);
  RTC_DCHECK_RUN_ON(encoder_queue_);
  codec_max_framerate_ = codec_max_framerate;
}

void OveruseFrameDetectorResourceAdaptationModule::SetEncoderStartBitrateBps(
    uint32_t encoder_start_bitrate_bps) {
  RTC_DCHECK(encoder_queue_);
  RTC_DCHECK_RUN_ON(encoder_queue_);
  encoder_start_bitrate_bps_ = encoder_start_bitrate_bps;
}

void OveruseFrameDetectorResourceAdaptationModule::SetIsQualityScalerEnabled(
    bool is_quality_scaler_enabled) {
  RTC_DCHECK(encoder_queue_);
  RTC_DCHECK_RUN_ON(encoder_queue_);
  is_quality_scaler_enabled_ = is_quality_scaler_enabled;
}

void OveruseFrameDetectorResourceAdaptationModule::
    SetHasInputVideoAndDegradationPreference(
        bool has_input_video,
        DegradationPreference degradation_preference) {
  source_restrictor_->SetHasInputVideoAndDegradationPreference(
      has_input_video, degradation_preference);
  encoder_queue_->PostTask([this, degradation_preference] {
    RTC_DCHECK_RUN_ON(encoder_queue_);
    if (degradation_preference_ != degradation_preference) {
      // Reset adaptation state, so that we're not tricked into thinking there's
      // an already pending request of the same type.
      last_adaptation_request_.reset();
      if (degradation_preference == DegradationPreference::BALANCED ||
          degradation_preference_ == DegradationPreference::BALANCED) {
        // TODO(asapersson): Consider removing |adapt_counters_| map and use one
        // AdaptCounter for all modes.
        source_restrictor_->ResetPixelFpsCount();
        adapt_counters_.clear();
      }
    }
    degradation_preference_ = degradation_preference;
  });
}

void OveruseFrameDetectorResourceAdaptationModule::RefreshTargetFramerate() {
  RTC_DCHECK(encoder_queue_);
  RTC_DCHECK_RUN_ON(encoder_queue_);
  // We need the "sink wants" from the |video_source_sink_controller_| because
  // the controller filters its current settings as "sink wants" differently
  // depending degradation preferences.
  // TODO(https://crbug.com/webrtc/11222): When degradation preference-related
  // changes to settings are handled by this class instead, we can remove the
  // dependency on the controller; the VideoSourceRestrictions outputted by this
  // module will then be the "final" settings, including the max frame rate.
  auto sink_wants = video_source_sink_controller_->CurrentSettingsToSinkWants();
  // Get the current target framerate, ie the maximum framerate as specified by
  // the current codec configuration, or any limit imposed by cpu adaption in
  // maintain-resolution or balanced mode. This is used to make sure overuse
  // detection doesn't needlessly trigger in low and/or variable framerate
  // scenarios.
  int target_framerate =
      std::min(codec_max_framerate_, sink_wants.max_framerate_fps);
  overuse_detector_->OnTargetFramerateUpdated(target_framerate);
}

void OveruseFrameDetectorResourceAdaptationModule::ResetAdaptationCounters() {
  RTC_DCHECK(encoder_queue_);
  RTC_DCHECK_RUN_ON(encoder_queue_);
  last_adaptation_request_.reset();
  source_restrictor_->ResetPixelFpsCount();
  adapt_counters_.clear();
}

void OveruseFrameDetectorResourceAdaptationModule::AdaptUp(AdaptReason reason) {
  RTC_DCHECK(encoder_queue_);
  RTC_DCHECK_RUN_ON(encoder_queue_);
  const AdaptCounter& adapt_counter = GetConstAdaptCounter();
  int num_downgrades = adapt_counter.TotalCount(reason);
  if (num_downgrades == 0)
    return;
  RTC_DCHECK_GT(num_downgrades, 0);

  AdaptationRequest adaptation_request = {
      *last_frame_pixel_count_, encoder_stats_observer_->GetInputFrameRate(),
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
      if (reason == kQuality &&
          !balanced_settings_.CanAdaptUp(encoder_config_.codec_type,
                                         *last_frame_pixel_count_,
                                         encoder_start_bitrate_bps_)) {
        return;
      }
      // Try scale up framerate, if higher.
      int fps = balanced_settings_.MaxFps(encoder_config_.codec_type,
                                          *last_frame_pixel_count_);
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
      if (reason == kQuality &&
          !balanced_settings_.CanAdaptUpResolution(
              encoder_config_.codec_type, *last_frame_pixel_count_,
              encoder_start_bitrate_bps_)) {
        return;
      }
      // Scale up resolution.
      RTC_FALLTHROUGH();
    }
    case DegradationPreference::MAINTAIN_FRAMERATE: {
      // Check if resolution should be increased based on bitrate and
      // limits specified by encoder capabilities.
      if (reason == kQuality &&
          !CanAdaptUpResolution(*last_frame_pixel_count_,
                                encoder_start_bitrate_bps_)) {
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
        overuse_detector_->OnTargetFramerateUpdated(codec_max_framerate_);
        return;
      }
      overuse_detector_->OnTargetFramerateUpdated(
          std::min(codec_max_framerate_, requested_framerate));
      GetAdaptCounter().DecrementFramerate(reason);
      break;
    }
    case DegradationPreference::DISABLED:
      return;
  }

  // Tell the adaptation listener to reconfigure the source for us according to
  // the latest adaptation.
  adaptation_listener_->OnVideoSourceRestrictionsUpdated(
      source_restrictor_->source_restrictions());

  last_adaptation_request_.emplace(adaptation_request);

  UpdateAdaptationStats(reason);

  RTC_LOG(LS_INFO) << adapt_counter.ToString();
}

bool OveruseFrameDetectorResourceAdaptationModule::AdaptDown(
    AdaptReason reason) {
  RTC_DCHECK(encoder_queue_);
  RTC_DCHECK_RUN_ON(encoder_queue_);
  AdaptationRequest adaptation_request = {
      *last_frame_pixel_count_, encoder_stats_observer_->GetInputFrameRate(),
      AdaptationRequest::Mode::kAdaptDown};

  bool downgrade_requested =
      last_adaptation_request_ &&
      last_adaptation_request_->mode_ == AdaptationRequest::Mode::kAdaptDown;

  bool did_adapt = true;

  switch (EffectiveDegradataionPreference()) {
    case DegradationPreference::BALANCED:
      break;
    case DegradationPreference::MAINTAIN_FRAMERATE:
      if (downgrade_requested &&
          adaptation_request.input_pixel_count_ >=
              last_adaptation_request_->input_pixel_count_) {
        // Don't request lower resolution if the current resolution is not
        // lower than the last time we asked for the resolution to be lowered.
        return true;
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
        return true;
      }
      break;
    case DegradationPreference::DISABLED:
      return true;
  }

  switch (EffectiveDegradataionPreference()) {
    case DegradationPreference::BALANCED: {
      // Try scale down framerate, if lower.
      int fps = balanced_settings_.MinFps(encoder_config_.codec_type,
                                          *last_frame_pixel_count_);
      if (source_restrictor_->RestrictFramerate(fps)) {
        GetAdaptCounter().IncrementFramerate(reason);
        // Check if requested fps is higher (or close to) input fps.
        absl::optional<int> min_diff =
            balanced_settings_.MinFpsDiff(*last_frame_pixel_count_);
        if (min_diff && adaptation_request.framerate_fps_ > 0) {
          int fps_diff = adaptation_request.framerate_fps_ - fps;
          if (fps_diff < min_diff.value()) {
            did_adapt = false;
          }
        }
        break;
      }
      // Scale down resolution.
      RTC_FALLTHROUGH();
    }
    case DegradationPreference::MAINTAIN_FRAMERATE: {
      // Scale down resolution.
      bool min_pixels_reached = false;
      if (!source_restrictor_->RequestResolutionLowerThan(
              adaptation_request.input_pixel_count_,
              encoder_->GetEncoderInfo().scaling_settings.min_pixels_per_frame,
              &min_pixels_reached)) {
        if (min_pixels_reached)
          encoder_stats_observer_->OnMinPixelLimitReached();
        return true;
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
        return true;
      RTC_DCHECK_NE(codec_max_framerate_, -1);
      overuse_detector_->OnTargetFramerateUpdated(
          std::min(codec_max_framerate_, requested_framerate));
      GetAdaptCounter().IncrementFramerate(reason);
      break;
    }
    case DegradationPreference::DISABLED:
      RTC_NOTREACHED();
  }

  // Tell the adaptation listener to reconfigure the source for us according to
  // the latest adaptation.
  adaptation_listener_->OnVideoSourceRestrictionsUpdated(
      source_restrictor_->source_restrictions());

  last_adaptation_request_.emplace(adaptation_request);

  UpdateAdaptationStats(reason);

  RTC_LOG(LS_INFO) << GetConstAdaptCounter().ToString();
  return did_adapt;
}

// TODO(nisse): Delete, once AdaptReason and AdaptationReason are merged.
void OveruseFrameDetectorResourceAdaptationModule::UpdateAdaptationStats(
    AdaptReason reason) {
  switch (reason) {
    case kCpu:
      encoder_stats_observer_->OnAdaptationChanged(
          VideoStreamEncoderObserver::AdaptationReason::kCpu,
          GetActiveCounts(kCpu), GetActiveCounts(kQuality));
      break;
    case kQuality:
      encoder_stats_observer_->OnAdaptationChanged(
          VideoStreamEncoderObserver::AdaptationReason::kQuality,
          GetActiveCounts(kCpu), GetActiveCounts(kQuality));
      break;
  }
}

VideoStreamEncoderObserver::AdaptationSteps
OveruseFrameDetectorResourceAdaptationModule::GetActiveCounts(
    AdaptReason reason) {
  RTC_DCHECK(encoder_queue_);
  RTC_DCHECK_RUN_ON(encoder_queue_);
  VideoStreamEncoderObserver::AdaptationSteps counts =
      GetConstAdaptCounter().Counts(reason);
  switch (reason) {
    case kCpu:
      if (!IsFramerateScalingEnabled(degradation_preference_))
        counts.num_framerate_reductions = absl::nullopt;
      if (!IsResolutionScalingEnabled(degradation_preference_))
        counts.num_resolution_reductions = absl::nullopt;
      break;
    case kQuality:
      if (!IsFramerateScalingEnabled(degradation_preference_) ||
          !is_quality_scaler_enabled_) {
        counts.num_framerate_reductions = absl::nullopt;
      }
      if (!IsResolutionScalingEnabled(degradation_preference_) ||
          !is_quality_scaler_enabled_) {
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
  return (encoder_config_.content_type ==
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
  RTC_DCHECK(encoder_queue_);
  RTC_DCHECK_RUN_ON(encoder_queue_);
  return adapt_counters_[degradation_preference_];
}

absl::optional<VideoEncoder::QpThresholds>
OveruseFrameDetectorResourceAdaptationModule::GetQpThresholds() const {
  RTC_DCHECK(encoder_queue_);
  RTC_DCHECK_RUN_ON(encoder_queue_);
  RTC_DCHECK(last_frame_pixel_count_.has_value());
  return balanced_settings_.GetQpThresholds(encoder_config_.codec_type,
                                            last_frame_pixel_count_.value());
}

bool OveruseFrameDetectorResourceAdaptationModule::CanAdaptUpResolution(
    int pixels,
    uint32_t bitrate_bps) const {
  absl::optional<VideoEncoder::ResolutionBitrateLimits> bitrate_limits =
      GetEncoderBitrateLimits(
          encoder_->GetEncoderInfo(),
          source_restrictor_->GetHigherResolutionThan(pixels));
  if (!bitrate_limits.has_value() || bitrate_bps == 0) {
    return true;  // No limit configured or bitrate provided.
  }
  RTC_DCHECK_GE(bitrate_limits->frame_size_pixels, pixels);
  return bitrate_bps >=
         static_cast<uint32_t>(bitrate_limits->min_start_bitrate_bps);
}

}  // namespace webrtc
