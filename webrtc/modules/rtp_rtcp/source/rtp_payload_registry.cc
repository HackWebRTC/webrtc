/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/modules/rtp_rtcp/source/rtp_payload_registry.h"

#include "webrtc/system_wrappers/interface/trace.h"

namespace webrtc {

RTPPayloadRegistry::RTPPayloadRegistry(
    const WebRtc_Word32 id)
    : id_(id),
      rtp_media_receiver_(NULL),
      red_payload_type_(-1),
      last_received_payload_type_(-1),
      last_received_media_payload_type_(-1) {
}

RTPPayloadRegistry::~RTPPayloadRegistry() {
  while (!payload_type_map_.empty()) {
    ModuleRTPUtility::PayloadTypeMap::iterator it = payload_type_map_.begin();
    delete it->second;
    payload_type_map_.erase(it);
  }
}

WebRtc_Word32 RTPPayloadRegistry::RegisterReceivePayload(
    const char payload_name[RTP_PAYLOAD_NAME_SIZE],
    const WebRtc_Word8 payload_type,
    const WebRtc_UWord32 frequency,
    const WebRtc_UWord8 channels,
    const WebRtc_UWord32 rate) {
  assert(rtp_media_receiver_);
  assert(payload_name);

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
      WEBRTC_TRACE(kTraceError, kTraceRtpRtcp, id_,
                   "%s invalid payloadtype:%d",
                   __FUNCTION__, payload_type);
      return -1;
    default:
      break;
  }
  size_t payload_name_length = strlen(payload_name);

  ModuleRTPUtility::PayloadTypeMap::iterator it =
    payload_type_map_.find(payload_type);

  if (it != payload_type_map_.end()) {
    // We already use this payload type.
    ModuleRTPUtility::Payload* payload = it->second;
    assert(payload);

    size_t name_length = strlen(payload->name);

    // Check if it's the same as we already have.
    // If same, ignore sending an error.
    if (payload_name_length == name_length &&
        ModuleRTPUtility::StringCompare(
            payload->name, payload_name, payload_name_length)) {
      if (rtp_media_receiver_->PayloadIsCompatible(*payload, frequency,
                                                   channels, rate)) {
        rtp_media_receiver_->UpdatePayloadRate(payload, rate);
        return 0;
      }
    }
    WEBRTC_TRACE(kTraceError, kTraceRtpRtcp, id_,
                 "%s invalid argument payload_type:%d already registered",
                 __FUNCTION__, payload_type);
    return -1;
  }

  rtp_media_receiver_->PossiblyRemoveExistingPayloadType(
    &payload_type_map_, payload_name, payload_name_length, frequency, channels,
    rate);

  ModuleRTPUtility::Payload* payload = NULL;

  // Save the RED payload type. Used in both audio and video.
  if (ModuleRTPUtility::StringCompare(payload_name, "red", 3)) {
    red_payload_type_ = payload_type;
    payload = new ModuleRTPUtility::Payload;
    payload->audio = false;
    payload->name[RTP_PAYLOAD_NAME_SIZE - 1] = 0;
    strncpy(payload->name, payload_name, RTP_PAYLOAD_NAME_SIZE - 1);
  } else {
    payload = rtp_media_receiver_->CreatePayloadType(
        payload_name, payload_type, frequency, channels, rate);
  }
  if (payload == NULL) {
    WEBRTC_TRACE(kTraceError, kTraceRtpRtcp, id_,
                 "%s failed to register payload",
                 __FUNCTION__);
    return -1;
  }
  payload_type_map_[payload_type] = payload;

  // Successful set of payload type, clear the value of last received payload
  // type since it might mean something else.
  last_received_payload_type_ = -1;
  last_received_media_payload_type_ = -1;
  return 0;
}

WebRtc_Word32 RTPPayloadRegistry::DeRegisterReceivePayload(
    const WebRtc_Word8 payload_type) {
  assert(rtp_media_receiver_);
  ModuleRTPUtility::PayloadTypeMap::iterator it =
    payload_type_map_.find(payload_type);

  if (it == payload_type_map_.end()) {
    WEBRTC_TRACE(kTraceError, kTraceRtpRtcp, id_,
                 "%s failed to find payload_type:%d",
                 __FUNCTION__, payload_type);
    return -1;
  }
  delete it->second;
  payload_type_map_.erase(it);
  return 0;
}

