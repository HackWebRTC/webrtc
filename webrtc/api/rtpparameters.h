/*
 *  Copyright 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_API_RTPPARAMETERS_H_
#define WEBRTC_API_RTPPARAMETERS_H_

#include <vector>

namespace webrtc {

// These structures are defined as part of the RtpSender interface.
// See http://w3c.github.io/webrtc-pc/#rtcrtpsender-interface for details.
struct RtpEncodingParameters {
  bool active = true;
  int max_bitrate_bps = -1;
};

struct RtpParameters {
  std::vector<RtpEncodingParameters> encodings;
};

}  // namespace webrtc

#endif  // WEBRTC_API_RTPPARAMETERS_H_
