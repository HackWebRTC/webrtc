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

#ifndef TALK_BASE_ASYNCPACKETSOCKET_H_
#define TALK_BASE_ASYNCPACKETSOCKET_H_

#include "talk/base/dscp.h"
#include "talk/base/sigslot.h"
#include "talk/base/socket.h"
#include "talk/base/timeutils.h"

namespace talk_base {

// This structure will have the information about when packet is actually
// received by socket.
struct PacketTime {
  PacketTime() : timestamp(0), not_before(0) {}
  PacketTime(int64 timestamp, int64 not_before)
      : timestamp(timestamp), not_before(not_before) {
  }

  int64 timestamp;  // Receive time after socket delivers the data.
  int64 not_before; // Earliest possible time the data could have arrived,
                    // indicating the potential error in the |timestamp| value,
                    // in case the system, is busy. For example, the time of
                    // the last select() call.
                    // If unknown, this value will be set to zero.
};

inline PacketTime CreatePacketTime(int64 not_before) {
  return PacketTime(TimeMicros(), not_before);
}

// Provides the ability to receive packets asynchronously. Sends are not
// buffered since it is acceptable to drop packets under high load.
class AsyncPacketSocket : public sigslot::has_slots<> {
 public:
  enum State {
    STATE_CLOSED,
    STATE_BINDING,
    STATE_BOUND,
    STATE_CONNECTING,
    STATE_CONNECTED
  };

  AsyncPacketSocket() { }
  virtual ~AsyncPacketSocket() { }

  // Returns current local address. Address may be set to NULL if the
  // socket is not bound yet (GetState() returns STATE_BINDING).
  virtual SocketAddress GetLocalAddress() const = 0;

  // Returns remote address. Returns zeroes if this is not a client TCP socket.
  virtual SocketAddress GetRemoteAddress() const = 0;

  // Send a packet.
  virtual int Send(const void *pv, size_t cb, DiffServCodePoint dscp) = 0;
  virtual int SendTo(const void *pv, size_t cb, const SocketAddress& addr,
                     DiffServCodePoint) = 0;

  // Close the socket.
  virtual int Close() = 0;

  // Returns current state of the socket.
  virtual State GetState() const = 0;

  // Get/set options.
  virtual int GetOption(Socket::Option opt, int* value) = 0;
  virtual int SetOption(Socket::Option opt, int value) = 0;

  // Get/Set current error.
  // TODO: Remove SetError().
  virtual int GetError() const = 0;
  virtual void SetError(int error) = 0;

  // Emitted each time a packet is read. Used only for UDP and
  // connected TCP sockets.
  sigslot::signal5<AsyncPacketSocket*, const char*, size_t,
                   const SocketAddress&,
                   const PacketTime&> SignalReadPacket;

  // Emitted when the socket is currently able to send.
  sigslot::signal1<AsyncPacketSocket*> SignalReadyToSend;

  // Emitted after address for the socket is allocated, i.e. binding
  // is finished. State of the socket is changed from BINDING to BOUND
  // (for UDP and server TCP sockets) or CONNECTING (for client TCP
  // sockets).
  sigslot::signal2<AsyncPacketSocket*, const SocketAddress&> SignalAddressReady;

  // Emitted for client TCP sockets when state is changed from
  // CONNECTING to CONNECTED.
  sigslot::signal1<AsyncPacketSocket*> SignalConnect;

  // Emitted for client TCP sockets when state is changed from
  // CONNECTED to CLOSED.
  sigslot::signal2<AsyncPacketSocket*, int> SignalClose;

  // Used only for listening TCP sockets.
  sigslot::signal2<AsyncPacketSocket*, AsyncPacketSocket*> SignalNewConnection;

 private:
  DISALLOW_EVIL_CONSTRUCTORS(AsyncPacketSocket);
};

}  // namespace talk_base

#endif  // TALK_BASE_ASYNCPACKETSOCKET_H_
