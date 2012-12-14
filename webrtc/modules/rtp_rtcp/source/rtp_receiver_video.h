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

#include "rtp_rtcp_defines.h"
#include "rtp_utility.h"

#include "typedefs.h"

#include "Bitrate.h"
#include "scoped_ptr.h"

namespace webrtc {
class CriticalSectionWrapper;
class ModuleRtpRtcpImpl;
class ReceiverFEC;
class RTPReceiver;

class RTPReceiverVideo {
 public:
  RTPReceiverVideo(const WebRtc_Word32 id,
                   RTPReceiver* parent,
                   ModuleRtpRtcpImpl* owner);

  virtual ~RTPReceiverVideo();

  ModuleRTPUtility::Payload* RegisterReceiveVideoPayload(
      const char payloadName[RTP_PAYLOAD_NAME_SIZE],
      const WebRtc_Word8 payloadType,
      const WebRtc_UWord32 maxRate);

  WebRtc_Word32 ParseVideoCodecSpecific(
      WebRtcRTPHeader* rtpHeader,
      const WebRtc_UWord8* payloadData,
      const WebRtc_UWord16 payloadDataLength,
      const RtpVideoCodecTypes videoType,
      const bool isRED,
      const WebRtc_UWord8* incomingRtpPacket,
      const WebRtc_UWord16 incomingRtpPacketSize,
      const WebRtc_Word64 nowMS);

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
      const RtpVideoCodecTypes videoType);

  WebRtc_Word32 ReceiveGenericCodec(WebRtcRTPHeader *rtpHeader,
                                    const WebRtc_UWord8* payloadData,
                                    const WebRtc_UWord16 payloadDataLength);

  WebRtc_Word32 ReceiveVp8Codec(WebRtcRTPHeader *rtpHeader,
                                const WebRtc_UWord8* payloadData,
                                const WebRtc_UWord16 payloadDataLength);

  WebRtc_Word32 BuildRTPheader(const WebRtcRTPHeader* rtpHeader,
                               WebRtc_UWord8* dataBuffer) const;

 private:
  WebRtc_Word32             _id;
  RTPReceiver*              _parent;

  CriticalSectionWrapper*   _criticalSectionReceiverVideo;

  // FEC
  bool                      _currentFecFrameDecoded;
  ReceiverFEC*              _receiveFEC;
};
} // namespace webrtc
#endif // WEBRTC_MODULES_RTP_RTCP_SOURCE_RTP_RECEIVER_VIDEO_H_
