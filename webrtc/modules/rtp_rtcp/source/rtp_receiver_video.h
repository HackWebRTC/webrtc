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

#include "bitrate.h"
#include "rtp_receiver_strategy.h"
#include "rtp_rtcp_defines.h"
#include "rtp_utility.h"
#include "scoped_ptr.h"
#include "typedefs.h"

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
      const ModuleRTPUtility::PayloadUnion& specificPayload,
      const bool is_red,
      const WebRtc_UWord8* packet,
      const WebRtc_UWord16 packet_length,
      const WebRtc_Word64 timestamp,
      const bool is_first_packet);

  WebRtc_Word32 GetFrequencyHz() const;

  RTPAliveType ProcessDeadOrAlive(WebRtc_UWord16 lastPayloadLength) const;

  bool ShouldReportCsrcChanges(WebRtc_UWord8 payload_type) const;

  WebRtc_Word32 OnNewPayloadTypeCreated(
      const char payloadName[RTP_PAYLOAD_NAME_SIZE],
      const WebRtc_Word8 payloadType,
      const WebRtc_UWord32 frequency);

  WebRtc_Word32 InvokeOnInitializeDecoder(
      RtpFeedback* callback,
      const WebRtc_Word32 id,
      const WebRtc_Word8 payloadType,
      const char payloadName[RTP_PAYLOAD_NAME_SIZE],
      const ModuleRTPUtility::PayloadUnion& specificPayload) const;

  virtual WebRtc_Word32 ReceiveRecoveredPacketCallback(
      WebRtcRTPHeader* rtpHeader,
      const WebRtc_UWord8* payloadData,
      const WebRtc_UWord16 payloadDataLength);

  void SetPacketOverHead(WebRtc_UWord16 packetOverHead);

 protected:
  WebRtc_Word32 SetCodecType(const RtpVideoCodecTypes videoType,
                             WebRtcRTPHeader* rtpHeader) const;

  WebRtc_Word32 ParseVideoCodecSpecificSwitch(
      WebRtcRTPHeader* rtpHeader,
      const WebRtc_UWord8* payloadData,
      const WebRtc_UWord16 payloadDataLength,
      const RtpVideoCodecTypes videoType,
      const bool isFirstPacket);

  WebRtc_Word32 ReceiveGenericCodec(WebRtcRTPHeader *rtpHeader,
                                    const WebRtc_UWord8* payloadData,
                                    const WebRtc_UWord16 payloadDataLength);

  WebRtc_Word32 ReceiveVp8Codec(WebRtcRTPHeader *rtpHeader,
                                const WebRtc_UWord8* payloadData,
                                const WebRtc_UWord16 payloadDataLength);

  WebRtc_Word32 BuildRTPheader(const WebRtcRTPHeader* rtpHeader,
                               WebRtc_UWord8* dataBuffer) const;

 private:
  WebRtc_Word32 ParseVideoCodecSpecific(
      WebRtcRTPHeader* rtpHeader,
      const WebRtc_UWord8* payloadData,
      const WebRtc_UWord16 payloadDataLength,
      const RtpVideoCodecTypes videoType,
      const bool isRED,
      const WebRtc_UWord8* incomingRtpPacket,
      const WebRtc_UWord16 incomingRtpPacketSize,
      const WebRtc_Word64 nowMS,
      const bool isFirstPacket);

  WebRtc_Word32             _id;
  const RTPPayloadRegistry* _rtpRtpPayloadRegistry;

  CriticalSectionWrapper*   _criticalSectionReceiverVideo;

  // FEC
  bool                      _currentFecFrameDecoded;
  ReceiverFEC*              _receiveFEC;
};
} // namespace webrtc
#endif // WEBRTC_MODULES_RTP_RTCP_SOURCE_RTP_RECEIVER_VIDEO_H_
