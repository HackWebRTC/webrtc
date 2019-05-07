/*
 *  Copyright (c) 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "test/pc/e2e/sdp/sdp_changer.h"

#include <utility>

#include "media/base/media_constants.h"
#include "rtc_base/strings/string_builder.h"

namespace webrtc {
namespace webrtc_pc_e2e {
namespace {

std::string CodecRequiredParamsToString(
    const std::map<std::string, std::string>& codec_required_params) {
  rtc::StringBuilder out;
  for (auto entry : codec_required_params) {
    out << entry.first << "=" << entry.second << ";";
  }
  return out.str();
}

}  // namespace

std::vector<RtpCodecCapability> FilterCodecCapabilities(
    absl::string_view codec_name,
    const std::map<std::string, std::string>& codec_required_params,
    bool ulpfec,
    bool flexfec,
    std::vector<RtpCodecCapability> supported_codecs) {
  std::vector<RtpCodecCapability> output_codecs;
  // Find main requested codecs among supported and add them to output.
  for (auto& codec : supported_codecs) {
    if (codec.name != codec_name) {
      continue;
    }
    bool parameters_matched = true;
    for (auto item : codec_required_params) {
      auto it = codec.parameters.find(item.first);
      if (it == codec.parameters.end()) {
        parameters_matched = false;
        break;
      }
      if (item.second != it->second) {
        parameters_matched = false;
        break;
      }
    }
    if (parameters_matched) {
      output_codecs.push_back(codec);
    }
  }

  RTC_CHECK_GT(output_codecs.size(), 0)
      << "Codec with name=" << codec_name << " and params {"
      << CodecRequiredParamsToString(codec_required_params)
      << "} is unsupported for this peer connection";

  // Add required FEC and RTX codecs to output.
  for (auto& codec : supported_codecs) {
    if (codec.name == cricket::kRtxCodecName) {
      output_codecs.push_back(codec);
    } else if (codec.name == cricket::kFlexfecCodecName && flexfec) {
      output_codecs.push_back(codec);
    } else if ((codec.name == cricket::kRedCodecName ||
                codec.name == cricket::kUlpfecCodecName) &&
               ulpfec) {
      // Red and ulpfec should be enabled or disabled together.
      output_codecs.push_back(codec);
    }
  }
  return output_codecs;
}

}  // namespace webrtc_pc_e2e
}  // namespace webrtc
