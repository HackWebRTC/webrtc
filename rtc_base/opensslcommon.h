/*
 *  Copyright 2018 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef RTC_BASE_OPENSSLCOMMON_H_
#define RTC_BASE_OPENSSLCOMMON_H_

#include <string>

typedef struct ssl_st SSL;

namespace rtc {
// The openssl namespace holds static helper methods. All methods related
// to OpenSSL that are commonly used and don't require global state should be
// placed here.
namespace openssl {
// Verifies that the hostname provided matches that in the peer certificate
// attached to this SSL state.
bool VerifyPeerCertMatchesHost(SSL* ssl, const std::string& host);
}  // namespace openssl
}  // namespace rtc

#endif  // RTC_BASE_OPENSSLCOMMON_H_
