/*
 * libjingle
 * Copyright 2012, Google Inc.
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
#include "talk/app/webrtc/datachannel.h"

#include <string>

#include "talk/app/webrtc/mediastreamprovider.h"
#include "talk/app/webrtc/sctputils.h"
#include "talk/base/logging.h"
#include "talk/base/refcount.h"

namespace webrtc {

static size_t kMaxQueuedReceivedDataPackets = 100;
static size_t kMaxQueuedSendDataPackets = 100;

enum {
  MSG_CHANNELREADY,
};

talk_base::scoped_refptr<DataChannel> DataChannel::Create(
    DataChannelProviderInterface* provider,
    cricket::DataChannelType dct,
    const std::string& label,
    const InternalDataChannelInit& config) {
  talk_base::scoped_refptr<DataChannel> channel(
      new talk_base::RefCountedObject<DataChannel>(provider, dct, label));
  if (!channel->Init(config)) {
    return NULL;
  }
  return channel;
}

DataChannel::DataChannel(
    DataChannelProviderInterface* provider,
    cricket::DataChannelType dct,
    const std::string& label)
    : label_(label),
      observer_(NULL),
      state_(kConnecting),
      data_channel_type_(dct),
      provider_(provider),
      waiting_for_open_ack_(false),
      was_ever_writable_(false),
      connected_to_provider_(false),
      send_ssrc_set_(false),
      receive_ssrc_set_(false),
      send_ssrc_(0),
      receive_ssrc_(0) {
}

bool DataChannel::Init(const InternalDataChannelInit& config) {
  if (data_channel_type_ == cricket::DCT_RTP &&
      (config.reliable ||
       config.id != -1 ||
       config.maxRetransmits != -1 ||
       config.maxRetransmitTime != -1)) {
    LOG(LS_ERROR) << "Failed to initialize the RTP data channel due to "
                  << "invalid DataChannelInit.";
    return false;
  } else if (data_channel_type_ == cricket::DCT_SCTP) {
    if (config.id < -1 ||
        config.maxRetransmits < -1 ||
        config.maxRetransmitTime < -1) {
      LOG(LS_ERROR) << "Failed to initialize the SCTP data channel due to "
                    << "invalid DataChannelInit.";
      return false;
    }
    if (config.maxRetransmits != -1 && config.maxRetransmitTime != -1) {
      LOG(LS_ERROR) <<
          "maxRetransmits and maxRetransmitTime should not be both set.";
      return false;
    }
    config_ = config;

    // Try to connect to the transport in case the transport channel already
    // exists.
    OnTransportChannelCreated();

    // Checks if the transport is ready to send because the initial channel
    // ready signal may have been sent before the DataChannel creation.
    // This has to be done async because the upper layer objects (e.g.
    // Chrome glue and WebKit) are not wired up properly until after this
    // function returns.
    if (provider_->ReadyToSendData()) {
      talk_base::Thread::Current()->Post(this, MSG_CHANNELREADY, NULL);
    }
  }

  return true;
}

DataChannel::~DataChannel() {
  ClearQueuedReceivedData();
  ClearQueuedSendData();
  ClearQueuedControlData();
}

void DataChannel::RegisterObserver(DataChannelObserver* observer) {
  observer_ = observer;
  DeliverQueuedReceivedData();
}

void DataChannel::UnregisterObserver() {
  observer_ = NULL;
}

bool DataChannel::reliable() const {
  if (data_channel_type_ == cricket::DCT_RTP) {
    return false;
  } else {
    return config_.maxRetransmits == -1 &&
           config_.maxRetransmitTime == -1;
  }
}

uint64 DataChannel::buffered_amount() const {
  uint64 buffered_amount = 0;
  for (std::deque<DataBuffer*>::const_iterator it = queued_send_data_.begin();
      it != queued_send_data_.end();
      ++it) {
    buffered_amount += (*it)->size();
  }
  return buffered_amount;
}

void DataChannel::Close() {
  if (state_ == kClosed)
    return;
  send_ssrc_ = 0;
  send_ssrc_set_ = false;
  SetState(kClosing);
  UpdateState();
}

bool DataChannel::Send(const DataBuffer& buffer) {
  if (state_ != kOpen) {
    return false;
  }
  // If the queue is non-empty, we're waiting for SignalReadyToSend,
  // so just add to the end of the queue and keep waiting.
  if (!queued_send_data_.empty()) {
    return QueueSendData(buffer);
  }

  cricket::SendDataResult send_result;
  if (!InternalSendWithoutQueueing(buffer, &send_result)) {
    if (send_result == cricket::SDR_BLOCK) {
      return QueueSendData(buffer);
    }
    // Fail for other results.
    // TODO(jiayl): We should close the data channel in this case.
    return false;
  }
  return true;
}

void DataChannel::QueueControl(const talk_base::Buffer* buffer) {
  queued_control_data_.push(buffer);
}

bool DataChannel::SendOpenMessage(const talk_base::Buffer* raw_buffer) {
  ASSERT(data_channel_type_ == cricket::DCT_SCTP &&
         was_ever_writable_ &&
         config_.id >= 0 &&
         !config_.negotiated);

  talk_base::scoped_ptr<const talk_base::Buffer> buffer(raw_buffer);

  cricket::SendDataParams send_params;
  send_params.ssrc = config_.id;
  send_params.ordered = true;
  send_params.type = cricket::DMT_CONTROL;

  cricket::SendDataResult send_result;
  bool retval = provider_->SendData(send_params, *buffer, &send_result);
  if (retval) {
    LOG(LS_INFO) << "Sent OPEN message on channel " << config_.id;
    // Send data as ordered before we receive any mesage from the remote peer
    // to make sure the remote peer will not receive any data before it receives
    // the OPEN message.
    waiting_for_open_ack_ = true;
  } else if (send_result == cricket::SDR_BLOCK) {
    // Link is congested.  Queue for later.
    QueueControl(buffer.release());
  } else {
    LOG(LS_ERROR) << "Failed to send OPEN message with result "
                  << send_result << " on channel " << config_.id;
  }
  return retval;
}

bool DataChannel::SendOpenAckMessage(const talk_base::Buffer* raw_buffer) {
  ASSERT(data_channel_type_ == cricket::DCT_SCTP &&
         was_ever_writable_ &&
         config_.id >= 0);

  talk_base::scoped_ptr<const talk_base::Buffer> buffer(raw_buffer);

  cricket::SendDataParams send_params;
  send_params.ssrc = config_.id;
  send_params.ordered = config_.ordered;
  send_params.type = cricket::DMT_CONTROL;

  cricket::SendDataResult send_result;
  bool retval = provider_->SendData(send_params, *buffer, &send_result);
  if (retval) {
    LOG(LS_INFO) << "Sent OPEN_ACK message on channel " << config_.id;
  } else if (send_result == cricket::SDR_BLOCK) {
    // Link is congested.  Queue for later.
    QueueControl(buffer.release());
  } else {
    LOG(LS_ERROR) << "Failed to send OPEN_ACK message with result "
                  << send_result << " on channel " << config_.id;
  }
  return retval;
}

void DataChannel::SetReceiveSsrc(uint32 receive_ssrc) {
  ASSERT(data_channel_type_ == cricket::DCT_RTP);

  if (receive_ssrc_set_) {
    return;
  }
  receive_ssrc_ = receive_ssrc;
  receive_ssrc_set_ = true;
  UpdateState();
}

// The remote peer request that this channel shall be closed.
void DataChannel::RemotePeerRequestClose() {
  DoClose();
}

void DataChannel::SetSendSsrc(uint32 send_ssrc) {
  ASSERT(data_channel_type_ == cricket::DCT_RTP);
  if (send_ssrc_set_) {
    return;
  }
  send_ssrc_ = send_ssrc;
  send_ssrc_set_ = true;
  UpdateState();
}

void DataChannel::OnMessage(talk_base::Message* msg) {
  switch (msg->message_id) {
    case MSG_CHANNELREADY:
      OnChannelReady(true);
      break;
  }
}

// The underlaying data engine is closing.
// This function makes sure the DataChannel is disconnected and changes state to
// kClosed.
void DataChannel::OnDataEngineClose() {
  DoClose();
}

void DataChannel::OnDataReceived(cricket::DataChannel* channel,
                                 const cricket::ReceiveDataParams& params,
                                 const talk_base::Buffer& payload) {
  uint32 expected_ssrc =
      (data_channel_type_ == cricket::DCT_RTP) ? receive_ssrc_ : config_.id;
  if (params.ssrc != expected_ssrc) {
    return;
  }

  if (params.type == cricket::DMT_CONTROL) {
    ASSERT(data_channel_type_ == cricket::DCT_SCTP);
    if (!waiting_for_open_ack_) {
      // Ignore it if we are not expecting an ACK message.
      LOG(LS_WARNING) << "DataChannel received unexpected CONTROL message, "
                      << "sid = " << params.ssrc;
      return;
    }
    if (ParseDataChannelOpenAckMessage(payload)) {
      // We can send unordered as soon as we receive the ACK message.
      waiting_for_open_ack_ = false;
      LOG(LS_INFO) << "DataChannel received OPEN_ACK message, sid = "
                   << params.ssrc;
    } else {
      LOG(LS_WARNING) << "DataChannel failed to parse OPEN_ACK message, sid = "
                      << params.ssrc;
    }
    return;
  }

  ASSERT(params.type == cricket::DMT_BINARY ||
         params.type == cricket::DMT_TEXT);

  LOG(LS_VERBOSE) << "DataChannel received DATA message, sid = " << params.ssrc;
  // We can send unordered as soon as we receive any DATA message since the
  // remote side must have received the OPEN (and old clients do not send
  // OPEN_ACK).
  waiting_for_open_ack_ = false;

  bool binary = (params.type == cricket::DMT_BINARY);
  talk_base::scoped_ptr<DataBuffer> buffer(new DataBuffer(payload, binary));
  if (was_ever_writable_ && observer_) {
    observer_->OnMessage(*buffer.get());
  } else {
    if (queued_received_data_.size() > kMaxQueuedReceivedDataPackets) {
      // TODO(jiayl): We should close the data channel in this case.
      LOG(LS_ERROR)
          << "Queued received data exceeds the max number of packes.";
      ClearQueuedReceivedData();
    }
    queued_received_data_.push(buffer.release());
  }
}

void DataChannel::OnChannelReady(bool writable) {
  if (!writable) {
    return;
  }
  // Update the readyState and send the queued control message if the channel
  // is writable for the first time; otherwise it means the channel was blocked
  // for sending and now unblocked, so send the queued data now.
  if (!was_ever_writable_) {
    was_ever_writable_ = true;

    if (data_channel_type_ == cricket::DCT_SCTP) {
      if (config_.open_handshake_role == InternalDataChannelInit::kOpener) {
        talk_base::Buffer* payload = new talk_base::Buffer;
        WriteDataChannelOpenMessage(label_, config_, payload);
        SendOpenMessage(payload);
      } else if (config_.open_handshake_role ==
                 InternalDataChannelInit::kAcker) {
        talk_base::Buffer* payload = new talk_base::Buffer;
        WriteDataChannelOpenAckMessage(payload);
        SendOpenAckMessage(payload);
      }
    }

    UpdateState();
    ASSERT(queued_send_data_.empty());
  } else if (state_ == kOpen) {
    DeliverQueuedSendData();
  }
}

void DataChannel::DoClose() {
  receive_ssrc_set_ = false;
  send_ssrc_set_ = false;
  SetState(kClosing);
  UpdateState();
}

void DataChannel::UpdateState() {
  switch (state_) {
    case kConnecting: {
      if (send_ssrc_set_ == receive_ssrc_set_) {
        if (data_channel_type_ == cricket::DCT_RTP && !connected_to_provider_) {
          connected_to_provider_ = provider_->ConnectDataChannel(this);
        }
        if (was_ever_writable_) {
          // TODO(jiayl): Do not transition to kOpen if we failed to send the
          // OPEN message.
          DeliverQueuedControlData();
          SetState(kOpen);
          // If we have received buffers before the channel got writable.
          // Deliver them now.
          DeliverQueuedReceivedData();
        }
      }
      break;
    }
    case kOpen: {
      break;
    }
    case kClosing: {
      DisconnectFromTransport();

      if (!send_ssrc_set_ && !receive_ssrc_set_) {
        SetState(kClosed);
      }
      break;
    }
    case kClosed:
      break;
  }
}

void DataChannel::SetState(DataState state) {
  state_ = state;
  if (observer_) {
    observer_->OnStateChange();
  }
}

void DataChannel::DisconnectFromTransport() {
  if (!connected_to_provider_)
    return;

  provider_->DisconnectDataChannel(this);
  connected_to_provider_ = false;

  if (data_channel_type_ == cricket::DCT_SCTP) {
    provider_->RemoveSctpDataStream(config_.id);
  }
}

void DataChannel::DeliverQueuedReceivedData() {
  if (!was_ever_writable_ || !observer_) {
    return;
  }

  while (!queued_received_data_.empty()) {
    DataBuffer* buffer = queued_received_data_.front();
    observer_->OnMessage(*buffer);
    queued_received_data_.pop();
    delete buffer;
  }
}

void DataChannel::ClearQueuedReceivedData() {
  while (!queued_received_data_.empty()) {
    DataBuffer* buffer = queued_received_data_.front();
    queued_received_data_.pop();
    delete buffer;
  }
}

void DataChannel::DeliverQueuedSendData() {
  ASSERT(was_ever_writable_ && state_ == kOpen);

  // TODO(jiayl): Sending OPEN message here contradicts with the pre-condition
  // that the readyState is open. According to the standard, the channel should
  // not become open before the OPEN message is sent.
  DeliverQueuedControlData();

  while (!queued_send_data_.empty()) {
    DataBuffer* buffer = queued_send_data_.front();
    cricket::SendDataResult send_result;
    if (!InternalSendWithoutQueueing(*buffer, &send_result)) {
      LOG(LS_WARNING) << "DeliverQueuedSendData aborted due to send_result "
                      << send_result;
      break;
    }
    queued_send_data_.pop_front();
    delete buffer;
  }
}

void DataChannel::ClearQueuedControlData() {
  while (!queued_control_data_.empty()) {
    const talk_base::Buffer *buf = queued_control_data_.front();
    queued_control_data_.pop();
    delete buf;
  }
}

void DataChannel::DeliverQueuedControlData() {
  ASSERT(was_ever_writable_);
  while (!queued_control_data_.empty()) {
    const talk_base::Buffer* buf = queued_control_data_.front();
    queued_control_data_.pop();
    if (config_.open_handshake_role == InternalDataChannelInit::kOpener) {
      SendOpenMessage(buf);
    } else {
      ASSERT(config_.open_handshake_role == InternalDataChannelInit::kAcker);
      SendOpenAckMessage(buf);
    }
  }
}

void DataChannel::ClearQueuedSendData() {
  while (!queued_send_data_.empty()) {
    DataBuffer* buffer = queued_send_data_.front();
    queued_send_data_.pop_front();
    delete buffer;
  }
}

bool DataChannel::InternalSendWithoutQueueing(
    const DataBuffer& buffer, cricket::SendDataResult* send_result) {
  cricket::SendDataParams send_params;

  if (data_channel_type_ == cricket::DCT_SCTP) {
    send_params.ordered = config_.ordered;
    // Send as ordered if it is waiting for the OPEN_ACK message.
    if (waiting_for_open_ack_ && !config_.ordered) {
      send_params.ordered = true;
      LOG(LS_VERBOSE) << "Sending data as ordered for unordered DataChannel "
                      << "because the OPEN_ACK message has not been received.";
    }

    send_params.max_rtx_count = config_.maxRetransmits;
    send_params.max_rtx_ms = config_.maxRetransmitTime;
    send_params.ssrc = config_.id;
  } else {
    send_params.ssrc = send_ssrc_;
  }
  send_params.type = buffer.binary ? cricket::DMT_BINARY : cricket::DMT_TEXT;

  return provider_->SendData(send_params, buffer.data, send_result);
}

bool DataChannel::QueueSendData(const DataBuffer& buffer) {
  if (queued_send_data_.size() > kMaxQueuedSendDataPackets) {
    LOG(LS_ERROR) << "Can't buffer any more data in the data channel.";
    return false;
  }
  queued_send_data_.push_back(new DataBuffer(buffer));
  return true;
}

void DataChannel::SetSctpSid(int sid) {
  ASSERT(config_.id < 0 && sid >= 0 && data_channel_type_ == cricket::DCT_SCTP);
  config_.id = sid;
  provider_->AddSctpDataStream(sid);
}

void DataChannel::OnTransportChannelCreated() {
  ASSERT(data_channel_type_ == cricket::DCT_SCTP);
  if (!connected_to_provider_) {
    connected_to_provider_ = provider_->ConnectDataChannel(this);
  }
  // The sid may have been unassigned when provider_->ConnectDataChannel was
  // done. So always add the streams even if connected_to_provider_ is true.
  if (config_.id >= 0) {
    provider_->AddSctpDataStream(config_.id);
  }
}

}  // namespace webrtc
