/*
 *  Copyright (c) 2014 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/modules/rtp_rtcp/source/rtp_format.h"

#include "webrtc/modules/rtp_rtcp/source/rtp_format_h264.h"

namespace webrtc {
RtpPacketizer* RtpPacketizer::Create(RtpVideoCodecTypes type,
                                     size_t max_payload_len) {
  switch (type) {
    case kRtpVideoH264:
      return new RtpPacketizerH264(max_payload_len);
    case kRtpVideoNone:
    case kRtpVideoGeneric:
    case kRtpVideoVp8:
      assert(false);
  }
  return NULL;
}

RtpDepacketizer* RtpDepacketizer::Create(RtpVideoCodecTypes type,
                                         RtpData* const callback) {
  switch (type) {
    case kRtpVideoH264:
      return new RtpDepacketizerH264(callback);
    case kRtpVideoNone:
    case kRtpVideoGeneric:
    case kRtpVideoVp8:
      assert(false);
  }
  return NULL;
}
}  // namespace webrtc
