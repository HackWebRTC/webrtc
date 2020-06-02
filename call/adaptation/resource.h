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
#include "api/scoped_refptr.h"
#include "api/task_queue/task_queue_base.h"
#include "call/adaptation/video_source_restrictions.h"
#include "call/adaptation/video_stream_input_state.h"
#include "rtc_base/ref_count.h"

namespace webrtc {

class Resource;

enum class ResourceUsageState {
  // Action is needed to minimze the load on this resource.
  kOveruse,
  // Increasing the load on this resource is desired, if possible.
  kUnderuse,
};

const char* ResourceUsageStateToString(ResourceUsageState usage_state);

class ResourceListener {
 public:
  virtual ~ResourceListener();

  // Informs the listener of a new measurement of resource usage. This means
  // that |resource->usage_state()| is now up-to-date.
  virtual void OnResourceUsageStateMeasured(
      rtc::scoped_refptr<Resource> resource) = 0;
};

// A Resource monitors an implementation-specific system resource. It may report
// kOveruse or kUnderuse when resource usage is high or low enough that we
// should perform some sort of mitigation to fulfil the resource's constraints.
//
// All methods defined in this interface, except SetResourceListener(), MUST be
// invoked on the resource adaptation task queue.
//
// Usage measurements may be performed on an implementation-specific task queue.
// The Resource is reference counted to prevent use-after-free when posting
// between task queues. As such, the implementation MUST NOT make any
// assumptions about which task queue Resource is destructed on.
class Resource : public rtc::RefCountInterface {
 public:
  Resource();
  // Destruction may happen on any task queue.
  ~Resource() override;

  virtual std::string Name() const = 0;
  // The listener MUST be informed any time UsageState() changes.
  virtual void SetResourceListener(ResourceListener* listener) = 0;
  // Within a single task running on the adaptation task queue, UsageState()
  // MUST return the same value every time it is called.
  // TODO(https://crbug.com/webrtc/11618): Remove the UsageState() getter in
  // favor of passing the use usage state directly to the ResourceListener. This
  // gets rid of this strange requirement of having to return the same thing
  // every time.
  virtual absl::optional<ResourceUsageState> UsageState() const = 0;
  // Invalidates current usage measurements, i.e. in response to the system load
  // changing. Example: an adaptation was just applied.
  virtual void ClearUsageState() = 0;
};

}  // namespace webrtc

#endif  // CALL_ADAPTATION_RESOURCE_H_
