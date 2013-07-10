/*
 * libjingle
 * Copyright 2004--2005, Google Inc.
 *
 * Redistribution and use in source and binary forms, with or without 
 * modification, are permitted provided that the following conditions are met:
 *
 *  1. Redistributions of source code must retain the above copyright notice, 
 *     this list of conditions and the following disclaimer.
 *  2. Redistributions in binary form must reproduce the above copyright notice,
 *     this list of conditions and the following disclaimer in the documentation
 *     and/or other materials provided with the distribution.
 *  3. The name of the author may not be used to endorse or promote products 
 *     derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF 
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO
 * EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, 
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR 
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF 
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <iostream>

#include "talk/base/natserver.h"
#include "talk/base/host.h"
#include "talk/base/physicalsocketserver.h"

using namespace talk_base;

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
