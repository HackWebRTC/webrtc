/*
 *  Copyright 2020 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "call/adaptation/resource_adaptation_processor.h"

#include <algorithm>
#include <string>
#include <utility>

#include "absl/algorithm/container.h"
#include "rtc_base/logging.h"
#include "rtc_base/strings/string_builder.h"

namespace webrtc {

ResourceAdaptationProcessor::MitigationResultAndLogMessage::
    MitigationResultAndLogMessage()
    : result(MitigationResult::kAdaptationApplied), message() {}

ResourceAdaptationProcessor::MitigationResultAndLogMessage::
    MitigationResultAndLogMessage(MitigationResult result, std::string message)
    : result(result), message(std::move(message)) {}

ResourceAdaptationProcessor::ResourceAdaptationProcessor(
    VideoStreamInputStateProvider* input_state_provider,
    VideoStreamEncoderObserver* encoder_stats_observer)
    : sequence_checker_(),
      is_resource_adaptation_enabled_(false),
      input_state_provider_(input_state_provider),
      encoder_stats_observer_(encoder_stats_observer),
      resources_(),
      degradation_preference_(DegradationPreference::DISABLED),
      effective_degradation_preference_(DegradationPreference::DISABLED),
      is_screenshare_(false),
      stream_adapter_(std::make_unique<VideoStreamAdapter>()),
      last_reported_source_restrictions_(),
      previous_mitigation_results_(),
      processing_in_progress_(false) {
  sequence_checker_.Detach();
}

ResourceAdaptationProcessor::~ResourceAdaptationProcessor() {
  RTC_DCHECK_RUN_ON(&sequence_checker_);
  RTC_DCHECK(!is_resource_adaptation_enabled_);
  RTC_DCHECK(restrictions_listeners_.empty())
      << "There are restrictions listener(s) depending on a "
      << "ResourceAdaptationProcessor being destroyed.";
  RTC_DCHECK(resources_.empty())
      << "There are resource(s) attached to a ResourceAdaptationProcessor "
      << "being destroyed.";
  RTC_DCHECK(adaptation_constraints_.empty())
      << "There are constaint(s) attached to a ResourceAdaptationProcessor "
      << "being destroyed.";
  RTC_DCHECK(adaptation_listeners_.empty())
      << "There are listener(s) attached to a ResourceAdaptationProcessor "
      << "being destroyed.";
}

void ResourceAdaptationProcessor::InitializeOnResourceAdaptationQueue() {
  // Allows |sequence_checker_| to attach to the resource adaptation queue.
  // The caller is responsible for ensuring that this is the current queue.
  RTC_DCHECK_RUN_ON(&sequence_checker_);
}

DegradationPreference ResourceAdaptationProcessor::degradation_preference()
    const {
  RTC_DCHECK_RUN_ON(&sequence_checker_);
  return degradation_preference_;
}

DegradationPreference
ResourceAdaptationProcessor::effective_degradation_preference() const {
  RTC_DCHECK_RUN_ON(&sequence_checker_);
  return effective_degradation_preference_;
}

void ResourceAdaptationProcessor::StartResourceAdaptation() {
  RTC_DCHECK_RUN_ON(&sequence_checker_);
  if (is_resource_adaptation_enabled_)
    return;
  for (const auto& resource : resources_) {
    resource->SetResourceListener(this);
  }
  is_resource_adaptation_enabled_ = true;
}

void ResourceAdaptationProcessor::StopResourceAdaptation() {
  RTC_DCHECK_RUN_ON(&sequence_checker_);
  if (!is_resource_adaptation_enabled_)
    return;
  for (const auto& resource : resources_) {
    resource->SetResourceListener(nullptr);
  }
  is_resource_adaptation_enabled_ = false;
}

void ResourceAdaptationProcessor::AddRestrictionsListener(
    VideoSourceRestrictionsListener* restrictions_listener) {
  RTC_DCHECK_RUN_ON(&sequence_checker_);
  RTC_DCHECK(std::find(restrictions_listeners_.begin(),
                       restrictions_listeners_.end(),
                       restrictions_listener) == restrictions_listeners_.end());
  restrictions_listeners_.push_back(restrictions_listener);
}

void ResourceAdaptationProcessor::RemoveRestrictionsListener(
    VideoSourceRestrictionsListener* restrictions_listener) {
  RTC_DCHECK_RUN_ON(&sequence_checker_);
  auto it = std::find(restrictions_listeners_.begin(),
                      restrictions_listeners_.end(), restrictions_listener);
  RTC_DCHECK(it != restrictions_listeners_.end());
  restrictions_listeners_.erase(it);
}

void ResourceAdaptationProcessor::AddResource(
    rtc::scoped_refptr<Resource> resource) {
  RTC_DCHECK_RUN_ON(&sequence_checker_);
  // TODO(hbos): Allow adding resources while |is_resource_adaptation_enabled_|
  // by registering as a listener of the resource on adding it.
  RTC_DCHECK(!is_resource_adaptation_enabled_);
  RTC_DCHECK(std::find(resources_.begin(), resources_.end(), resource) ==
             resources_.end());
  resources_.push_back(resource);
}

void ResourceAdaptationProcessor::RemoveResource(
    rtc::scoped_refptr<Resource> resource) {
  RTC_DCHECK_RUN_ON(&sequence_checker_);
  // TODO(hbos): Allow removing resources while
  // |is_resource_adaptation_enabled_| by unregistering as a listener of the
  // resource on removing it.
  RTC_DCHECK(!is_resource_adaptation_enabled_);
  auto it = std::find(resources_.begin(), resources_.end(), resource);
  RTC_DCHECK(it != resources_.end());
  resources_.erase(it);
}

void ResourceAdaptationProcessor::AddAdaptationConstraint(
    AdaptationConstraint* adaptation_constraint) {
  RTC_DCHECK_RUN_ON(&sequence_checker_);
  RTC_DCHECK(std::find(adaptation_constraints_.begin(),
                       adaptation_constraints_.end(),
                       adaptation_constraint) == adaptation_constraints_.end());
  adaptation_constraints_.push_back(adaptation_constraint);
}

void ResourceAdaptationProcessor::RemoveAdaptationConstraint(
    AdaptationConstraint* adaptation_constraint) {
  RTC_DCHECK_RUN_ON(&sequence_checker_);
  auto it = std::find(adaptation_constraints_.begin(),
                      adaptation_constraints_.end(), adaptation_constraint);
  RTC_DCHECK(it != adaptation_constraints_.end());
  adaptation_constraints_.erase(it);
}

void ResourceAdaptationProcessor::AddAdaptationListener(
    AdaptationListener* adaptation_listener) {
  RTC_DCHECK_RUN_ON(&sequence_checker_);
  RTC_DCHECK(std::find(adaptation_listeners_.begin(),
                       adaptation_listeners_.end(),
                       adaptation_listener) == adaptation_listeners_.end());
  adaptation_listeners_.push_back(adaptation_listener);
}

void ResourceAdaptationProcessor::RemoveAdaptationListener(
    AdaptationListener* adaptation_listener) {
  RTC_DCHECK_RUN_ON(&sequence_checker_);
  auto it = std::find(adaptation_listeners_.begin(),
                      adaptation_listeners_.end(), adaptation_listener);
  RTC_DCHECK(it != adaptation_listeners_.end());
  adaptation_listeners_.erase(it);
}

void ResourceAdaptationProcessor::SetDegradationPreference(
    DegradationPreference degradation_preference) {
  RTC_DCHECK_RUN_ON(&sequence_checker_);
  degradation_preference_ = degradation_preference;
  MaybeUpdateEffectiveDegradationPreference();
}

void ResourceAdaptationProcessor::SetIsScreenshare(bool is_screenshare) {
  RTC_DCHECK_RUN_ON(&sequence_checker_);
  is_screenshare_ = is_screenshare;
  MaybeUpdateEffectiveDegradationPreference();
}

void ResourceAdaptationProcessor::MaybeUpdateEffectiveDegradationPreference() {
  RTC_DCHECK_RUN_ON(&sequence_checker_);
  effective_degradation_preference_ =
      (is_screenshare_ &&
       degradation_preference_ == DegradationPreference::BALANCED)
          ? DegradationPreference::MAINTAIN_RESOLUTION
          : degradation_preference_;
  stream_adapter_->SetDegradationPreference(effective_degradation_preference_);
  MaybeUpdateVideoSourceRestrictions(nullptr);
}

void ResourceAdaptationProcessor::ResetVideoSourceRestrictions() {
  RTC_DCHECK_RUN_ON(&sequence_checker_);
  RTC_LOG(INFO) << "Resetting restrictions";
  stream_adapter_->ClearRestrictions();
  adaptations_counts_by_resource_.clear();
  MaybeUpdateVideoSourceRestrictions(nullptr);
}

void ResourceAdaptationProcessor::MaybeUpdateVideoSourceRestrictions(
    rtc::scoped_refptr<Resource> reason) {
  RTC_DCHECK_RUN_ON(&sequence_checker_);
  VideoSourceRestrictions new_source_restrictions =
      FilterRestrictionsByDegradationPreference(
          stream_adapter_->source_restrictions(),
          effective_degradation_preference_);
  if (last_reported_source_restrictions_ != new_source_restrictions) {
    RTC_LOG(INFO) << "Reporting new restrictions (in "
                  << DegradationPreferenceToString(
                         effective_degradation_preference_)
                  << "): " << new_source_restrictions.ToString();
    last_reported_source_restrictions_ = std::move(new_source_restrictions);
    for (auto* restrictions_listener : restrictions_listeners_) {
      restrictions_listener->OnVideoSourceRestrictionsUpdated(
          last_reported_source_restrictions_,
          stream_adapter_->adaptation_counters(), reason);
    }
    if (reason) {
      UpdateResourceDegradationCounts(reason);
    }
  }
}

void ResourceAdaptationProcessor::OnResourceUsageStateMeasured(
    rtc::scoped_refptr<Resource> resource) {
  RTC_DCHECK_RUN_ON(&sequence_checker_);
  RTC_DCHECK(resource->UsageState().has_value());
  ResourceUsageState usage_state = resource->UsageState().value();
  MitigationResultAndLogMessage result_and_message;
  switch (usage_state) {
    case ResourceUsageState::kOveruse:
      result_and_message = OnResourceOveruse(resource);
      break;
    case ResourceUsageState::kUnderuse:
      result_and_message = OnResourceUnderuse(resource);
      break;
  }
  // Maybe log the result of the operation.
  auto it = previous_mitigation_results_.find(resource.get());
  if (it != previous_mitigation_results_.end() &&
      it->second == result_and_message.result) {
    // This resource has previously reported the same result and we haven't
    // successfully adapted since - don't log to avoid spam.
    return;
  }
  RTC_LOG(INFO) << "Resource \"" << resource->Name() << "\" signalled "
                << ResourceUsageStateToString(usage_state) << ". "
                << result_and_message.message;
  if (result_and_message.result == MitigationResult::kAdaptationApplied) {
    previous_mitigation_results_.clear();
  } else {
    previous_mitigation_results_.insert(
        std::make_pair(resource.get(), result_and_message.result));
  }
}

bool ResourceAdaptationProcessor::HasSufficientInputForAdaptation(
    const VideoStreamInputState& input_state) const {
  RTC_DCHECK_RUN_ON(&sequence_checker_);
  return input_state.HasInputFrameSizeAndFramesPerSecond() &&
         (effective_degradation_preference_ !=
              DegradationPreference::MAINTAIN_RESOLUTION ||
          input_state.frames_per_second() >= kMinFrameRateFps);
}

ResourceAdaptationProcessor::MitigationResultAndLogMessage
ResourceAdaptationProcessor::OnResourceUnderuse(
    rtc::scoped_refptr<Resource> reason_resource) {
  RTC_DCHECK_RUN_ON(&sequence_checker_);
  RTC_DCHECK(!processing_in_progress_);
  processing_in_progress_ = true;
  // Clear all usage states. In order to re-run adaptation logic, resources need
  // to provide new resource usage measurements.
  // TODO(hbos): Support not unconditionally clearing usage states by having the
  // ResourceAdaptationProcessor check in on its resources at certain intervals.
  for (const auto& resource : resources_) {
    resource->ClearUsageState();
  }
  if (effective_degradation_preference_ == DegradationPreference::DISABLED) {
    processing_in_progress_ = false;
    return MitigationResultAndLogMessage(
        MitigationResult::kDisabled,
        "Not adapting up because DegradationPreference is disabled");
  }
  VideoStreamInputState input_state = input_state_provider_->InputState();
  if (!HasSufficientInputForAdaptation(input_state)) {
    processing_in_progress_ = false;
    return MitigationResultAndLogMessage(
        MitigationResult::kInsufficientInput,
        "Not adapting up because input is insufficient");
  }
  if (!IsResourceAllowedToAdaptUp(reason_resource)) {
    processing_in_progress_ = false;
    return MitigationResultAndLogMessage(
        MitigationResult::kRejectedByAdaptationCounts,
        "Not adapting up because this resource has not previously adapted down "
        "(according to adaptation counters)");
  }
  // Update video input states and encoder settings for accurate adaptation.
  stream_adapter_->SetInput(input_state);
  // How can this stream be adapted up?
  Adaptation adaptation = stream_adapter_->GetAdaptationUp();
  if (adaptation.status() != Adaptation::Status::kValid) {
    processing_in_progress_ = false;
    rtc::StringBuilder message;
    message << "Not adapting up because VideoStreamAdapter returned "
            << Adaptation::StatusToString(adaptation.status());
    return MitigationResultAndLogMessage(MitigationResult::kRejectedByAdapter,
                                         message.Release());
  }
  // Are all resources OK with this adaptation being applied?
  VideoSourceRestrictions restrictions_before =
      stream_adapter_->source_restrictions();
  VideoSourceRestrictions restrictions_after =
      stream_adapter_->PeekNextRestrictions(adaptation);
  for (const auto* constraint : adaptation_constraints_) {
    if (!constraint->IsAdaptationUpAllowed(input_state, restrictions_before,
                                           restrictions_after,
                                           reason_resource)) {
      processing_in_progress_ = false;
      rtc::StringBuilder message;
      message << "Not adapting up because constraint \"" << constraint->Name()
              << "\" disallowed it";
      return MitigationResultAndLogMessage(
          MitigationResult::kRejectedByConstraint, message.Release());
    }
  }
  // Apply adaptation.
  stream_adapter_->ApplyAdaptation(adaptation);
  for (auto* adaptation_listener : adaptation_listeners_) {
    adaptation_listener->OnAdaptationApplied(
        input_state, restrictions_before, restrictions_after, reason_resource);
  }
  // Update VideoSourceRestrictions based on adaptation. This also informs the
  // |restrictions_listeners_|.
  MaybeUpdateVideoSourceRestrictions(reason_resource);
  processing_in_progress_ = false;
  rtc::StringBuilder message;
  message << "Adapted up successfully. Unfiltered adaptations: "
          << stream_adapter_->adaptation_counters().ToString();
  return MitigationResultAndLogMessage(MitigationResult::kAdaptationApplied,
                                       message.Release());
}

ResourceAdaptationProcessor::MitigationResultAndLogMessage
ResourceAdaptationProcessor::OnResourceOveruse(
    rtc::scoped_refptr<Resource> reason_resource) {
  RTC_DCHECK_RUN_ON(&sequence_checker_);
  RTC_DCHECK(!processing_in_progress_);
  processing_in_progress_ = true;
  // Clear all usage states. In order to re-run adaptation logic, resources need
  // to provide new resource usage measurements.
  // TODO(hbos): Support not unconditionally clearing usage states by having the
  // ResourceAdaptationProcessor check in on its resources at certain intervals.
  for (const auto& resource : resources_) {
    resource->ClearUsageState();
  }
  if (effective_degradation_preference_ == DegradationPreference::DISABLED) {
    processing_in_progress_ = false;
    return MitigationResultAndLogMessage(
        MitigationResult::kDisabled,
        "Not adapting down because DegradationPreference is disabled");
  }
  VideoStreamInputState input_state = input_state_provider_->InputState();
  if (!HasSufficientInputForAdaptation(input_state)) {
    processing_in_progress_ = false;
    return MitigationResultAndLogMessage(
        MitigationResult::kInsufficientInput,
        "Not adapting down because input is insufficient");
  }
  // Update video input states and encoder settings for accurate adaptation.
  stream_adapter_->SetInput(input_state);
  // How can this stream be adapted up?
  Adaptation adaptation = stream_adapter_->GetAdaptationDown();
  if (adaptation.min_pixel_limit_reached()) {
    encoder_stats_observer_->OnMinPixelLimitReached();
  }
  if (adaptation.status() != Adaptation::Status::kValid) {
    processing_in_progress_ = false;
    rtc::StringBuilder message;
    message << "Not adapting down because VideoStreamAdapter returned "
            << Adaptation::StatusToString(adaptation.status());
    return MitigationResultAndLogMessage(MitigationResult::kRejectedByAdapter,
                                         message.Release());
  }
  // Apply adaptation.
  VideoSourceRestrictions restrictions_before =
      stream_adapter_->source_restrictions();
  VideoSourceRestrictions restrictions_after =
      stream_adapter_->PeekNextRestrictions(adaptation);
  stream_adapter_->ApplyAdaptation(adaptation);
  for (auto* adaptation_listener : adaptation_listeners_) {
    adaptation_listener->OnAdaptationApplied(
        input_state, restrictions_before, restrictions_after, reason_resource);
  }
  // Update VideoSourceRestrictions based on adaptation. This also informs the
  // |restrictions_listeners_|.
  MaybeUpdateVideoSourceRestrictions(reason_resource);
  processing_in_progress_ = false;
  rtc::StringBuilder message;
  message << "Adapted down successfully. Unfiltered adaptations: "
          << stream_adapter_->adaptation_counters().ToString();
  return MitigationResultAndLogMessage(MitigationResult::kAdaptationApplied,
                                       message.Release());
}

void ResourceAdaptationProcessor::TriggerAdaptationDueToFrameDroppedDueToSize(
    rtc::scoped_refptr<Resource> reason_resource) {
  RTC_DCHECK_RUN_ON(&sequence_checker_);
  RTC_LOG(INFO) << "TriggerAdaptationDueToFrameDroppedDueToSize called";
  VideoAdaptationCounters counters_before =
      stream_adapter_->adaptation_counters();
  OnResourceOveruse(reason_resource);
  if (degradation_preference_ == DegradationPreference::BALANCED &&
      stream_adapter_->adaptation_counters().fps_adaptations >
          counters_before.fps_adaptations) {
    // Oops, we adapted frame rate. Adapt again, maybe it will adapt resolution!
    // Though this is not guaranteed...
    OnResourceOveruse(reason_resource);
  }
  if (stream_adapter_->adaptation_counters().resolution_adaptations >
      counters_before.resolution_adaptations) {
    encoder_stats_observer_->OnInitialQualityResolutionAdaptDown();
  }
}

void ResourceAdaptationProcessor::UpdateResourceDegradationCounts(
    rtc::scoped_refptr<Resource> resource) {
  RTC_DCHECK_RUN_ON(&sequence_checker_);
  RTC_DCHECK(resource);
  int delta = stream_adapter_->adaptation_counters().Total();
  for (const auto& adaptations : adaptations_counts_by_resource_) {
    delta -= adaptations.second;
  }

  // Default value is 0, inserts the value if missing.
  adaptations_counts_by_resource_[resource] += delta;
  RTC_DCHECK_GE(adaptations_counts_by_resource_[resource], 0);
}

bool ResourceAdaptationProcessor::IsResourceAllowedToAdaptUp(
    rtc::scoped_refptr<Resource> resource) const {
  RTC_DCHECK_RUN_ON(&sequence_checker_);
  RTC_DCHECK(resource);
  const auto& adaptations = adaptations_counts_by_resource_.find(resource);
  return adaptations != adaptations_counts_by_resource_.end() &&
         adaptations->second > 0;
}

}  // namespace webrtc
