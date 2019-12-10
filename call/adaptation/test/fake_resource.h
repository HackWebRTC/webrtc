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

namespace webrtc {

// Fake resource used for testing. ResourceUsageState is controlled with a
// setter. The arbitrarily chosen unit of measurement is percentage, with the
// following current usage reported based on the current usage: kOveruse = 120%,
// kStable = 80% and kUnderuse = 40%.
class FakeResource : public Resource {
 public:
  FakeResource(std::string name, ResourceUsageState usage);
  explicit FakeResource(ResourceUsageState usage);
  ~FakeResource() override;

  void set_usage(ResourceUsageState usage);

  std::string Name() const override;
  std::string UsageUnitsOfMeasurement() const override;
  double CurrentUsage() const override;
  ResourceUsageState CurrentUsageState() const override;

 private:
  std::string name_;
  ResourceUsageState usage_;
};

}  // namespace webrtc

#endif  // CALL_ADAPTATION_TEST_FAKE_RESOURCE_H_
