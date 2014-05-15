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

#include <sstream>
#include <string>

namespace webrtc {
std::string FecConfig::ToString() const {
  std::stringstream ss;
  ss << "{ulpfec_payload_type: " << ulpfec_payload_type;
  ss << ", red_payload_type: " << red_payload_type;
  ss << '}';
  return ss.str();
}

std::string RtpExtension::ToString() const {
  std::stringstream ss;
  ss << "{name: " << name;
  ss << ", id: " << id;
  ss << '}';
  return ss.str();
}

std::string VideoStream::ToString() const {
  std::stringstream ss;
  ss << "{width: " << width;
  ss << ", height: " << height;
  ss << ", max_framerate: " << max_framerate;
  ss << ", min_bitrate_bps:" << min_bitrate_bps;
  ss << ", target_bitrate_bps:" << target_bitrate_bps;
  ss << ", max_bitrate_bps:" << max_bitrate_bps;
  ss << ", max_qp: " << max_qp;

  ss << ", temporal_layers: {";
  for (size_t i = 0; i < temporal_layers.size(); ++i) {
    ss << temporal_layers[i];
    if (i != temporal_layers.size() - 1)
      ss << "}, {";
  }
  ss << '}';

  ss << '}';
  return ss.str();
}
}  // namespace webrtc
