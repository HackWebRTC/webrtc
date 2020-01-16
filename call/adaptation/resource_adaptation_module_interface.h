/*
 *  Copyright 2019 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef CALL_ADAPTATION_RESOURCE_ADAPTATION_MODULE_INTERFACE_H_
#define CALL_ADAPTATION_RESOURCE_ADAPTATION_MODULE_INTERFACE_H_

#include "api/rtp_parameters.h"
#include "call/adaptation/video_source_restrictions.h"

namespace webrtc {

// The listener is responsible for carrying out the reconfiguration of the video
// source such that the VideoSourceRestrictions are fulfilled.
class ResourceAdaptationModuleListener {
 public:
  virtual ~ResourceAdaptationModuleListener();

  // TODO(hbos): When we support the muli-stream use case, the arguments need to
  // specify which video stream's source needs to be reconfigured.
  virtual void OnVideoSourceRestrictionsUpdated(
      VideoSourceRestrictions restrictions) = 0;
};

// Responsible for reconfiguring encoded streams based on resource consumption,
// such as scaling down resolution or frame rate when CPU is overused. This
// interface is meant to be injectable into VideoStreamEncoder.
//
// [UNDER CONSTRUCTION] This interface is work-in-progress. In the future it
// needs to be able to handle all the necessary input and output for resource
// adaptation decision making.
//
// TODO(https://crbug.com/webrtc/11222): Make this interface feature-complete so
// that a module (such as OveruseFrameDetectorResourceAdaptationModule) is fully
// operational through this abstract interface.
class ResourceAdaptationModuleInterface {
 public:
  virtual ~ResourceAdaptationModuleInterface();

  // TODO(hbos): When input/output of the module is adequetly handled by this
  // interface, these methods need to say which stream to start/stop, enabling
  // multi-stream aware implementations of ResourceAdaptationModuleInterface. We
  // don't want to do this before we have the right interfaces (e.g. if we pass
  // in a VideoStreamEncoder here directly then have a dependency on a different
  // build target). For the multi-stream use case we may consider making
  // ResourceAdaptationModuleInterface reference counted.
  virtual void StartResourceAdaptation(
      ResourceAdaptationModuleListener* adaptation_listener) = 0;
  virtual void StopResourceAdaptation() = 0;

  // The following methods are callable whether or not adaption is started.

  // Informs the module whether we have input video. By default, the module must
  // assume the value is false.
  virtual void SetHasInputVideo(bool has_input_video) = 0;
  virtual void SetDegradationPreference(
      DegradationPreference degradation_preference) = 0;
};

}  // namespace webrtc

#endif  // CALL_ADAPTATION_RESOURCE_ADAPTATION_MODULE_INTERFACE_H_
