/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MODULES_RTP_RTCP_SOURCE_RTP_SENDER_H_
#define WEBRTC_MODULES_RTP_RTCP_SOURCE_RTP_SENDER_H_

#include <cassert>
#include <cmath>
#include <map>

#include "webrtc/common_types.h"
#include "webrtc/modules/rtp_rtcp/interface/rtp_rtcp_defines.h"
#include "webrtc/modules/rtp_rtcp/source/bitrate.h"
#include "webrtc/modules/rtp_rtcp/source/rtp_header_extension.h"
#include "webrtc/modules/rtp_rtcp/source/rtp_rtcp_config.h"
#include "webrtc/modules/rtp_rtcp/source/ssrc_database.h"
#include "webrtc/modules/rtp_rtcp/source/video_codec_information.h"

#define MAX_INIT_RTP_SEQ_NUMBER 32767  // 2^15 -1.

namespace webrtc {

class CriticalSectionWrapper;
class PacedSender;
class RTPPacketHistory;
class RTPSenderAudio;
class RTPSenderVideo;

class RTPSenderInterface {
 public:
  RTPSenderInterface() {}
  virtual ~RTPSenderInterface() {}

  virtual WebRtc_UWord32 SSRC() const = 0;
  virtual WebRtc_UWord32 Timestamp() const = 0;

  virtual WebRtc_Word32 BuildRTPheader(
      WebRtc_UWord8 *data_buffer, const WebRtc_Word8 payload_type,
      const bool marker_bit, const WebRtc_UWord32 capture_time_stamp,
      const bool time_stamp_provided = true,
      const bool inc_sequence_number = true) = 0;

  virtual WebRtc_UWord16 RTPHeaderLength() const = 0;
  virtual WebRtc_UWord16 IncrementSequenceNumber() = 0;
  virtual WebRtc_UWord16 SequenceNumber() const = 0;
  virtual WebRtc_UWord16 MaxPayloadLength() const = 0;
  virtual WebRtc_UWord16 MaxDataPayloadLength() const = 0;
  virtual WebRtc_UWord16 PacketOverHead() const = 0;
  virtual WebRtc_UWord16 ActualSendBitrateKbit() const = 0;

  virtual WebRtc_Word32 SendToNetwork(
      uint8_t *data_buffer, int payload_length, int rtp_header_length,
      int64_t capture_time_ms, StorageType storage) = 0;
};

class RTPSender : public Bitrate, public RTPSenderInterface {
 public:
  RTPSender(const WebRtc_Word32 id, const bool audio, Clock *clock,
            Transport *transport, RtpAudioFeedback *audio_feedback,
            PacedSender *paced_sender);
  virtual ~RTPSender();

  void ProcessBitrate();

  WebRtc_UWord16 ActualSendBitrateKbit() const;

  WebRtc_UWord32 VideoBitrateSent() const;
  WebRtc_UWord32 FecOverheadRate() const;
  WebRtc_UWord32 NackOverheadRate() const;

  void SetTargetSendBitrate(const WebRtc_UWord32 bits);

  WebRtc_UWord16 MaxDataPayloadLength() const;  // with RTP and FEC headers.

  WebRtc_Word32 RegisterPayload(
      const char payload_name[RTP_PAYLOAD_NAME_SIZE],
      const WebRtc_Word8 payload_type, const WebRtc_UWord32 frequency,
      const WebRtc_UWord8 channels, const WebRtc_UWord32 rate);

  WebRtc_Word32 DeRegisterSendPayload(const WebRtc_Word8 payload_type);

  WebRtc_Word8 SendPayloadType() const;

  int SendPayloadFrequency() const;

  void SetSendingStatus(const bool enabled);

  void SetSendingMediaStatus(const bool enabled);
  bool SendingMedia() const;

  // Number of sent RTP packets.
  WebRtc_UWord32 Packets() const;

  // Number of sent RTP bytes.
  WebRtc_UWord32 Bytes() const;

  void ResetDataCounters();

  WebRtc_UWord32 StartTimestamp() const;
  void SetStartTimestamp(WebRtc_UWord32 timestamp, bool force);

  WebRtc_UWord32 GenerateNewSSRC();
  void SetSSRC(const WebRtc_UWord32 ssrc);

  WebRtc_UWord16 SequenceNumber() const;
  void SetSequenceNumber(WebRtc_UWord16 seq);

  WebRtc_Word32 CSRCs(WebRtc_UWord32 arr_of_csrc[kRtpCsrcSize]) const;

