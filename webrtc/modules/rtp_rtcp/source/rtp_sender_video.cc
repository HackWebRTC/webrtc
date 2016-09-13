/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/modules/rtp_rtcp/source/rtp_sender_video.h"

#include <stdlib.h>
#include <string.h>

#include <memory>
#include <vector>

#include "webrtc/base/checks.h"
#include "webrtc/base/logging.h"
#include "webrtc/base/trace_event.h"
#include "webrtc/modules/rtp_rtcp/include/rtp_rtcp_defines.h"
#include "webrtc/modules/rtp_rtcp/source/byte_io.h"
#include "webrtc/modules/rtp_rtcp/source/producer_fec.h"
#include "webrtc/modules/rtp_rtcp/source/rtp_format_video_generic.h"
#include "webrtc/modules/rtp_rtcp/source/rtp_format_vp8.h"
#include "webrtc/modules/rtp_rtcp/source/rtp_format_vp9.h"

namespace webrtc {

namespace {
constexpr size_t kRedForFecHeaderLength = 1;
}  // namespace

RTPSenderVideo::RTPSenderVideo(Clock* clock, RTPSender* rtp_sender)
    : rtp_sender_(rtp_sender),
      clock_(clock),
      fec_bitrate_(1000, RateStatistics::kBpsScale),
      video_bitrate_(1000, RateStatistics::kBpsScale) {}

RTPSenderVideo::~RTPSenderVideo() {}

void RTPSenderVideo::SetVideoCodecType(RtpVideoCodecTypes video_type) {
  video_type_ = video_type;
}

RtpVideoCodecTypes RTPSenderVideo::VideoCodecType() const {
  return video_type_;
}

// Static.
RtpUtility::Payload* RTPSenderVideo::CreateVideoPayload(
    const char payload_name[RTP_PAYLOAD_NAME_SIZE],
    int8_t payload_type) {
  RtpVideoCodecTypes video_type = kRtpVideoGeneric;
  if (RtpUtility::StringCompare(payload_name, "VP8", 3)) {
    video_type = kRtpVideoVp8;
  } else if (RtpUtility::StringCompare(payload_name, "VP9", 3)) {
    video_type = kRtpVideoVp9;
  } else if (RtpUtility::StringCompare(payload_name, "H264", 4)) {
    video_type = kRtpVideoH264;
  } else if (RtpUtility::StringCompare(payload_name, "I420", 4)) {
    video_type = kRtpVideoGeneric;
  } else {
    video_type = kRtpVideoGeneric;
  }
  RtpUtility::Payload* payload = new RtpUtility::Payload();
  payload->name[RTP_PAYLOAD_NAME_SIZE - 1] = 0;
  strncpy(payload->name, payload_name, RTP_PAYLOAD_NAME_SIZE - 1);
  payload->typeSpecific.Video.videoCodecType = video_type;
  payload->audio = false;
  return payload;
}

void RTPSenderVideo::SendVideoPacket(uint8_t* data_buffer,
                                     size_t payload_length,
                                     size_t rtp_header_length,
                                     uint16_t seq_num,
                                     uint32_t rtp_timestamp,
                                     int64_t capture_time_ms,
                                     StorageType storage) {
  if (!rtp_sender_->SendToNetwork(data_buffer, payload_length,
                                  rtp_header_length, capture_time_ms, storage,
                                  RtpPacketSender::kLowPriority)) {
    LOG(LS_WARNING) << "Failed to send video packet " << seq_num;
    return;
  }
  rtc::CritScope cs(&stats_crit_);
  video_bitrate_.Update(payload_length + rtp_header_length,
                        clock_->TimeInMilliseconds());
  TRACE_EVENT_INSTANT2(TRACE_DISABLED_BY_DEFAULT("webrtc_rtp"),
                       "Video::PacketNormal", "timestamp", rtp_timestamp,
                       "seqnum", seq_num);
}

void RTPSenderVideo::SendVideoPacketAsRed(uint8_t* data_buffer,
                                          size_t payload_length,
                                          size_t rtp_header_length,
                                          uint16_t media_seq_num,
                                          uint32_t rtp_timestamp,
                                          int64_t capture_time_ms,
                                          StorageType media_packet_storage,
                                          bool protect) {
  std::unique_ptr<RedPacket> red_packet;
  std::vector<std::unique_ptr<RedPacket>> fec_packets;
  StorageType fec_storage = kDontRetransmit;
  uint16_t next_fec_sequence_number = 0;
  {
    // Only protect while creating RED and FEC packets, not when sending.
    rtc::CritScope cs(&crit_);
    red_packet = ProducerFec::BuildRedPacket(
        data_buffer, payload_length, rtp_header_length, red_payload_type_);
    if (protect) {
      producer_fec_.AddRtpPacketAndGenerateFec(data_buffer, payload_length,
                                               rtp_header_length);
    }
    uint16_t num_fec_packets = producer_fec_.NumAvailableFecPackets();
    if (num_fec_packets > 0) {
      next_fec_sequence_number =
          rtp_sender_->AllocateSequenceNumber(num_fec_packets);
      fec_packets = producer_fec_.GetFecPacketsAsRed(
          red_payload_type_, fec_payload_type_, next_fec_sequence_number,
          rtp_header_length);
      RTC_DCHECK_EQ(num_fec_packets, fec_packets.size());
      if (retransmission_settings_ & kRetransmitFECPackets)
        fec_storage = kAllowRetransmission;
    }
  }
  if (rtp_sender_->SendToNetwork(
          red_packet->data(), red_packet->length() - rtp_header_length,
          rtp_header_length, capture_time_ms, media_packet_storage,
          RtpPacketSender::kLowPriority)) {
    rtc::CritScope cs(&stats_crit_);
    video_bitrate_.Update(red_packet->length(), clock_->TimeInMilliseconds());
    TRACE_EVENT_INSTANT2(TRACE_DISABLED_BY_DEFAULT("webrtc_rtp"),
                         "Video::PacketRed", "timestamp", rtp_timestamp,
                         "seqnum", media_seq_num);
  } else {
    LOG(LS_WARNING) << "Failed to send RED packet " << media_seq_num;
  }
  for (const auto& fec_packet : fec_packets) {
    if (rtp_sender_->SendToNetwork(
            fec_packet->data(), fec_packet->length() - rtp_header_length,
            rtp_header_length, capture_time_ms, fec_storage,
            RtpPacketSender::kLowPriority)) {
      rtc::CritScope cs(&stats_crit_);
      fec_bitrate_.Update(fec_packet->length(), clock_->TimeInMilliseconds());
      TRACE_EVENT_INSTANT2(TRACE_DISABLED_BY_DEFAULT("webrtc_rtp"),
                           "Video::PacketFec", "timestamp", rtp_timestamp,
                           "seqnum", next_fec_sequence_number);
    } else {
      LOG(LS_WARNING) << "Failed to send FEC packet "
                      << next_fec_sequence_number;
    }
    ++next_fec_sequence_number;
  }
}

void RTPSenderVideo::SetGenericFECStatus(bool enable,
                                         uint8_t payload_type_red,
                                         uint8_t payload_type_fec) {
  RTC_DCHECK(!enable || payload_type_red > 0);
  rtc::CritScope cs(&crit_);
  fec_enabled_ = enable;
  red_payload_type_ = payload_type_red;
  fec_payload_type_ = payload_type_fec;
  delta_fec_params_ = FecProtectionParams{0, 1, kFecMaskRandom};
  key_fec_params_ = FecProtectionParams{0, 1, kFecMaskRandom};
}

void RTPSenderVideo::GenericFECStatus(bool* enable,
                                      uint8_t* payload_type_red,
                                      uint8_t* payload_type_fec) const {
  rtc::CritScope cs(&crit_);
  *enable = fec_enabled_;
  *payload_type_red = red_payload_type_;
  *payload_type_fec = fec_payload_type_;
}

size_t RTPSenderVideo::FecPacketOverhead() const {
  rtc::CritScope cs(&crit_);
  size_t overhead = 0;
  if (red_payload_type_ != 0) {
    // Overhead is FEC headers plus RED for FEC header plus anything in RTP
    // header beyond the 12 bytes base header (CSRC list, extensions...)
    // This reason for the header extensions to be included here is that
    // from an FEC viewpoint, they are part of the payload to be protected.
    // (The base RTP header is already protected by the FEC header.)
    return producer_fec_.MaxPacketOverhead() + kRedForFecHeaderLength +
           (rtp_sender_->RtpHeaderLength() - kRtpHeaderSize);
  }
  if (fec_enabled_)
    overhead += producer_fec_.MaxPacketOverhead();
  return overhead;
}

void RTPSenderVideo::SetFecParameters(const FecProtectionParams* delta_params,
                                      const FecProtectionParams* key_params) {
  rtc::CritScope cs(&crit_);
  RTC_DCHECK(delta_params);
  RTC_DCHECK(key_params);
  if (fec_enabled_) {
    delta_fec_params_ = *delta_params;
    key_fec_params_ = *key_params;
  }
}

bool RTPSenderVideo::SendVideo(RtpVideoCodecTypes video_type,
                               FrameType frame_type,
                               int8_t payload_type,
                               uint32_t rtp_timestamp,
                               int64_t capture_time_ms,
                               const uint8_t* payload_data,
                               size_t payload_size,
                               const RTPFragmentationHeader* fragmentation,
                               const RTPVideoHeader* video_header) {
  if (payload_size == 0)
    return false;

  std::unique_ptr<RtpPacketizer> packetizer(RtpPacketizer::Create(
      video_type, rtp_sender_->MaxDataPayloadLength(),
      video_header ? &(video_header->codecHeader) : nullptr, frame_type));

  StorageType storage;
  int red_payload_type;
  bool first_frame = first_frame_sent_();
  {
    rtc::CritScope cs(&crit_);
    FecProtectionParams* fec_params =
        frame_type == kVideoFrameKey ? &key_fec_params_ : &delta_fec_params_;
    // We currently do not use unequal protection in the FEC.
    // This is signalled both here (by setting the number of important
    // packets to zero), as well as in ProducerFec::AddRtpPacketAndGenerateFec.
    constexpr int kNumImportantPackets = 0;
    producer_fec_.SetFecParameters(fec_params, kNumImportantPackets);
    storage = packetizer->GetStorageType(retransmission_settings_);
    red_payload_type = red_payload_type_;
  }

  // Register CVO rtp header extension at the first time when we receive a frame
  // with pending rotation.
  bool video_rotation_active = false;
  if (video_header && video_header->rotation != kVideoRotation_0) {
    video_rotation_active = rtp_sender_->ActivateCVORtpHeaderExtension();
  }

  int rtp_header_length = rtp_sender_->RtpHeaderLength();
  size_t payload_bytes_to_send = payload_size;
  const uint8_t* data = payload_data;

  // TODO(changbin): we currently don't support to configure the codec to
  // output multiple partitions for VP8. Should remove below check after the
  // issue is fixed.
  const RTPFragmentationHeader* frag =
      (video_type == kRtpVideoVp8) ? NULL : fragmentation;

  packetizer->SetPayloadData(data, payload_bytes_to_send, frag);

  bool first = true;
  bool last = false;
  while (!last) {
    uint8_t dataBuffer[IP_PACKET_SIZE] = {0};
    size_t payload_bytes_in_packet = 0;

    if (!packetizer->NextPacket(&dataBuffer[rtp_header_length],
                                &payload_bytes_in_packet, &last)) {
      return false;
    }

    // Write RTP header.
    int32_t header_length = rtp_sender_->BuildRtpHeader(
        dataBuffer, payload_type, last, rtp_timestamp, capture_time_ms);
    if (header_length <= 0)
      return false;

    // According to
    // http://www.etsi.org/deliver/etsi_ts/126100_126199/126114/12.07.00_60/
    // ts_126114v120700p.pdf Section 7.4.5:
    // The MTSI client shall add the payload bytes as defined in this clause
    // onto the last RTP packet in each group of packets which make up a key
    // frame (I-frame or IDR frame in H.264 (AVC), or an IRAP picture in H.265
    // (HEVC)). The MTSI client may also add the payload bytes onto the last RTP
    // packet in each group of packets which make up another type of frame
    // (e.g. a P-Frame) only if the current value is different from the previous
    // value sent.
    // Here we are adding it to every packet of every frame at this point.
    if (!video_header) {
      RTC_DCHECK(!rtp_sender_->IsRtpHeaderExtensionRegistered(
          kRtpExtensionVideoRotation));
    } else if (video_rotation_active) {
      // Checking whether CVO header extension is registered will require taking
      // a lock. It'll be a no-op if it's not registered.
      // TODO(guoweis): For now, all packets sent will carry the CVO such that
      // the RTP header length is consistent, although the receiver side will
      // only exam the packets with marker bit set.
      size_t packetSize = payload_size + rtp_header_length;
      RtpUtility::RtpHeaderParser rtp_parser(dataBuffer, packetSize);
      RTPHeader rtp_header;
      rtp_parser.Parse(&rtp_header);
      rtp_sender_->UpdateVideoRotation(dataBuffer, packetSize, rtp_header,
                                       video_header->rotation);
    }
    if (red_payload_type != 0) {
      SendVideoPacketAsRed(dataBuffer, payload_bytes_in_packet,
                           rtp_header_length, rtp_sender_->SequenceNumber(),
                           rtp_timestamp, capture_time_ms, storage,
                           packetizer->GetProtectionType() == kProtectedPacket);
    } else {
      SendVideoPacket(dataBuffer, payload_bytes_in_packet, rtp_header_length,
                      rtp_sender_->SequenceNumber(), rtp_timestamp,
                      capture_time_ms, storage);
    }

    if (first_frame) {
      if (first) {
        LOG(LS_INFO)
            << "Sent first RTP packet of the first video frame (pre-pacer)";
      }
      if (last) {
        LOG(LS_INFO)
            << "Sent last RTP packet of the first video frame (pre-pacer)";
      }
    }
    first = false;
  }

  TRACE_EVENT_ASYNC_END1("webrtc", "Video", capture_time_ms, "timestamp",
                         rtp_timestamp);
  return true;
}

uint32_t RTPSenderVideo::VideoBitrateSent() const {
  rtc::CritScope cs(&stats_crit_);
  return video_bitrate_.Rate(clock_->TimeInMilliseconds()).value_or(0);
}

uint32_t RTPSenderVideo::FecOverheadRate() const {
  rtc::CritScope cs(&stats_crit_);
  return fec_bitrate_.Rate(clock_->TimeInMilliseconds()).value_or(0);
}

int RTPSenderVideo::SelectiveRetransmissions() const {
  rtc::CritScope cs(&crit_);
  return retransmission_settings_;
}

void RTPSenderVideo::SetSelectiveRetransmissions(uint8_t settings) {
  rtc::CritScope cs(&crit_);
  retransmission_settings_ = settings;
}

}  // namespace webrtc
