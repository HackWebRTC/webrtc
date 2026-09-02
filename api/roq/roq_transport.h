/*
 *  Copyright (c) 2026 The WebRTC project authors. All Rights Reserved.
 */

#ifndef API_ROQ_ROQ_TRANSPORT_H_
#define API_ROQ_ROQ_TRANSPORT_H_

#include <cstddef>
#include <cstdint>
#include <functional>
#include <span>

#include "api/call/transport.h"
#include "api/roq/roq_session.h"

namespace webrtc::roq {

class RoqPacketTransport final : public Transport {
 public:
  using PacketAcceptedCallback = std::function<
      void(uint64_t, std::span<const uint8_t>, const PacketOptions&)>;

  RoqPacketTransport(RoqSession* session,
                     uint64_t rtp_flow_id,
                     uint64_t rtcp_flow_id,
                     PacketAcceptedCallback packet_accepted = {});

  bool SendRtp(std::span<const uint8_t> packet,
               const PacketOptions& options) override;
  bool SendRtcp(std::span<const uint8_t> packet,
                const PacketOptions& options) override;

 private:
  bool Send(uint64_t flow_id,
            std::span<const uint8_t> packet,
            const PacketOptions& options);

  RoqSession* const session_;
  const uint64_t rtp_flow_id_;
  const uint64_t rtcp_flow_id_;
  PacketAcceptedCallback packet_accepted_;
};

}  // namespace webrtc::roq

#endif  // API_ROQ_ROQ_TRANSPORT_H_