  void SetCSRCStatus(const bool include);

  void SetCSRCs(const WebRtc_UWord32 arr_of_csrc[kRtpCsrcSize],
                const WebRtc_UWord8 arr_length);

  WebRtc_Word32 SetMaxPayloadLength(const WebRtc_UWord16 length,
                                    const WebRtc_UWord16 packet_over_head);

  WebRtc_Word32 SendOutgoingData(
      const FrameType frame_type, const WebRtc_Word8 payload_type,
      const WebRtc_UWord32 time_stamp, int64_t capture_time_ms,
      const WebRtc_UWord8 *payload_data, const WebRtc_UWord32 payload_size,
      const RTPFragmentationHeader *fragmentation,
      VideoCodecInformation *codec_info = NULL,
      const RTPVideoTypeHeader * rtp_type_hdr = NULL);

  WebRtc_Word32 SendPadData(WebRtc_Word8 payload_type,
                            WebRtc_UWord32 capture_timestamp,
                            int64_t capture_time_ms, WebRtc_Word32 bytes);
  // RTP header extension
  WebRtc_Word32 SetTransmissionTimeOffset(
      const WebRtc_Word32 transmission_time_offset);

  WebRtc_Word32 RegisterRtpHeaderExtension(const RTPExtensionType type,
                                           const WebRtc_UWord8 id);

  WebRtc_Word32 DeregisterRtpHeaderExtension(const RTPExtensionType type);

  WebRtc_UWord16 RtpHeaderExtensionTotalLength() const;

  WebRtc_UWord16 BuildRTPHeaderExtension(WebRtc_UWord8 *data_buffer) const;

  WebRtc_UWord8 BuildTransmissionTimeOffsetExtension(
      WebRtc_UWord8 *data_buffer) const;

  bool UpdateTransmissionTimeOffset(WebRtc_UWord8 *rtp_packet,
                                    const WebRtc_UWord16 rtp_packet_length,
                                    const WebRtcRTPHeader &rtp_header,
                                    const WebRtc_Word64 time_diff_ms) const;

  void TimeToSendPacket(uint16_t sequence_number, int64_t capture_time_ms);

  // NACK.
  int SelectiveRetransmissions() const;
  int SetSelectiveRetransmissions(uint8_t settings);
  void OnReceivedNACK(const WebRtc_UWord16 nack_sequence_numbers_length,
                      const WebRtc_UWord16 *nack_sequence_numbers,
                      const WebRtc_UWord16 avg_rtt);

  void SetStorePacketsStatus(const bool enable,
                             const WebRtc_UWord16 number_to_store);

  bool StorePackets() const;

  WebRtc_Word32 ReSendPacket(WebRtc_UWord16 packet_id,
                             WebRtc_UWord32 min_resend_time = 0);

  WebRtc_Word32 ReSendToNetwork(const WebRtc_UWord8 *packet,
                                const WebRtc_UWord32 size);

  bool ProcessNACKBitRate(const WebRtc_UWord32 now);

  // RTX.
  void SetRTXStatus(const bool enable, const bool set_ssrc,
                    const WebRtc_UWord32 SSRC);

  void RTXStatus(bool *enable, WebRtc_UWord32 *SSRC) const;

  // Functions wrapping RTPSenderInterface.
  virtual WebRtc_Word32 BuildRTPheader(
      WebRtc_UWord8 *data_buffer, const WebRtc_Word8 payload_type,
      const bool marker_bit, const WebRtc_UWord32 capture_time_stamp,
      const bool time_stamp_provided = true,
      const bool inc_sequence_number = true);

  virtual WebRtc_UWord16 RTPHeaderLength() const;
  virtual WebRtc_UWord16 IncrementSequenceNumber();
  virtual WebRtc_UWord16 MaxPayloadLength() const;
  virtual WebRtc_UWord16 PacketOverHead() const;

  // Current timestamp.
  virtual WebRtc_UWord32 Timestamp() const;
  virtual WebRtc_UWord32 SSRC() const;

  virtual WebRtc_Word32 SendToNetwork(
      uint8_t *data_buffer, int payload_length, int rtp_header_length,
      int64_t capture_time_ms, StorageType storage);

  // Audio.

  // Send a DTMF tone using RFC 2833 (4733).
  WebRtc_Word32 SendTelephoneEvent(const WebRtc_UWord8 key,
                                   const WebRtc_UWord16 time_ms,
                                   const WebRtc_UWord8 level);

