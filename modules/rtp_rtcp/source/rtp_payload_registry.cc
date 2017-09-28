/*
 *  Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/rtp_rtcp/include/rtp_payload_registry.h"

#include <algorithm>

#include "common_types.h"  // NOLINT(build/include)
#include "modules/audio_coding/codecs/audio_format_conversion.h"
#include "rtc_base/checks.h"
#include "rtc_base/logging.h"
#include "rtc_base/stringutils.h"

namespace webrtc {

namespace {

bool PayloadIsCompatible(const RtpUtility::Payload& payload,
                         const CodecInst& audio_codec) {
  if (!payload.typeSpecific.is_audio())
    return false;
  if (_stricmp(payload.name, audio_codec.plname) != 0)
    return false;
  const AudioPayload& audio_payload = payload.typeSpecific.audio_payload();
  return audio_payload.frequency == static_cast<uint32_t>(audio_codec.plfreq) &&
         audio_payload.channels == audio_codec.channels;
}

bool PayloadIsCompatible(const RtpUtility::Payload& payload,
                         const VideoCodec& video_codec) {
  if (!payload.typeSpecific.is_video() ||
      _stricmp(payload.name, video_codec.plName) != 0)
    return false;
  // For H264, profiles must match as well.
  if (video_codec.codecType == kVideoCodecH264) {
    return video_codec.H264().profile ==
           payload.typeSpecific.video_payload().h264_profile;
  }
  return true;
}

RtpUtility::Payload CreatePayloadType(const CodecInst& audio_codec) {
  RTC_DCHECK_GE(audio_codec.plfreq, 1000);
  return {audio_codec.plname,
          PayloadUnion(
              AudioPayload{rtc::dchecked_cast<uint32_t>(audio_codec.plfreq),
                           audio_codec.channels, 0})};
}

RtpVideoCodecTypes ConvertToRtpVideoCodecType(VideoCodecType type) {
  switch (type) {
    case kVideoCodecVP8:
      return kRtpVideoVp8;
    case kVideoCodecVP9:
      return kRtpVideoVp9;
    case kVideoCodecH264:
      return kRtpVideoH264;
    case kVideoCodecRED:
    case kVideoCodecULPFEC:
      return kRtpVideoNone;
    default:
      return kRtpVideoGeneric;
  }
}

RtpUtility::Payload CreatePayloadType(const VideoCodec& video_codec) {
  VideoPayload p;
  p.videoCodecType = ConvertToRtpVideoCodecType(video_codec.codecType);
  if (video_codec.codecType == kVideoCodecH264)
    p.h264_profile = video_codec.H264().profile;
  return {video_codec.plName, PayloadUnion(p)};
}

bool IsPayloadTypeValid(int8_t payload_type) {
  assert(payload_type >= 0);

  // Sanity check.
  switch (payload_type) {
    // Reserved payload types to avoid RTCP conflicts when marker bit is set.
    case 64:        //  192 Full INTRA-frame request.
    case 72:        //  200 Sender report.
    case 73:        //  201 Receiver report.
    case 74:        //  202 Source description.
    case 75:        //  203 Goodbye.
    case 76:        //  204 Application-defined.
    case 77:        //  205 Transport layer FB message.
    case 78:        //  206 Payload-specific FB message.
    case 79:        //  207 Extended report.
      LOG(LS_ERROR) << "Can't register invalid receiver payload type: "
                    << payload_type;
      return false;
    default:
      return true;
  }
}

}  // namespace

RTPPayloadRegistry::RTPPayloadRegistry()
    : incoming_payload_type_(-1),
      last_received_payload_type_(-1),
      last_received_media_payload_type_(-1),
      rtx_(false),
      ssrc_rtx_(0) {}

RTPPayloadRegistry::~RTPPayloadRegistry() = default;

void RTPPayloadRegistry::SetAudioReceivePayloads(
    std::map<int, SdpAudioFormat> codecs) {
  rtc::CritScope cs(&crit_sect_);

#if RTC_DCHECK_IS_ON
  RTC_DCHECK(!used_for_video_);
  used_for_audio_ = true;
#endif

  payload_type_map_.clear();
  for (const auto& kv : codecs) {
    const int& rtp_payload_type = kv.first;
    const SdpAudioFormat& audio_format = kv.second;
    const CodecInst ci = SdpToCodecInst(rtp_payload_type, audio_format);
    RTC_DCHECK(IsPayloadTypeValid(rtp_payload_type));
    payload_type_map_.insert(
        std::make_pair(rtp_payload_type, CreatePayloadType(ci)));
  }

  // Clear the value of last received payload type since it might mean
  // something else now.
  last_received_payload_type_ = -1;
  last_received_media_payload_type_ = -1;
}

int32_t RTPPayloadRegistry::RegisterReceivePayload(const CodecInst& audio_codec,
                                                   bool* created_new_payload) {
  rtc::CritScope cs(&crit_sect_);

#if RTC_DCHECK_IS_ON
  RTC_DCHECK(!used_for_video_);
  used_for_audio_ = true;
#endif

  *created_new_payload = false;
  if (!IsPayloadTypeValid(audio_codec.pltype))
    return -1;

  auto it = payload_type_map_.find(audio_codec.pltype);
  if (it != payload_type_map_.end()) {
    // We already use this payload type. Check if it's the same as we already
    // have. If same, ignore sending an error.
    if (PayloadIsCompatible(it->second, audio_codec)) {
      it->second.typeSpecific.audio_payload().rate = 0;
      return 0;
    }
    LOG(LS_ERROR) << "Payload type already registered: " << audio_codec.pltype;
    return -1;
  }

  // Audio codecs must be unique.
  DeregisterAudioCodecOrRedTypeRegardlessOfPayloadType(audio_codec);

  const auto insert_status = payload_type_map_.emplace(
      audio_codec.pltype, CreatePayloadType(audio_codec));
  RTC_DCHECK(insert_status.second);  // Insertion succeeded.
  *created_new_payload = true;

  // Successful set of payload type, clear the value of last received payload
  // type since it might mean something else.
  last_received_payload_type_ = -1;
  last_received_media_payload_type_ = -1;
  return 0;
}

int32_t RTPPayloadRegistry::RegisterReceivePayload(
    const VideoCodec& video_codec) {
  rtc::CritScope cs(&crit_sect_);

#if RTC_DCHECK_IS_ON
  RTC_DCHECK(!used_for_audio_);
  used_for_video_ = true;
#endif

  if (!IsPayloadTypeValid(video_codec.plType))
    return -1;

  auto it = payload_type_map_.find(video_codec.plType);
  if (it != payload_type_map_.end()) {
    // We already use this payload type. Check if it's the same as we already
    // have. If same, ignore sending an error.
    if (PayloadIsCompatible(it->second, video_codec))
      return 0;
    LOG(LS_ERROR) << "Payload type already registered: "
                  << static_cast<int>(video_codec.plType);
    return -1;
  }

  const auto insert_status = payload_type_map_.emplace(
      video_codec.plType, CreatePayloadType(video_codec));
  RTC_DCHECK(insert_status.second);  // Insertion succeeded.

  // Successful set of payload type, clear the value of last received payload
  // type since it might mean something else.
  last_received_payload_type_ = -1;
  last_received_media_payload_type_ = -1;
  return 0;
}

int32_t RTPPayloadRegistry::DeRegisterReceivePayload(
    const int8_t payload_type) {
  rtc::CritScope cs(&crit_sect_);
  payload_type_map_.erase(payload_type);
  return 0;
}

// There can't be several codecs with the same rate, frequency and channels
// for audio codecs, but there can for video.
// Always called from within a critical section.
void RTPPayloadRegistry::DeregisterAudioCodecOrRedTypeRegardlessOfPayloadType(
    const CodecInst& audio_codec) {
  for (auto iterator = payload_type_map_.begin();
       iterator != payload_type_map_.end(); ++iterator) {
    if (PayloadIsCompatible(iterator->second, audio_codec)) {
      // Remove old setting.
      payload_type_map_.erase(iterator);
      break;
    }
  }
}

int32_t RTPPayloadRegistry::ReceivePayloadType(const CodecInst& audio_codec,
                                               int8_t* payload_type) const {
  assert(payload_type);
  rtc::CritScope cs(&crit_sect_);

  for (const auto& it : payload_type_map_) {
    if (PayloadIsCompatible(it.second, audio_codec)) {
      *payload_type = it.first;
      return 0;
    }
  }
  return -1;
}

int32_t RTPPayloadRegistry::ReceivePayloadType(const VideoCodec& video_codec,
                                               int8_t* payload_type) const {
  assert(payload_type);
  rtc::CritScope cs(&crit_sect_);

  for (const auto& it : payload_type_map_) {
    if (PayloadIsCompatible(it.second, video_codec)) {
      *payload_type = it.first;
      return 0;
    }
  }
  return -1;
}

bool RTPPayloadRegistry::RtxEnabled() const {
  rtc::CritScope cs(&crit_sect_);
  return rtx_;
}

bool RTPPayloadRegistry::IsRtxInternal(const RTPHeader& header) const {
  return rtx_ && ssrc_rtx_ == header.ssrc;
}

void RTPPayloadRegistry::SetRtxSsrc(uint32_t ssrc) {
  rtc::CritScope cs(&crit_sect_);
  ssrc_rtx_ = ssrc;
  rtx_ = true;
}

bool RTPPayloadRegistry::GetRtxSsrc(uint32_t* ssrc) const {
  rtc::CritScope cs(&crit_sect_);
  *ssrc = ssrc_rtx_;
  return rtx_;
}

void RTPPayloadRegistry::SetRtxPayloadType(int payload_type,
                                           int associated_payload_type) {
  rtc::CritScope cs(&crit_sect_);
  if (payload_type < 0) {
    LOG(LS_ERROR) << "Invalid RTX payload type: " << payload_type;
    return;
  }

  rtx_payload_type_map_[payload_type] = associated_payload_type;
  rtx_ = true;
}

bool RTPPayloadRegistry::IsRed(const RTPHeader& header) const {
  rtc::CritScope cs(&crit_sect_);
  auto it = payload_type_map_.find(header.payloadType);
  return it != payload_type_map_.end() && _stricmp(it->second.name, "red") == 0;
}

int RTPPayloadRegistry::GetPayloadTypeFrequency(
    uint8_t payload_type) const {
  const auto payload = PayloadTypeToPayload(payload_type);
  if (!payload) {
    return -1;
  }
  rtc::CritScope cs(&crit_sect_);
  return payload->typeSpecific.is_audio()
             ? payload->typeSpecific.audio_payload().frequency
             : kVideoPayloadTypeFrequency;
}

rtc::Optional<RtpUtility::Payload> RTPPayloadRegistry::PayloadTypeToPayload(
    uint8_t payload_type) const {
  rtc::CritScope cs(&crit_sect_);
  const auto it = payload_type_map_.find(payload_type);
  return it == payload_type_map_.end()
             ? rtc::Optional<RtpUtility::Payload>()
             : rtc::Optional<RtpUtility::Payload>(it->second);
}

void RTPPayloadRegistry::SetIncomingPayloadType(const RTPHeader& header) {
  rtc::CritScope cs(&crit_sect_);
  if (!IsRtxInternal(header))
    incoming_payload_type_ = header.payloadType;
}

bool RTPPayloadRegistry::ReportMediaPayloadType(uint8_t media_payload_type) {
  rtc::CritScope cs(&crit_sect_);
  if (last_received_media_payload_type_ == media_payload_type) {
    // Media type unchanged.
    return true;
  }
  last_received_media_payload_type_ = media_payload_type;
  return false;
}

// Returns -1 if a payload with name |payload_name| is not registered.
int8_t RTPPayloadRegistry::GetPayloadTypeWithName(
    const char* payload_name) const {
  rtc::CritScope cs(&crit_sect_);
  for (const auto& it : payload_type_map_) {
    if (_stricmp(it.second.name, payload_name) == 0)
      return it.first;
  }
  return -1;
}

}  // namespace webrtc
