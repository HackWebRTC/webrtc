/*
 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/modules/rtp_rtcp/source/rtp_header_extensions.h"

#include "webrtc/base/checks.h"
#include "webrtc/base/logging.h"
#include "webrtc/modules/rtp_rtcp/include/rtp_cvo.h"
#include "webrtc/modules/rtp_rtcp/source/byte_io.h"

namespace webrtc {
// Absolute send time in RTP streams.
//
// The absolute send time is signaled to the receiver in-band using the
// general mechanism for RTP header extensions [RFC5285]. The payload
// of this extension (the transmitted value) is a 24-bit unsigned integer
// containing the sender's current time in seconds as a fixed point number
// with 18 bits fractional part.
//
// The form of the absolute send time extension block:
//
//    0                   1                   2                   3
//    0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
//   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//   |  ID   | len=2 |              absolute send time               |
//   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
const char* AbsoluteSendTime::kName =
    "http://www.webrtc.org/experiments/rtp-hdrext/abs-send-time";
bool AbsoluteSendTime::IsSupportedFor(MediaType type) {
  return true;
}

bool AbsoluteSendTime::Parse(const uint8_t* data, uint32_t* value) {
  *value = ByteReader<uint32_t, 3>::ReadBigEndian(data);
  return true;
}

bool AbsoluteSendTime::Write(uint8_t* data, int64_t time_ms) {
  const uint32_t kAbsSendTimeFraction = 18;
  uint32_t time_24_bits =
      static_cast<uint32_t>(((time_ms << kAbsSendTimeFraction) + 500) / 1000) &
      0x00FFFFFF;

  ByteWriter<uint32_t, 3>::WriteBigEndian(data, time_24_bits);
  return true;
}

// An RTP Header Extension for Client-to-Mixer Audio Level Indication
//
// https://datatracker.ietf.org/doc/draft-lennox-avt-rtp-audio-level-exthdr/
//
// The form of the audio level extension block:
//
//    0                   1
//    0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5
//   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//   |  ID   | len=0 |V|   level     |
//   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//
const char* AudioLevel::kName = "urn:ietf:params:rtp-hdrext:ssrc-audio-level";
bool AudioLevel::IsSupportedFor(MediaType type) {
  switch (type) {
    case MediaType::ANY:
    case MediaType::AUDIO:
      return true;
    case MediaType::VIDEO:
    case MediaType::DATA:
      return false;
  }
  RTC_NOTREACHED();
  return false;
}

bool AudioLevel::Parse(const uint8_t* data,
                       bool* voice_activity,
                       uint8_t* audio_level) {
  *voice_activity = (data[0] & 0x80) != 0;
  *audio_level = data[0] & 0x7F;
  return true;
}

bool AudioLevel::Write(uint8_t* data,
                       bool voice_activity,
                       uint8_t audio_level) {
  RTC_CHECK_LE(audio_level, 0x7f);
  data[0] = (voice_activity ? 0x80 : 0x00) | audio_level;
  return true;
}

// From RFC 5450: Transmission Time Offsets in RTP Streams.
//
// The transmission time is signaled to the receiver in-band using the
// general mechanism for RTP header extensions [RFC5285]. The payload
// of this extension (the transmitted value) is a 24-bit signed integer.
// When added to the RTP timestamp of the packet, it represents the
// "effective" RTP transmission time of the packet, on the RTP
// timescale.
//
// The form of the transmission offset extension block:
//
//    0                   1                   2                   3
//    0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
//   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//   |  ID   | len=2 |              transmission offset              |
//   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
const char* TransmissionOffset::kName = "urn:ietf:params:rtp-hdrext:toffset";
bool TransmissionOffset::IsSupportedFor(MediaType type) {
  switch (type) {
    case MediaType::ANY:
    case MediaType::VIDEO:
      return true;
    case MediaType::AUDIO:
    case MediaType::DATA:
      return false;
  }
  RTC_NOTREACHED();
  return false;
}

bool TransmissionOffset::Parse(const uint8_t* data, int32_t* value) {
  *value = ByteReader<int32_t, 3>::ReadBigEndian(data);
  return true;
}

bool TransmissionOffset::Write(uint8_t* data, int64_t value) {
  RTC_CHECK_LE(value, 0x00ffffff);
  ByteWriter<int32_t, 3>::WriteBigEndian(data, value);
  return true;
}

//   0                   1                   2
//   0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3
//  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//  |  ID   | L=1   |transport wide sequence number |
//  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
const char* TransportSequenceNumber::kName =
    "http://www.ietf.org/id/draft-holmer-rmcat-transport-wide-cc-extensions";
bool TransportSequenceNumber::IsSupportedFor(MediaType type) {
  return true;
}

bool TransportSequenceNumber::Parse(const uint8_t* data, uint16_t* value) {
  *value = ByteReader<uint16_t>::ReadBigEndian(data);
  return true;
}

bool TransportSequenceNumber::Write(uint8_t* data, uint16_t value) {
  ByteWriter<uint16_t>::WriteBigEndian(data, value);
  return true;
}

// Coordination of Video Orientation in RTP streams.
//
// Coordination of Video Orientation consists in signaling of the current
// orientation of the image captured on the sender side to the receiver for
// appropriate rendering and displaying.
//
//    0                   1
//    0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5
//   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//   |  ID   | len=0 |0 0 0 0 C F R R|
//   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
const char* VideoOrientation::kName = "urn:3gpp:video-orientation";
bool VideoOrientation::IsSupportedFor(MediaType type) {
  switch (type) {
    case MediaType::ANY:
    case MediaType::VIDEO:
      return true;
    case MediaType::AUDIO:
    case MediaType::DATA:
      return false;
  }
  RTC_NOTREACHED();
  return false;
}

bool VideoOrientation::Parse(const uint8_t* data, VideoRotation* rotation) {
  *rotation = ConvertCVOByteToVideoRotation(data[0] & 0x03);
  return true;
}

bool VideoOrientation::Write(uint8_t* data, VideoRotation rotation) {
  data[0] = ConvertVideoRotationToCVOByte(rotation);
  return true;
}

bool VideoOrientation::Parse(const uint8_t* data, uint8_t* value) {
  *value = data[0];
  return true;
}

bool VideoOrientation::Write(uint8_t* data, uint8_t value) {
  data[0] = value;
  return true;
}
}  // namespace webrtc
