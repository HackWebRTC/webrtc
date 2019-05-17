/*
 *  Copyright 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#include "test/scenario/network/feedback_generator.h"

#include "absl/memory/memory.h"
#include "rtc_base/checks.h"

namespace webrtc {

FeedbackGeneratorImpl::FeedbackGeneratorImpl(
    FeedbackGeneratorImpl::Config config)
    : conf_(config),
      time_controller_{Timestamp::seconds(100000)},
      net_{&time_controller_},
      send_link_{new SimulatedNetwork(conf_.send_link)},
      ret_link_{new SimulatedNetwork(conf_.return_link)},
      send_ep_{net_.CreateEndpoint(EmulatedEndpointConfig())},
      ret_ep_{net_.CreateEndpoint(EmulatedEndpointConfig())},
      send_route_{net_.CreateRoute(
          send_ep_,
          {net_.CreateEmulatedNode(absl::WrapUnique(send_link_))},
          ret_ep_)},
      ret_route_{net_.CreateRoute(
          ret_ep_,
          {net_.CreateEmulatedNode(absl::WrapUnique(ret_link_))},
          send_ep_)},
      send_addr_{rtc::SocketAddress(send_ep_->GetPeerLocalAddress(), 0)},
      ret_addr_{rtc::SocketAddress(ret_ep_->GetPeerLocalAddress(), 0)},
      received_packet_handler_{send_route_,
                               [&](SentPacket packet, Timestamp arrival_time) {
                                 OnPacketReceived(std::move(packet),
                                                  arrival_time);
                               }},
      received_feedback_handler_{
          ret_route_,
          [&](TransportPacketsFeedback packet, Timestamp arrival_time) {
            packet.feedback_time = arrival_time;
            feedback_.push_back(packet);
          }} {}

Timestamp FeedbackGeneratorImpl::Now() {
  return Timestamp::ms(time_controller_.GetClock()->TimeInMilliseconds());
}

void FeedbackGeneratorImpl::Sleep(TimeDelta duration) {
  time_controller_.Sleep(duration);
}

void FeedbackGeneratorImpl::SendPacket(size_t size) {
  SentPacket sent;
  sent.send_time = Now();
  sent.size = DataSize::bytes(size);
  received_packet_handler_.SendPacket(send_ep_, size, sent);
}

std::vector<TransportPacketsFeedback> FeedbackGeneratorImpl::PopFeedback() {
  std::vector<TransportPacketsFeedback> ret;
  ret.swap(feedback_);
  return ret;
}

void FeedbackGeneratorImpl::SetSendConfig(BuiltInNetworkBehaviorConfig config) {
  conf_.send_link = config;
  send_link_->SetConfig(conf_.send_link);
}

void FeedbackGeneratorImpl::SetReturnConfig(
    BuiltInNetworkBehaviorConfig config) {
  conf_.return_link = config;
  ret_link_->SetConfig(conf_.return_link);
}

void FeedbackGeneratorImpl::SetSendLinkCapacity(DataRate capacity) {
  conf_.send_link.link_capacity_kbps = capacity.kbps<int>();
  send_link_->SetConfig(conf_.send_link);
}

void FeedbackGeneratorImpl::OnPacketReceived(SentPacket packet,
                                             Timestamp arrival_time) {
  PacketResult result;
  result.sent_packet = packet;
  result.receive_time = arrival_time;
  builder_.packet_feedbacks.push_back(result);
  Timestamp first_recv = builder_.packet_feedbacks.front().receive_time;
  if (Now() - first_recv > conf_.feedback_interval) {
    received_feedback_handler_.SendPacket(
        ret_ep_, conf_.feedback_packet_size.bytes<size_t>(), builder_);
    builder_ = {};
  }
}

}  // namespace webrtc
