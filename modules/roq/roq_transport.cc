/*
 *  Copyright (c) 2026 The WebRTC project authors. All Rights Reserved.
 */

#include "api/roq/roq_transport.h"

#include <cstddef>
#include <cstdint>
#include <map>
#include <mutex>
#include <span>
#include <utility>

#include "api/units/time_delta.h"
#include "modules/roq/rtp_packet_metadata.h"
#include "rtc_base/logging.h"
#include "rtc_base/time_utils.h"

namespace webrtc::roq {

struct RoqPacketTransport::KeyFrameSubmissionState {
  struct Frame {
    size_t registered_packets = 0;
    size_t submitted_packets = 0;
    size_t rtp_bytes = 0;
    int64_t first_submit_us = -1;
    bool marker_seen = false;
    bool is_key = false;
    uint32_t ssrc = 0;
    uint16_t marker_sequence = 0;
  };

  void Register(const RtpPacketMetadata& metadata, size_t bytes) {
    std::lock_guard lock(mutex);
    while (frames.size() >= 128 && !frames.contains(metadata.timestamp))
      frames.erase(frames.begin());
    Frame& frame = frames[metadata.timestamp];
    ++frame.registered_packets;
    frame.rtp_bytes += bytes;
    frame.is_key |= metadata.is_key;
    frame.ssrc = metadata.ssrc;
    if (metadata.marker) {
      frame.marker_seen = true;
      frame.marker_sequence = metadata.sequence_number;
    }
  }

  void Submitted(const RtpPacketMetadata& metadata) {
    std::function<void(uint32_t, uint32_t, uint16_t)> callback;
    uint32_t ssrc = 0;
    uint32_t timestamp = 0;
    uint16_t sequence = 0;
    {
      std::lock_guard lock(mutex);
      submitted_vps |= metadata.contains_vps;
      submitted_sps |= metadata.contains_sps;
      submitted_pps |= metadata.contains_pps;
      Frame& frame = frames[metadata.timestamp];
      ++frame.submitted_packets;
      if (frame.first_submit_us < 0)
        frame.first_submit_us = TimeMicros();
      if (frame.marker_seen &&
          frame.submitted_packets == frame.registered_packets) {
        if (frame.is_key && submitted_sps && submitted_pps &&
            (video_codec != kVideoCodecH265 || submitted_vps)) {
          RTC_LOG(LS_INFO) << "roq_idr_submitted ssrc=" << frame.ssrc
                           << " timestamp=" << metadata.timestamp
                           << " packets=" << frame.registered_packets
                           << " rtp_bytes=" << frame.rtp_bytes
                           << " first_submit_us=" << frame.first_submit_us
                           << " last_submit_us=" << TimeMicros();
          callback = on_submitted;
          ssrc = frame.ssrc;
          timestamp = metadata.timestamp;
          sequence = frame.marker_sequence;
        }
        frames.erase(metadata.timestamp);
      }
    }
    if (callback)
      callback(ssrc, timestamp, sequence);
  }

