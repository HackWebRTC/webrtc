/*
 *  Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
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
#include "webrtc/system_wrappers/interface/scoped_ptr.h"

namespace webrtc {

// This strategy deals with the audio/video-specific aspects
// of payload handling.
class RTPPayloadStrategy {
 public:
  virtual ~RTPPayloadStrategy() {};

  virtual bool CodecsMustBeUnique() const = 0;

  virtual bool PayloadIsCompatible(
      const ModuleRTPUtility::Payload& payload,
      const WebRtc_UWord32 frequency,
      const WebRtc_UWord8 channels,
      const WebRtc_UWord32 rate) const = 0;

  virtual void UpdatePayloadRate(
      ModuleRTPUtility::Payload* payload,
      const WebRtc_UWord32 rate) const = 0;

  virtual ModuleRTPUtility::Payload* CreatePayloadType(
      const char payloadName[RTP_PAYLOAD_NAME_SIZE],
      const WebRtc_Word8 payloadType,
      const WebRtc_UWord32 frequency,
      const WebRtc_UWord8 channels,
      const WebRtc_UWord32 rate) const = 0;

  static RTPPayloadStrategy* CreateStrategy(const bool handling_audio);

 protected:
   RTPPayloadStrategy() {};
};

class RTPPayloadRegistry {
 public:
  // The registry takes ownership of the strategy.
  RTPPayloadRegistry(const WebRtc_Word32 id,
                     RTPPayloadStrategy* rtp_payload_strategy);
  ~RTPPayloadRegistry();

  WebRtc_Word32 RegisterReceivePayload(
      const char payload_name[RTP_PAYLOAD_NAME_SIZE],
      const WebRtc_Word8 payload_type,
      const WebRtc_UWord32 frequency,
      const WebRtc_UWord8 channels,
      const WebRtc_UWord32 rate,
      bool* created_new_payload_type);

  WebRtc_Word32 DeRegisterReceivePayload(
      const WebRtc_Word8 payload_type);

  WebRtc_Word32 ReceivePayloadType(
      const char payload_name[RTP_PAYLOAD_NAME_SIZE],
      const WebRtc_UWord32 frequency,
      const WebRtc_UWord8 channels,
      const WebRtc_UWord32 rate,
      WebRtc_Word8* payload_type) const;

  WebRtc_Word32 PayloadTypeToPayload(
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
  // Prunes the payload type map of the specific payload type, if it exists.
  void DeregisterAudioCodecOrRedTypeRegardlessOfPayloadType(
      const char payload_name[RTP_PAYLOAD_NAME_SIZE],
      const size_t payload_name_length,
      const WebRtc_UWord32 frequency,
      const WebRtc_UWord8 channels,
      const WebRtc_UWord32 rate);

  ModuleRTPUtility::PayloadTypeMap payload_type_map_;
  WebRtc_Word32 id_;
  scoped_ptr<RTPPayloadStrategy> rtp_payload_strategy_;
  WebRtc_Word8  red_payload_type_;
  WebRtc_Word8  last_received_payload_type_;
  WebRtc_Word8  last_received_media_payload_type_;
};

}  // namespace webrtc

#endif  // WEBRTC_MODULES_RTP_RTCP_SOURCE_RTP_PAYLOAD_REGISTRY_H_
