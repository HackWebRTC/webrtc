/*
 *  Copyright (c) 2014 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#include "webrtc/config.h"

#include <algorithm>
#include <sstream>
#include <string>

#include "webrtc/rtc_base/checks.h"

namespace webrtc {
std::string NackConfig::ToString() const {
  std::stringstream ss;
  ss << "{rtp_history_ms: " << rtp_history_ms;
  ss << '}';
  return ss.str();
}

std::string UlpfecConfig::ToString() const {
  std::stringstream ss;
  ss << "{ulpfec_payload_type: " << ulpfec_payload_type;
  ss << ", red_payload_type: " << red_payload_type;
  ss << ", red_rtx_payload_type: " << red_rtx_payload_type;
  ss << '}';
  return ss.str();
}

bool UlpfecConfig::operator==(const UlpfecConfig& other) const {
  return ulpfec_payload_type == other.ulpfec_payload_type &&
         red_payload_type == other.red_payload_type &&
         red_rtx_payload_type == other.red_rtx_payload_type;
}

std::string RtpExtension::ToString() const {
  std::stringstream ss;
  ss << "{uri: " << uri;
  ss << ", id: " << id;
  if (encrypt) {
    ss << ", encrypt";
  }
  ss << '}';
  return ss.str();
}

const char RtpExtension::kAudioLevelUri[] =
    "urn:ietf:params:rtp-hdrext:ssrc-audio-level";
const int RtpExtension::kAudioLevelDefaultId = 1;

const char RtpExtension::kTimestampOffsetUri[] =
    "urn:ietf:params:rtp-hdrext:toffset";
const int RtpExtension::kTimestampOffsetDefaultId = 2;

const char RtpExtension::kAbsSendTimeUri[] =
    "http://www.webrtc.org/experiments/rtp-hdrext/abs-send-time";
const int RtpExtension::kAbsSendTimeDefaultId = 3;

const char RtpExtension::kVideoRotationUri[] = "urn:3gpp:video-orientation";
const int RtpExtension::kVideoRotationDefaultId = 4;

const char RtpExtension::kTransportSequenceNumberUri[] =
    "http://www.ietf.org/id/draft-holmer-rmcat-transport-wide-cc-extensions-01";
const int RtpExtension::kTransportSequenceNumberDefaultId = 5;

// This extension allows applications to adaptively limit the playout delay
// on frames as per the current needs. For example, a gaming application
// has very different needs on end-to-end delay compared to a video-conference
// application.
const char RtpExtension::kPlayoutDelayUri[] =
    "http://www.webrtc.org/experiments/rtp-hdrext/playout-delay";
const int RtpExtension::kPlayoutDelayDefaultId = 6;

const char RtpExtension::kVideoContentTypeUri[] =
    "http://www.webrtc.org/experiments/rtp-hdrext/video-content-type";
const int RtpExtension::kVideoContentTypeDefaultId = 7;

const char RtpExtension::kVideoTimingUri[] =
    "http://www.webrtc.org/experiments/rtp-hdrext/video-timing";
const int RtpExtension::kVideoTimingDefaultId = 8;

const char RtpExtension::kEncryptHeaderExtensionsUri[] =
    "urn:ietf:params:rtp-hdrext:encrypt";

const int RtpExtension::kMinId = 1;
const int RtpExtension::kMaxId = 14;

bool RtpExtension::IsSupportedForAudio(const std::string& uri) {
  return uri == webrtc::RtpExtension::kAudioLevelUri ||
         uri == webrtc::RtpExtension::kTransportSequenceNumberUri;
}

bool RtpExtension::IsSupportedForVideo(const std::string& uri) {
  return uri == webrtc::RtpExtension::kTimestampOffsetUri ||
         uri == webrtc::RtpExtension::kAbsSendTimeUri ||
         uri == webrtc::RtpExtension::kVideoRotationUri ||
         uri == webrtc::RtpExtension::kTransportSequenceNumberUri ||
         uri == webrtc::RtpExtension::kPlayoutDelayUri ||
         uri == webrtc::RtpExtension::kVideoContentTypeUri ||
         uri == webrtc::RtpExtension::kVideoTimingUri;
}

bool RtpExtension::IsEncryptionSupported(const std::string& uri) {
  return uri == webrtc::RtpExtension::kAudioLevelUri ||
         uri == webrtc::RtpExtension::kTimestampOffsetUri ||
#if !defined(ENABLE_EXTERNAL_AUTH)
         // TODO(jbauch): Figure out a way to always allow "kAbsSendTimeUri"
         // here and filter out later if external auth is really used in
         // srtpfilter. External auth is used by Chromium and replaces the
         // extension header value of "kAbsSendTimeUri", so it must not be
         // encrypted (which can't be done by Chromium).
         uri == webrtc::RtpExtension::kAbsSendTimeUri ||
#endif
         uri == webrtc::RtpExtension::kVideoRotationUri ||
         uri == webrtc::RtpExtension::kTransportSequenceNumberUri ||
         uri == webrtc::RtpExtension::kPlayoutDelayUri ||
         uri == webrtc::RtpExtension::kVideoContentTypeUri;
}

const RtpExtension* RtpExtension::FindHeaderExtensionByUri(
    const std::vector<RtpExtension>& extensions,
    const std::string& uri) {
  for (const auto& extension : extensions) {
    if (extension.uri == uri) {
      return &extension;
    }
  }
  return nullptr;
}

std::vector<RtpExtension> RtpExtension::FilterDuplicateNonEncrypted(
    const std::vector<RtpExtension>& extensions) {
  std::vector<RtpExtension> filtered;
  for (auto extension = extensions.begin(); extension != extensions.end();
      ++extension) {
    if (extension->encrypt) {
      filtered.push_back(*extension);
      continue;
    }

    // Only add non-encrypted extension if no encrypted with the same URI
    // is also present...
    if (std::find_if(extension + 1, extensions.end(),
        [extension](const RtpExtension& check) {
          return extension->uri == check.uri;
        }) != extensions.end()) {
      continue;
    }

    // ...and has not been added before.
    if (!FindHeaderExtensionByUri(filtered, extension->uri)) {
      filtered.push_back(*extension);
    }
  }
  return filtered;
}

VideoStream::VideoStream()
    : width(0),
      height(0),
      max_framerate(-1),
      min_bitrate_bps(-1),
      target_bitrate_bps(-1),
      max_bitrate_bps(-1),
      max_qp(-1) {}

VideoStream::~VideoStream() = default;

std::string VideoStream::ToString() const {
  std::stringstream ss;
  ss << "{width: " << width;
  ss << ", height: " << height;
  ss << ", max_framerate: " << max_framerate;
  ss << ", min_bitrate_bps:" << min_bitrate_bps;
  ss << ", target_bitrate_bps:" << target_bitrate_bps;
  ss << ", max_bitrate_bps:" << max_bitrate_bps;
  ss << ", max_qp: " << max_qp;

  ss << ", temporal_layer_thresholds_bps: [";
  for (size_t i = 0; i < temporal_layer_thresholds_bps.size(); ++i) {
    ss << temporal_layer_thresholds_bps[i];
    if (i != temporal_layer_thresholds_bps.size() - 1)
      ss << ", ";
  }
  ss << ']';

  ss << '}';
  return ss.str();
}

VideoEncoderConfig::VideoEncoderConfig()
    : content_type(ContentType::kRealtimeVideo),
      encoder_specific_settings(nullptr),
      min_transmit_bitrate_bps(0),
      max_bitrate_bps(0),
      number_of_streams(0) {}

VideoEncoderConfig::VideoEncoderConfig(VideoEncoderConfig&&) = default;

VideoEncoderConfig::~VideoEncoderConfig() = default;

std::string VideoEncoderConfig::ToString() const {
  std::stringstream ss;
  ss << "{content_type: ";
  switch (content_type) {
    case ContentType::kRealtimeVideo:
      ss << "kRealtimeVideo";
      break;
    case ContentType::kScreen:
      ss << "kScreenshare";
      break;
  }
  ss << ", encoder_specific_settings: ";
  ss << (encoder_specific_settings != NULL ? "(ptr)" : "NULL");

  ss << ", min_transmit_bitrate_bps: " << min_transmit_bitrate_bps;
  ss << '}';
  return ss.str();
}

VideoEncoderConfig::VideoEncoderConfig(const VideoEncoderConfig&) = default;

void VideoEncoderConfig::EncoderSpecificSettings::FillEncoderSpecificSettings(
    VideoCodec* codec) const {
  if (codec->codecType == kVideoCodecH264) {
    FillVideoCodecH264(codec->H264());
  } else if (codec->codecType == kVideoCodecVP8) {
    FillVideoCodecVp8(codec->VP8());
  } else if (codec->codecType == kVideoCodecVP9) {
    FillVideoCodecVp9(codec->VP9());
  } else {
    RTC_NOTREACHED() << "Encoder specifics set/used for unknown codec type.";
  }
}

void VideoEncoderConfig::EncoderSpecificSettings::FillVideoCodecH264(
    VideoCodecH264* h264_settings) const {
  RTC_NOTREACHED();
}

void VideoEncoderConfig::EncoderSpecificSettings::FillVideoCodecVp8(
    VideoCodecVP8* vp8_settings) const {
  RTC_NOTREACHED();
}

void VideoEncoderConfig::EncoderSpecificSettings::FillVideoCodecVp9(
    VideoCodecVP9* vp9_settings) const {
  RTC_NOTREACHED();
}

VideoEncoderConfig::H264EncoderSpecificSettings::H264EncoderSpecificSettings(
    const VideoCodecH264& specifics)
    : specifics_(specifics) {}

void VideoEncoderConfig::H264EncoderSpecificSettings::FillVideoCodecH264(
    VideoCodecH264* h264_settings) const {
  *h264_settings = specifics_;
}

VideoEncoderConfig::Vp8EncoderSpecificSettings::Vp8EncoderSpecificSettings(
    const VideoCodecVP8& specifics)
    : specifics_(specifics) {}

void VideoEncoderConfig::Vp8EncoderSpecificSettings::FillVideoCodecVp8(
    VideoCodecVP8* vp8_settings) const {
  *vp8_settings = specifics_;
}

VideoEncoderConfig::Vp9EncoderSpecificSettings::Vp9EncoderSpecificSettings(
    const VideoCodecVP9& specifics)
    : specifics_(specifics) {}

void VideoEncoderConfig::Vp9EncoderSpecificSettings::FillVideoCodecVp9(
    VideoCodecVP9* vp9_settings) const {
  *vp9_settings = specifics_;
}

}  // namespace webrtc
