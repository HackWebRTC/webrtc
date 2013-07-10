// Copyright 2008 Google Inc. All Rights Reserved.

//
// MacAsyncSocket is a kind of AsyncSocket. It only creates sockets
// of the TCP type, and does not (yet) support listen and accept. It works
// asynchronously, which means that users of this socket should connect to
// the various events declared in asyncsocket.h to receive notifications about
// this socket.

#ifndef TALK_BASE_MACASYNCSOCKET_H__
#define TALK_BASE_MACASYNCSOCKET_H__

#include <CoreFoundation/CoreFoundation.h>

#include "talk/base/asyncsocket.h"
#include "talk/base/nethelpers.h"

namespace talk_base {

class MacBaseSocketServer;

class MacAsyncSocket : public AsyncSocket, public sigslot::has_slots<> {
 public:
  MacAsyncSocket(MacBaseSocketServer* ss, int family);
  virtual ~MacAsyncSocket();

  bool valid() const { return source_ != NULL; }

  // Socket interface
  virtual SocketAddress GetLocalAddress() const;
  virtual SocketAddress GetRemoteAddress() const;
  virtual int Bind(const SocketAddress& addr);
  virtual int Connect(const SocketAddress& addr);
  virtual int Send(const void* buffer, size_t length);
  virtual int SendTo(const void* buffer, size_t length,
                     const SocketAddress& addr);
  virtual int Recv(void* buffer, size_t length);
  virtual int RecvFrom(void* buffer, size_t length, SocketAddress* out_addr);
  virtual int Listen(int backlog);
  virtual MacAsyncSocket* Accept(SocketAddress* out_addr);
  virtual int Close();
  virtual int GetError() const;
  virtual void SetError(int error);
  virtual ConnState GetState() const;
  virtual int EstimateMTU(uint16* mtu);
  virtual int GetOption(Option opt, int* value);
  virtual int SetOption(Option opt, int value);

  // For the MacBaseSocketServer to disable callbacks when process_io is false.
  void EnableCallbacks();
  void DisableCallbacks();

 protected:
  void OnResolveResult(SignalThread* thread);
  int DoConnect(const SocketAddress& addr);

 private:
  // Creates an async socket from an existing bsd socket
  MacAsyncSocket(MacBaseSocketServer* ss, int family, int native_socket);

   // Attaches the socket to the CFRunloop and sets the wrapped bsd socket
  // to async mode
  void Initialize(int family);

  // Translate the SocketAddress into a CFDataRef to pass to CF socket
  // functions. Caller must call CFRelease on the result when done.
  static CFDataRef CopyCFAddress(const SocketAddress& address);

  // Callback for the underlying CFSocketRef.
  static void MacAsyncSocketCallBack(CFSocketRef s,
                                     CFSocketCallBackType callbackType,
                                     CFDataRef address,
                                     const void* data,
                                     void* info);

  MacBaseSocketServer* ss_;
  CFSocketRef socket_;
  int native_socket_;
  CFRunLoopSourceRef source_;
  int current_callbacks_;
  bool disabled_;
  int error_;
  ConnState state_;
  AsyncResolver* resolver_;

  DISALLOW_EVIL_CONSTRUCTORS(MacAsyncSocket);
};

}  // namespace talk_base

#endif  // TALK_BASE_MACASYNCSOCKET_H__
