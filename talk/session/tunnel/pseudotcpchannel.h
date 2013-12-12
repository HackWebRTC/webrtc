/*
 * libjingle
 * Copyright 2004--2006, Google Inc.
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

#ifndef TALK_SESSION_TUNNEL_PSEUDOTCPCHANNEL_H_
#define TALK_SESSION_TUNNEL_PSEUDOTCPCHANNEL_H_

#include "talk/base/criticalsection.h"
#include "talk/base/messagequeue.h"
#include "talk/base/stream.h"
#include "talk/p2p/base/pseudotcp.h"
#include "talk/p2p/base/session.h"

namespace talk_base {
class Thread;
}

namespace cricket {

class Candidate;
class TransportChannel;

///////////////////////////////////////////////////////////////////////////////
// PseudoTcpChannel
// Note: The PseudoTcpChannel must persist until both of:
// 1) The StreamInterface provided via GetStream has been closed.
//    This is tracked via non-null stream_.
// 2) The PseudoTcp session has completed.
//    This is tracked via non-null worker_thread_.  When PseudoTcp is done,
//    the TransportChannel is signalled to tear-down.  Once the channel is
//    torn down, the worker thread is purged.
// These indicators are checked by CheckDestroy, invoked whenever one of them
// changes.
///////////////////////////////////////////////////////////////////////////////
// PseudoTcpChannel::GetStream
// Note: The stream pointer returned by GetStream is owned by the caller.
// They can close & immediately delete the stream while PseudoTcpChannel still
// has cleanup work to do.  They can also close the stream but not delete it
// until long after PseudoTcpChannel has finished.  We must cope with both.
///////////////////////////////////////////////////////////////////////////////

class PseudoTcpChannel
    : public IPseudoTcpNotify,
      public talk_base::MessageHandler,
      public sigslot::has_slots<> {
 public:
  // Signal thread methods
  PseudoTcpChannel(talk_base::Thread* stream_thread,
                   Session* session);

  bool Connect(const std::string& content_name,
               const std::string& channel_name,
               int component);
  talk_base::StreamInterface* GetStream();

  sigslot::signal1<PseudoTcpChannel*> SignalChannelClosed;

  // Call this when the Session used to create this channel is being torn
  // down, to ensure that things get cleaned up properly.
  void OnSessionTerminate(Session* session);

  // See the PseudoTcp class for available options.
  void GetOption(PseudoTcp::Option opt, int* value);
  void SetOption(PseudoTcp::Option opt, int value);

 private:
  class InternalStream;
  friend class InternalStream;

  virtual ~PseudoTcpChannel();

  // Stream thread methods
  talk_base::StreamState GetState() const;
  talk_base::StreamResult Read(void* buffer, size_t buffer_len,
                               size_t* read, int* error);
  talk_base::StreamResult Write(const void* data, size_t data_len,
                                size_t* written, int* error);
  void Close();

  // Multi-thread methods
  void OnMessage(talk_base::Message* pmsg);
  void AdjustClock(bool clear = true);
  void CheckDestroy();

  // Signal thread methods
  void OnChannelDestroyed(TransportChannel* channel);

  // Worker thread methods
  void OnChannelWritableState(TransportChannel* channel);
  void OnChannelRead(TransportChannel* channel, const char* data, size_t size,
                     const talk_base::PacketTime& packet_time, int flags);
  void OnChannelConnectionChanged(TransportChannel* channel,
                                  const Candidate& candidate);

  virtual void OnTcpOpen(PseudoTcp* ptcp);
  virtual void OnTcpReadable(PseudoTcp* ptcp);
  virtual void OnTcpWriteable(PseudoTcp* ptcp);
  virtual void OnTcpClosed(PseudoTcp* ptcp, uint32 nError);
  virtual IPseudoTcpNotify::WriteResult TcpWritePacket(PseudoTcp* tcp,
                                                       const char* buffer,
                                                       size_t len);

  talk_base::Thread* signal_thread_, * worker_thread_, * stream_thread_;
  Session* session_;
  TransportChannel* channel_;
  std::string content_name_;
  std::string channel_name_;
  PseudoTcp* tcp_;
  InternalStream* stream_;
  bool stream_readable_, pending_read_event_;
  bool ready_to_connect_;
  mutable talk_base::CriticalSection cs_;
};

}  // namespace cricket

#endif  // TALK_SESSION_TUNNEL_PSEUDOTCPCHANNEL_H_
