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
class RTPReceiver;
class RTPPayloadRegistry;

class RTPReceiverVideo : public RTPReceiverStrategy {
 public:
  RTPReceiverVideo(const WebRtc_Word32 id,
                   const RTPPayloadRegistry* rtp_payload_registry,
                   RtpData* data_callback);

  virtual ~RTPReceiverVideo();

  WebRtc_Word32 ParseRtpPacket(
      WebRtcRTPHeader* rtp_header,
      const ModuleRTPUtility::PayloadUnion& specific_payload,
      const bool is_red,
      const WebRtc_UWord8* packet,
      const WebRtc_UWord16 packet_length,
      const WebRtc_Word64 timestamp,
      const bool is_first_packet);

  WebRtc_Word32 GetFrequencyHz() const;

  RTPAliveType ProcessDeadOrAlive(WebRtc_UWord16 last_payload_length) const;

  bool ShouldReportCsrcChanges(WebRtc_UWord8 payload_type) const;

  WebRtc_Word32 OnNewPayloadTypeCreated(
      const char payload_name[RTP_PAYLOAD_NAME_SIZE],
      const WebRtc_Word8 payload_type,
      const WebRtc_UWord32 frequency);

  WebRtc_Word32 InvokeOnInitializeDecoder(
      RtpFeedback* callback,
      const WebRtc_Word32 id,
      const WebRtc_Word8 payload_type,
      const char payload_name[RTP_PAYLOAD_NAME_SIZE],
      const ModuleRTPUtility::PayloadUnion& specific_payload) const;

  virtual WebRtc_Word32 ReceiveRecoveredPacketCallback(
      WebRtcRTPHeader* rtp_header,
      const WebRtc_UWord8* payload_data,
      const WebRtc_UWord16 payload_data_length);

  void SetPacketOverHead(WebRtc_UWord16 packet_over_head);

 protected:
  WebRtc_Word32 SetCodecType(const RtpVideoCodecTypes video_type,
                             WebRtcRTPHeader* rtp_header) const;

  WebRtc_Word32 ParseVideoCodecSpecificSwitch(
      WebRtcRTPHeader* rtp_header,
      const WebRtc_UWord8* payload_data,
      const WebRtc_UWord16 payload_data_length,
      const RtpVideoCodecTypes video_type,
      const bool is_first_packet);

  WebRtc_Word32 ReceiveGenericCodec(WebRtcRTPHeader* rtp_header,
                                    const WebRtc_UWord8* payload_data,
                                    const WebRtc_UWord16 payload_data_length);

  WebRtc_Word32 ReceiveVp8Codec(WebRtcRTPHeader* rtp_header,
                                const WebRtc_UWord8* payload_data,
                                const WebRtc_UWord16 payload_data_length);

  WebRtc_Word32 BuildRTPheader(const WebRtcRTPHeader* rtp_header,
                               WebRtc_UWord8* data_buffer) const;

 private:
  WebRtc_Word32 ParseVideoCodecSpecific(
      WebRtcRTPHeader* rtp_header,
      const WebRtc_UWord8* payload_data,
      const WebRtc_UWord16 payload_data_length,
      const RtpVideoCodecTypes video_type,
      const bool is_red,
      const WebRtc_UWord8* incoming_rtp_packet,
      const WebRtc_UWord16 incoming_rtp_packet_size,
      const WebRtc_Word64 now_ms,
      const bool is_first_packet);

  WebRtc_Word32 id_;
  const RTPPayloadRegistry* rtp_rtp_payload_registry_;

  CriticalSectionWrapper* critical_section_receiver_video_;

  // FEC
  bool current_fec_frame_decoded_;
  ReceiverFEC* receive_fec_;
};
}  // namespace webrtc

#endif  // WEBRTC_MODULES_RTP_RTCP_SOURCE_RTP_RECEIVER_VIDEO_H_
