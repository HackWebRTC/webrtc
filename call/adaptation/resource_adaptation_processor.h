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
#include <string>
#include <utility>
#include <vector>

#include "absl/types/optional.h"
#include "api/adaptation/resource.h"
#include "api/rtp_parameters.h"
#include "api/scoped_refptr.h"
#include "api/task_queue/task_queue_base.h"
#include "api/video/video_frame.h"
#include "api/video/video_stream_encoder_observer.h"
#include "call/adaptation/adaptation_constraint.h"
#include "call/adaptation/adaptation_listener.h"
#include "call/adaptation/resource_adaptation_processor_interface.h"
#include "call/adaptation/video_source_restrictions.h"
#include "call/adaptation/video_stream_adapter.h"
#include "call/adaptation/video_stream_input_state.h"
#include "call/adaptation/video_stream_input_state_provider.h"

namespace webrtc {

// The Resource Adaptation Processor is responsible for reacting to resource
// usage measurements (e.g. overusing or underusing CPU). When a resource is
// overused the Processor is responsible for performing mitigations in order to
// consume less resources.
//
// Today we have one Processor per VideoStreamEncoder and the Processor is only
// capable of restricting resolution or frame rate of the encoded stream. In the
// future we should have a single Processor responsible for all encoded streams,
// and it should be capable of reconfiguring other things than just
// VideoSourceRestrictions (e.g. reduce render frame rate).
// See Resource-Adaptation hotlist:
// https://bugs.chromium.org/u/590058293/hotlists/Resource-Adaptation
//
// The ResourceAdaptationProcessor is single-threaded. It may be constructed on
// any thread but MUST subsequently be used and destroyed on a single sequence,
// i.e. the "resource adaptation task queue".
class ResourceAdaptationProcessor : public ResourceAdaptationProcessorInterface,
                                    public ResourceListener {
 public:
  ResourceAdaptationProcessor(
      VideoStreamInputStateProvider* input_state_provider,
      VideoStreamEncoderObserver* encoder_stats_observer);
  ~ResourceAdaptationProcessor() override;

  void SetResourceAdaptationQueue(
      TaskQueueBase* resource_adaptation_queue) override;

  // ResourceAdaptationProcessorInterface implementation.
  DegradationPreference degradation_preference() const override;
  DegradationPreference effective_degradation_preference() const override;

  void AddRestrictionsListener(
      VideoSourceRestrictionsListener* restrictions_listener) override;
  void RemoveRestrictionsListener(
      VideoSourceRestrictionsListener* restrictions_listener) override;
  void AddResource(rtc::scoped_refptr<Resource> resource) override;
  std::vector<rtc::scoped_refptr<Resource>> GetResources() const override;
  void RemoveResource(rtc::scoped_refptr<Resource> resource) override;
  void AddAdaptationConstraint(
      AdaptationConstraint* adaptation_constraint) override;
  void RemoveAdaptationConstraint(
      AdaptationConstraint* adaptation_constraint) override;
  void AddAdaptationListener(AdaptationListener* adaptation_listener) override;
  void RemoveAdaptationListener(
      AdaptationListener* adaptation_listener) override;

  void SetDegradationPreference(
      DegradationPreference degradation_preference) override;
  void SetIsScreenshare(bool is_screenshare) override;
  void ResetVideoSourceRestrictions() override;

  // ResourceListener implementation.
  // Triggers OnResourceUnderuse() or OnResourceOveruse().
  void OnResourceUsageStateMeasured(rtc::scoped_refptr<Resource> resource,
                                    ResourceUsageState usage_state) override;

  // May trigger 1-2 adaptations. It is meant to reduce resolution but this is
  // not guaranteed. It may adapt frame rate, which does not address the issue.
  // TODO(hbos): Can we get rid of this?
  void TriggerAdaptationDueToFrameDroppedDueToSize(
      rtc::scoped_refptr<Resource> reason_resource) override;

 private:
  bool HasSufficientInputForAdaptation(
      const VideoStreamInputState& input_state) const;

  // If resource usage measurements happens off the adaptation task queue, this
  // class takes care of posting the measurement for the processor to handle it
  // on the adaptation task queue.
  class ResourceListenerDelegate : public rtc::RefCountInterface,
                                   public ResourceListener {
   public:
    explicit ResourceListenerDelegate(ResourceAdaptationProcessor* processor);

    void SetResourceAdaptationQueue(TaskQueueBase* resource_adaptation_queue);
    void OnProcessorDestroyed();

    // ResourceListener implementation.
    void OnResourceUsageStateMeasured(rtc::scoped_refptr<Resource> resource,
                                      ResourceUsageState usage_state) override;

   private:
    TaskQueueBase* resource_adaptation_queue_;
    ResourceAdaptationProcessor* processor_
        RTC_GUARDED_BY(resource_adaptation_queue_);
  };

  enum class MitigationResult {
    kDisabled,
    kInsufficientInput,
    kNotMostLimitedResource,
    kSharedMostLimitedResource,
    kRejectedByAdapter,
    kRejectedByConstraint,
    kAdaptationApplied,
  };

  struct MitigationResultAndLogMessage {
    MitigationResultAndLogMessage();
    MitigationResultAndLogMessage(MitigationResult result, std::string message);
    MitigationResult result;
    std::string message;
  };

  // Performs the adaptation by getting the next target, applying it and
  // informing listeners of the new VideoSourceRestriction and adaptation
  // counters.
  MitigationResultAndLogMessage OnResourceUnderuse(
      rtc::scoped_refptr<Resource> reason_resource);
  MitigationResultAndLogMessage OnResourceOveruse(
      rtc::scoped_refptr<Resource> reason_resource);

  // Needs to be invoked any time |degradation_preference_| or |is_screenshare_|
  // changes to ensure |effective_degradation_preference_| is up-to-date.
  void MaybeUpdateEffectiveDegradationPreference();
  // If the filtered source restrictions are different than
  // |last_reported_source_restrictions_|, inform the listeners.
  void MaybeUpdateVideoSourceRestrictions(rtc::scoped_refptr<Resource> reason);

  void UpdateResourceLimitations(
      rtc::scoped_refptr<Resource> reason_resource,
      const VideoStreamAdapter::RestrictionsWithCounters&
          peek_next_restrictions) RTC_RUN_ON(resource_adaptation_queue_);

  // Searches |adaptation_limits_by_resources_| for each resource with the
  // highest total adaptation counts. Adaptation up may only occur if the
  // resource performing the adaptation is the only most limited resource. This
  // function returns the list of all most limited resources as well as the
  // corresponding adaptation of that resource.
  std::pair<std::vector<rtc::scoped_refptr<Resource>>,
            VideoStreamAdapter::RestrictionsWithCounters>
  FindMostLimitedResources() const RTC_RUN_ON(resource_adaptation_queue_);

  void MaybeUpdateResourceLimitationsOnResourceRemoval(
      VideoStreamAdapter::RestrictionsWithCounters removed_limitations)
      RTC_RUN_ON(resource_adaptation_queue_);

  TaskQueueBase* resource_adaptation_queue_;
  rtc::scoped_refptr<ResourceListenerDelegate> resource_listener_delegate_;
  // Input and output.
  VideoStreamInputStateProvider* const input_state_provider_
      RTC_GUARDED_BY(resource_adaptation_queue_);
  VideoStreamEncoderObserver* const encoder_stats_observer_
      RTC_GUARDED_BY(resource_adaptation_queue_);
  std::vector<VideoSourceRestrictionsListener*> restrictions_listeners_
      RTC_GUARDED_BY(resource_adaptation_queue_);
  std::vector<rtc::scoped_refptr<Resource>> resources_
      RTC_GUARDED_BY(resource_adaptation_queue_);
  std::vector<AdaptationConstraint*> adaptation_constraints_
      RTC_GUARDED_BY(resource_adaptation_queue_);
  std::vector<AdaptationListener*> adaptation_listeners_
      RTC_GUARDED_BY(resource_adaptation_queue_);
  // Purely used for statistics, does not ensure mapped resources stay alive.
  std::map<rtc::scoped_refptr<Resource>,
           VideoStreamAdapter::RestrictionsWithCounters>
      adaptation_limits_by_resources_
          RTC_GUARDED_BY(resource_adaptation_queue_);
  // Adaptation strategy settings.
  DegradationPreference degradation_preference_
      RTC_GUARDED_BY(resource_adaptation_queue_);
  DegradationPreference effective_degradation_preference_
      RTC_GUARDED_BY(resource_adaptation_queue_);
  bool is_screenshare_ RTC_GUARDED_BY(resource_adaptation_queue_);
  // Responsible for generating and applying possible adaptations.
  const std::unique_ptr<VideoStreamAdapter> stream_adapter_
      RTC_GUARDED_BY(resource_adaptation_queue_);
  VideoSourceRestrictions last_reported_source_restrictions_
      RTC_GUARDED_BY(resource_adaptation_queue_);
  // Keeps track of previous mitigation results per resource since the last
  // successful adaptation. Used to avoid RTC_LOG spam.
  std::map<Resource*, MitigationResult> previous_mitigation_results_
      RTC_GUARDED_BY(resource_adaptation_queue_);
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
  bool processing_in_progress_ RTC_GUARDED_BY(resource_adaptation_queue_);
};

}  // namespace webrtc

#endif  // CALL_ADAPTATION_RESOURCE_ADAPTATION_PROCESSOR_H_
