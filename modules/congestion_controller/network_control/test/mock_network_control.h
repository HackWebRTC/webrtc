/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_CONGESTION_CONTROLLER_NETWORK_CONTROL_TEST_MOCK_NETWORK_CONTROL_H_
#define MODULES_CONGESTION_CONTROLLER_NETWORK_CONTROL_TEST_MOCK_NETWORK_CONTROL_H_

#include "modules/congestion_controller/network_control/include/network_control.h"
#include "test/gmock.h"

namespace webrtc {
namespace test {
class MockNetworkControllerObserver : public NetworkControllerObserver {
 public:
  MOCK_METHOD1(OnCongestionWindow, void(CongestionWindow));
  MOCK_METHOD1(OnPacerConfig, void(PacerConfig));
  MOCK_METHOD1(OnProbeClusterConfig, void(ProbeClusterConfig));
  MOCK_METHOD1(OnTargetTransferRate, void(TargetTransferRate));
};
}  // namespace test
}  // namespace webrtc

#endif  // MODULES_CONGESTION_CONTROLLER_NETWORK_CONTROL_TEST_MOCK_NETWORK_CONTROL_H_
