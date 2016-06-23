/*
 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <bitset>

#include "webrtc/modules/rtp_rtcp/source/rtp_packet_received.h"
#include "webrtc/modules/rtp_rtcp/include/rtp_rtcp_defines.h"
#include "webrtc/modules/rtp_rtcp/source/rtp_utility.h"

namespace webrtc {
void FuzzOneInput(const uint8_t* data, size_t size) {
  if (size <= 1)
    return;

  // We decide which header extensions to register by reading one byte
  // from the beginning of |data| and interpreting it as a bitmask
  // over the RTPExtensionType enum. That byte shouldn't also be part
  // of the packet, so we adjust |data| and |size| to remove it.
  std::bitset<8> extensionMask(data[0]);
  data++;
  size--;

  RtpPacketReceived::ExtensionManager extensions;
  for (int i = 1; i <= kRtpExtensionNumberOfExtensions; i++) {
    // Skip i=0 which is kRtpExtensionNone i.e. not an actual extension.
    if (extensionMask[i]) {
      // We use i as the ID; it's used in negotiation so not relevant.
      extensions.Register(static_cast<RTPExtensionType>(i), i);
    }
  }

  RTPHeader rtp_header;
  RtpUtility::RtpHeaderParser rtp_parser(data, size);
  rtp_parser.Parse(&rtp_header, &extensions);
}
}  // namespace webrtc
