/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MODULES_RTP_RTCP_SOURCE_RTP_RECEIVER_VIDEO_H_
#define WEBRTC_MODULES_RTP_RTCP_SOURCE_RTP_RECEIVER_VIDEO_H_

#include "webrtc/modules/rtp_rtcp/interface/rtp_rtcp_defines.h"
#include "webrtc/modules/rtp_rtcp/source/bitrate.h"
#include "webrtc/modules/rtp_rtcp/source/rtp_receiver_strategy.h"
#include "webrtc/modules/rtp_rtcp/source/rtp_utility.h"
#include "webrtc/system_wrappers/interface/scoped_ptr.h"
#include "webrtc/typedefs.h"

namespace webrtc {
class CriticalSectionWrapper;
class ModuleRtpRtcpImpl;
class ReceiverFEC;
class RtpReceiver;

class RTPReceiverVideo : public RTPReceiverStrategy {
 public:
  RTPReceiverVideo(const int32_t id, RtpData* data_callback);

  virtual ~RTPReceiverVideo();

  int32_t ParseRtpPacket(
      WebRtcRTPHeader* rtp_header,
      const PayloadUnion& specific_payload,
      bool is_red,
      const uint8_t* packet,
      uint16_t packet_length,
      int64_t timestamp,
      bool is_first_packet);

  TelephoneEventHandler* GetTelephoneEventHandler() {
    return NULL;
  }

  int GetPayloadTypeFrequency() const;

  RTPAliveType ProcessDeadOrAlive(uint16_t last_payload_length) const;

  bool ShouldReportCsrcChanges(uint8_t payload_type) const;

  int32_t OnNewPayloadTypeCreated(
      const char payload_name[RTP_PAYLOAD_NAME_SIZE],
      int8_t payload_type,
      uint32_t frequency);

  int32_t InvokeOnInitializeDecoder(
      RtpFeedback* callback,
      int32_t id,
      int8_t payload_type,
      const char payload_name[RTP_PAYLOAD_NAME_SIZE],
      const PayloadUnion& specific_payload) const;

  void SetPacketOverHead(uint16_t packet_over_head);

 protected:
  int32_t ParseVideoCodecSpecificSwitch(
      WebRtcRTPHeader* rtp_header,
      const uint8_t* payload_data,
      uint16_t payload_data_length,
      bool is_first_packet);

  int32_t ReceiveGenericCodec(WebRtcRTPHeader* rtp_header,
                              const uint8_t* payload_data,
                              uint16_t payload_data_length);

  int32_t ReceiveVp8Codec(WebRtcRTPHeader* rtp_header,
                          const uint8_t* payload_data,
                          uint16_t payload_data_length);

  int32_t BuildRTPheader(const WebRtcRTPHeader* rtp_header,
                         uint8_t* data_buffer) const;

 private:
  int32_t ParseVideoCodecSpecific(
      WebRtcRTPHeader* rtp_header,
      const uint8_t* payload_data,
      uint16_t payload_data_length,
      RtpVideoCodecTypes video_type,
      bool is_red,
      const uint8_t* incoming_rtp_packet,
      uint16_t incoming_rtp_packet_size,
      int64_t now_ms,
      bool is_first_packet);

  int32_t id_;

  // FEC
  ReceiverFEC* receive_fec_;
};
}  // namespace webrtc

#endif  // WEBRTC_MODULES_RTP_RTCP_SOURCE_RTP_RECEIVER_VIDEO_H_
