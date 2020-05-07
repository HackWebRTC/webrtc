/*
 *  Copyright 2020 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef CALL_ADAPTATION_RESOURCE_ADAPTATION_PROCESSOR_H_
#define CALL_ADAPTATION_RESOURCE_ADAPTATION_PROCESSOR_H_

#include <map>
#include <memory>
#include <vector>

#include "absl/types/optional.h"
#include "api/rtp_parameters.h"
#include "api/video/video_frame.h"
#include "api/video/video_stream_encoder_observer.h"
#include "call/adaptation/resource.h"
#include "call/adaptation/resource_adaptation_processor_interface.h"
#include "call/adaptation/video_source_restrictions.h"
#include "call/adaptation/video_stream_adapter.h"
#include "call/adaptation/video_stream_input_state.h"
#include "call/adaptation/video_stream_input_state_provider.h"

namespace webrtc {

class ResourceAdaptationProcessor : public ResourceAdaptationProcessorInterface,
                                    public ResourceListener {
 public:
  ResourceAdaptationProcessor(
      VideoStreamInputStateProvider* input_state_provider,
      VideoStreamEncoderObserver* encoder_stats_observer);
  ~ResourceAdaptationProcessor() override;

  // ResourceAdaptationProcessorInterface implementation.
  DegradationPreference degradation_preference() const override;
  DegradationPreference effective_degradation_preference() const override;

  void StartResourceAdaptation() override;
  void StopResourceAdaptation() override;
  void AddAdaptationListener(
      ResourceAdaptationProcessorListener* adaptation_listener) override;
  void AddResource(Resource* resource) override;

  void SetDegradationPreference(
      DegradationPreference degradation_preference) override;
  void SetIsScreenshare(bool is_screenshare) override;
  void ResetVideoSourceRestrictions() override;

  // ResourceListener implementation.
  // Triggers OnResourceUnderuse() or OnResourceOveruse().
  void OnResourceUsageStateMeasured(const Resource& resource) override;

  // May trigger 1-2 adaptations. It is meant to reduce resolution but this is
  // not guaranteed. It may adapt frame rate, which does not address the issue.
  // TODO(hbos): Can we get rid of this?
  void TriggerAdaptationDueToFrameDroppedDueToSize(
      const Resource& reason_resource) override;

 private:
  bool HasSufficientInputForAdaptation(
      const VideoStreamInputState& input_state) const;

  // Performs the adaptation by getting the next target, applying it and
  // informing listeners of the new VideoSourceRestriction and adaptation
  // counters.
  void OnResourceUnderuse(const Resource& reason_resource);
  void OnResourceOveruse(const Resource& reason_resource);

  // Needs to be invoked any time |degradation_preference_| or |is_screenshare_|
  // changes to ensure |effective_degradation_preference_| is up-to-date.
  void MaybeUpdateEffectiveDegradationPreference();
  // If the filtered source restrictions are different than
  // |last_reported_source_restrictions_|, inform the listeners.
  void MaybeUpdateVideoSourceRestrictions(const Resource* reason);
  // Updates the number of times the resource has degraded based on the latest
  // degradation applied.
  void UpdateResourceDegradationCounts(const Resource* resource);
  // Returns true if a Resource has been overused in the pass and is responsible
  // for creating a VideoSourceRestriction. The current algorithm counts the
  // number of times the resource caused an adaptation and allows adapting up
  // if that number is non-zero. This is consistent with how adaptation has
  // traditionally been handled.
  // TODO(crbug.com/webrtc/11553) Change this algorithm to look at the resources
  // restrictions rather than just the counters.
  bool IsResourceAllowedToAdaptUp(const Resource* resource) const;

  // Input and output.
  VideoStreamInputStateProvider* const input_state_provider_;
  VideoStreamEncoderObserver* const encoder_stats_observer_;
  std::vector<ResourceAdaptationProcessorListener*> adaptation_listeners_;
  std::vector<Resource*> resources_;
  std::map<const Resource*, int> adaptations_counts_by_resource_;
  // Adaptation strategy settings.
  DegradationPreference degradation_preference_;
  DegradationPreference effective_degradation_preference_;
  bool is_screenshare_;
  // Responsible for generating and applying possible adaptations.
  const std::unique_ptr<VideoStreamAdapter> stream_adapter_;
  VideoSourceRestrictions last_reported_source_restrictions_;
  // Prevents recursion.
  //
  // This is used to prevent triggering resource adaptation in the process of
  // already handling resouce adaptation, since that could cause the same states
  // to be modified in unexpected ways. Example:
  //
  // Resource::OnResourceUsageStateMeasured() ->
  // ResourceAdaptationProcessor::OnResourceOveruse() ->
  // Resource::OnAdaptationApplied() ->
  // Resource::OnResourceUsageStateMeasured() ->
  // ResourceAdaptationProcessor::OnResourceOveruse() // Boom, not allowed.
  bool processing_in_progress_;
};

}  // namespace webrtc

#endif  // CALL_ADAPTATION_RESOURCE_ADAPTATION_PROCESSOR_H_
