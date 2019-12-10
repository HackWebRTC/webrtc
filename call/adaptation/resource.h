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

namespace webrtc {

enum class ResourceUsageState {
  // Action is needed to minimze the load on this resource.
  kOveruse,
  // No action needed for this resource, increasing the load on this resource
  // is not allowed.
  kStable,
  // Increasing the load on this resource is allowed.
  kUnderuse,
};

// A Resource is something which can be measured as "overused", "stable" or
// "underused". For example, if we are overusing CPU we may need to lower the
// resolution of one of the streams. In other words, one of the ResourceConumers
// - representing an encoder - needs to be reconfigured with a different
// ResourceConsumerConfiguration - representing a different encoder setting.
//
// This is an abstract class used by the ResourceAdaptationProcessor to make
// decisions about which configurations to use. How a resource is measured or
// what measurements map to different ResourceUsageState values is
// implementation-specific.
class Resource {
 public:
  virtual ~Resource();

  // Informational, not formally part of the decision-making process.
  virtual std::string Name() const = 0;
  virtual std::string UsageUnitsOfMeasurement() const = 0;
  // Valid ranges are implementation-specific.
  virtual double CurrentUsage() const = 0;

  // The current usage state of this resource. Used by the
  // ResourceAdaptationProcessor to calculate the desired consumer
  // configurations.
  virtual ResourceUsageState CurrentUsageState() const = 0;

  std::string ToString() const;
};

}  // namespace webrtc

#endif  // CALL_ADAPTATION_RESOURCE_H_
