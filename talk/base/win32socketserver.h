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

#ifndef TALK_BASE_WIN32SOCKETSERVER_H_
#define TALK_BASE_WIN32SOCKETSERVER_H_

#ifdef WIN32
#include "talk/base/asyncsocket.h"
#include "talk/base/criticalsection.h"
#include "talk/base/messagequeue.h"
#include "talk/base/socketserver.h"
#include "talk/base/socketfactory.h"
#include "talk/base/socket.h"
#include "talk/base/thread.h"
#include "talk/base/win32window.h"

namespace talk_base {

///////////////////////////////////////////////////////////////////////////////
// Win32Socket
///////////////////////////////////////////////////////////////////////////////

class Win32Socket : public AsyncSocket {
 public:
  Win32Socket();
  virtual ~Win32Socket();

  bool CreateT(int family, int type);

  int Attach(SOCKET s);
  void SetTimeout(int ms);

  // AsyncSocket Interface
  virtual SocketAddress GetLocalAddress() const;
  virtual SocketAddress GetRemoteAddress() const;
  virtual int Bind(const SocketAddress& addr);
  virtual int Connect(const SocketAddress& addr);
  virtual int Send(const void *buffer, size_t length);
  virtual int SendTo(const void *buffer, size_t length, const SocketAddress& addr);
  virtual int Recv(void *buffer, size_t length);
  virtual int RecvFrom(void *buffer, size_t length, SocketAddress *out_addr);
  virtual int Listen(int backlog);
  virtual Win32Socket *Accept(SocketAddress *out_addr);
  virtual int Close();
  virtual int GetError() const;
  virtual void SetError(int error);
  virtual ConnState GetState() const;
  virtual int EstimateMTU(uint16* mtu);
  virtual int GetOption(Option opt, int* value);
  virtual int SetOption(Option opt, int value);

 private:
  void CreateSink();
  bool SetAsync(int events);
  int DoConnect(const SocketAddress& addr);
  bool HandleClosed(int close_error);
  void PostClosed();
  void UpdateLastError();
  static int TranslateOption(Option opt, int* slevel, int* sopt);

  void OnSocketNotify(SOCKET socket, int event, int error);
  void OnDnsNotify(HANDLE task, int error);

  SOCKET socket_;
  int error_;
  ConnState state_;
  SocketAddress addr_;         // address that we connected to (see DoConnect)
  uint32 connect_time_;
  bool closing_;
  int close_error_;

  class EventSink;
  friend class EventSink;
  EventSink * sink_;

  struct DnsLookup;
  DnsLookup * dns_;
};

///////////////////////////////////////////////////////////////////////////////
// Win32SocketServer
///////////////////////////////////////////////////////////////////////////////

class Win32SocketServer : public SocketServer {
 public:
  explicit Win32SocketServer(MessageQueue* message_queue);
  virtual ~Win32SocketServer();

  void set_modeless_dialog(HWND hdlg) {
    hdlg_ = hdlg;
  }

  // SocketServer Interface
  virtual Socket* CreateSocket(int type);
  virtual Socket* CreateSocket(int family, int type);

  virtual AsyncSocket* CreateAsyncSocket(int type);
  virtual AsyncSocket* CreateAsyncSocket(int family, int type);

  virtual void SetMessageQueue(MessageQueue* queue);
  virtual bool Wait(int cms, bool process_io);
  virtual void WakeUp();

  void Pump();

  HWND handle() { return wnd_.handle(); }

 private:
  class MessageWindow : public Win32Window {
   public:
    explicit MessageWindow(Win32SocketServer* ss) : ss_(ss) {}
   private:
    virtual bool OnMessage(UINT msg, WPARAM wp, LPARAM lp, LRESULT& result);
    Win32SocketServer* ss_;
  };

  static const TCHAR kWindowName[];
  MessageQueue *message_queue_;
  MessageWindow wnd_;
  CriticalSection cs_;
  bool posted_;
  HWND hdlg_;
};

///////////////////////////////////////////////////////////////////////////////
// Win32Thread. Automatically pumps Windows messages.
///////////////////////////////////////////////////////////////////////////////

class Win32Thread : public Thread {
 public:
  Win32Thread() : ss_(this), id_(0) {
    set_socketserver(&ss_);
  }
  virtual ~Win32Thread() {
    Stop();
    set_socketserver(NULL);
  }
  virtual void Run() {
    id_ = GetCurrentThreadId();
    Thread::Run();
    id_ = 0;
  }
  virtual void Quit() {
    PostThreadMessage(id_, WM_QUIT, 0, 0);
  }
 private:
  Win32SocketServer ss_;
  DWORD id_;
};

///////////////////////////////////////////////////////////////////////////////

}  // namespace talk_base

#endif  // WIN32

#endif  // TALK_BASE_WIN32SOCKETSERVER_H_
