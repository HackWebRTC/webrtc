/*
 *  Copyright (c) 2026 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS. All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef RTC_TOOLS_ROQ_CAPABILITY_H_
#define RTC_TOOLS_ROQ_CAPABILITY_H_

#include <cstddef>
#include <string>

namespace webrtc::roq {

struct CapabilityResult {
  bool quic_tls_ok = false;
  bool ffmpeg_mov_demuxer_ok = false;
  bool ffmpeg_h264_decoder_ok = false;
  bool ffmpeg_aac_decoder_ok = false;
  bool openh264_encode_ok = false;
  size_t encoded_bytes = 0;
  std::string alpn;
  std::string error;
};

CapabilityResult RunCapabilityChecks(const std::string& executable_path);

}  // namespace webrtc::roq

#endif  // RTC_TOOLS_ROQ_CAPABILITY_H_