  std::mutex mutex;
  std::map<uint32_t, Frame> frames;
  VideoCodecType video_codec = kVideoCodecH264;
  bool submitted_vps = false;
  bool submitted_sps = false;
  bool submitted_pps = false;
  std::function<void(uint32_t, uint32_t, uint16_t)> on_submitted;
};

RoqPacketTransport::RoqPacketTransport(RoqPacketTransportConfig config,
                                       PacketAcceptedCallback packet_accepted)
    : packet_accepted_(std::move(packet_accepted)),
      config_(std::move(config)),
      rtp_flow_id_(config_.rtp_flow_id),
      rtcp_flow_id_(config_.rtcp_flow_id),
      key_frame_submission_state_(std::make_shared<KeyFrameSubmissionState>()) {
  key_frame_submission_state_->video_codec = config_.video_codec;
  key_frame_submission_state_->on_submitted = config_.key_frame_submitted;
}

bool RoqPacketTransport::SendRtp(std::span<const uint8_t> packet,
                                 const PacketOptions& options) {
  return Send(false, packet, options);
}

bool RoqPacketTransport::SendRtcp(std::span<const uint8_t> packet,
                                  const PacketOptions& options) {
  return Send(true, packet, options);
}

bool RoqPacketTransport::Send(bool rtcp,
                              std::span<const uint8_t> packet,
                              const PacketOptions& options) {
  const Timestamp now = config_.clock->CurrentTime();
  const uint64_t flow_id = rtcp ? rtcp_flow_id_.load() : rtp_flow_id_.load();
  bool track_video_submission = false;
  RtpPacketMetadata submitted_metadata;
  RoqDatagramMetadata datagram = {.binding_id = config_.binding_id,
                                  .media_type = config_.media_type,
                                  .enqueue_time = now,
                                  .video_codec = config_.video_codec};
  if (rtcp) {
    bool critical = false;
    if (!ClassifyRtcpPacket(packet, &critical)) {
      config_.session->RecordInvalidDatagram();
      return false;
    }
    datagram.media_type = RoqMediaType::kRtcp;
    datagram.priority = critical ? RoqDatagramPriority::kRtcp
                                 : RoqDatagramPriority::kPeriodicRtcp;
  } else {
    RtpPacketMetadata metadata;
    if (config_.media_type == RoqMediaType::kAudio) {
      if (!ParseRtpPacketMetadata(packet, &metadata) ||
          metadata.payload_type != config_.primary_payload_type ||
          metadata.ssrc != config_.primary_ssrc) {
        config_.session->RecordInvalidDatagram();
        return false;
      }
      datagram.priority = RoqDatagramPriority::kAudio;
      datagram.packet_id = RoqPacketId{metadata.ssrc, metadata.sequence_number};
    } else if (ParseVideoRtpPacketMetadata(packet, config_.video_codec,
                                           &metadata) &&
               metadata.payload_type == config_.primary_payload_type &&
               metadata.ssrc == config_.primary_ssrc) {
      const bool key = metadata.is_key || metadata.contains_vps ||
                       metadata.contains_sps || metadata.contains_pps;
      datagram.priority = key ? RoqDatagramPriority::kVideoKey
                              : RoqDatagramPriority::kVideoDelta;
      datagram.frame_id =
          RoqFrameId{metadata.ssrc, metadata.timestamp, config_.binding_id};
      datagram.packet_id = RoqPacketId{metadata.ssrc, metadata.sequence_number};
      datagram.frame_complete = metadata.marker;
      datagram.is_key = metadata.is_key;
      datagram.contains_vps = metadata.contains_vps;
      datagram.contains_sps = metadata.contains_sps;
      datagram.contains_pps = metadata.contains_pps;
      track_video_submission = true;
      submitted_metadata = metadata;
      key_frame_submission_state_->Register(metadata, packet.size());
    } else if (ParseRtpPacketMetadata(packet, &metadata) &&
               metadata.payload_type == config_.rtx_payload_type &&
               metadata.ssrc == config_.rtx_ssrc) {
      uint16_t original_sequence_number = 0;
      if (!ParseRtxOriginalSequenceNumber(packet, &original_sequence_number)) {
        config_.session->RecordInvalidDatagram();
        return false;
      }
      datagram.original_packet_id =
          RoqPacketId{config_.primary_ssrc, original_sequence_number};
    } else {
      config_.session->RecordInvalidDatagram();
      return false;
    }
  }
  PacketAcceptedCallback accepted = packet_accepted_;
  auto key_frame_submission_state = key_frame_submission_state_;
  std::shared_ptr<std::atomic<uint64_t>> accepted_rtp_packets =
      rtcp ? nullptr : accepted_rtp_packets_;
  std::vector<uint8_t> accepted_packet(packet.begin(), packet.end());
  const RoqSendResult result = config_.session->SendMedia(
      flow_id, packet, std::move(datagram),
      [accepted = std::move(accepted), accepted_rtp_packets, flow_id,
       packet = std::move(accepted_packet), options, key_frame_submission_state,
       submitted_metadata, track_video_submission] {
        if (accepted_rtp_packets)
          ++*accepted_rtp_packets;
        if (accepted)
          accepted(flow_id, packet, options);
        if (track_video_submission)
          key_frame_submission_state->Submitted(submitted_metadata);
      });
  if (result != RoqSendResult::kQueued) {
    return false;
  }
  return true;
}

}  // namespace webrtc::roq
