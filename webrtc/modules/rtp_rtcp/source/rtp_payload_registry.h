/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MODULES_RTP_RTCP_SOURCE_RTP_PAYLOAD_REGISTRY_H_
#define WEBRTC_MODULES_RTP_RTCP_SOURCE_RTP_PAYLOAD_REGISTRY_H_

#include "webrtc/modules/rtp_rtcp/source/rtp_receiver_strategy.h"
#include "webrtc/modules/rtp_rtcp/source/rtp_utility.h"

namespace webrtc {

class RTPPayloadRegistry {
 public:
  explicit RTPPayloadRegistry(const WebRtc_Word32 id);
  ~RTPPayloadRegistry();

  // Must be called before any other methods are used!
  // TODO(phoglund): We shouldn't really have to talk to a media receiver here.
  // It would make more sense to talk to some media-specific payload handling
  // strategy. Can't do that right now because audio payload type handling is
  // too tightly coupled with packet parsing.
  void set_rtp_media_receiver(RTPReceiverStrategy* rtp_media_receiver) {
    rtp_media_receiver_ = rtp_media_receiver;
  }

  WebRtc_Word32 RegisterReceivePayload(
      const char payload_name[RTP_PAYLOAD_NAME_SIZE],
      const WebRtc_Word8 payload_type,
      const WebRtc_UWord32 frequency,
      const WebRtc_UWord8 channels,
      const WebRtc_UWord32 rate);

  WebRtc_Word32 DeRegisterReceivePayload(
      const WebRtc_Word8 payload_type);

  WebRtc_Word32 ReceivePayloadType(
      const char payload_name[RTP_PAYLOAD_NAME_SIZE],
      const WebRtc_UWord32 frequency,
      const WebRtc_UWord8 channels,
      const WebRtc_UWord32 rate,
      WebRtc_Word8* payload_type) const;

  WebRtc_Word32 ReceivePayload(
      const WebRtc_Word8 payload_type,
      char payload_name[RTP_PAYLOAD_NAME_SIZE],
      WebRtc_UWord32* frequency,
      WebRtc_UWord8* channels,
      WebRtc_UWord32* rate) const;

  WebRtc_UWord32 PayloadTypeToPayload(
    const WebRtc_UWord8 payload_type,
    ModuleRTPUtility::Payload*& payload) const;

  void ResetLastReceivedPayloadTypes() {
    last_received_payload_type_ = -1;
    last_received_media_payload_type_ = -1;
  }

  // Returns true if the new media payload type has not changed.
  bool ReportMediaPayloadType(WebRtc_UWord8 media_payload_type);

  WebRtc_Word8 red_payload_type() const { return red_payload_type_; }
  WebRtc_Word8 last_received_payload_type() const {
    return last_received_payload_type_;
  }
  void set_last_received_payload_type(WebRtc_Word8 last_received_payload_type) {
    last_received_payload_type_ = last_received_payload_type;
  }

 private:
  ModuleRTPUtility::PayloadTypeMap payload_type_map_;
  WebRtc_Word32 id_;
  RTPReceiverStrategy* rtp_media_receiver_;
  WebRtc_Word8  red_payload_type_;
  WebRtc_Word8  last_received_payload_type_;
  WebRtc_Word8  last_received_media_payload_type_;
};

}  // namespace webrtc

#endif  // WEBRTC_MODULES_RTP_RTCP_SOURCE_RTP_PAYLOAD_REGISTRY_H_
