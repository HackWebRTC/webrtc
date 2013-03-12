/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MODULES_RTP_RTCP_SOURCE_RTP_RECEIVER_AUDIO_H_
#define WEBRTC_MODULES_RTP_RTCP_SOURCE_RTP_RECEIVER_AUDIO_H_

#include <set>

#include "webrtc/modules/rtp_rtcp/interface/rtp_rtcp_defines.h"
#include "webrtc/modules/rtp_rtcp/source/rtp_receiver.h"
#include "webrtc/modules/rtp_rtcp/source/rtp_receiver_strategy.h"
#include "webrtc/modules/rtp_rtcp/source/rtp_utility.h"
#include "webrtc/system_wrappers/interface/scoped_ptr.h"
#include "webrtc/typedefs.h"

namespace webrtc {

class CriticalSectionWrapper;

// Handles audio RTP packets. This class is thread-safe.
class RTPReceiverAudio : public RTPReceiverStrategy {
 public:
  RTPReceiverAudio(const WebRtc_Word32 id,
                   RtpData* data_callback,
                   RtpAudioFeedback* incoming_messages_callback);

  WebRtc_UWord32 AudioFrequency() const;

  // Forward DTMFs to decoder for playout.
  int SetTelephoneEventForwardToDecoder(bool forward_to_decoder);

  // Is forwarding of outband telephone events turned on/off?
  bool TelephoneEventForwardToDecoder() const;

  // Is TelephoneEvent configured with payload type payload_type
  bool TelephoneEventPayloadType(const WebRtc_Word8 payload_type) const;

  // Returns true if CNG is configured with payload type payload_type. If so,
  // the frequency and cng_payload_type_has_changed are filled in.
  bool CNGPayloadType(const WebRtc_Word8 payload_type,
                      WebRtc_UWord32* frequency,
                      bool* cng_payload_type_has_changed);

  WebRtc_Word32 ParseRtpPacket(
      WebRtcRTPHeader* rtp_header,
      const ModuleRTPUtility::PayloadUnion& specific_payload,
      const bool is_red,
      const WebRtc_UWord8* packet,
      const WebRtc_UWord16 packet_length,
      const WebRtc_Word64 timestamp_ms,
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

  // We do not allow codecs to have multiple payload types for audio, so we
  // need to override the default behavior (which is to do nothing).
  void PossiblyRemoveExistingPayloadType(
      ModuleRTPUtility::PayloadTypeMap* payload_type_map,
      const char payload_name[RTP_PAYLOAD_NAME_SIZE],
      const size_t payload_name_length,
      const WebRtc_UWord32 frequency,
      const WebRtc_UWord8 channels,
      const WebRtc_UWord32 rate) const;

  // We need to look out for special payload types here and sometimes reset
  // statistics. In addition we sometimes need to tweak the frequency.
  void CheckPayloadChanged(const WebRtc_Word8 payload_type,
                           ModuleRTPUtility::PayloadUnion* specific_payload,
                           bool* should_reset_statistics,
                           bool* should_discard_changes);

 private:

  WebRtc_Word32 ParseAudioCodecSpecific(
      WebRtcRTPHeader* rtp_header,
      const WebRtc_UWord8* payload_data,
      const WebRtc_UWord16 payload_length,
      const ModuleRTPUtility::AudioPayload& audio_specific,
      const bool is_red);

  WebRtc_Word32 id_;
  scoped_ptr<CriticalSectionWrapper> critical_section_rtp_receiver_audio_;

  WebRtc_UWord32 last_received_frequency_;

  bool telephone_event_forward_to_decoder_;
  WebRtc_Word8 telephone_event_payload_type_;
  std::set<WebRtc_UWord8> telephone_event_reported_;

  WebRtc_Word8 cng_nb_payload_type_;
  WebRtc_Word8 cng_wb_payload_type_;
  WebRtc_Word8 cng_swb_payload_type_;
  WebRtc_Word8 cng_fb_payload_type_;
  WebRtc_Word8 cng_payload_type_;

  // G722 is special since it use the wrong number of RTP samples in timestamp
  // VS. number of samples in the frame
  WebRtc_Word8 g722_payload_type_;
  bool last_received_g722_;

  RtpAudioFeedback* cb_audio_feedback_;
};
}  // namespace webrtc

#endif  // WEBRTC_MODULES_RTP_RTCP_SOURCE_RTP_RECEIVER_AUDIO_H_
