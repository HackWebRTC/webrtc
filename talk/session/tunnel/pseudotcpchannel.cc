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

#include <string>
#include "talk/base/basictypes.h"
#include "talk/base/common.h"
#include "talk/base/logging.h"
#include "talk/base/scoped_ptr.h"
#include "talk/base/stringutils.h"
#include "talk/p2p/base/candidate.h"
#include "talk/p2p/base/transportchannel.h"
#include "pseudotcpchannel.h"

using namespace talk_base;

namespace cricket {

extern const talk_base::ConstantLabel SESSION_STATES[];

// MSG_WK_* - worker thread messages
// MSG_ST_* - stream thread messages
// MSG_SI_* - signal thread messages

enum {
  MSG_WK_CLOCK = 1,
  MSG_WK_PURGE,
  MSG_ST_EVENT,
  MSG_SI_DESTROYCHANNEL,
  MSG_SI_DESTROY,
};

struct EventData : public MessageData {
  int event, error;
  EventData(int ev, int err = 0) : event(ev), error(err) { }
};

///////////////////////////////////////////////////////////////////////////////
// PseudoTcpChannel::InternalStream
///////////////////////////////////////////////////////////////////////////////

class PseudoTcpChannel::InternalStream : public StreamInterface {
public:
  InternalStream(PseudoTcpChannel* parent);
  virtual ~InternalStream();

