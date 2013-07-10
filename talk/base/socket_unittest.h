/*
 * libjingle
 * Copyright 2009, Google Inc.
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

#ifndef TALK_BASE_SOCKET_UNITTEST_H_
#define TALK_BASE_SOCKET_UNITTEST_H_

#include "talk/base/gunit.h"
#include "talk/base/thread.h"

namespace talk_base {

// Generic socket tests, to be used when testing individual socketservers.
// Derive your specific test class from SocketTest, install your
// socketserver, and call the SocketTest test methods.
class SocketTest : public testing::Test {
 protected:
  SocketTest() : ss_(NULL), kIPv4Loopback(INADDR_LOOPBACK),
                 kIPv6Loopback(in6addr_loopback) {}
  virtual void SetUp() { ss_ = Thread::Current()->socketserver(); }
  void TestConnectIPv4();
  void TestConnectIPv6();
  void TestConnectWithDnsLookupIPv4();
  void TestConnectWithDnsLookupIPv6();
  void TestConnectFailIPv4();
  void TestConnectFailIPv6();
  void TestConnectWithDnsLookupFailIPv4();
  void TestConnectWithDnsLookupFailIPv6();
  void TestConnectWithClosedSocketIPv4();
  void TestConnectWithClosedSocketIPv6();
  void TestConnectWhileNotClosedIPv4();
  void TestConnectWhileNotClosedIPv6();
  void TestServerCloseDuringConnectIPv4();
  void TestServerCloseDuringConnectIPv6();
  void TestClientCloseDuringConnectIPv4();
  void TestClientCloseDuringConnectIPv6();
  void TestServerCloseIPv4();
  void TestServerCloseIPv6();
  void TestCloseInClosedCallbackIPv4();
  void TestCloseInClosedCallbackIPv6();
  void TestSocketServerWaitIPv4();
  void TestSocketServerWaitIPv6();
  void TestTcpIPv4();
  void TestTcpIPv6();
  void TestSingleFlowControlCallbackIPv4();
  void TestSingleFlowControlCallbackIPv6();
  void TestUdpIPv4();
  void TestUdpIPv6();
  void TestUdpReadyToSendIPv4();
  void TestUdpReadyToSendIPv6();
  void TestGetSetOptionsIPv4();
  void TestGetSetOptionsIPv6();

 private:
  void ConnectInternal(const IPAddress& loopback);
  void ConnectWithDnsLookupInternal(const IPAddress& loopback,
                                    const std::string& host);
  void ConnectFailInternal(const IPAddress& loopback);

  void ConnectWithDnsLookupFailInternal(const IPAddress& loopback);
  void ConnectWithClosedSocketInternal(const IPAddress& loopback);
  void ConnectWhileNotClosedInternal(const IPAddress& loopback);
  void ServerCloseDuringConnectInternal(const IPAddress& loopback);
  void ClientCloseDuringConnectInternal(const IPAddress& loopback);
  void ServerCloseInternal(const IPAddress& loopback);
  void CloseInClosedCallbackInternal(const IPAddress& loopback);
  void SocketServerWaitInternal(const IPAddress& loopback);
  void TcpInternal(const IPAddress& loopback);
  void SingleFlowControlCallbackInternal(const IPAddress& loopback);
  void UdpInternal(const IPAddress& loopback);
  void UdpReadyToSend(const IPAddress& loopback);
  void GetSetOptionsInternal(const IPAddress& loopback);

  static const int kTimeout = 5000;  // ms
  SocketServer* ss_;
  const IPAddress kIPv4Loopback;
  const IPAddress kIPv6Loopback;
};

}  // namespace talk_base

#endif  // TALK_BASE_SOCKET_UNITTEST_H_
