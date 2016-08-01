/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/modules/rtp_rtcp/source/rtp_sender_audio.h"

#include <string.h>

#include "webrtc/base/logging.h"
#include "webrtc/base/timeutils.h"
#include "webrtc/base/trace_event.h"
#include "webrtc/modules/rtp_rtcp/include/rtp_rtcp_defines.h"
#include "webrtc/modules/rtp_rtcp/source/byte_io.h"

namespace webrtc {

static const int kDtmfFrequencyHz = 8000;

RTPSenderAudio::RTPSenderAudio(Clock* clock, RTPSender* rtp_sender)
    : clock_(clock),
      rtp_sender_(rtp_sender),
      packet_size_samples_(160),
      dtmf_event_is_on_(false),
      dtmf_event_first_packet_sent_(false),
      dtmf_payload_type_(-1),
      dtmf_timestamp_(0),
      dtmf_key_(0),
      dtmf_length_samples_(0),
      dtmf_level_(0),
      dtmf_time_last_sent_(0),
      dtmf_timestamp_last_sent_(0),
      red_payload_type_(-1),
      inband_vad_active_(false),
      cngnb_payload_type_(-1),
      cngwb_payload_type_(-1),
      cngswb_payload_type_(-1),
      cngfb_payload_type_(-1),
      last_payload_type_(-1),
      audio_level_dbov_(0) {}

RTPSenderAudio::~RTPSenderAudio() {}

int RTPSenderAudio::AudioFrequency() const {
  return kDtmfFrequencyHz;
}

// set audio packet size, used to determine when it's time to send a DTMF packet
// in silence (CNG)
int32_t RTPSenderAudio::SetAudioPacketSize(uint16_t packet_size_samples) {
  rtc::CritScope cs(&send_audio_critsect_);
  packet_size_samples_ = packet_size_samples;
  return 0;
}

int32_t RTPSenderAudio::RegisterAudioPayload(
    const char payloadName[RTP_PAYLOAD_NAME_SIZE],
    const int8_t payload_type,
    const uint32_t frequency,
    const size_t channels,
    const uint32_t rate,
    RtpUtility::Payload** payload) {
  if (RtpUtility::StringCompare(payloadName, "cn", 2)) {
    rtc::CritScope cs(&send_audio_critsect_);
    //  we can have multiple CNG payload types
    switch (frequency) {
      case 8000:
        cngnb_payload_type_ = payload_type;
        break;
      case 16000:
        cngwb_payload_type_ = payload_type;
        break;
      case 32000:
        cngswb_payload_type_ = payload_type;
        break;
      case 48000:
        cngfb_payload_type_ = payload_type;
        break;
      default:
        return -1;
    }
  } else if (RtpUtility::StringCompare(payloadName, "telephone-event", 15)) {
    rtc::CritScope cs(&send_audio_critsect_);
    // Don't add it to the list
    // we dont want to allow send with a DTMF payloadtype
    dtmf_payload_type_ = payload_type;
    return 0;
    // The default timestamp rate is 8000 Hz, but other rates may be defined.
  }
  *payload = new RtpUtility::Payload;
  (*payload)->typeSpecific.Audio.frequency = frequency;
  (*payload)->typeSpecific.Audio.channels = channels;
  (*payload)->typeSpecific.Audio.rate = rate;
  (*payload)->audio = true;
  (*payload)->name[RTP_PAYLOAD_NAME_SIZE - 1] = '\0';
  strncpy((*payload)->name, payloadName, RTP_PAYLOAD_NAME_SIZE - 1);
  return 0;
}

bool RTPSenderAudio::MarkerBit(FrameType frame_type, int8_t payload_type) {
  rtc::CritScope cs(&send_audio_critsect_);
  // for audio true for first packet in a speech burst
  bool marker_bit = false;
  if (last_payload_type_ != payload_type) {
    if (payload_type != -1 && (cngnb_payload_type_ == payload_type ||
                               cngwb_payload_type_ == payload_type ||
                               cngswb_payload_type_ == payload_type ||
                               cngfb_payload_type_ == payload_type)) {
      // Only set a marker bit when we change payload type to a non CNG
      return false;
    }

    // payload_type differ
    if (last_payload_type_ == -1) {
      if (frame_type != kAudioFrameCN) {
        // first packet and NOT CNG
        return true;
      } else {
        // first packet and CNG
        inband_vad_active_ = true;
        return false;
      }
    }

    // not first packet AND
    // not CNG AND
    // payload_type changed

    // set a marker bit when we change payload type
    marker_bit = true;
  }

  // For G.723 G.729, AMR etc we can have inband VAD
  if (frame_type == kAudioFrameCN) {
    inband_vad_active_ = true;
  } else if (inband_vad_active_) {
    inband_vad_active_ = false;
    marker_bit = true;
  }
  return marker_bit;
}

int32_t RTPSenderAudio::SendAudio(FrameType frame_type,
                                  int8_t payload_type,
                                  uint32_t capture_timestamp,
                                  const uint8_t* payload_data,
                                  size_t data_size,
                                  const RTPFragmentationHeader* fragmentation) {
  // TODO(pwestin) Breakup function in smaller functions.
  size_t payload_size = data_size;
  size_t max_payload_length = rtp_sender_->MaxPayloadLength();
  uint16_t dtmf_length_ms = 0;
  uint8_t key = 0;
  int red_payload_type;
  uint8_t audio_level_dbov;
  int8_t dtmf_payload_type;
  uint16_t packet_size_samples;
  {
    rtc::CritScope cs(&send_audio_critsect_);
    red_payload_type = red_payload_type_;
    audio_level_dbov = audio_level_dbov_;
    dtmf_payload_type = dtmf_payload_type_;
    packet_size_samples = packet_size_samples_;
  }

  // Check if we have pending DTMFs to send
  if (!dtmf_event_is_on_ && PendingDTMF()) {
    int64_t delaySinceLastDTMF =
        clock_->TimeInMilliseconds() - dtmf_time_last_sent_;

    if (delaySinceLastDTMF > 100) {
      // New tone to play
      dtmf_timestamp_ = capture_timestamp;
      if (NextDTMF(&key, &dtmf_length_ms, &dtmf_level_) >= 0) {
        dtmf_event_first_packet_sent_ = false;
        dtmf_key_ = key;
        dtmf_length_samples_ = (kDtmfFrequencyHz / 1000) * dtmf_length_ms;
        dtmf_event_is_on_ = true;
      }
    }
  }

  // A source MAY send events and coded audio packets for the same time
  // but we don't support it
  if (dtmf_event_is_on_) {
    if (frame_type == kEmptyFrame) {
      // kEmptyFrame is used to drive the DTMF when in CN mode
      // it can be triggered more frequently than we want to send the
      // DTMF packets.
      if (packet_size_samples >
          (capture_timestamp - dtmf_timestamp_last_sent_)) {
        // not time to send yet
        return 0;
      }
    }
    dtmf_timestamp_last_sent_ = capture_timestamp;
    uint32_t dtmf_duration_samples = capture_timestamp - dtmf_timestamp_;
    bool ended = false;
    bool send = true;

    if (dtmf_length_samples_ > dtmf_duration_samples) {
      if (dtmf_duration_samples <= 0) {
        // Skip send packet at start, since we shouldn't use duration 0
        send = false;
      }
    } else {
      ended = true;
      dtmf_event_is_on_ = false;
      dtmf_time_last_sent_ = clock_->TimeInMilliseconds();
    }
    if (send) {
      if (dtmf_duration_samples > 0xffff) {
        // RFC 4733 2.5.2.3 Long-Duration Events
        SendTelephoneEventPacket(ended, dtmf_payload_type, dtmf_timestamp_,
                                 static_cast<uint16_t>(0xffff), false);

        // set new timestap for this segment
        dtmf_timestamp_ = capture_timestamp;
        dtmf_duration_samples -= 0xffff;
        dtmf_length_samples_ -= 0xffff;

        return SendTelephoneEventPacket(
            ended, dtmf_payload_type, dtmf_timestamp_,
            static_cast<uint16_t>(dtmf_duration_samples), false);
      } else {
        if (SendTelephoneEventPacket(ended, dtmf_payload_type, dtmf_timestamp_,
                                     dtmf_duration_samples,
                                     !dtmf_event_first_packet_sent_) != 0) {
          return -1;
        }
        dtmf_event_first_packet_sent_ = true;
        return 0;
      }
    }
    return 0;
  }
  if (payload_size == 0 || payload_data == NULL) {
    if (frame_type == kEmptyFrame) {
      // we don't send empty audio RTP packets
      // no error since we use it to drive DTMF when we use VAD
      return 0;
    }
    return -1;
  }
  uint8_t data_buffer[IP_PACKET_SIZE];
  bool marker_bit = MarkerBit(frame_type, payload_type);

  int32_t rtpHeaderLength = 0;
  uint16_t timestampOffset = 0;

  if (red_payload_type >= 0 && fragmentation && !marker_bit &&
      fragmentation->fragmentationVectorSize > 1) {
    // have we configured RED? use its payload type
    // we need to get the current timestamp to calc the diff
    uint32_t old_timestamp = rtp_sender_->Timestamp();
    rtpHeaderLength = rtp_sender_->BuildRtpHeader(data_buffer, red_payload_type,
                                                  marker_bit, capture_timestamp,
                                                  clock_->TimeInMilliseconds());

    timestampOffset = uint16_t(rtp_sender_->Timestamp() - old_timestamp);
  } else {
    rtpHeaderLength = rtp_sender_->BuildRtpHeader(data_buffer, payload_type,
                                                  marker_bit, capture_timestamp,
                                                  clock_->TimeInMilliseconds());
  }
  if (rtpHeaderLength <= 0) {
    return -1;
  }
  if (max_payload_length < (rtpHeaderLength + payload_size)) {
    // Too large payload buffer.
    return -1;
  }
  if (red_payload_type >= 0 &&  // Have we configured RED?
      fragmentation && fragmentation->fragmentationVectorSize > 1 &&
      !marker_bit) {
    if (timestampOffset <= 0x3fff) {
      if (fragmentation->fragmentationVectorSize != 2) {
        // we only support 2 codecs when using RED
        return -1;
      }
      // only 0x80 if we have multiple blocks
      data_buffer[rtpHeaderLength++] =
          0x80 + fragmentation->fragmentationPlType[1];
      size_t blockLength = fragmentation->fragmentationLength[1];

      // sanity blockLength
      if (blockLength > 0x3ff) {  // block length 10 bits 1023 bytes
        return -1;
      }
      uint32_t REDheader = (timestampOffset << 10) + blockLength;
      ByteWriter<uint32_t>::WriteBigEndian(data_buffer + rtpHeaderLength,
                                           REDheader);
      rtpHeaderLength += 3;

      data_buffer[rtpHeaderLength++] = fragmentation->fragmentationPlType[0];
      // copy the RED data
      memcpy(data_buffer + rtpHeaderLength,
             payload_data + fragmentation->fragmentationOffset[1],
             fragmentation->fragmentationLength[1]);

      // copy the normal data
      memcpy(
          data_buffer + rtpHeaderLength + fragmentation->fragmentationLength[1],
          payload_data + fragmentation->fragmentationOffset[0],
          fragmentation->fragmentationLength[0]);

      payload_size = fragmentation->fragmentationLength[0] +
                     fragmentation->fragmentationLength[1];
    } else {
      // silence for too long send only new data
      data_buffer[rtpHeaderLength++] = fragmentation->fragmentationPlType[0];
      memcpy(data_buffer + rtpHeaderLength,
             payload_data + fragmentation->fragmentationOffset[0],
             fragmentation->fragmentationLength[0]);

      payload_size = fragmentation->fragmentationLength[0];
    }
  } else {
    if (fragmentation && fragmentation->fragmentationVectorSize > 0) {
      // use the fragment info if we have one
      data_buffer[rtpHeaderLength++] = fragmentation->fragmentationPlType[0];
      memcpy(data_buffer + rtpHeaderLength,
             payload_data + fragmentation->fragmentationOffset[0],
             fragmentation->fragmentationLength[0]);

      payload_size = fragmentation->fragmentationLength[0];
    } else {
      memcpy(data_buffer + rtpHeaderLength, payload_data, payload_size);
    }
  }

  {
    rtc::CritScope cs(&send_audio_critsect_);
    last_payload_type_ = payload_type;
  }
  // Update audio level extension, if included.
  size_t packetSize = payload_size + rtpHeaderLength;
  RtpUtility::RtpHeaderParser rtp_parser(data_buffer, packetSize);
  RTPHeader rtp_header;
  rtp_parser.Parse(&rtp_header);
  rtp_sender_->UpdateAudioLevel(data_buffer, packetSize, rtp_header,
                                (frame_type == kAudioFrameSpeech),
                                audio_level_dbov);
  TRACE_EVENT_ASYNC_END2("webrtc", "Audio", capture_timestamp, "timestamp",
                         rtp_sender_->Timestamp(), "seqnum",
                         rtp_sender_->SequenceNumber());
  int32_t send_result = rtp_sender_->SendToNetwork(
      data_buffer, payload_size, rtpHeaderLength, rtc::TimeMillis(),
      kAllowRetransmission, RtpPacketSender::kHighPriority);
  if (first_packet_sent_()) {
    LOG(LS_INFO) << "First audio RTP packet sent to pacer";
  }
  return send_result;
}

// Audio level magnitude and voice activity flag are set for each RTP packet
int32_t RTPSenderAudio::SetAudioLevel(uint8_t level_dbov) {
  if (level_dbov > 127) {
    return -1;
  }
  rtc::CritScope cs(&send_audio_critsect_);
  audio_level_dbov_ = level_dbov;
  return 0;
}

// Set payload type for Redundant Audio Data RFC 2198
int32_t RTPSenderAudio::SetRED(int8_t payload_type) {
  if (payload_type < -1) {
    return -1;
  }
  rtc::CritScope cs(&send_audio_critsect_);
  red_payload_type_ = payload_type;
  return 0;
}

// Get payload type for Redundant Audio Data RFC 2198
int32_t RTPSenderAudio::RED(int8_t* payload_type) const {
  rtc::CritScope cs(&send_audio_critsect_);
  if (red_payload_type_ == -1) {
    // not configured
    return -1;
  }
  *payload_type = red_payload_type_;
  return 0;
}

// Send a TelephoneEvent tone using RFC 2833 (4733)
int32_t RTPSenderAudio::SendTelephoneEvent(uint8_t key,
                                           uint16_t time_ms,
                                           uint8_t level) {
  {
    rtc::CritScope lock(&send_audio_critsect_);
    if (dtmf_payload_type_ < 0) {
      // TelephoneEvent payloadtype not configured
      return -1;
    }
  }
  return AddDTMF(key, time_ms, level);
}

int32_t RTPSenderAudio::SendTelephoneEventPacket(bool ended,
                                                 int8_t dtmf_payload_type,
                                                 uint32_t dtmf_timestamp,
                                                 uint16_t duration,
                                                 bool marker_bit) {
  uint8_t dtmfbuffer[IP_PACKET_SIZE];
  uint8_t sendCount = 1;
  int32_t retVal = 0;

  if (ended) {
    // resend last packet in an event 3 times
    sendCount = 3;
  }
  do {
    // Send DTMF data
    int32_t header_length = rtp_sender_->BuildRtpHeader(
        dtmfbuffer, dtmf_payload_type, marker_bit, dtmf_timestamp,
        clock_->TimeInMilliseconds());
    if (header_length <= 0)
      return -1;

    // reset CSRC and X bit
    dtmfbuffer[0] &= 0xe0;

    // Create DTMF data
    /*    From RFC 2833:

     0                   1                   2                   3
     0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
    |     event     |E|R| volume    |          duration             |
    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
    */
    // R bit always cleared
    uint8_t R = 0x00;
    uint8_t volume = dtmf_level_;

    // First packet un-ended
    uint8_t E = ended ? 0x80 : 0x00;

    // First byte is Event number, equals key number
    dtmfbuffer[12] = dtmf_key_;
    dtmfbuffer[13] = E | R | volume;
    ByteWriter<uint16_t>::WriteBigEndian(dtmfbuffer + 14, duration);

    TRACE_EVENT_INSTANT2(
        TRACE_DISABLED_BY_DEFAULT("webrtc_rtp"), "Audio::SendTelephoneEvent",
        "timestamp", dtmf_timestamp, "seqnum", rtp_sender_->SequenceNumber());
    retVal = rtp_sender_->SendToNetwork(dtmfbuffer, 4, 12, rtc::TimeMillis(),
                                        kAllowRetransmission,
                                        RtpPacketSender::kHighPriority);
    sendCount--;
  } while (sendCount > 0 && retVal == 0);

  return retVal;
}
}  // namespace webrtc