  bool SendTelephoneEventActive(WebRtc_Word8 *telephone_event) const;

  // Set audio packet size, used to determine when it's time to send a DTMF
  // packet in silence (CNG).
  WebRtc_Word32 SetAudioPacketSize(const WebRtc_UWord16 packet_size_samples);

  // Set status and ID for header-extension-for-audio-level-indication.
  WebRtc_Word32 SetAudioLevelIndicationStatus(const bool enable,
                                              const WebRtc_UWord8 ID);

  // Get status and ID for header-extension-for-audio-level-indication.
  WebRtc_Word32 AudioLevelIndicationStatus(bool *enable,
                                           WebRtc_UWord8 *id) const;

  // Store the audio level in d_bov for
  // header-extension-for-audio-level-indication.
  WebRtc_Word32 SetAudioLevel(const WebRtc_UWord8 level_d_bov);

  // Set payload type for Redundant Audio Data RFC 2198.
  WebRtc_Word32 SetRED(const WebRtc_Word8 payload_type);

  // Get payload type for Redundant Audio Data RFC 2198.
  WebRtc_Word32 RED(WebRtc_Word8 *payload_type) const;

  // Video.
  VideoCodecInformation *CodecInformationVideo();

  RtpVideoCodecTypes VideoCodecType() const;

  WebRtc_UWord32 MaxConfiguredBitrateVideo() const;

  WebRtc_Word32 SendRTPIntraRequest();

  // FEC.
  WebRtc_Word32 SetGenericFECStatus(const bool enable,
                                    const WebRtc_UWord8 payload_type_red,
                                    const WebRtc_UWord8 payload_type_fec);

  WebRtc_Word32 GenericFECStatus(bool *enable, WebRtc_UWord8 *payload_type_red,
                                 WebRtc_UWord8 *payload_type_fec) const;

  WebRtc_Word32 SetFecParameters(const FecProtectionParams *delta_params,
                                 const FecProtectionParams *key_params);

 protected:
  WebRtc_Word32 CheckPayloadType(const WebRtc_Word8 payload_type,
                                 RtpVideoCodecTypes *video_type);

 private:
  void UpdateNACKBitRate(const WebRtc_UWord32 bytes, const WebRtc_UWord32 now);

  WebRtc_Word32 SendPaddingAccordingToBitrate(WebRtc_Word8 payload_type,
                                              WebRtc_UWord32 capture_timestamp,
                                              int64_t capture_time_ms);

  WebRtc_Word32 id_;
  const bool audio_configured_;
  RTPSenderAudio *audio_;
  RTPSenderVideo *video_;

  PacedSender *paced_sender_;
  CriticalSectionWrapper *send_critsect_;

  Transport *transport_;
  bool sending_media_;

  WebRtc_UWord16 max_payload_length_;
  WebRtc_UWord16 target_send_bitrate_;
  WebRtc_UWord16 packet_over_head_;

  WebRtc_Word8 payload_type_;
  std::map<WebRtc_Word8, ModuleRTPUtility::Payload *> payload_type_map_;

  RtpHeaderExtensionMap rtp_header_extension_map_;
  WebRtc_Word32 transmission_time_offset_;

  // NACK
  WebRtc_UWord32 nack_byte_count_times_[NACK_BYTECOUNT_SIZE];
  WebRtc_Word32 nack_byte_count_[NACK_BYTECOUNT_SIZE];
  Bitrate nack_bitrate_;

  RTPPacketHistory *packet_history_;

  // Statistics
  WebRtc_UWord32 packets_sent_;
  WebRtc_UWord32 payload_bytes_sent_;

  // RTP variables
  bool start_time_stamp_forced_;
  WebRtc_UWord32 start_time_stamp_;
  SSRCDatabase &ssrc_db_;
  WebRtc_UWord32 remote_ssrc_;
  bool sequence_number_forced_;
  WebRtc_UWord16 sequence_number_;
  WebRtc_UWord16 sequence_number_rtx_;
  bool ssrc_forced_;
  WebRtc_UWord32 ssrc_;
  WebRtc_UWord32 time_stamp_;
  WebRtc_UWord8 csrcs_;
  WebRtc_UWord32 csrc_[kRtpCsrcSize];
  bool include_csrcs_;
  bool rtx_;
  WebRtc_UWord32 ssrc_rtx_;
};

}  // namespace webrtc

#endif  // WEBRTC_MODULES_RTP_RTCP_SOURCE_RTP_SENDER_H_
