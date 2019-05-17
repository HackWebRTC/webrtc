/*
 *  Copyright 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#ifndef TEST_SCENARIO_NETWORK_FEEDBACK_GENERATOR_H_
#define TEST_SCENARIO_NETWORK_FEEDBACK_GENERATOR_H_

#include <map>
#include <utility>
#include <vector>

#include "api/transport/test/feedback_generator_interface.h"
#include "call/simulated_network.h"
#include "test/scenario/network/network_emulation.h"
#include "test/scenario/network/network_emulation_manager.h"
#include "test/time_controller/simulated_time_controller.h"

namespace webrtc {

template <typename FakePacketType>
class FakePacketRoute : public EmulatedNetworkReceiverInterface {
 public:
  FakePacketRoute(EmulatedRoute* route,
                  std::function<void(FakePacketType, Timestamp)> action)
      : route_(route),
        action_(std::move(action)),
        send_addr_(route_->from->GetPeerLocalAddress(), 0),
        recv_addr_(route_->to->GetPeerLocalAddress(),
                   *route_->to->BindReceiver(0, this)) {}

  void SendPacket(EmulatedEndpoint* from, size_t size, FakePacketType packet) {
    RTC_CHECK_GE(size, sizeof(int));
    sent_.emplace(next_packet_id_, packet);
    rtc::CopyOnWriteBuffer buf(size);
    reinterpret_cast<int*>(buf.data())[0] = next_packet_id_++;
    from->SendPacket(send_addr_, recv_addr_, buf);
  }

  void OnPacketReceived(EmulatedIpPacket packet) override {
    int packet_id = reinterpret_cast<int*>(packet.data.data())[0];
    action_(std::move(sent_[packet_id]), packet.arrival_time);
    sent_.erase(packet_id);
  }

 private:
  EmulatedRoute* const route_;
  const std::function<void(FakePacketType, Timestamp)> action_;
  const rtc::SocketAddress send_addr_;
  const rtc::SocketAddress recv_addr_;

  int next_packet_id_ = 0;
  std::map<int, FakePacketType> sent_;
};

class FeedbackGeneratorImpl : public FeedbackGenerator {
 public:
  explicit FeedbackGeneratorImpl(Config config);
  Timestamp Now() override;
  void Sleep(TimeDelta duration) override;
  void SendPacket(size_t size) override;
  std::vector<TransportPacketsFeedback> PopFeedback() override;

  void SetSendConfig(BuiltInNetworkBehaviorConfig config) override;
  void SetReturnConfig(BuiltInNetworkBehaviorConfig config) override;

  void SetSendLinkCapacity(DataRate capacity) override;

 private:
  void OnPacketReceived(SentPacket packet, Timestamp arrival_time);
  Config conf_;
  GlobalSimulatedTimeController time_controller_;
  ::webrtc::test::NetworkEmulationManagerImpl net_;
  SimulatedNetwork* const send_link_;
  SimulatedNetwork* const ret_link_;
  EmulatedEndpoint* const send_ep_;
  EmulatedEndpoint* const ret_ep_;
  EmulatedRoute* const send_route_;
  EmulatedRoute* const ret_route_;
  const rtc::SocketAddress send_addr_;
  const rtc::SocketAddress ret_addr_;
  FakePacketRoute<SentPacket> received_packet_handler_;
  FakePacketRoute<TransportPacketsFeedback> received_feedback_handler_;

  TransportPacketsFeedback builder_;
  std::vector<TransportPacketsFeedback> feedback_;
};
}  // namespace webrtc
#endif  // TEST_SCENARIO_NETWORK_FEEDBACK_GENERATOR_H_
