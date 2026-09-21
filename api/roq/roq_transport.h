/*
 *  Copyright (c) 2026 The WebRTC project authors. All Rights Reserved.
 */

#ifndef API_ROQ_ROQ_TRANSPORT_H_
#define API_ROQ_ROQ_TRANSPORT_H_

#include <algorithm>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <span>
#include <string>

#include "api/call/transport.h"
#include "api/roq/roq_session.h"
#include "api/video/video_codec_type.h"
#include "system_wrappers/include/clock.h"

namespace webrtc::roq {

struct RoqPacketTransportConfig {
  RoqSession* session = nullptr;
  std::string binding_id;
  RoqMediaType media_type = RoqMediaType::kVideo;
  VideoCodecType video_codec = VideoCodecType::kVideoCodecH264;
  uint64_t rtp_flow_id = 0;
  uint64_t rtcp_flow_id = 0;
  uint8_t primary_payload_type = 0;
  uint32_t primary_ssrc = 0;
  uint8_t rtx_payload_type = 0;
  uint32_t rtx_ssrc = 0;
  bool allow_early_publisher_media = false;
  Clock* clock = nullptr;
  std::function<void(uint32_t, uint32_t, uint16_t)> key_frame_submitted;
};

class RoqPacketTransport final : public Transport {
 public:
  using PacketAcceptedCallback = std::function<
      void(uint64_t, std::span<const uint8_t>, const PacketOptions&)>;

  explicit RoqPacketTransport(RoqPacketTransportConfig config,
                              PacketAcceptedCallback packet_accepted = {});

  bool SendRtp(std::span<const uint8_t> packet,
               const PacketOptions& options) override;
  bool HasAsyncSendCompletion() const override { return true; }
  bool UsesMediaPayloadBudget() const override { return true; }
  size_t MaxRtpPacketSize() const override {
    return std::min(
        std::min(application_packet_limit_.load(), size_t{1100}),
        config_.session->maximum_packet_payload(rtp_flow_id_.load()));
  }
  void SetMaxRtpPacketSize(size_t bytes) {
    application_packet_limit_.store(bytes);
  }
  bool SetFlowIds(uint64_t rtp_flow_id, uint64_t rtcp_flow_id) {
    const bool rtp_changed = rtp_flow_id_.exchange(rtp_flow_id) != rtp_flow_id;
    const bool rtcp_changed =
        rtcp_flow_id_.exchange(rtcp_flow_id) != rtcp_flow_id;
    return rtp_changed || rtcp_changed;
  }
  bool SendRtcp(std::span<const uint8_t> packet,
                const PacketOptions& options) override;
  RtpPacingMode GetRtpPacingMode() const override {
    return RtpPacingMode::kExternalTransport;
  }
  uint64_t accepted_rtp_packets() const {
    return accepted_rtp_packets_->load();
  }

 private:
  struct KeyFrameSubmissionState;
  bool Send(bool rtcp,
            std::span<const uint8_t> packet,
            const PacketOptions& options);

  PacketAcceptedCallback packet_accepted_;
  std::atomic<size_t> application_packet_limit_ = 1500;
  const RoqPacketTransportConfig config_;
  std::atomic<uint64_t> rtp_flow_id_;
  std::atomic<uint64_t> rtcp_flow_id_;
  std::shared_ptr<std::atomic<uint64_t>> accepted_rtp_packets_ =
      std::make_shared<std::atomic<uint64_t>>(0);
  std::shared_ptr<KeyFrameSubmissionState> key_frame_submission_state_;
};

}  // namespace webrtc::roq

#endif  // API_ROQ_ROQ_TRANSPORT_H_
