/*
 *  Copyright 2016 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_P2P_BASE_MOCKICETRANSPORT_H_
#define WEBRTC_P2P_BASE_MOCKICETRANSPORT_H_

#include <memory>
#include <string>
#include <vector>

#include "webrtc/base/gunit.h"
#include "webrtc/p2p/base/icetransportinternal.h"
#include "webrtc/test/gmock.h"

using testing::_;
using testing::Return;

namespace cricket {

// Used in Chromium/remoting/protocol/channel_socket_adapter_unittest.cc
class MockIceTransport : public cricket::TransportChannel {
 public:
  MockIceTransport() : cricket::TransportChannel(std::string(), 0) {
    set_writable(true);
  }

  MOCK_METHOD4(SendPacket,
               int(const char* data,
                   size_t len,
                   const rtc::PacketOptions& options,
                   int flags));
  MOCK_METHOD2(SetOption, int(rtc::Socket::Option opt, int value));
  MOCK_METHOD0(GetError, int());
  MOCK_CONST_METHOD0(GetIceRole, cricket::IceRole());
  MOCK_METHOD1(GetStats, bool(cricket::ConnectionInfos* infos));
  MOCK_CONST_METHOD0(IsDtlsActive, bool());
  MOCK_CONST_METHOD1(GetSslRole, bool(rtc::SSLRole* role));
  MOCK_METHOD1(SetSrtpCiphers, bool(const std::vector<std::string>& ciphers));
  MOCK_METHOD1(GetSrtpCipher, bool(std::string* cipher));
  MOCK_METHOD1(GetSslCipher, bool(std::string* cipher));
  MOCK_CONST_METHOD0(GetLocalCertificate,
                     rtc::scoped_refptr<rtc::RTCCertificate>());

  // This can't be a real mock method because gmock doesn't support move-only
  // return values.
  std::unique_ptr<rtc::SSLCertificate> GetRemoteSSLCertificate()
      const override {
    EXPECT_TRUE(false);  // Never called.
    return nullptr;
  }

  MOCK_METHOD6(ExportKeyingMaterial,
               bool(const std::string& label,
                    const uint8_t* context,
                    size_t context_len,
                    bool use_context,
                    uint8_t* result,
                    size_t result_len));
};

}  // namespace cricket

#endif  // WEBRTC_P2P_BASE_MOCKICETRANSPORT_H_
