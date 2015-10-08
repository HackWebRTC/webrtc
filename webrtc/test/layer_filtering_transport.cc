/*
 *  Copyright (c) 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/base/checks.h"
#include "webrtc/modules/rtp_rtcp/interface/rtp_header_parser.h"
#include "webrtc/modules/rtp_rtcp/interface/rtp_rtcp_defines.h"
#include "webrtc/modules/rtp_rtcp/source/byte_io.h"
#include "webrtc/modules/rtp_rtcp/source/rtp_format.h"
#include "webrtc/modules/rtp_rtcp/source/rtp_utility.h"
#include "webrtc/test/layer_filtering_transport.h"

namespace webrtc {
namespace test {

LayerFilteringTransport::LayerFilteringTransport(
    const FakeNetworkPipe::Config& config,
    uint8_t vp8_video_payload_type,
    uint8_t vp9_video_payload_type,
    int selected_tl,
    int selected_sl)
    : test::DirectTransport(config),
      vp8_video_payload_type_(vp8_video_payload_type),
      vp9_video_payload_type_(vp9_video_payload_type),
      selected_tl_(selected_tl),
      selected_sl_(selected_sl),
      discarded_last_packet_(false) {
}

bool LayerFilteringTransport::DiscardedLastPacket() const {
  return discarded_last_packet_;
}

uint16_t LayerFilteringTransport::NextSequenceNumber(uint32_t ssrc) {
  auto it = current_seq_nums_.find(ssrc);
  if (it == current_seq_nums_.end())
    return current_seq_nums_[ssrc] = 10000;
  return ++it->second;
}

bool LayerFilteringTransport::SendRtp(const uint8_t* packet,
                                      size_t length,
                                      const PacketOptions& options) {
  if (selected_tl_ == -1 && selected_sl_ == -1) {
    // Nothing to change, forward the packet immediately.
    return test::DirectTransport::SendRtp(packet, length, options);
  }

  bool set_marker_bit = false;
  rtc::scoped_ptr<RtpHeaderParser> parser(RtpHeaderParser::Create());
  RTPHeader header;
  parser->Parse(packet, length, &header);

  if (header.payloadType == vp8_video_payload_type_ ||
      header.payloadType == vp9_video_payload_type_) {
    const uint8_t* payload = packet + header.headerLength;
    RTC_DCHECK_GT(length, header.headerLength);
    const size_t payload_length = length - header.headerLength;
    RTC_DCHECK_GT(payload_length, header.paddingLength);
    const size_t payload_data_length = payload_length - header.paddingLength;

    const bool is_vp8 = header.payloadType == vp8_video_payload_type_;
    rtc::scoped_ptr<RtpDepacketizer> depacketizer(
        RtpDepacketizer::Create(is_vp8 ? kRtpVideoVp8 : kRtpVideoVp9));
    RtpDepacketizer::ParsedPayload parsed_payload;
    if (depacketizer->Parse(&parsed_payload, payload, payload_data_length)) {
      const int temporal_idx = static_cast<int>(
          is_vp8 ? parsed_payload.type.Video.codecHeader.VP8.temporalIdx
                 : parsed_payload.type.Video.codecHeader.VP9.temporal_idx);
      const int spatial_idx = static_cast<int>(
          is_vp8 ? kNoSpatialIdx
                 : parsed_payload.type.Video.codecHeader.VP9.spatial_idx);
      if (selected_sl_ >= 0 &&
          spatial_idx == selected_sl_ &&
          parsed_payload.type.Video.codecHeader.VP9.end_of_frame) {
        // This layer is now the last in the superframe.
        set_marker_bit = true;
      }
      if ((selected_tl_ >= 0 && temporal_idx != kNoTemporalIdx &&
           temporal_idx > selected_tl_) ||
          (selected_sl_ >= 0 && spatial_idx != kNoSpatialIdx &&
           spatial_idx > selected_sl_)) {
        discarded_last_packet_ = true;
        return true;
      }
    } else {
      RTC_NOTREACHED() << "Parse error";
    }
  }

  uint8_t temp_buffer[IP_PACKET_SIZE];
  memcpy(temp_buffer, packet, length);

  // We are discarding some of the packets (specifically, whole layers), so
  // make sure the marker bit is set properly, and that sequence numbers are
  // continuous.
  if (set_marker_bit)
    temp_buffer[1] |= kRtpMarkerBitMask;

  uint16_t seq_num = NextSequenceNumber(header.ssrc);
  ByteWriter<uint16_t>::WriteBigEndian(&temp_buffer[2], seq_num);
  return test::DirectTransport::SendRtp(temp_buffer, length, options);
}

}  // namespace test
}  // namespace webrtc
