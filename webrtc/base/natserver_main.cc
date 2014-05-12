/*
 *  Copyright 2004 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <iostream>

#include "webrtc/base/natserver.h"
#include "webrtc/base/host.h"
#include "webrtc/base/physicalsocketserver.h"

using namespace rtc;

int main(int argc, char* argv[]) {
  if (argc != 3) {
    std::cerr << "usage: natserver <internal-ip> <external-ip>" << std::endl;
    exit(1);
  }

  SocketAddress internal = SocketAddress(argv[1]);
  SocketAddress external = SocketAddress(argv[2]);
  if (internal.EqualIPs(external)) {
    std::cerr << "internal and external IPs must differ" << std::endl;
    exit(1);
  }

  Thread* pthMain = Thread::Current();
  PhysicalSocketServer* ss = new PhysicalSocketServer();
  pthMain->set_socketserver(ss);
  NATServer* server = new NATServer(NAT_OPEN_CONE, ss, internal, ss, external);
  server = server;

  pthMain->Run();
  return 0;
}
