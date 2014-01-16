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

#include <iostream>  // NOLINT

#include "talk/base/thread.h"
#include "talk/base/scoped_ptr.h"
#include "talk/p2p/base/relayserver.h"

int main(int argc, char **argv) {
  if (argc != 3) {
    std::cerr << "usage: relayserver internal-address external-address"
              << std::endl;
    return 1;
  }

  talk_base::SocketAddress int_addr;
  if (!int_addr.FromString(argv[1])) {
    std::cerr << "Unable to parse IP address: " << argv[1];
    return 1;
  }

  talk_base::SocketAddress ext_addr;
  if (!ext_addr.FromString(argv[2])) {
    std::cerr << "Unable to parse IP address: " << argv[2];
    return 1;
  }

  talk_base::Thread *pthMain = talk_base::Thread::Current();

  talk_base::scoped_ptr<talk_base::AsyncUDPSocket> int_socket(
      talk_base::AsyncUDPSocket::Create(pthMain->socketserver(), int_addr));
  if (!int_socket) {
    std::cerr << "Failed to create a UDP socket bound at"
              << int_addr.ToString() << std::endl;
    return 1;
  }

  talk_base::scoped_ptr<talk_base::AsyncUDPSocket> ext_socket(
      talk_base::AsyncUDPSocket::Create(pthMain->socketserver(), ext_addr));
  if (!ext_socket) {
    std::cerr << "Failed to create a UDP socket bound at"
              << ext_addr.ToString() << std::endl;
    return 1;
  }

  cricket::RelayServer server(pthMain);
  server.AddInternalSocket(int_socket.get());
  server.AddExternalSocket(ext_socket.get());

  std::cout << "Listening internally at " << int_addr.ToString() << std::endl;
  std::cout << "Listening externally at " << ext_addr.ToString() << std::endl;

  pthMain->Run();
  return 0;
}
