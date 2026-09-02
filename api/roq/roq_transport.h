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
#include "system_wrappers/include/clock.h"

namespace webrtc::roq {

struct RoqPacketTransportConfig {
  RoqSession* session = nullptr;
  RoqMediaType media_type = RoqMediaType::kVideo;
  uint64_t rtp_flow_id = 0;
  uint64_t rtcp_flow_id = 0;
  uint8_t primary_payload_type = 0;
  uint32_t primary_ssrc = 0;
  uint8_t rtx_payload_type = 0;
  uint32_t rtx_ssrc = 0;
  Clock* clock = nullptr;
  TimeDelta critical_rtcp_deadline = TimeDelta::Millis(50);
  TimeDelta audio_deadline = TimeDelta::Millis(60);
  TimeDelta video_delta_deadline = TimeDelta::Millis(100);
  TimeDelta video_key_deadline = TimeDelta::Millis(150);
  TimeDelta periodic_rtcp_deadline = TimeDelta::Millis(250);
};

class RoqPacketTransport final : public Transport {
 public:
  using PacketAcceptedCallback = std::function<
      void(uint64_t, std::span<const uint8_t>, const PacketOptions&)>;

  explicit RoqPacketTransport(RoqPacketTransportConfig config,
                              PacketAcceptedCallback packet_accepted = {});

  bool SendRtp(std::span<const uint8_t> packet,
               const PacketOptions& options) override;
  bool SendRtcp(std::span<const uint8_t> packet,
                const PacketOptions& options) override;

 private:
  bool Send(uint64_t flow_id,
            std::span<const uint8_t> packet,
            const PacketOptions& options);

  PacketAcceptedCallback packet_accepted_;
  const RoqPacketTransportConfig config_;
};

}  // namespace webrtc::roq

#endif  // API_ROQ_ROQ_TRANSPORT_H_
