/*
 *  Copyright (c) 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef TEST_SCENARIO_NETWORK_EMULATED_NETWORK_MANAGER_H_
#define TEST_SCENARIO_NETWORK_EMULATED_NETWORK_MANAGER_H_

#include <memory>
#include <vector>

#include "api/test/network_emulation_manager.h"
#include "rtc_base/critical_section.h"
#include "rtc_base/ip_address.h"
#include "rtc_base/network.h"
#include "rtc_base/socket_server.h"
#include "rtc_base/thread.h"
#include "rtc_base/thread_checker.h"
#include "test/scenario/network/fake_network_socket_server.h"
#include "test/scenario/network/network_emulation.h"

namespace webrtc {
namespace test {

// Framework assumes that rtc::NetworkManager is called from network thread.
class EmulatedNetworkManager : public rtc::NetworkManagerBase,
                               public sigslot::has_slots<>,
                               public EmulatedNetworkManagerInterface {
 public:
  EmulatedNetworkManager(Clock* clock, EndpointsContainer* endpoints_container);

  void EnableEndpoint(EmulatedEndpoint* endpoint);
  void DisableEndpoint(EmulatedEndpoint* endpoint);

  // NetworkManager interface. All these methods are supposed to be called from
  // the same thread.
  void StartUpdating() override;
  void StopUpdating() override;

  // EmulatedNetworkManagerInterface API
  rtc::Thread* network_thread() override { return &network_thread_; }
  rtc::NetworkManager* network_manager() override { return this; }

 private:
  void UpdateNetworksOnce();
  void MaybeSignalNetworksChanged();

  EndpointsContainer* const endpoints_container_;
  FakeNetworkSocketServer socket_server_;
  rtc::Thread network_thread_;

  bool sent_first_update_ RTC_GUARDED_BY(network_thread_);
  int start_count_ RTC_GUARDED_BY(network_thread_);
};

}  // namespace test
}  // namespace webrtc

#endif  // TEST_SCENARIO_NETWORK_EMULATED_NETWORK_MANAGER_H_
