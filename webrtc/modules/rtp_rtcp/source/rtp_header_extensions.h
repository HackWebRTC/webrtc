/*
 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#ifndef WEBRTC_MODULES_RTP_RTCP_SOURCE_RTP_HEADER_EXTENSIONS_H_
#define WEBRTC_MODULES_RTP_RTCP_SOURCE_RTP_HEADER_EXTENSIONS_H_

#include "webrtc/base/basictypes.h"
#include "webrtc/call.h"
#include "webrtc/common_video/rotation.h"
#include "webrtc/modules/rtp_rtcp/include/rtp_rtcp_defines.h"

namespace webrtc {

class AbsoluteSendTime {
 public:
  static constexpr RTPExtensionType kId = kRtpExtensionAbsoluteSendTime;
  static constexpr uint8_t kValueSizeBytes = 3;
  static const char* kName;
  static bool IsSupportedFor(MediaType type);
  static bool Parse(const uint8_t* data, uint32_t* time_ms);
  static bool Write(uint8_t* data, int64_t time_ms);
};

class AudioLevel {
 public:
  static constexpr RTPExtensionType kId = kRtpExtensionAudioLevel;
  static constexpr uint8_t kValueSizeBytes = 1;
  static const char* kName;
  static bool IsSupportedFor(MediaType type);
  static bool Parse(const uint8_t* data,
                    bool* voice_activity,
                    uint8_t* audio_level);
  static bool Write(uint8_t* data, bool voice_activity, uint8_t audio_level);
};

class TransmissionOffset {
 public:
  static constexpr RTPExtensionType kId = kRtpExtensionTransmissionTimeOffset;
  static constexpr uint8_t kValueSizeBytes = 3;
  static const char* kName;
  static bool IsSupportedFor(MediaType type);
  static bool Parse(const uint8_t* data, int32_t* time_ms);
  static bool Write(uint8_t* data, int64_t time_ms);
};

class TransportSequenceNumber {
 public:
  static constexpr RTPExtensionType kId = kRtpExtensionTransportSequenceNumber;
  static constexpr uint8_t kValueSizeBytes = 2;
  static const char* kName;
  static bool IsSupportedFor(MediaType type);
  static bool Parse(const uint8_t* data, uint16_t* value);
  static bool Write(uint8_t* data, uint16_t value);
};

class VideoOrientation {
 public:
  static constexpr RTPExtensionType kId = kRtpExtensionVideoRotation;
  static constexpr uint8_t kValueSizeBytes = 1;
  static const char* kName;
  static bool IsSupportedFor(MediaType type);
  static bool Parse(const uint8_t* data, VideoRotation* value);
  static bool Write(uint8_t* data, VideoRotation value);
  static bool Parse(const uint8_t* data, uint8_t* value);
  static bool Write(uint8_t* data, uint8_t value);
};

}  // namespace webrtc
#endif  // WEBRTC_MODULES_RTP_RTCP_SOURCE_RTP_HEADER_EXTENSIONS_H_
