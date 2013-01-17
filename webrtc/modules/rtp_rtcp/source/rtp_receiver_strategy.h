/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MODULES_RTP_RTCP_SOURCE_RTP_RECEIVER_STRATEGY_H_
#define WEBRTC_MODULES_RTP_RTCP_SOURCE_RTP_RECEIVER_STRATEGY_H_

#include "webrtc/modules/rtp_rtcp/interface/rtp_rtcp.h"
#include "webrtc/modules/rtp_rtcp/interface/rtp_rtcp_defines.h"
#include "webrtc/modules/rtp_rtcp/source/rtp_utility.h"
#include "webrtc/typedefs.h"

namespace webrtc {

// This strategy deals with media-specific RTP packet processing.
// This class is not thread-safe and must be protected by its caller.
class RTPReceiverStrategy {
 public:
  // The data callback is where we should send received payload data.
  // See ParseRtpPacket. This class does not claim ownership of the callback.
  // Implementations must NOT hold any critical sections while calling the
  // callback.
  //
  // Note: Implementations may call the callback for other reasons than calls
  // to ParseRtpPacket, for instance if the implementation somehow recovers a
  // packet.
  RTPReceiverStrategy(RtpData* data_callback);
  virtual ~RTPReceiverStrategy() {}

  // Parses the RTP packet and calls the data callback with the payload data.
  // Implementations are encouraged to use the provided packet buffer and RTP
  // header as arguments to the callback; implementations are also allowed to
  // make changes in the data as necessary. The specific_payload argument
  // provides audio or video-specific data. The is_first_packet argument is true
  // if this packet is either the first packet ever or the first in its frame.
  virtual WebRtc_Word32 ParseRtpPacket(
    WebRtcRTPHeader* rtp_header,
    const ModuleRTPUtility::PayloadUnion& specific_payload,
    const bool is_red,
    const WebRtc_UWord8* packet,
    const WebRtc_UWord16 packet_length,
    const WebRtc_Word64 timestamp_ms,
    const bool is_first_packet) = 0;

  // Retrieves the last known applicable frequency.
  virtual WebRtc_Word32 GetFrequencyHz() const = 0;

  // Computes the current dead-or-alive state.
  virtual RTPAliveType ProcessDeadOrAlive(
    WebRtc_UWord16 last_payload_length) const = 0;

  // Checks if the provided payload can be handled by this strategy and if
  // it is compatible with the provided parameters.
  virtual bool PayloadIsCompatible(
    const ModuleRTPUtility::Payload& payload,
    const WebRtc_UWord32 frequency,
    const WebRtc_UWord8 channels,
    const WebRtc_UWord32 rate) const = 0;

  // Updates the rate in the payload in a media-specific way.
  virtual void UpdatePayloadRate(
    ModuleRTPUtility::Payload* payload,
    const WebRtc_UWord32 rate) const = 0;

  // Creates a media-specific payload instance from the provided parameters.
  virtual ModuleRTPUtility::Payload* CreatePayloadType(
    const char payload_name[RTP_PAYLOAD_NAME_SIZE],
    const WebRtc_Word8 payload_type,
    const WebRtc_UWord32 frequency,
    const WebRtc_UWord8 channels,
    const WebRtc_UWord32 rate) = 0;

  // Invokes the OnInitializeDecoder callback in a media-specific way.
  virtual WebRtc_Word32 InvokeOnInitializeDecoder(
    RtpFeedback* callback,
    const WebRtc_Word32 id,
    const WebRtc_Word8 payload_type,
    const char payload_name[RTP_PAYLOAD_NAME_SIZE],
    const ModuleRTPUtility::PayloadUnion& specific_payload) const = 0;

  // Prunes the payload type map of the specific payload type, if it exists.
  // TODO(phoglund): Move this responsibility into some payload management
  // class along with rtp_receiver's payload management.
  virtual void PossiblyRemoveExistingPayloadType(
    ModuleRTPUtility::PayloadTypeMap* payload_type_map,
    const char payload_name[RTP_PAYLOAD_NAME_SIZE],
    const size_t payload_name_length,
    const WebRtc_UWord32 frequency,
    const WebRtc_UWord8 channels,
    const WebRtc_UWord32 rate) const {
    // Default: do nothing.
  }

  // Checks if the payload type has changed, and returns whether we should
  // reset statistics and/or discard this packet.
  virtual void CheckPayloadChanged(
    const WebRtc_Word8 payload_type,
    ModuleRTPUtility::PayloadUnion* specific_payload,
    bool* should_reset_statistics,
    bool* should_discard_changes) {
    // Default: Keep changes and don't reset statistics.
    *should_discard_changes = false;
    *should_reset_statistics = false;
  }

  // Stores / retrieves the last media specific payload for later reference.
  void GetLastMediaSpecificPayload(
    ModuleRTPUtility::PayloadUnion* payload) const;
  void SetLastMediaSpecificPayload(
    const ModuleRTPUtility::PayloadUnion& payload);

 protected:
  ModuleRTPUtility::PayloadUnion last_payload_;
  RtpData* data_callback_;
};

}  // namespace webrtc

#endif  // WEBRTC_MODULES_RTP_RTCP_SOURCE_RTP_RECEIVER_STRATEGY_H_
