/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MODULES_RTP_RTCP_SOURCE_RTP_SENDER_VIDEO_H_
#define WEBRTC_MODULES_RTP_RTCP_SOURCE_RTP_SENDER_VIDEO_H_

#include <list>

#include "webrtc/base/criticalsection.h"
#include "webrtc/base/onetimeevent.h"
#include "webrtc/base/rate_statistics.h"
#include "webrtc/base/thread_annotations.h"
#include "webrtc/common_types.h"
#include "webrtc/modules/rtp_rtcp/include/rtp_rtcp_defines.h"
#include "webrtc/modules/rtp_rtcp/source/forward_error_correction.h"
#include "webrtc/modules/rtp_rtcp/source/producer_fec.h"
#include "webrtc/modules/rtp_rtcp/source/rtp_rtcp_config.h"
#include "webrtc/modules/rtp_rtcp/source/rtp_sender.h"
#include "webrtc/modules/rtp_rtcp/source/rtp_utility.h"
#include "webrtc/modules/rtp_rtcp/source/video_codec_information.h"
#include "webrtc/typedefs.h"

namespace webrtc {

class RTPSenderVideo {
 public:
  RTPSenderVideo(Clock* clock, RTPSender* rtpSender);
  virtual ~RTPSenderVideo();

  virtual RtpVideoCodecTypes VideoCodecType() const;

  size_t FecPacketOverhead() const;

  static RtpUtility::Payload* CreateVideoPayload(
      const char payload_name[RTP_PAYLOAD_NAME_SIZE],
      int8_t payload_type);

  bool SendVideo(RtpVideoCodecTypes video_type,
                 FrameType frame_type,
                 int8_t payload_type,
                 uint32_t capture_timestamp,
                 int64_t capture_time_ms,
                 const uint8_t* payload_data,
                 size_t payload_size,
                 const RTPFragmentationHeader* fragmentation,
                 const RTPVideoHeader* video_header);

  int32_t SendRTPIntraRequest();

  void SetVideoCodecType(RtpVideoCodecTypes type);

  // FEC
  void SetGenericFECStatus(bool enable,
                           uint8_t payload_type_red,
                           uint8_t payload_type_fec);

  void GenericFECStatus(bool* enable,
                        uint8_t* payload_type_red,
                        uint8_t* payload_type_fec) const;

  void SetFecParameters(const FecProtectionParams* delta_params,
                        const FecProtectionParams* key_params);

  uint32_t VideoBitrateSent() const;
  uint32_t FecOverheadRate() const;

  int SelectiveRetransmissions() const;
  void SetSelectiveRetransmissions(uint8_t settings);

 private:
  void SendVideoPacket(uint8_t* data_buffer,
                       size_t payload_length,
                       size_t rtp_header_length,
                       uint16_t seq_num,
                       uint32_t capture_timestamp,
                       int64_t capture_time_ms,
                       StorageType storage);

  void SendVideoPacketAsRed(uint8_t* data_buffer,
                            size_t payload_length,
                            size_t rtp_header_length,
                            uint16_t video_seq_num,
                            uint32_t capture_timestamp,
                            int64_t capture_time_ms,
                            StorageType media_packet_storage,
                            bool protect);

  RTPSender* const rtp_sender_;
  Clock* const clock_;

  // Should never be held when calling out of this class.
  rtc::CriticalSection crit_;

  RtpVideoCodecTypes video_type_ = kRtpVideoGeneric;
  int32_t retransmission_settings_ GUARDED_BY(crit_) = kRetransmitBaseLayer;

  // FEC
  bool fec_enabled_ GUARDED_BY(crit_) = false;
  int8_t red_payload_type_ GUARDED_BY(crit_) = 0;
  int8_t fec_payload_type_ GUARDED_BY(crit_) = 0;
  FecProtectionParams delta_fec_params_ GUARDED_BY(crit_) = FecProtectionParams{
      0, 1, kFecMaskRandom};
  FecProtectionParams key_fec_params_ GUARDED_BY(crit_) = FecProtectionParams{
      0, 1, kFecMaskRandom};
  ProducerFec producer_fec_ GUARDED_BY(crit_);

  rtc::CriticalSection stats_crit_;
  // Bitrate used for FEC payload, RED headers, RTP headers for FEC packets
  // and any padding overhead.
  RateStatistics fec_bitrate_ GUARDED_BY(stats_crit_);
  // Bitrate used for video payload and RTP headers.
  RateStatistics video_bitrate_ GUARDED_BY(stats_crit_);
  OneTimeEvent first_frame_sent_;
};

}  // namespace webrtc

#endif  // WEBRTC_MODULES_RTP_RTCP_SOURCE_RTP_SENDER_VIDEO_H_
