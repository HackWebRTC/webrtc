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
  ResourceListenerResponse OnResourceUsageStateMeasured(
      const Resource& resource) override;

  // May trigger 1-2 adaptations. It is meant to reduce resolution - useful if a
  // frame was dropped due to its size - but if you look at the implementation
  // you'll find that this is not guaranteed. It could adapt frame rate, which
  // does not address the issue.
  // TODO(hbos): Can we replace this by something which actually satisfies the
  // resolution constraints, or get rid of it altogether?
  void TriggerAdaptationDueToFrameDroppedDueToSize(
      const Resource& reason_resource);

 private:
  bool HasSufficientInputForAdaptation(
      const VideoStreamInputState& input_state) const;

  // Performs the adaptation by getting the next target, applying it and
  // informing listeners of the new VideoSourceRestriction and adaptation
  // counters.
  void OnResourceUnderuse(const Resource& reason_resource);
  ResourceListenerResponse OnResourceOveruse(const Resource& reason_resource);

  // Needs to be invoked any time |degradation_preference_| or |is_screenshare_|
  // changes to ensure |effective_degradation_preference_| is up-to-date.
  void MaybeUpdateEffectiveDegradationPreference();
  // If the filtered source restrictions are different than
  // |last_reported_source_restrictions_|, inform the listeners.
  void MaybeUpdateVideoSourceRestrictions(const Resource* reason);

  // Input and output.
  VideoStreamInputStateProvider* const input_state_provider_;
  VideoStreamEncoderObserver* const encoder_stats_observer_;
  std::vector<ResourceAdaptationProcessorListener*> adaptation_listeners_;
  std::vector<Resource*> resources_;
  // Adaptation strategy settings.
  DegradationPreference degradation_preference_;
  DegradationPreference effective_degradation_preference_;
  bool is_screenshare_;
  // Responsible for generating and applying possible adaptations.
  const std::unique_ptr<VideoStreamAdapter> stream_adapter_;
  VideoSourceRestrictions last_reported_source_restrictions_;
};

}  // namespace webrtc

#endif  // CALL_ADAPTATION_RESOURCE_ADAPTATION_PROCESSOR_H_
