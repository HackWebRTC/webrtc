/*
 *  Copyright 2020 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "call/adaptation/video_stream_adapter.h"

#include <algorithm>
#include <limits>
#include <utility>

#include "absl/types/optional.h"
#include "api/video/video_adaptation_reason.h"
#include "api/video_codecs/video_encoder.h"
#include "rtc_base/constructor_magic.h"
#include "rtc_base/logging.h"
#include "rtc_base/numerics/safe_conversions.h"

namespace webrtc {

const int kMinFrameRateFps = 2;

namespace {

// Generate suggested higher and lower frame rates and resolutions, to be
// applied to the VideoSourceRestrictor. These are used in "maintain-resolution"
// and "maintain-framerate". The "balanced" degradation preference also makes
// use of BalancedDegradationPreference when generating suggestions. The
// VideoSourceRestrictor decidedes whether or not a proposed adaptation is
// valid.

// For frame rate, the steps we take are 2/3 (down) and 3/2 (up).
int GetLowerFrameRateThan(int fps) {
  RTC_DCHECK(fps != std::numeric_limits<int>::max());
  return (fps * 2) / 3;
}
// TODO(hbos): Use absl::optional<> instead?
int GetHigherFrameRateThan(int fps) {
  return fps != std::numeric_limits<int>::max()
             ? (fps * 3) / 2
             : std::numeric_limits<int>::max();
}

// For resolution, the steps we take are 3/5 (down) and 5/3 (up).
// Notice the asymmetry of which restriction property is set depending on if
// we are adapting up or down:
// - VideoSourceRestrictor::DecreaseResolution() sets the max_pixels_per_frame()
//   to the desired target and target_pixels_per_frame() to null.
// - VideoSourceRestrictor::IncreaseResolutionTo() sets the
//   target_pixels_per_frame() to the desired target, and max_pixels_per_frame()
//   is set according to VideoSourceRestrictor::GetIncreasedMaxPixelsWanted().
int GetLowerResolutionThan(int pixel_count) {
  RTC_DCHECK(pixel_count != std::numeric_limits<int>::max());
  return (pixel_count * 3) / 5;
}

}  // namespace

VideoSourceRestrictions FilterRestrictionsByDegradationPreference(
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

VideoAdaptationCounters FilterVideoAdaptationCountersByDegradationPreference(
    VideoAdaptationCounters counters,
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

// TODO(hbos): Use absl::optional<> instead?
int GetHigherResolutionThan(int pixel_count) {
  return pixel_count != std::numeric_limits<int>::max()
             ? (pixel_count * 5) / 3
             : std::numeric_limits<int>::max();
}

Adaptation::Step::Step(StepType type, int target)
    : type(type), target(target) {}

Adaptation::Adaptation(int validation_id, Step step)
    : validation_id_(validation_id),
      status_(Status::kValid),
      step_(std::move(step)),
      min_pixel_limit_reached_(false) {}

Adaptation::Adaptation(int validation_id,
                       Step step,
                       bool min_pixel_limit_reached)
    : validation_id_(validation_id),
      status_(Status::kValid),
      step_(std::move(step)),
      min_pixel_limit_reached_(min_pixel_limit_reached) {}

Adaptation::Adaptation(int validation_id, Status invalid_status)
    : validation_id_(validation_id),
      status_(invalid_status),
      step_(absl::nullopt),
      min_pixel_limit_reached_(false) {
  RTC_DCHECK_NE(status_, Status::kValid);
}

Adaptation::Adaptation(int validation_id,
                       Status invalid_status,
                       bool min_pixel_limit_reached)
    : validation_id_(validation_id),
      status_(invalid_status),
      step_(absl::nullopt),
      min_pixel_limit_reached_(min_pixel_limit_reached) {
  RTC_DCHECK_NE(status_, Status::kValid);
}

Adaptation::Status Adaptation::status() const {
  return status_;
}

bool Adaptation::min_pixel_limit_reached() const {
  return min_pixel_limit_reached_;
}

const Adaptation::Step& Adaptation::step() const {
  RTC_DCHECK_EQ(status_, Status::kValid);
  return step_.value();
}

// VideoSourceRestrictor is responsible for keeping track of current
// VideoSourceRestrictions.
class VideoStreamAdapter::VideoSourceRestrictor {
 public:
  VideoSourceRestrictor() {}

  VideoSourceRestrictions source_restrictions() const {
    return source_restrictions_;
  }
  const VideoAdaptationCounters& adaptation_counters() const {
    return adaptations_;
  }
  void ClearRestrictions() {
    source_restrictions_ = VideoSourceRestrictions();
    adaptations_ = VideoAdaptationCounters();
  }

  void set_min_pixels_per_frame(int min_pixels_per_frame) {
    min_pixels_per_frame_ = min_pixels_per_frame;
  }

  int min_pixels_per_frame() const { return min_pixels_per_frame_; }

  bool CanDecreaseResolutionTo(int target_pixels) {
    int max_pixels_per_frame = rtc::dchecked_cast<int>(
        source_restrictions_.max_pixels_per_frame().value_or(
            std::numeric_limits<int>::max()));
    return target_pixels < max_pixels_per_frame &&
           target_pixels >= min_pixels_per_frame_;
  }

  bool CanIncreaseResolutionTo(int target_pixels) {
    int max_pixels_wanted = GetIncreasedMaxPixelsWanted(target_pixels);
    int max_pixels_per_frame = rtc::dchecked_cast<int>(
        source_restrictions_.max_pixels_per_frame().value_or(
            std::numeric_limits<int>::max()));
    return max_pixels_wanted > max_pixels_per_frame;
  }

  bool CanDecreaseFrameRateTo(int max_frame_rate) {
    const int fps_wanted = std::max(kMinFrameRateFps, max_frame_rate);
    return fps_wanted < rtc::dchecked_cast<int>(
                            source_restrictions_.max_frame_rate().value_or(
                                std::numeric_limits<int>::max()));
  }

  bool CanIncreaseFrameRateTo(int max_frame_rate) {
    return max_frame_rate > rtc::dchecked_cast<int>(
                                source_restrictions_.max_frame_rate().value_or(
                                    std::numeric_limits<int>::max()));
  }

  void ApplyAdaptationStep(const Adaptation::Step& step,
                           DegradationPreference degradation_preference) {
    switch (step.type) {
      case Adaptation::StepType::kIncreaseResolution:
        IncreaseResolutionTo(step.target);
        break;
      case Adaptation::StepType::kDecreaseResolution:
        DecreaseResolutionTo(step.target);
        break;
      case Adaptation::StepType::kIncreaseFrameRate:
        IncreaseFrameRateTo(step.target);
        // TODO(https://crbug.com/webrtc/11222): Don't adapt in two steps.
        // GetAdaptationUp() should tell us the correct value, but BALANCED
        // logic in DecrementFramerate() makes it hard to predict whether this
        // will be the last step. Remove the dependency on
        // adaptation_counters().
        if (degradation_preference == DegradationPreference::BALANCED &&
            adaptation_counters().fps_adaptations == 0 &&
            step.target != std::numeric_limits<int>::max()) {
          RTC_LOG(LS_INFO) << "Removing framerate down-scaling setting.";
          IncreaseFrameRateTo(std::numeric_limits<int>::max());
        }
        break;
      case Adaptation::StepType::kDecreaseFrameRate:
        DecreaseFrameRateTo(step.target);
        break;
    }
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

  void DecreaseResolutionTo(int target_pixels) {
    RTC_DCHECK(CanDecreaseResolutionTo(target_pixels));
    RTC_LOG(LS_INFO) << "Scaling down resolution, max pixels: "
                     << target_pixels;
    source_restrictions_.set_max_pixels_per_frame(
        target_pixels != std::numeric_limits<int>::max()
            ? absl::optional<size_t>(target_pixels)
            : absl::nullopt);
    source_restrictions_.set_target_pixels_per_frame(absl::nullopt);
    ++adaptations_.resolution_adaptations;
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

  void DecreaseFrameRateTo(int max_frame_rate) {
    RTC_DCHECK(CanDecreaseFrameRateTo(max_frame_rate));
    max_frame_rate = std::max(kMinFrameRateFps, max_frame_rate);
    RTC_LOG(LS_INFO) << "Scaling down framerate: " << max_frame_rate;
    source_restrictions_.set_max_frame_rate(
        max_frame_rate != std::numeric_limits<int>::max()
            ? absl::optional<double>(max_frame_rate)
            : absl::nullopt);
    ++adaptations_.fps_adaptations;
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

  // Needed by CanDecreaseResolutionTo().
  int min_pixels_per_frame_ = 0;
  // Current State.
  VideoSourceRestrictions source_restrictions_;
  VideoAdaptationCounters adaptations_;
};

// static
VideoStreamAdapter::AdaptationRequest::Mode
VideoStreamAdapter::AdaptationRequest::GetModeFromAdaptationAction(
    Adaptation::StepType step_type) {
  switch (step_type) {
    case Adaptation::StepType::kIncreaseResolution:
      return AdaptationRequest::Mode::kAdaptUp;
    case Adaptation::StepType::kDecreaseResolution:
      return AdaptationRequest::Mode::kAdaptDown;
    case Adaptation::StepType::kIncreaseFrameRate:
      return AdaptationRequest::Mode::kAdaptUp;
    case Adaptation::StepType::kDecreaseFrameRate:
      return AdaptationRequest::Mode::kAdaptDown;
  }
}

VideoStreamAdapter::VideoStreamAdapter()
    : source_restrictor_(std::make_unique<VideoSourceRestrictor>()),
      balanced_settings_(),
      adaptation_validation_id_(0),
      degradation_preference_(DegradationPreference::DISABLED),
      input_state_(),
      last_adaptation_request_(absl::nullopt) {}

VideoStreamAdapter::~VideoStreamAdapter() {}

VideoSourceRestrictions VideoStreamAdapter::source_restrictions() const {
  return source_restrictor_->source_restrictions();
}

const VideoAdaptationCounters& VideoStreamAdapter::adaptation_counters() const {
  return source_restrictor_->adaptation_counters();
}

const BalancedDegradationSettings& VideoStreamAdapter::balanced_settings()
    const {
  return balanced_settings_;
}

void VideoStreamAdapter::ClearRestrictions() {
  // Invalidate any previously returned Adaptation.
  ++adaptation_validation_id_;
  source_restrictor_->ClearRestrictions();
  last_adaptation_request_.reset();
}

void VideoStreamAdapter::SetDegradationPreference(
    DegradationPreference degradation_preference) {
  if (degradation_preference_ == degradation_preference)
    return;
  // Invalidate any previously returned Adaptation.
  ++adaptation_validation_id_;
  if (degradation_preference == DegradationPreference::BALANCED ||
      degradation_preference_ == DegradationPreference::BALANCED) {
    ClearRestrictions();
  }
  degradation_preference_ = degradation_preference;
}

void VideoStreamAdapter::SetInput(VideoStreamInputState input_state) {
  // Invalidate any previously returned Adaptation.
  ++adaptation_validation_id_;
  input_state_ = input_state;
  source_restrictor_->set_min_pixels_per_frame(
      input_state_.min_pixels_per_frame());
}

Adaptation VideoStreamAdapter::GetAdaptationUp() const {
  RTC_DCHECK_NE(degradation_preference_, DegradationPreference::DISABLED);
  RTC_DCHECK(input_state_.HasInputFrameSizeAndFramesPerSecond());
  // Don't adapt if we're awaiting a previous adaptation to have an effect.
  bool last_adaptation_was_up =
      last_adaptation_request_ &&
      last_adaptation_request_->mode_ == AdaptationRequest::Mode::kAdaptUp;
  if (last_adaptation_was_up &&
      degradation_preference_ == DegradationPreference::MAINTAIN_FRAMERATE &&
      input_state_.frame_size_pixels().value() <=
          last_adaptation_request_->input_pixel_count_) {
    return Adaptation(adaptation_validation_id_,
                      Adaptation::Status::kAwaitingPreviousAdaptation);
  }

  // Maybe propose targets based on degradation preference.
  switch (degradation_preference_) {
    case DegradationPreference::BALANCED: {
      // Attempt to increase target frame rate.
      int target_fps =
          balanced_settings_.MaxFps(input_state_.video_codec_type(),
                                    input_state_.frame_size_pixels().value());
      if (source_restrictor_->CanIncreaseFrameRateTo(target_fps)) {
        return Adaptation(
            adaptation_validation_id_,
            Adaptation::Step(Adaptation::StepType::kIncreaseFrameRate,
                             target_fps));
      }
      // Scale up resolution.
      ABSL_FALLTHROUGH_INTENDED;
    }
    case DegradationPreference::MAINTAIN_FRAMERATE: {
      // Attempt to increase pixel count.
      int target_pixels = input_state_.frame_size_pixels().value();
      if (source_restrictor_->adaptation_counters().resolution_adaptations ==
          1) {
        RTC_LOG(LS_INFO) << "Removing resolution down-scaling setting.";
        target_pixels = std::numeric_limits<int>::max();
      }
      target_pixels = GetHigherResolutionThan(target_pixels);
      if (!source_restrictor_->CanIncreaseResolutionTo(target_pixels)) {
        return Adaptation(adaptation_validation_id_,
                          Adaptation::Status::kLimitReached);
      }
      return Adaptation(
          adaptation_validation_id_,
          Adaptation::Step(Adaptation::StepType::kIncreaseResolution,
                           target_pixels));
    }
    case DegradationPreference::MAINTAIN_RESOLUTION: {
      // Scale up framerate.
      int target_fps = input_state_.frames_per_second();
      if (source_restrictor_->adaptation_counters().fps_adaptations == 1) {
        RTC_LOG(LS_INFO) << "Removing framerate down-scaling setting.";
        target_fps = std::numeric_limits<int>::max();
      }
      target_fps = GetHigherFrameRateThan(target_fps);
      if (!source_restrictor_->CanIncreaseFrameRateTo(target_fps)) {
        return Adaptation(adaptation_validation_id_,
                          Adaptation::Status::kLimitReached);
      }
      return Adaptation(
          adaptation_validation_id_,
          Adaptation::Step(Adaptation::StepType::kIncreaseFrameRate,
                           target_fps));
    }
    case DegradationPreference::DISABLED:
      RTC_NOTREACHED();
      return Adaptation(adaptation_validation_id_,
                        Adaptation::Status::kLimitReached);
  }
}

Adaptation VideoStreamAdapter::GetAdaptationDown() const {
  RTC_DCHECK_NE(degradation_preference_, DegradationPreference::DISABLED);
  RTC_DCHECK(input_state_.HasInputFrameSizeAndFramesPerSecond());
  // Don't adapt adaptation is disabled.
  bool last_adaptation_was_down =
      last_adaptation_request_ &&
      last_adaptation_request_->mode_ == AdaptationRequest::Mode::kAdaptDown;
  // Don't adapt if we're awaiting a previous adaptation to have an effect.
  if (last_adaptation_was_down &&
      degradation_preference_ == DegradationPreference::MAINTAIN_FRAMERATE &&
      input_state_.frame_size_pixels().value() >=
          last_adaptation_request_->input_pixel_count_) {
    return Adaptation(adaptation_validation_id_,
                      Adaptation::Status::kAwaitingPreviousAdaptation);
  }

  // Maybe propose targets based on degradation preference.
  switch (degradation_preference_) {
    case DegradationPreference::BALANCED: {
      // Try scale down framerate, if lower.
      int target_fps =
          balanced_settings_.MinFps(input_state_.video_codec_type(),
                                    input_state_.frame_size_pixels().value());
      if (source_restrictor_->CanDecreaseFrameRateTo(target_fps)) {
        return Adaptation(
            adaptation_validation_id_,
            Adaptation::Step(Adaptation::StepType::kDecreaseFrameRate,
                             target_fps));
      }
      // Scale down resolution.
      ABSL_FALLTHROUGH_INTENDED;
    }
    case DegradationPreference::MAINTAIN_FRAMERATE: {
      // Scale down resolution.
      int target_pixels =
          GetLowerResolutionThan(input_state_.frame_size_pixels().value());
      bool min_pixel_limit_reached =
          target_pixels < source_restrictor_->min_pixels_per_frame();
      if (!source_restrictor_->CanDecreaseResolutionTo(target_pixels)) {
        return Adaptation(adaptation_validation_id_,
                          Adaptation::Status::kLimitReached,
                          min_pixel_limit_reached);
      }
      return Adaptation(
          adaptation_validation_id_,
          Adaptation::Step(Adaptation::StepType::kDecreaseResolution,
                           target_pixels),
          min_pixel_limit_reached);
    }
    case DegradationPreference::MAINTAIN_RESOLUTION: {
      int target_fps = GetLowerFrameRateThan(input_state_.frames_per_second());
      if (!source_restrictor_->CanDecreaseFrameRateTo(target_fps)) {
        return Adaptation(adaptation_validation_id_,
                          Adaptation::Status::kLimitReached);
      }
      return Adaptation(
          adaptation_validation_id_,
          Adaptation::Step(Adaptation::StepType::kDecreaseFrameRate,
                           target_fps));
    }
    case DegradationPreference::DISABLED:
      RTC_NOTREACHED();
      return Adaptation(adaptation_validation_id_,
                        Adaptation::Status::kLimitReached);
  }
}

VideoSourceRestrictions VideoStreamAdapter::PeekNextRestrictions(
    const Adaptation& adaptation) const {
  RTC_DCHECK_EQ(adaptation.validation_id_, adaptation_validation_id_);
  if (adaptation.status() != Adaptation::Status::kValid)
    return source_restrictor_->source_restrictions();
  VideoSourceRestrictor restrictor_copy = *source_restrictor_;
  restrictor_copy.ApplyAdaptationStep(adaptation.step(),
                                      degradation_preference_);
  return restrictor_copy.source_restrictions();
}

void VideoStreamAdapter::ApplyAdaptation(const Adaptation& adaptation) {
  RTC_DCHECK_EQ(adaptation.validation_id_, adaptation_validation_id_);
  if (adaptation.status() != Adaptation::Status::kValid)
    return;
  // Remember the input pixels and fps of this adaptation. Used to avoid
  // adapting again before this adaptation has had an effect.
  last_adaptation_request_.emplace(AdaptationRequest{
      input_state_.frame_size_pixels().value(),
      input_state_.frames_per_second(),
      AdaptationRequest::GetModeFromAdaptationAction(adaptation.step().type)});
  // Adapt!
  source_restrictor_->ApplyAdaptationStep(adaptation.step(),
                                          degradation_preference_);
}

}  // namespace webrtc
