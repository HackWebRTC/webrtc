/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/modules/rtp_rtcp/source/rtp_receiver_video.h"

#include <math.h>

#include <assert.h>  // assert
#include <string.h>  // memcpy()

#include "webrtc/modules/rtp_rtcp/interface/rtp_payload_registry.h"
#include "webrtc/modules/rtp_rtcp/source/receiver_fec.h"
#include "webrtc/modules/rtp_rtcp/source/rtp_format_video_generic.h"
#include "webrtc/modules/rtp_rtcp/source/rtp_rtcp_impl.h"
#include "webrtc/modules/rtp_rtcp/source/rtp_utility.h"
#include "webrtc/system_wrappers/interface/critical_section_wrapper.h"
#include "webrtc/system_wrappers/interface/trace.h"
#include "webrtc/system_wrappers/interface/trace_event.h"

namespace webrtc {
uint32_t BitRateBPS(uint16_t x) {
  return (x & 0x3fff) * uint32_t(pow(10.0f, (2 + (x >> 14))));
}

RTPReceiverStrategy* RTPReceiverStrategy::CreateVideoStrategy(
    int32_t id, RtpData* data_callback) {
  return new RTPReceiverVideo(id, data_callback);
}

RTPReceiverVideo::RTPReceiverVideo(int32_t id, RtpData* data_callback)
    : RTPReceiverStrategy(data_callback),
      id_(id),
      receive_fec_(NULL) {
}

RTPReceiverVideo::~RTPReceiverVideo() {
  delete receive_fec_;
}

bool RTPReceiverVideo::ShouldReportCsrcChanges(
    uint8_t payload_type) const {
  // Always do this for video packets.
  return true;
}

int32_t RTPReceiverVideo::OnNewPayloadTypeCreated(
    const char payload_name[RTP_PAYLOAD_NAME_SIZE],
    int8_t payload_type,
    uint32_t frequency) {
  if (ModuleRTPUtility::StringCompare(payload_name, "ULPFEC", 6)) {
    // Enable FEC if not enabled.
    if (receive_fec_ == NULL) {
      receive_fec_ = new ReceiverFEC(id_, data_callback_);
    }
    receive_fec_->SetPayloadTypeFEC(payload_type);
  }
  return 0;
}

int32_t RTPReceiverVideo::ParseRtpPacket(
    WebRtcRTPHeader* rtp_header,
    const PayloadUnion& specific_payload,
    bool is_red,
    const uint8_t* packet,
    uint16_t packet_length,
    int64_t timestamp_ms,
    bool is_first_packet) {
  TRACE_EVENT2("webrtc_rtp", "Video::ParseRtp",
               "seqnum", rtp_header->header.sequenceNumber,
               "timestamp", rtp_header->header.timestamp);
  rtp_header->type.Video.codec = specific_payload.Video.videoCodecType;
  const uint8_t* payload_data =
      ModuleRTPUtility::GetPayloadData(rtp_header->header, packet);
  const uint16_t payload_data_length =
      ModuleRTPUtility::GetPayloadDataLength(rtp_header->header, packet_length);
  return ParseVideoCodecSpecific(rtp_header,
                                 payload_data,
                                 payload_data_length,
                                 specific_payload.Video.videoCodecType,
                                 is_red,
                                 packet,
                                 packet_length,
                                 timestamp_ms,
                                 is_first_packet);
}

int RTPReceiverVideo::GetPayloadTypeFrequency() const {
  return kVideoPayloadTypeFrequency;
}

RTPAliveType RTPReceiverVideo::ProcessDeadOrAlive(
    uint16_t last_payload_length) const {
  return kRtpDead;
}

int32_t RTPReceiverVideo::InvokeOnInitializeDecoder(
    RtpFeedback* callback,
    int32_t id,
    int8_t payload_type,
    const char payload_name[RTP_PAYLOAD_NAME_SIZE],
    const PayloadUnion& specific_payload) const {
  // For video we just go with default values.
  if (-1 == callback->OnInitializeDecoder(
      id, payload_type, payload_name, kVideoPayloadTypeFrequency, 1, 0)) {
    WEBRTC_TRACE(kTraceError,
                 kTraceRtpRtcp,
                 id,
                 "Failed to create video decoder for payload type:%d",
                 payload_type);
    return -1;
  }
  return 0;
}

// we have no critext when calling this
// we are not allowed to have any critsects when calling
// CallbackOfReceivedPayloadData
int32_t RTPReceiverVideo::ParseVideoCodecSpecific(
    WebRtcRTPHeader* rtp_header,
    const uint8_t* payload_data,
    uint16_t payload_data_length,
    RtpVideoCodecTypes video_type,
    bool is_red,
    const uint8_t* incoming_rtp_packet,
    uint16_t incoming_rtp_packet_size,
    int64_t now_ms,
    bool is_first_packet) {
  int32_t ret_val = 0;

  crit_sect_->Enter();

  if (is_red) {
    if (receive_fec_ == NULL) {
      crit_sect_->Leave();
      return -1;
    }
    crit_sect_->Leave();
    bool FECpacket = false;
    ret_val = receive_fec_->AddReceivedFECPacket(
        rtp_header, incoming_rtp_packet, payload_data_length, FECpacket);
    if (ret_val != -1) {
      ret_val = receive_fec_->ProcessReceivedFEC();
    }

    if (ret_val == 0 && FECpacket) {
      // Callback with the received FEC packet.
      // The normal packets are delivered after parsing.
      // This contains the original RTP packet header but with
      // empty payload and data length.
      rtp_header->frameType = kFrameEmpty;
      // We need this for the routing.
      rtp_header->type.Video.codec = video_type;
      // Pass the length of FEC packets so that they can be accounted for in
      // the bandwidth estimator.
      ret_val = data_callback_->OnReceivedPayloadData(
          NULL, payload_data_length, rtp_header);
    }
  } else {
    // will leave the crit_sect_ critsect
    ret_val = ParseVideoCodecSpecificSwitch(rtp_header,
                                            payload_data,
                                            payload_data_length,
                                            is_first_packet);
  }
  return ret_val;
}

int32_t RTPReceiverVideo::BuildRTPheader(
    const WebRtcRTPHeader* rtp_header,
    uint8_t* data_buffer) const {
  data_buffer[0] = static_cast<uint8_t>(0x80);  // version 2
  data_buffer[1] = static_cast<uint8_t>(rtp_header->header.payloadType);
  if (rtp_header->header.markerBit) {
    data_buffer[1] |= kRtpMarkerBitMask;  // MarkerBit is 1
  }
  ModuleRTPUtility::AssignUWord16ToBuffer(data_buffer + 2,
                                          rtp_header->header.sequenceNumber);
  ModuleRTPUtility::AssignUWord32ToBuffer(data_buffer + 4,
                                          rtp_header->header.timestamp);
  ModuleRTPUtility::AssignUWord32ToBuffer(data_buffer + 8,
                                          rtp_header->header.ssrc);

  int32_t rtp_header_length = 12;

  // Add the CSRCs if any
  if (rtp_header->header.numCSRCs > 0) {
    if (rtp_header->header.numCSRCs > 16) {
      // error
      assert(false);
    }
    uint8_t* ptr = &data_buffer[rtp_header_length];
    for (uint32_t i = 0; i < rtp_header->header.numCSRCs; ++i) {
      ModuleRTPUtility::AssignUWord32ToBuffer(ptr,
                                              rtp_header->header.arrOfCSRCs[i]);
      ptr += 4;
    }
    data_buffer[0] = (data_buffer[0] & 0xf0) | rtp_header->header.numCSRCs;
    // Update length of header
    rtp_header_length += sizeof(uint32_t) * rtp_header->header.numCSRCs;
  }
  return rtp_header_length;
}

int32_t RTPReceiverVideo::ParseVideoCodecSpecificSwitch(
    WebRtcRTPHeader* rtp_header,
    const uint8_t* payload_data,
    uint16_t payload_data_length,
    bool is_first_packet) {
  WEBRTC_TRACE(kTraceStream,
               kTraceRtpRtcp,
               id_,
               "%s(timestamp:%u)",
               __FUNCTION__,
               rtp_header->header.timestamp);

  // Critical section has already been taken.
  switch (rtp_header->type.Video.codec) {
    case kRtpVideoGeneric:
      rtp_header->type.Video.isFirstPacket = is_first_packet;
      return ReceiveGenericCodec(rtp_header, payload_data, payload_data_length);
    case kRtpVideoVp8:
      return ReceiveVp8Codec(rtp_header, payload_data, payload_data_length);
    case kRtpVideoFec:
      break;
    default:
      assert(false);
  }
  // Releasing the already taken critical section here.
  crit_sect_->Leave();
  return -1;
}

int32_t RTPReceiverVideo::ReceiveVp8Codec(WebRtcRTPHeader* rtp_header,
                                          const uint8_t* payload_data,
                                          uint16_t payload_data_length) {
  bool success;
  ModuleRTPUtility::RTPPayload parsed_packet;
  if (payload_data_length == 0) {
    success = true;
    parsed_packet.info.VP8.dataLength = 0;
  } else {
    ModuleRTPUtility::RTPPayloadParser rtp_payload_parser(
        kRtpVideoVp8, payload_data, payload_data_length, id_);

    success = rtp_payload_parser.Parse(parsed_packet);
  }
  // from here down we only work on local data
  crit_sect_->Leave();

  if (!success) {
    return -1;
  }
  if (parsed_packet.info.VP8.dataLength == 0) {
    // we have an "empty" VP8 packet, it's ok, could be one way video
    // Inform the jitter buffer about this packet.
    rtp_header->frameType = kFrameEmpty;
    if (data_callback_->OnReceivedPayloadData(NULL, 0, rtp_header) != 0) {
      return -1;
    }
    return 0;
  }
  rtp_header->frameType = (parsed_packet.frameType == ModuleRTPUtility::kIFrame)
      ? kVideoFrameKey : kVideoFrameDelta;

  RTPVideoHeaderVP8* to_header = &rtp_header->type.Video.codecHeader.VP8;
  ModuleRTPUtility::RTPPayloadVP8* from_header = &parsed_packet.info.VP8;

  rtp_header->type.Video.isFirstPacket =
      from_header->beginningOfPartition && (from_header->partitionID == 0);
  to_header->nonReference = from_header->nonReferenceFrame;
  to_header->pictureId =
      from_header->hasPictureID ? from_header->pictureID : kNoPictureId;
  to_header->tl0PicIdx =
      from_header->hasTl0PicIdx ? from_header->tl0PicIdx : kNoTl0PicIdx;
  if (from_header->hasTID) {
    to_header->temporalIdx = from_header->tID;
    to_header->layerSync = from_header->layerSync;
  } else {
    to_header->temporalIdx = kNoTemporalIdx;
    to_header->layerSync = false;
  }
  to_header->keyIdx = from_header->hasKeyIdx ? from_header->keyIdx : kNoKeyIdx;

  rtp_header->type.Video.width = from_header->frameWidth;
  rtp_header->type.Video.height = from_header->frameHeight;

  to_header->partitionId = from_header->partitionID;
  to_header->beginningOfPartition = from_header->beginningOfPartition;

  if (data_callback_->OnReceivedPayloadData(parsed_packet.info.VP8.data,
                                            parsed_packet.info.VP8.dataLength,
                                            rtp_header) != 0) {
    return -1;
  }
  return 0;
}

int32_t RTPReceiverVideo::ReceiveGenericCodec(
    WebRtcRTPHeader* rtp_header,
    const uint8_t* payload_data,
    uint16_t payload_data_length) {
  uint8_t generic_header = *payload_data++;
  --payload_data_length;

  rtp_header->frameType =
      ((generic_header & RtpFormatVideoGeneric::kKeyFrameBit) != 0) ?
      kVideoFrameKey : kVideoFrameDelta;
  rtp_header->type.Video.isFirstPacket =
      (generic_header & RtpFormatVideoGeneric::kFirstPacketBit) != 0;

  crit_sect_->Leave();

  if (data_callback_->OnReceivedPayloadData(
          payload_data, payload_data_length, rtp_header) != 0) {
    return -1;
  }
  return 0;
}
}  // namespace webrtc
