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

#ifndef TALK_P2P_BASE_TRANSPORTCHANNEL_H_
#define TALK_P2P_BASE_TRANSPORTCHANNEL_H_

#include <string>
#include "talk/base/basictypes.h"
#include "talk/base/sigslot.h"
#include "talk/base/socket.h"

namespace talk_base {
class Buffer;
}

namespace cricket {

class Candidate;
class P2PTransportChannel;

// A TransportChannel represents one logical stream of packets that are sent
// between the two sides of a session.
class TransportChannel: public sigslot::has_slots<> {
 public:
  TransportChannel(const std::string& name, const std::string &content_type)
      : name_(name), content_type_(content_type),
        readable_(false), writable_(false) {}
  virtual ~TransportChannel() {}

  // Returns the name of this channel.
  const std::string& name() const { return name_; }
  const std::string& content_type() const { return content_type_; }

  // Returns the readable and states of this channel.  Each time one of these
  // states changes, a signal is raised.  These states are aggregated by the
  // TransportManager.
  bool readable() const { return readable_; }
  bool writable() const { return writable_; }
  sigslot::signal1<TransportChannel*> SignalReadableState;
  sigslot::signal1<TransportChannel*> SignalWritableState;

  virtual int SendPacket(talk_base::Buffer* packet) = 0;
  // Attempts to send the given packet.  The return value is < 0 on failure.
  virtual int SendPacket(const char *data, size_t len) = 0;

  // Sets a socket option on this channel.  Note that not all options are
  // supported by all transport types.
  virtual int SetOption(talk_base::Socket::Option opt, int value) = 0;

  // Returns the most recent error that occurred on this channel.
  virtual int GetError() = 0;

  // This hack is here to allow the SocketMonitor to downcast to the
  // P2PTransportChannel safely.
  // TODO: Generalize network monitoring.
  virtual P2PTransportChannel* GetP2PChannel() { return NULL; }

  // Signalled each time a packet is received on this channel.
  sigslot::signal3<TransportChannel*, const char*, size_t> SignalReadPacket;

  // This signal occurs when there is a change in the way that packets are
  // being routed, i.e. to a different remote location. The candidate
  // indicates where and how we are currently sending media.
  sigslot::signal2<TransportChannel*, const Candidate&> SignalRouteChange;

  // Invoked when the channel is being destroyed.
  sigslot::signal1<TransportChannel*> SignalDestroyed;

  // Debugging description of this transport channel.
  std::string ToString() const;

 protected:
  // Sets the readable state, signaling if necessary.
  void set_readable(bool readable);

  // Sets the writable state, signaling if necessary.
  void set_writable(bool writable);

 private:
  std::string name_;
  std::string content_type_;
  bool readable_;
  bool writable_;

  DISALLOW_EVIL_CONSTRUCTORS(TransportChannel);
};

}  // namespace cricket

#endif  // TALK_P2P_BASE_TRANSPORTCHANNEL_H_
