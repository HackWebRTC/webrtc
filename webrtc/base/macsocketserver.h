/*
 *  Copyright 2007 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#ifndef WEBRTC_BASE_MACSOCKETSERVER_H__
#define WEBRTC_BASE_MACSOCKETSERVER_H__

#include <set>
#include <CoreFoundation/CoreFoundation.h>
#include "webrtc/base/physicalsocketserver.h"

namespace rtc {

///////////////////////////////////////////////////////////////////////////////
// MacBaseSocketServer
///////////////////////////////////////////////////////////////////////////////
class MacAsyncSocket;

class MacBaseSocketServer : public PhysicalSocketServer {
 public:
  MacBaseSocketServer();
  ~MacBaseSocketServer() override;

  // SocketServer Interface
  Socket* CreateSocket(int type) override;
  Socket* CreateSocket(int family, int type) override;

  AsyncSocket* CreateAsyncSocket(int type) override;
  AsyncSocket* CreateAsyncSocket(int family, int type) override;

  bool Wait(int cms, bool process_io) override = 0;
  void WakeUp() override = 0;

  void RegisterSocket(MacAsyncSocket* socket);
  void UnregisterSocket(MacAsyncSocket* socket);

  // PhysicalSocketServer Overrides
  bool SetPosixSignalHandler(int signum, void (*handler)(int)) override;

 protected:
  void EnableSocketCallbacks(bool enable);
  const std::set<MacAsyncSocket*>& sockets() {
    return sockets_;
  }

 private:
  static void FileDescriptorCallback(CFFileDescriptorRef ref,
                                     CFOptionFlags flags,
                                     void* context);

  std::set<MacAsyncSocket*> sockets_;
};

// Core Foundation implementation of the socket server. While idle it
// will run the current CF run loop. When the socket server has work
// to do the run loop will be paused. Does not support Carbon or Cocoa
// UI interaction.
class MacCFSocketServer : public MacBaseSocketServer {
 public:
  MacCFSocketServer();
  ~MacCFSocketServer() override;

  // SocketServer Interface
  bool Wait(int cms, bool process_io) override;
  void WakeUp() override;
  void OnWakeUpCallback();

 private:
  CFRunLoopRef run_loop_;
  CFRunLoopSourceRef wake_up_;
};
} // namespace rtc

#endif  // WEBRTC_BASE_MACSOCKETSERVER_H__