  virtual StreamState GetState() const;
  virtual StreamResult Read(void* buffer, size_t buffer_len,
                                       size_t* read, int* error);
  virtual StreamResult Write(const void* data, size_t data_len,
                                        size_t* written, int* error);
  virtual void Close();

private:
  // parent_ is accessed and modified exclusively on the event thread, to
  // avoid thread contention.  This means that the PseudoTcpChannel cannot go
  // away until after it receives a Close() from TunnelStream.
  PseudoTcpChannel* parent_;
};

///////////////////////////////////////////////////////////////////////////////
// PseudoTcpChannel
// Member object lifetime summaries:
//   session_ - passed in constructor, cleared when channel_ goes away.
//   channel_ - created in Connect, destroyed when session_ or tcp_ goes away.
//   tcp_ - created in Connect, destroyed when channel_ goes away, or connection
//     closes.
//   worker_thread_ - created when channel_ is created, purged when channel_ is
//     destroyed.
//   stream_ - created in GetStream, destroyed by owner at arbitrary time.
//   this - created in constructor, destroyed when worker_thread_ and stream_
//     are both gone.
///////////////////////////////////////////////////////////////////////////////

//
// Signal thread methods
//

PseudoTcpChannel::PseudoTcpChannel(Thread* stream_thread, Session* session)
  : signal_thread_(session->session_manager()->signaling_thread()),
    worker_thread_(NULL),
    stream_thread_(stream_thread),
    session_(session), channel_(NULL), tcp_(NULL), stream_(NULL),
    stream_readable_(false), pending_read_event_(false),
    ready_to_connect_(false) {
  ASSERT(signal_thread_->IsCurrent());
  ASSERT(NULL != session_);
}

PseudoTcpChannel::~PseudoTcpChannel() {
  ASSERT(signal_thread_->IsCurrent());
  ASSERT(worker_thread_ == NULL);
  ASSERT(session_ == NULL);
  ASSERT(channel_ == NULL);
  ASSERT(stream_ == NULL);
  ASSERT(tcp_ == NULL);
}

bool PseudoTcpChannel::Connect(const std::string& content_name,
                               const std::string& channel_name,
                               int component) {
  ASSERT(signal_thread_->IsCurrent());
  CritScope lock(&cs_);

  if (channel_)
    return false;

  ASSERT(session_ != NULL);
  worker_thread_ = session_->session_manager()->worker_thread();
  content_name_ = content_name;
  channel_ = session_->CreateChannel(
      content_name, channel_name, component);
  channel_name_ = channel_name;
  channel_->SetOption(Socket::OPT_DONTFRAGMENT, 1);

  channel_->SignalDestroyed.connect(this,
    &PseudoTcpChannel::OnChannelDestroyed);
  channel_->SignalWritableState.connect(this,
    &PseudoTcpChannel::OnChannelWritableState);
  channel_->SignalReadPacket.connect(this,
    &PseudoTcpChannel::OnChannelRead);
  channel_->SignalRouteChange.connect(this,
    &PseudoTcpChannel::OnChannelConnectionChanged);

  ASSERT(tcp_ == NULL);
  tcp_ = new PseudoTcp(this, 0);
  if (session_->initiator()) {
    // Since we may try several protocols and network adapters that won't work,
    // waiting until we get our first writable notification before initiating
    // TCP negotiation.
    ready_to_connect_ = true;
  }

  return true;
}

StreamInterface* PseudoTcpChannel::GetStream() {
  ASSERT(signal_thread_->IsCurrent());
  CritScope lock(&cs_);
  ASSERT(NULL != session_);
  if (!stream_)
    stream_ = new PseudoTcpChannel::InternalStream(this);
  //TODO("should we disallow creation of new stream at some point?");
  return stream_;
}

void PseudoTcpChannel::OnChannelDestroyed(TransportChannel* channel) {
  LOG_F(LS_INFO) << "(" << channel->component() << ")";
  ASSERT(signal_thread_->IsCurrent());
  CritScope lock(&cs_);
  ASSERT(channel == channel_);
  signal_thread_->Clear(this, MSG_SI_DESTROYCHANNEL);
  // When MSG_WK_PURGE is received, we know there will be no more messages from
  // the worker thread.
  worker_thread_->Clear(this, MSG_WK_CLOCK);
  worker_thread_->Post(this, MSG_WK_PURGE);
  session_ = NULL;
  channel_ = NULL;
  if ((stream_ != NULL)
      && ((tcp_ == NULL) || (tcp_->State() != PseudoTcp::TCP_CLOSED)))
    stream_thread_->Post(this, MSG_ST_EVENT, new EventData(SE_CLOSE, 0));
  if (tcp_) {
    tcp_->Close(true);
    AdjustClock();
  }
  SignalChannelClosed(this);
}

void PseudoTcpChannel::OnSessionTerminate(Session* session) {
  // When the session terminates before we even connected
  CritScope lock(&cs_);
  if (session_ != NULL && channel_ == NULL) {
    ASSERT(session == session_);
    ASSERT(worker_thread_ == NULL);
    ASSERT(tcp_ == NULL);
    LOG(LS_INFO) << "Destroying unconnected PseudoTcpChannel";
    session_ = NULL;
    if (stream_ != NULL)
      stream_thread_->Post(this, MSG_ST_EVENT, new EventData(SE_CLOSE, -1));
  }

  // Even though session_ is being destroyed, we mustn't clear the pointer,
  // since we'll need it to tear down channel_.
  //
  // TODO: Is it always the case that if channel_ != NULL then we'll get
  // a channel-destroyed notification?
}

void PseudoTcpChannel::GetOption(PseudoTcp::Option opt, int* value) {
  ASSERT(signal_thread_->IsCurrent());
  CritScope lock(&cs_);
  ASSERT(tcp_ != NULL);
  tcp_->GetOption(opt, value);
}

void PseudoTcpChannel::SetOption(PseudoTcp::Option opt, int value) {
  ASSERT(signal_thread_->IsCurrent());
  CritScope lock(&cs_);
  ASSERT(tcp_ != NULL);
  tcp_->SetOption(opt, value);
}

//
// Stream thread methods
//

StreamState PseudoTcpChannel::GetState() const {
  ASSERT(stream_ != NULL && stream_thread_->IsCurrent());
  CritScope lock(&cs_);
  if (!session_)
    return SS_CLOSED;
  if (!tcp_)
    return SS_OPENING;
  switch (tcp_->State()) {
    case PseudoTcp::TCP_LISTEN:
    case PseudoTcp::TCP_SYN_SENT:
    case PseudoTcp::TCP_SYN_RECEIVED:
      return SS_OPENING;
    case PseudoTcp::TCP_ESTABLISHED:
      return SS_OPEN;
    case PseudoTcp::TCP_CLOSED:
    default:
      return SS_CLOSED;
  }
}

StreamResult PseudoTcpChannel::Read(void* buffer, size_t buffer_len,
                                    size_t* read, int* error) {
  ASSERT(stream_ != NULL && stream_thread_->IsCurrent());
  CritScope lock(&cs_);
  if (!tcp_)
    return SR_BLOCK;

  stream_readable_ = false;
  int result = tcp_->Recv(static_cast<char*>(buffer), buffer_len);
  //LOG_F(LS_VERBOSE) << "Recv returned: " << result;
  if (result > 0) {
    if (read)
      *read = result;
    // PseudoTcp doesn't currently support repeated Readable signals.  Simulate
    // them here.
    stream_readable_ = true;
    if (!pending_read_event_) {
      pending_read_event_ = true;
      stream_thread_->Post(this, MSG_ST_EVENT, new EventData(SE_READ), true);
    }
    return SR_SUCCESS;
  } else if (IsBlockingError(tcp_->GetError())) {
    return SR_BLOCK;
  } else {
    if (error)
      *error = tcp_->GetError();
    return SR_ERROR;
  }
  // This spot is never reached.
}

StreamResult PseudoTcpChannel::Write(const void* data, size_t data_len,
                                     size_t* written, int* error) {
  ASSERT(stream_ != NULL && stream_thread_->IsCurrent());
  CritScope lock(&cs_);
  if (!tcp_)
    return SR_BLOCK;
  int result = tcp_->Send(static_cast<const char*>(data), data_len);
  //LOG_F(LS_VERBOSE) << "Send returned: " << result;
  if (result > 0) {
    if (written)
      *written = result;
    return SR_SUCCESS;
  } else if (IsBlockingError(tcp_->GetError())) {
    return SR_BLOCK;
  } else {
    if (error)
      *error = tcp_->GetError();
    return SR_ERROR;
  }
  // This spot is never reached.
}

void PseudoTcpChannel::Close() {
  ASSERT(stream_ != NULL && stream_thread_->IsCurrent());
  CritScope lock(&cs_);
  stream_ = NULL;
  // Clear out any pending event notifications
  stream_thread_->Clear(this, MSG_ST_EVENT);
  if (tcp_) {
    tcp_->Close(false);
    AdjustClock();
  } else {
    CheckDestroy();
  }
}

//
// Worker thread methods
//

void PseudoTcpChannel::OnChannelWritableState(TransportChannel* channel) {
  LOG_F(LS_VERBOSE) << "[" << channel_name_ << "]";
  ASSERT(worker_thread_->IsCurrent());
  CritScope lock(&cs_);
  if (!channel_) {
    LOG_F(LS_WARNING) << "NULL channel";
    return;
  }
  ASSERT(channel == channel_);
  if (!tcp_) {
    LOG_F(LS_WARNING) << "NULL tcp";
    return;
  }
  if (!ready_to_connect_ || !channel->writable())
    return;

  ready_to_connect_ = false;
  tcp_->Connect();
  AdjustClock();
}

void PseudoTcpChannel::OnChannelRead(TransportChannel* channel,
                                     const char* data, size_t size,
                                     const talk_base::PacketTime& packet_time,
                                     int flags) {
  //LOG_F(LS_VERBOSE) << "(" << size << ")";
  ASSERT(worker_thread_->IsCurrent());
  CritScope lock(&cs_);
  if (!channel_) {
    LOG_F(LS_WARNING) << "NULL channel";
    return;
  }
  ASSERT(channel == channel_);
  if (!tcp_) {
    LOG_F(LS_WARNING) << "NULL tcp";
    return;
  }
  tcp_->NotifyPacket(data, size);
  AdjustClock();
}

void PseudoTcpChannel::OnChannelConnectionChanged(TransportChannel* channel,
                                                  const Candidate& candidate) {
  LOG_F(LS_VERBOSE) << "[" << channel_name_ << "]";
  ASSERT(worker_thread_->IsCurrent());
  CritScope lock(&cs_);
  if (!channel_) {
    LOG_F(LS_WARNING) << "NULL channel";
    return;
  }
  ASSERT(channel == channel_);
  if (!tcp_) {
    LOG_F(LS_WARNING) << "NULL tcp";
    return;
  }

  uint16 mtu = 1280;  // safe default
  int family = candidate.address().family();
  Socket* socket =
      worker_thread_->socketserver()->CreateAsyncSocket(family, SOCK_DGRAM);
  talk_base::scoped_ptr<Socket> mtu_socket(socket);
  if (socket == NULL) {
    LOG_F(LS_WARNING) << "Couldn't create socket while estimating MTU.";
  } else {
    if (mtu_socket->Connect(candidate.address()) < 0 ||
        mtu_socket->EstimateMTU(&mtu) < 0) {
      LOG_F(LS_WARNING) << "Failed to estimate MTU, error="
                        << mtu_socket->GetError();
    }
  }

  LOG_F(LS_VERBOSE) << "Using MTU of " << mtu << " bytes";
  tcp_->NotifyMTU(mtu);
  AdjustClock();
}

void PseudoTcpChannel::OnTcpOpen(PseudoTcp* tcp) {
  LOG_F(LS_VERBOSE) << "[" << channel_name_ << "]";
  ASSERT(cs_.CurrentThreadIsOwner());
  ASSERT(worker_thread_->IsCurrent());
  ASSERT(tcp == tcp_);
  if (stream_) {
    stream_readable_ = true;
    pending_read_event_ = true;
    stream_thread_->Post(this, MSG_ST_EVENT,
                         new EventData(SE_OPEN | SE_READ | SE_WRITE));
  }
}

void PseudoTcpChannel::OnTcpReadable(PseudoTcp* tcp) {
  //LOG_F(LS_VERBOSE);
  ASSERT(cs_.CurrentThreadIsOwner());
  ASSERT(worker_thread_->IsCurrent());
  ASSERT(tcp == tcp_);
  if (stream_) {
    stream_readable_ = true;
    if (!pending_read_event_) {
      pending_read_event_ = true;
      stream_thread_->Post(this, MSG_ST_EVENT, new EventData(SE_READ));
    }
  }
}

void PseudoTcpChannel::OnTcpWriteable(PseudoTcp* tcp) {
  //LOG_F(LS_VERBOSE);
  ASSERT(cs_.CurrentThreadIsOwner());
  ASSERT(worker_thread_->IsCurrent());
  ASSERT(tcp == tcp_);
  if (stream_)
    stream_thread_->Post(this, MSG_ST_EVENT, new EventData(SE_WRITE));
}

void PseudoTcpChannel::OnTcpClosed(PseudoTcp* tcp, uint32 nError) {
  LOG_F(LS_VERBOSE) << "[" << channel_name_ << "]";
  ASSERT(cs_.CurrentThreadIsOwner());
  ASSERT(worker_thread_->IsCurrent());
  ASSERT(tcp == tcp_);
  if (stream_)
    stream_thread_->Post(this, MSG_ST_EVENT, new EventData(SE_CLOSE, nError));
}

//
// Multi-thread methods
//

void PseudoTcpChannel::OnMessage(Message* pmsg) {
  if (pmsg->message_id == MSG_WK_CLOCK) {

    ASSERT(worker_thread_->IsCurrent());
    //LOG(LS_INFO) << "PseudoTcpChannel::OnMessage(MSG_WK_CLOCK)";
    CritScope lock(&cs_);
    if (tcp_) {
      tcp_->NotifyClock(PseudoTcp::Now());
      AdjustClock(false);
    }

  } else if (pmsg->message_id == MSG_WK_PURGE) {

    ASSERT(worker_thread_->IsCurrent());
    LOG_F(LS_INFO) << "(MSG_WK_PURGE)";
    // At this point, we know there are no additional worker thread messages.
    CritScope lock(&cs_);
    ASSERT(NULL == session_);
    ASSERT(NULL == channel_);
    worker_thread_ = NULL;
    CheckDestroy();

  } else if (pmsg->message_id == MSG_ST_EVENT) {

    ASSERT(stream_thread_->IsCurrent());
    //LOG(LS_INFO) << "PseudoTcpChannel::OnMessage(MSG_ST_EVENT, "
    //             << data->event << ", " << data->error << ")";
    ASSERT(stream_ != NULL);
    EventData* data = static_cast<EventData*>(pmsg->pdata);
    if (data->event & SE_READ) {
      CritScope lock(&cs_);
      pending_read_event_ = false;
    }
    stream_->SignalEvent(stream_, data->event, data->error);
    delete data;

  } else if (pmsg->message_id == MSG_SI_DESTROYCHANNEL) {

    ASSERT(signal_thread_->IsCurrent());
    LOG_F(LS_INFO) << "(MSG_SI_DESTROYCHANNEL)";
    ASSERT(session_ != NULL);
    ASSERT(channel_ != NULL);
    session_->DestroyChannel(content_name_, channel_->component());

  } else if (pmsg->message_id == MSG_SI_DESTROY) {

    ASSERT(signal_thread_->IsCurrent());
    LOG_F(LS_INFO) << "(MSG_SI_DESTROY)";
    // The message queue is empty, so it is safe to destroy ourselves.
    delete this;

  } else {
    ASSERT(false);
  }
}

IPseudoTcpNotify::WriteResult PseudoTcpChannel::TcpWritePacket(
    PseudoTcp* tcp, const char* buffer, size_t len) {
  ASSERT(cs_.CurrentThreadIsOwner());
  ASSERT(tcp == tcp_);
  ASSERT(NULL != channel_);
  int sent = channel_->SendPacket(buffer, len, talk_base::DSCP_NO_CHANGE);
  if (sent > 0) {
    //LOG_F(LS_VERBOSE) << "(" << sent << ") Sent";
    return IPseudoTcpNotify::WR_SUCCESS;
  } else if (IsBlockingError(channel_->GetError())) {
    LOG_F(LS_VERBOSE) << "Blocking";
    return IPseudoTcpNotify::WR_SUCCESS;
  } else if (channel_->GetError() == EMSGSIZE) {
    LOG_F(LS_ERROR) << "EMSGSIZE";
    return IPseudoTcpNotify::WR_TOO_LARGE;
  } else {
    PLOG(LS_ERROR, channel_->GetError()) << "PseudoTcpChannel::TcpWritePacket";
    ASSERT(false);
    return IPseudoTcpNotify::WR_FAIL;
  }
}

void PseudoTcpChannel::AdjustClock(bool clear) {
  ASSERT(cs_.CurrentThreadIsOwner());
  ASSERT(NULL != tcp_);

  long timeout = 0;
  if (tcp_->GetNextClock(PseudoTcp::Now(), timeout)) {
    ASSERT(NULL != channel_);
    // Reset the next clock, by clearing the old and setting a new one.
    if (clear)
      worker_thread_->Clear(this, MSG_WK_CLOCK);
    worker_thread_->PostDelayed(_max(timeout, 0L), this, MSG_WK_CLOCK);
    return;
  }

  delete tcp_;
  tcp_ = NULL;
  ready_to_connect_ = false;

  if (channel_) {
    // If TCP has failed, no need for channel_ anymore
    signal_thread_->Post(this, MSG_SI_DESTROYCHANNEL);
  }
}

void PseudoTcpChannel::CheckDestroy() {
  ASSERT(cs_.CurrentThreadIsOwner());
  if ((worker_thread_ != NULL) || (stream_ != NULL))
    return;
  signal_thread_->Post(this, MSG_SI_DESTROY);
}

///////////////////////////////////////////////////////////////////////////////
// PseudoTcpChannel::InternalStream
///////////////////////////////////////////////////////////////////////////////

PseudoTcpChannel::InternalStream::InternalStream(PseudoTcpChannel* parent)
  : parent_(parent) {
}

PseudoTcpChannel::InternalStream::~InternalStream() {
  Close();
}

StreamState PseudoTcpChannel::InternalStream::GetState() const {
  if (!parent_)
    return SS_CLOSED;
  return parent_->GetState();
}

StreamResult PseudoTcpChannel::InternalStream::Read(
    void* buffer, size_t buffer_len, size_t* read, int* error) {
  if (!parent_) {
    if (error)
      *error = ENOTCONN;
    return SR_ERROR;
  }
  return parent_->Read(buffer, buffer_len, read, error);
}

StreamResult PseudoTcpChannel::InternalStream::Write(
    const void* data, size_t data_len,  size_t* written, int* error) {
  if (!parent_) {
    if (error)
      *error = ENOTCONN;
    return SR_ERROR;
  }
  return parent_->Write(data, data_len, written, error);
}

void PseudoTcpChannel::InternalStream::Close() {
  if (!parent_)
    return;
  parent_->Close();
  parent_ = NULL;
}

///////////////////////////////////////////////////////////////////////////////

} // namespace cricket
