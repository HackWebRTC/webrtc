/*
*  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
*
*  Use of this source code is governed by a BSD-style license
*  that can be found in the LICENSE file in the root of the source
*  tree. An additional intellectual property rights grant can be found
*  in the file PATENTS.  All contributing project authors may
*  be found in the AUTHORS file in the root of the source tree.
*/

#include <stddef.h>
#include <stdint.h>

#include "webrtc/p2p/base/pseudotcp.h"

namespace webrtc {
class FakeIPseudoTcpNotify : public cricket::IPseudoTcpNotify {
 public:
  void OnTcpOpen(cricket::PseudoTcp* tcp) {}
  void OnTcpReadable(cricket::PseudoTcp* tcp) {}
  void OnTcpWriteable(cricket::PseudoTcp* tcp) {}
  void OnTcpClosed(cricket::PseudoTcp* tcp, uint32_t error) {}

  cricket::IPseudoTcpNotify::WriteResult TcpWritePacket(cricket::PseudoTcp* tcp,
                                                        const char* buffer,
                                                        size_t len) {
    return cricket::IPseudoTcpNotify::WriteResult::WR_SUCCESS;
  }
};

struct Environment {
  cricket::PseudoTcp* ptcp;
  explicit Environment(cricket::IPseudoTcpNotify* notifier) {
    ptcp = new cricket::PseudoTcp(notifier, 0);
  }
};

Environment* env = new Environment(new FakeIPseudoTcpNotify());

void FuzzOneInput(const uint8_t* data, size_t size) {
  env->ptcp->NotifyPacket(reinterpret_cast<const char*>(data), size);
}
}  // namespace webrtc