WebRtc_Word32 RTPPayloadRegistry::ReceivePayloadType(
    const char payload_name[RTP_PAYLOAD_NAME_SIZE],
    const WebRtc_UWord32 frequency,
    const WebRtc_UWord8 channels,
    const WebRtc_UWord32 rate,
    WebRtc_Word8* payload_type) const {
  if (payload_type == NULL) {
    WEBRTC_TRACE(kTraceError, kTraceRtpRtcp, id_,
                 "%s invalid argument", __FUNCTION__);
    return -1;
  }
  size_t payload_name_length = strlen(payload_name);

  ModuleRTPUtility::PayloadTypeMap::const_iterator it =
      payload_type_map_.begin();

  while (it != payload_type_map_.end()) {
    ModuleRTPUtility::Payload* payload = it->second;
    assert(payload);

    size_t name_length = strlen(payload->name);
    if (payload_name_length == name_length &&
        ModuleRTPUtility::StringCompare(
            payload->name, payload_name, payload_name_length)) {
      // Name matches.
      if (payload->audio) {
        if (rate == 0) {
          // [default] audio, check freq and channels.
          if (payload->typeSpecific.Audio.frequency == frequency &&
              payload->typeSpecific.Audio.channels == channels) {
            *payload_type = it->first;
            return 0;
          }
        } else {
          // Non-default audio, check freq, channels and rate.
          if (payload->typeSpecific.Audio.frequency == frequency &&
              payload->typeSpecific.Audio.channels == channels &&
              payload->typeSpecific.Audio.rate == rate) {
            // extra rate condition added
            *payload_type = it->first;
            return 0;
          }
        }
      } else {
        // Video.
        *payload_type = it->first;
        return 0;
      }
    }
    it++;
  }
  return -1;
}

WebRtc_Word32 RTPPayloadRegistry::ReceivePayload(
    const WebRtc_Word8 payload_type,
    char payload_name[RTP_PAYLOAD_NAME_SIZE],
    WebRtc_UWord32* frequency,
    WebRtc_UWord8* channels,
    WebRtc_UWord32* rate) const {
  assert(rtp_media_receiver_);
  ModuleRTPUtility::PayloadTypeMap::const_iterator it =
    payload_type_map_.find(payload_type);

  if (it == payload_type_map_.end()) {
    return -1;
  }
  ModuleRTPUtility::Payload* payload = it->second;
  assert(payload);

  if (frequency) {
    if (payload->audio) {
      *frequency = payload->typeSpecific.Audio.frequency;
    } else {
      *frequency = kDefaultVideoFrequency;
    }
  }
  if (channels) {
    if (payload->audio) {
      *channels = payload->typeSpecific.Audio.channels;
    } else {
      *channels = 1;
    }
  }
  if (rate) {
    if (payload->audio) {
      *rate = payload->typeSpecific.Audio.rate;
    } else {
      assert(false);
      *rate = 0;
    }
  }
  if (payload_name) {
    payload_name[RTP_PAYLOAD_NAME_SIZE - 1] = 0;
    strncpy(payload_name, payload->name, RTP_PAYLOAD_NAME_SIZE - 1);
  }
  return 0;
}

WebRtc_UWord32 RTPPayloadRegistry::PayloadTypeToPayload(
  const WebRtc_UWord8 payload_type,
  ModuleRTPUtility::Payload*& payload) const {
  assert(rtp_media_receiver_);

  ModuleRTPUtility::PayloadTypeMap::const_iterator it =
    payload_type_map_.find(payload_type);

  // Check that this is a registered payload type.
  if (it == payload_type_map_.end()) {
    return -1;
  }
  payload = it->second;
  return 0;
}

bool RTPPayloadRegistry::ReportMediaPayloadType(
    WebRtc_UWord8 media_payload_type) {
  if (last_received_media_payload_type_ == media_payload_type) {
    // Media type unchanged.
    return true;
  }
  last_received_media_payload_type_ = media_payload_type;
  return false;
}

}  // namespace webrtc
