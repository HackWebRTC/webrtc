/*
 *  Copyright 2019 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef CALL_ADAPTATION_TEST_FAKE_RESOURCE_H_
#define CALL_ADAPTATION_TEST_FAKE_RESOURCE_H_

#include <string>

#include "call/adaptation/resource.h"
#include "rtc_base/ref_counted_object.h"

namespace webrtc {

// Fake resource used for testing.
class FakeResource : public rtc::RefCountedObject<Resource> {
 public:
  explicit FakeResource(std::string name);
  ~FakeResource() override;

  void set_usage_state(ResourceUsageState usage_state);
  void set_is_adaptation_up_allowed(bool is_adaptation_up_allowed);
  size_t num_adaptations_applied() const;

  // Resource implementation.
  std::string name() const override { return name_; }
  bool IsAdaptationUpAllowed(
      const VideoStreamInputState& input_state,
      const VideoSourceRestrictions& restrictions_before,
      const VideoSourceRestrictions& restrictions_after,
      rtc::scoped_refptr<Resource> reason_resource) const override;
  void OnAdaptationApplied(
      const VideoStreamInputState& input_state,
      const VideoSourceRestrictions& restrictions_before,
      const VideoSourceRestrictions& restrictions_after,
      rtc::scoped_refptr<Resource> reason_resource) override;

 private:
  const std::string name_;
  bool is_adaptation_up_allowed_;
  size_t num_adaptations_applied_;
};

}  // namespace webrtc

#endif  // CALL_ADAPTATION_TEST_FAKE_RESOURCE_H_
