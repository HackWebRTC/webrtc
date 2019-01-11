/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef TEST_SCENARIO_NETWORK_NETWORK_EMULATION_H_
#define TEST_SCENARIO_NETWORK_NETWORK_EMULATION_H_

#include <cstdint>
#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "absl/types/optional.h"
#include "api/test/simulated_network.h"
#include "api/units/timestamp.h"
#include "rtc_base/async_socket.h"
#include "rtc_base/copy_on_write_buffer.h"
#include "rtc_base/socket_address.h"
#include "rtc_base/thread.h"

namespace webrtc {
namespace test {

struct EmulatedIpPacket {
 public:
  EmulatedIpPacket(const rtc::SocketAddress& from,
                   const rtc::SocketAddress& to,
                   uint64_t dest_endpoint_id,
                   rtc::CopyOnWriteBuffer data,
                   Timestamp arrival_time);

  ~EmulatedIpPacket();
  // This object is not copyable or assignable.
  EmulatedIpPacket(const EmulatedIpPacket&) = delete;
  EmulatedIpPacket& operator=(const EmulatedIpPacket&) = delete;
  // This object is only moveable.
  EmulatedIpPacket(EmulatedIpPacket&&);
  EmulatedIpPacket& operator=(EmulatedIpPacket&&);

  size_t size() const { return data.size(); }
  const uint8_t* cdata() const { return data.cdata(); }

  rtc::SocketAddress from;
  rtc::SocketAddress to;
  uint64_t dest_endpoint_id;
  rtc::CopyOnWriteBuffer data;
  Timestamp arrival_time;
};

class EmulatedNetworkReceiverInterface {
 public:
  virtual ~EmulatedNetworkReceiverInterface() = default;

  virtual void OnPacketReceived(EmulatedIpPacket packet) = 0;
};

// Represents node in the emulated network. Nodes can be connected with each
// other to form different networks with different behavior. The behavior of
// the node itself is determined by a concrete implementation of
// NetworkBehaviorInterface that is provided on construction.
class EmulatedNetworkNode : public EmulatedNetworkReceiverInterface {
 public:
  // Creates node based on |network_behavior|. The specified |packet_overhead|
  // is added to the size of each packet in the information provided to
  // |network_behavior|.
  EmulatedNetworkNode(
      std::unique_ptr<NetworkBehaviorInterface> network_behavior,
      size_t packet_overhead = 0);
  ~EmulatedNetworkNode() override;
  RTC_DISALLOW_COPY_AND_ASSIGN(EmulatedNetworkNode);

  void OnPacketReceived(EmulatedIpPacket packet) override;
  void Process(Timestamp at_time);
  void SetReceiver(uint64_t dest_endpoint_id,
                   EmulatedNetworkReceiverInterface* receiver);
  void RemoveReceiver(uint64_t dest_endpoint_id);

  // Creates a route for the given receiver_id over all the given nodes to the
  // given receiver.
  static void CreateRoute(uint64_t receiver_id,
                          std::vector<EmulatedNetworkNode*> nodes,
                          EmulatedNetworkReceiverInterface* receiver);
  static void ClearRoute(uint64_t receiver_id,
                         std::vector<EmulatedNetworkNode*> nodes);

 private:
  struct StoredPacket {
    uint64_t id;
    EmulatedIpPacket packet;
    bool removed;
  };

  rtc::CriticalSection lock_;
  std::map<uint64_t, EmulatedNetworkReceiverInterface*> routing_
      RTC_GUARDED_BY(lock_);
  const std::unique_ptr<NetworkBehaviorInterface> network_behavior_
      RTC_GUARDED_BY(lock_);
  const size_t packet_overhead_ RTC_GUARDED_BY(lock_);
  std::deque<StoredPacket> packets_ RTC_GUARDED_BY(lock_);

  uint64_t next_packet_id_ RTC_GUARDED_BY(lock_) = 1;
};

}  // namespace test
}  // namespace webrtc

#endif  // TEST_SCENARIO_NETWORK_NETWORK_EMULATION_H_
