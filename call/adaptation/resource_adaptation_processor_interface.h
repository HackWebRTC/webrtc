/*
 *  Copyright 2020 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef CALL_ADAPTATION_RESOURCE_ADAPTATION_PROCESSOR_INTERFACE_H_
#define CALL_ADAPTATION_RESOURCE_ADAPTATION_PROCESSOR_INTERFACE_H_

#include "absl/types/optional.h"
#include "api/rtp_parameters.h"
#include "api/scoped_refptr.h"
#include "api/video/video_adaptation_counters.h"
#include "api/video/video_frame.h"
#include "call/adaptation/encoder_settings.h"
#include "call/adaptation/resource.h"
#include "call/adaptation/video_source_restrictions.h"
#include "rtc_base/task_queue.h"

namespace webrtc {

// The listener is responsible for carrying out the reconfiguration of the video
// source such that the VideoSourceRestrictions are fulfilled.
class ResourceAdaptationProcessorListener {
 public:
  virtual ~ResourceAdaptationProcessorListener();

  // The |restrictions| are filtered by degradation preference but not the
  // |adaptation_counters|, which are currently only reported for legacy stats
  // calculation purposes.
  virtual void OnVideoSourceRestrictionsUpdated(
      VideoSourceRestrictions restrictions,
      const VideoAdaptationCounters& adaptation_counters,
      rtc::scoped_refptr<Resource> reason) = 0;
};

// The Resource Adaptation Processor is responsible for reacting to resource
// usage measurements (e.g. overusing or underusing CPU). When a resource is
// overused the Processor is responsible for performing mitigations in order to
// consume less resources.
class ResourceAdaptationProcessorInterface {
 public:
  virtual ~ResourceAdaptationProcessorInterface();

  virtual void InitializeOnResourceAdaptationQueue() = 0;

  virtual DegradationPreference degradation_preference() const = 0;
  // Reinterprets "balanced + screenshare" as "maintain-resolution".
  // TODO(hbos): Don't do this. This is not what "balanced" means. If the
  // application wants to maintain resolution it should set that degradation
  // preference rather than depend on non-standard behaviors.
  virtual DegradationPreference effective_degradation_preference() const = 0;

  // Starts or stops listening to resources, effectively enabling or disabling
  // processing.
  // TODO(https://crbug.com/webrtc/11172): Automatically register and unregister
  // with AddResource() and RemoveResource() instead. When the processor is
  // multi-stream aware, stream-specific resouces will get added and removed
  // over time.
  virtual void StartResourceAdaptation() = 0;
  virtual void StopResourceAdaptation() = 0;
  virtual void AddAdaptationListener(
      ResourceAdaptationProcessorListener* adaptation_listener) = 0;
  virtual void RemoveAdaptationListener(
      ResourceAdaptationProcessorListener* adaptation_listener) = 0;
  virtual void AddResource(rtc::scoped_refptr<Resource> resource) = 0;
  virtual void RemoveResource(rtc::scoped_refptr<Resource> resource) = 0;

  virtual void SetDegradationPreference(
      DegradationPreference degradation_preference) = 0;
  virtual void SetIsScreenshare(bool is_screenshare) = 0;
  virtual void ResetVideoSourceRestrictions() = 0;

  // May trigger one or more adaptations. It is meant to reduce resolution -
  // useful if a frame was dropped due to its size - however, the implementation
  // may not guarantee this (see resource_adaptation_processor.h).
  // TODO(hbos): This is only part of the interface for backwards-compatiblity
  // reasons. Can we replace this by something which actually satisfies the
  // resolution constraints or get rid of it altogether?
  virtual void TriggerAdaptationDueToFrameDroppedDueToSize(
      rtc::scoped_refptr<Resource> reason_resource) = 0;
};

}  // namespace webrtc

#endif  // CALL_ADAPTATION_RESOURCE_ADAPTATION_PROCESSOR_INTERFACE_H_
