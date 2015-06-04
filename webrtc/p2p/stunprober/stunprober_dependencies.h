/*
 *  Copyright 2015 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_P2P_STUNPROBER_STUNPROBER_DEPENDENCIES_H_
#define WEBRTC_P2P_STUNPROBER_STUNPROBER_DEPENDENCIES_H_

#include "webrtc/base/checks.h"
#include "webrtc/base/helpers.h"
#include "webrtc/base/logging.h"
#include "webrtc/base/scoped_ptr.h"
#include "webrtc/base/thread.h"
#include "webrtc/base/timeutils.h"
#include "webrtc/p2p/stunprober/stunprober.h"

// Common classes used by both the command line driver and the unit tests.
namespace stunprober {

class Socket : public ClientSocketInterface,
               public ServerSocketInterface,
               public sigslot::has_slots<> {
 public:
  explicit Socket(rtc::AsyncSocket* socket) : socket_(socket) {
    socket_->SignalReadEvent.connect(this, &Socket::OnReadEvent);
    socket_->SignalWriteEvent.connect(this, &Socket::OnWriteEvent);
  }

  int Connect(const rtc::SocketAddress& addr) override {
    return MapResult(socket_->Connect(addr));
  }

  int SendTo(const rtc::SocketAddress& addr,
             char* buf,
             size_t buf_len,
             AsyncCallback callback) override {
    write_ = NetworkWrite(addr, buf, buf_len, callback);
    return MapResult(socket_->SendTo(buf, buf_len, addr));
  }

  int RecvFrom(char* buf,
               size_t buf_len,
               rtc::SocketAddress* addr,
               AsyncCallback callback) override {
    read_ = NetworkRead(buf, buf_len, addr, callback);
    return MapResult(socket_->RecvFrom(buf, buf_len, addr));
  }

  int GetLocalAddress(rtc::SocketAddress* local_address) override {
    *local_address = socket_->GetLocalAddress();
    return 0;
  }

  void Close() override { socket_->Close(); }

  virtual ~Socket() {}

 protected:
  int MapResult(int rv) {
    if (rv >= 0) {
      return rv;
    }
    int err = socket_->GetError();
    if (err == EWOULDBLOCK || err == EAGAIN) {
      return IO_PENDING;
    } else {
      return FAILED;
    }
  }

  void OnReadEvent(rtc::AsyncSocket* socket) {
    DCHECK(socket_ == socket);
    NetworkRead read = read_;
    read_ = NetworkRead();
    if (!read.callback.empty()) {
      read.callback(socket_->RecvFrom(read.buf, read.buf_len, read.addr));
    }
  }

  void OnWriteEvent(rtc::AsyncSocket* socket) {
    DCHECK(socket_ == socket);
    NetworkWrite write = write_;
    write_ = NetworkWrite();
    if (!write.callback.empty()) {
      write.callback(socket_->SendTo(write.buf, write.buf_len, write.addr));
    }
  }

  struct NetworkWrite {
    NetworkWrite() : buf(nullptr), buf_len(0) {}
    NetworkWrite(const rtc::SocketAddress& addr,
                 char* buf,
                 size_t buf_len,
                 AsyncCallback callback)
        : buf(buf), buf_len(buf_len), addr(addr), callback(callback) {}
    char* buf;
    size_t buf_len;
    rtc::SocketAddress addr;
    AsyncCallback callback;
  };

  NetworkWrite write_;

  struct NetworkRead {
    NetworkRead() : buf(nullptr), buf_len(0) {}
    NetworkRead(char* buf,
                size_t buf_len,
                rtc::SocketAddress* addr,
                AsyncCallback callback)
        : buf(buf), buf_len(buf_len), addr(addr), callback(callback) {}

    char* buf;
    size_t buf_len;
    rtc::SocketAddress* addr;
    AsyncCallback callback;
  };

  NetworkRead read_;

  rtc::scoped_ptr<rtc::AsyncSocket> socket_;
};

class SocketFactory : public SocketFactoryInterface {
 public:
  ClientSocketInterface* CreateClientSocket() override {
    return new Socket(
        rtc::Thread::Current()->socketserver()->CreateAsyncSocket(SOCK_DGRAM));
  }
  ServerSocketInterface* CreateServerSocket(size_t send_buffer_size,
                                            size_t recv_buffer_size) override {
    rtc::scoped_ptr<rtc::AsyncSocket> socket(
        rtc::Thread::Current()->socketserver()->CreateAsyncSocket(SOCK_DGRAM));

    if (socket) {
      socket->SetOption(rtc::AsyncSocket::OPT_SNDBUF,
                        static_cast<int>(send_buffer_size));
      socket->SetOption(rtc::AsyncSocket::OPT_RCVBUF,
                        static_cast<int>(recv_buffer_size));
      return new Socket(socket.release());
    } else {
      return nullptr;
    }
  }
};

class TaskRunner : public TaskRunnerInterface, public rtc::MessageHandler {
 protected:
  class CallbackMessageData : public rtc::MessageData {
   public:
    explicit CallbackMessageData(rtc::Callback0<void> callback)
        : callback_(callback) {}
    rtc::Callback0<void> callback_;
  };

 public:
  void PostTask(rtc::Callback0<void> callback, uint32_t delay_ms) {
    if (delay_ms == 0) {
      rtc::Thread::Current()->Post(this, 0, new CallbackMessageData(callback));
    } else {
      rtc::Thread::Current()->PostDelayed(delay_ms, this, 0,
                                          new CallbackMessageData(callback));
    }
  }

  void OnMessage(rtc::Message* msg) {
    rtc::scoped_ptr<CallbackMessageData> callback(
        reinterpret_cast<CallbackMessageData*>(msg->pdata));
    callback->callback_();
  }
};

}  // namespace stunprober
#endif  // WEBRTC_P2P_STUNPROBER_STUNPROBER_DEPENDENCIES_H_
