/*
 *  Copyright 2019 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef CALL_ADAPTATION_RESOURCE_H_
#define CALL_ADAPTATION_RESOURCE_H_

#include <string>
#include <vector>

#include "absl/types/optional.h"
#include "call/adaptation/video_source_restrictions.h"
#include "call/adaptation/video_stream_input_state.h"

namespace webrtc {

class Resource;

enum class ResourceUsageState {
  // Action is needed to minimze the load on this resource.
  kOveruse,
  // Increasing the load on this resource is desired, if possible.
  kUnderuse,
};

class ResourceListener {
 public:
  virtual ~ResourceListener();

  // Informs the listener of a new measurement of resource usage. This means
  // that |resource.usage_state()| is now up-to-date.
  virtual void OnResourceUsageStateMeasured(const Resource& resource) = 0;
};

class Resource {
 public:
  // By default, usage_state() is null until a measurement is made.
  Resource();
  virtual ~Resource();

  void SetResourceListener(ResourceListener* listener);

  absl::optional<ResourceUsageState> usage_state() const;
  void ClearUsageState();

  // This method allows the Resource to reject a proposed adaptation in the "up"
  // direction if it predicts this would cause overuse of this resource. The
  // default implementation unconditionally returns true (= allowed).
  virtual bool IsAdaptationUpAllowed(
      const VideoStreamInputState& input_state,
      const VideoSourceRestrictions& restrictions_before,
      const VideoSourceRestrictions& restrictions_after,
      const Resource& reason_resource) const;
  virtual void OnAdaptationApplied(
      const VideoStreamInputState& input_state,
      const VideoSourceRestrictions& restrictions_before,
      const VideoSourceRestrictions& restrictions_after,
      const Resource& reason_resource);

  virtual std::string name() const = 0;

 protected:
  // Updates the usage state and informs all registered listeners.
  void OnResourceUsageStateMeasured(ResourceUsageState usage_state);

 private:
  absl::optional<ResourceUsageState> usage_state_;
  ResourceListener* listener_;
};

}  // namespace webrtc

#endif  // CALL_ADAPTATION_RESOURCE_H_
