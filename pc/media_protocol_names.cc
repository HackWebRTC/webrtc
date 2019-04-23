/*
 *  Copyright 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "pc/media_protocol_names.h"

namespace cricket {

const char kMediaProtocolRtpPrefix[] = "RTP/";

const char kMediaProtocolSctp[] = "SCTP";
const char kMediaProtocolDtlsSctp[] = "DTLS/SCTP";
const char kMediaProtocolUdpDtlsSctp[] = "UDP/DTLS/SCTP";
const char kMediaProtocolTcpDtlsSctp[] = "TCP/DTLS/SCTP";

bool IsDtlsSctp(const std::string& protocol) {
  return protocol == kMediaProtocolDtlsSctp ||
         protocol == kMediaProtocolUdpDtlsSctp ||
         protocol == kMediaProtocolTcpDtlsSctp;
}

bool IsPlainSctp(const std::string& protocol) {
  return protocol == kMediaProtocolSctp;
}

bool IsRtpProtocol(const std::string& protocol) {
  return protocol.empty() ||
         (protocol.find(cricket::kMediaProtocolRtpPrefix) != std::string::npos);
}

bool IsSctpProtocol(const std::string& protocol) {
  return IsPlainSctp(protocol) || IsDtlsSctp(protocol);
}

}  // namespace cricket
