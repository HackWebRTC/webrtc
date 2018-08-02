/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/rtp_rtcp/source/rtp_receiver_audio.h"

#include <assert.h>  // assert
#include <math.h>    // pow()
#include <string.h>  // memcpy()

#include "common_types.h"  // NOLINT(build/include)
#include "rtc_base/logging.h"
#include "rtc_base/trace_event.h"

namespace webrtc {
RTPReceiverStrategy* RTPReceiverStrategy::CreateAudioStrategy(
    RtpData* data_callback) {
  return new RTPReceiverAudio(data_callback);
}

RTPReceiverAudio::RTPReceiverAudio(RtpData* data_callback)
    : RTPReceiverStrategy(data_callback),
      telephone_event_payload_type_(-1),
      cng_nb_payload_type_(-1),
      cng_wb_payload_type_(-1),
      cng_swb_payload_type_(-1),
      cng_fb_payload_type_(-1) {}

RTPReceiverAudio::~RTPReceiverAudio() = default;

bool RTPReceiverAudio::TelephoneEventPayloadType(int8_t payload_type) const {
  rtc::CritScope lock(&crit_sect_);
  return telephone_event_payload_type_ == payload_type;
}

bool RTPReceiverAudio::CNGPayloadType(int8_t payload_type) {
  rtc::CritScope lock(&crit_sect_);
  return payload_type == cng_nb_payload_type_ ||
         payload_type == cng_wb_payload_type_ ||
         payload_type == cng_swb_payload_type_ ||
         payload_type == cng_fb_payload_type_;
}

// -   Sample based or frame based codecs based on RFC 3551
// -
// -   NOTE! There is one error in the RFC, stating G.722 uses 8 bits/samples.
// -   The correct rate is 4 bits/sample.
// -
// -   name of                              sampling              default
// -   encoding  sample/frame  bits/sample      rate  ms/frame  ms/packet
// -
// -   Sample based audio codecs
// -   DVI4      sample        4                var.                   20
// -   G722      sample        4              16,000                   20
// -   G726-40   sample        5               8,000                   20
// -   G726-32   sample        4               8,000                   20
// -   G726-24   sample        3               8,000                   20
// -   G726-16   sample        2               8,000                   20
// -   L8        sample        8                var.                   20
// -   L16       sample        16               var.                   20
// -   PCMA      sample        8                var.                   20
// -   PCMU      sample        8                var.                   20
// -
// -   Frame based audio codecs
// -   G723      frame         N/A             8,000        30         30
// -   G728      frame         N/A             8,000       2.5         20
// -   G729      frame         N/A             8,000        10         20
// -   G729D     frame         N/A             8,000        10         20
// -   G729E     frame         N/A             8,000        10         20
// -   GSM       frame         N/A             8,000        20         20
// -   GSM-EFR   frame         N/A             8,000        20         20
// -   LPC       frame         N/A             8,000        20         20
// -   MPA       frame         N/A              var.      var.
// -
// -   G7221     frame         N/A
int32_t RTPReceiverAudio::OnNewPayloadTypeCreated(
    int payload_type,
    const SdpAudioFormat& audio_format) {
  rtc::CritScope lock(&crit_sect_);

  if (RtpUtility::StringCompare(audio_format.name.c_str(), "telephone-event",
                                15)) {
    telephone_event_payload_type_ = payload_type;
  }
  if (RtpUtility::StringCompare(audio_format.name.c_str(), "cn", 2)) {
    // We support comfort noise at four different frequencies.
    if (audio_format.clockrate_hz == 8000) {
      cng_nb_payload_type_ = payload_type;
    } else if (audio_format.clockrate_hz == 16000) {
      cng_wb_payload_type_ = payload_type;
    } else if (audio_format.clockrate_hz == 32000) {
      cng_swb_payload_type_ = payload_type;
    } else if (audio_format.clockrate_hz == 48000) {
      cng_fb_payload_type_ = payload_type;
    } else {
      assert(false);
      return -1;
    }
  }
  return 0;
}

int32_t RTPReceiverAudio::ParseRtpPacket(WebRtcRTPHeader* rtp_header,
                                         const PayloadUnion& specific_payload,
                                         const uint8_t* payload,
                                         size_t payload_length,
                                         int64_t timestamp_ms) {
  if (first_packet_received_()) {
    RTC_LOG(LS_INFO) << "Received first audio RTP packet";
  }

  return ParseAudioCodecSpecific(rtp_header, payload, payload_length,
                                 specific_payload.audio_payload());
}

RTPAliveType RTPReceiverAudio::ProcessDeadOrAlive(
    uint16_t last_payload_length) const {
  // Our CNG is 9 bytes; if it's a likely CNG the receiver needs to check
  // kRtpNoRtp against NetEq speech_type kOutputPLCtoCNG.
  if (last_payload_length < 10) {  // our CNG is 9 bytes
    return kRtpNoRtp;
  } else {
    return kRtpDead;
  }
}

void RTPReceiverAudio::CheckPayloadChanged(int8_t payload_type,
                                           PayloadUnion* /* specific_payload */,
                                           bool* should_discard_changes) {
  *should_discard_changes =
      TelephoneEventPayloadType(payload_type) || CNGPayloadType(payload_type);
}

// We are not allowed to have any critsects when calling data_callback.
int32_t RTPReceiverAudio::ParseAudioCodecSpecific(
    WebRtcRTPHeader* rtp_header,
    const uint8_t* payload_data,
    size_t payload_length,
    const AudioPayload& audio_specific) {
  RTC_DCHECK_GE(payload_length, rtp_header->header.paddingLength);
  const size_t payload_data_length =
      payload_length - rtp_header->header.paddingLength;
  if (payload_data_length == 0) {
    rtp_header->frameType = kEmptyFrame;
    return data_callback_->OnReceivedPayloadData(nullptr, 0, rtp_header);
  }

  return data_callback_->OnReceivedPayloadData(payload_data,
                                               payload_data_length, rtp_header);
}
}  // namespace webrtc
