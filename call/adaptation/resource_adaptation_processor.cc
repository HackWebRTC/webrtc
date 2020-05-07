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

#include <utility>

#include "absl/algorithm/container.h"

namespace webrtc {

ResourceAdaptationProcessor::ResourceAdaptationProcessor(
    VideoStreamInputStateProvider* input_state_provider,
    VideoStreamEncoderObserver* encoder_stats_observer)
    : input_state_provider_(input_state_provider),
      encoder_stats_observer_(encoder_stats_observer),
      resources_(),
      degradation_preference_(DegradationPreference::DISABLED),
      effective_degradation_preference_(DegradationPreference::DISABLED),
      is_screenshare_(false),
      stream_adapter_(std::make_unique<VideoStreamAdapter>()),
      last_reported_source_restrictions_(),
      processing_in_progress_(false) {}

ResourceAdaptationProcessor::~ResourceAdaptationProcessor() = default;

DegradationPreference ResourceAdaptationProcessor::degradation_preference()
    const {
  return degradation_preference_;
}

DegradationPreference
ResourceAdaptationProcessor::effective_degradation_preference() const {
  return effective_degradation_preference_;
}

void ResourceAdaptationProcessor::StartResourceAdaptation() {
  for (auto* resource : resources_) {
    resource->SetResourceListener(this);
  }
}

void ResourceAdaptationProcessor::StopResourceAdaptation() {
  for (auto* resource : resources_) {
    resource->SetResourceListener(nullptr);
  }
}

void ResourceAdaptationProcessor::AddAdaptationListener(
    ResourceAdaptationProcessorListener* adaptation_listener) {
  adaptation_listeners_.push_back(adaptation_listener);
}

void ResourceAdaptationProcessor::AddResource(Resource* resource) {
  resources_.push_back(resource);
}

void ResourceAdaptationProcessor::SetDegradationPreference(
    DegradationPreference degradation_preference) {
  degradation_preference_ = degradation_preference;
  MaybeUpdateEffectiveDegradationPreference();
}

void ResourceAdaptationProcessor::SetIsScreenshare(bool is_screenshare) {
  is_screenshare_ = is_screenshare;
  MaybeUpdateEffectiveDegradationPreference();
}

void ResourceAdaptationProcessor::MaybeUpdateEffectiveDegradationPreference() {
  effective_degradation_preference_ =
      (is_screenshare_ &&
       degradation_preference_ == DegradationPreference::BALANCED)
          ? DegradationPreference::MAINTAIN_RESOLUTION
          : degradation_preference_;
  stream_adapter_->SetDegradationPreference(effective_degradation_preference_);
  MaybeUpdateVideoSourceRestrictions(nullptr);
}

void ResourceAdaptationProcessor::ResetVideoSourceRestrictions() {
  stream_adapter_->ClearRestrictions();
  adaptations_counts_by_resource_.clear();
  MaybeUpdateVideoSourceRestrictions(nullptr);
}

void ResourceAdaptationProcessor::MaybeUpdateVideoSourceRestrictions(
    const Resource* reason) {
  VideoSourceRestrictions new_source_restrictions =
      FilterRestrictionsByDegradationPreference(
          stream_adapter_->source_restrictions(),
          effective_degradation_preference_);
  if (last_reported_source_restrictions_ != new_source_restrictions) {
    last_reported_source_restrictions_ = std::move(new_source_restrictions);
    for (auto* adaptation_listener : adaptation_listeners_) {
      adaptation_listener->OnVideoSourceRestrictionsUpdated(
          last_reported_source_restrictions_,
          stream_adapter_->adaptation_counters(), reason);
    }
    if (reason) {
      UpdateResourceDegradationCounts(reason);
    }
  }
}

void ResourceAdaptationProcessor::OnResourceUsageStateMeasured(
    const Resource& resource) {
  RTC_DCHECK(resource.usage_state().has_value());
  switch (resource.usage_state().value()) {
    case ResourceUsageState::kOveruse:
      OnResourceOveruse(resource);
      break;
    case ResourceUsageState::kUnderuse:
      OnResourceUnderuse(resource);
      break;
  }
}

bool ResourceAdaptationProcessor::HasSufficientInputForAdaptation(
    const VideoStreamInputState& input_state) const {
  return input_state.HasInputFrameSizeAndFramesPerSecond() &&
         (effective_degradation_preference_ !=
              DegradationPreference::MAINTAIN_RESOLUTION ||
          input_state.frames_per_second() >= kMinFrameRateFps);
}

void ResourceAdaptationProcessor::OnResourceUnderuse(
    const Resource& reason_resource) {
  RTC_DCHECK(!processing_in_progress_);
  processing_in_progress_ = true;
  // Clear all usage states. In order to re-run adaptation logic, resources need
  // to provide new resource usage measurements.
  // TODO(hbos): Support not unconditionally clearing usage states by having the
  // ResourceAdaptationProcessor check in on its resources at certain intervals.
  for (Resource* resource : resources_) {
    resource->ClearUsageState();
  }
  VideoStreamInputState input_state = input_state_provider_->InputState();
  if (effective_degradation_preference_ == DegradationPreference::DISABLED ||
      !HasSufficientInputForAdaptation(input_state)) {
    processing_in_progress_ = false;
    return;
  }
  if (!IsResourceAllowedToAdaptUp(&reason_resource)) {
    processing_in_progress_ = false;
    return;
  }
  // Update video input states and encoder settings for accurate adaptation.
  stream_adapter_->SetInput(input_state);
  // How can this stream be adapted up?
  Adaptation adaptation = stream_adapter_->GetAdaptationUp();
  if (adaptation.status() != Adaptation::Status::kValid) {
    processing_in_progress_ = false;
    return;
  }
  // Are all resources OK with this adaptation being applied?
  VideoSourceRestrictions restrictions_before =
      stream_adapter_->source_restrictions();
  VideoSourceRestrictions restrictions_after =
      stream_adapter_->PeekNextRestrictions(adaptation);
  if (!absl::c_all_of(resources_, [&input_state, &restrictions_before,
                                   &restrictions_after,
                                   &reason_resource](const Resource* resource) {
        return resource->IsAdaptationUpAllowed(input_state, restrictions_before,
                                               restrictions_after,
                                               reason_resource);
      })) {
    processing_in_progress_ = false;
    return;
  }
  // Apply adaptation.
  stream_adapter_->ApplyAdaptation(adaptation);
  for (Resource* resource : resources_) {
    resource->OnAdaptationApplied(input_state, restrictions_before,
                                  restrictions_after, reason_resource);
  }
  // Update VideoSourceRestrictions based on adaptation. This also informs the
  // |adaptation_listeners_|.
  MaybeUpdateVideoSourceRestrictions(&reason_resource);
  processing_in_progress_ = false;
}

void ResourceAdaptationProcessor::OnResourceOveruse(
    const Resource& reason_resource) {
  RTC_DCHECK(!processing_in_progress_);
  processing_in_progress_ = true;
  // Clear all usage states. In order to re-run adaptation logic, resources need
  // to provide new resource usage measurements.
  // TODO(hbos): Support not unconditionally clearing usage states by having the
  // ResourceAdaptationProcessor check in on its resources at certain intervals.
  for (Resource* resource : resources_) {
    resource->ClearUsageState();
  }
  VideoStreamInputState input_state = input_state_provider_->InputState();
  if (!input_state.has_input()) {
    processing_in_progress_ = false;
    return;
  }
  if (effective_degradation_preference_ == DegradationPreference::DISABLED ||
      !HasSufficientInputForAdaptation(input_state)) {
    processing_in_progress_ = false;
    return;
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
    return;
  }
  // Apply adaptation.
  VideoSourceRestrictions restrictions_before =
      stream_adapter_->source_restrictions();
  VideoSourceRestrictions restrictions_after =
      stream_adapter_->PeekNextRestrictions(adaptation);
  stream_adapter_->ApplyAdaptation(adaptation);
  for (Resource* resource : resources_) {
    resource->OnAdaptationApplied(input_state, restrictions_before,
                                  restrictions_after, reason_resource);
  }
  // Update VideoSourceRestrictions based on adaptation. This also informs the
  // |adaptation_listeners_|.
  MaybeUpdateVideoSourceRestrictions(&reason_resource);
  processing_in_progress_ = false;
}

void ResourceAdaptationProcessor::TriggerAdaptationDueToFrameDroppedDueToSize(
    const Resource& reason_resource) {
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
    const Resource* resource) {
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
    const Resource* resource) const {
  RTC_DCHECK(resource);
  const auto& adaptations = adaptations_counts_by_resource_.find(resource);
  return adaptations != adaptations_counts_by_resource_.end() &&
         adaptations->second > 0;
}

}  // namespace webrtc
