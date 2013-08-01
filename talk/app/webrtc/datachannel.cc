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

#include "talk/app/webrtc/webrtcsession.h"
#include "talk/base/logging.h"
#include "talk/base/refcount.h"

namespace webrtc {

static size_t kMaxQueuedReceivedDataPackets = 100;
static size_t kMaxQueuedSendDataPackets = 100;

talk_base::scoped_refptr<DataChannel> DataChannel::Create(
    WebRtcSession* session,
    const std::string& label,
    const DataChannelInit* config) {
  talk_base::scoped_refptr<DataChannel> channel(
      new talk_base::RefCountedObject<DataChannel>(session, label));
  if (!channel->Init(config)) {
    return NULL;
  }
  return channel;
}

DataChannel::DataChannel(WebRtcSession* session, const std::string& label)
    : label_(label),
      observer_(NULL),
      state_(kConnecting),
      was_ever_writable_(false),
      session_(session),
      data_session_(NULL),
      send_ssrc_set_(false),
      send_ssrc_(0),
      receive_ssrc_set_(false),
      receive_ssrc_(0) {
}

bool DataChannel::Init(const DataChannelInit* config) {
  if (config) {
    if (session_->data_channel_type() == cricket::DCT_RTP &&
        (config->reliable ||
         config->id != -1 ||
         config->maxRetransmits != -1 ||
         config->maxRetransmitTime != -1)) {
      LOG(LS_ERROR) << "Failed to initialize the RTP data channel due to "
                    << "invalid DataChannelInit.";
      return false;
    } else if (session_->data_channel_type() == cricket::DCT_SCTP) {
      if (config->id < -1 ||
          config->maxRetransmits < -1 ||
          config->maxRetransmitTime < -1) {
        LOG(LS_ERROR) << "Failed to initialize the SCTP data channel due to "
                      << "invalid DataChannelInit.";
        return false;
      }
      if (config->maxRetransmits != -1 && config->maxRetransmitTime != -1) {
        LOG(LS_ERROR) <<
            "maxRetransmits and maxRetransmitTime should not be both set.";
        return false;
      }
    }
    config_ = *config;
  }
  return true;
}

bool DataChannel::HasNegotiationCompleted() {
  return send_ssrc_set_ == receive_ssrc_set_;
}

DataChannel::~DataChannel() {
  ClearQueuedReceivedData();
  ClearQueuedSendData();
}

void DataChannel::RegisterObserver(DataChannelObserver* observer) {
  observer_ = observer;
  DeliverQueuedReceivedData();
}

void DataChannel::UnregisterObserver() {
  observer_ = NULL;
}

bool DataChannel::reliable() const {
  if (session_->data_channel_type() == cricket::DCT_RTP) {
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

void DataChannel::SetReceiveSsrc(uint32 receive_ssrc) {
  if (receive_ssrc_set_) {
    ASSERT(session_->data_channel_type() == cricket::DCT_RTP ||
        receive_ssrc_ == send_ssrc_);
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
  if (send_ssrc_set_) {
    ASSERT(session_->data_channel_type() == cricket::DCT_RTP ||
        receive_ssrc_ == send_ssrc_);
    return;
  }
  send_ssrc_ = send_ssrc;
  send_ssrc_set_ = true;
  UpdateState();
}

// The underlaying data engine is closing.
// This function make sure the DataChannel is disconneced and change state to
// kClosed.
void DataChannel::OnDataEngineClose() {
  DoClose();
}

void DataChannel::OnDataReceived(cricket::DataChannel* channel,
                                 const cricket::ReceiveDataParams& params,
                                 const talk_base::Buffer& payload) {
  if (params.ssrc != receive_ssrc_) {
    return;
  }

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
  // Update the readyState if the channel is writable for the first time;
  // otherwise it means the channel was blocked for sending and now unblocked,
  // so send the queued data now.
  if (!was_ever_writable_) {
    was_ever_writable_ = true;
    UpdateState();
  } else if (state_ == kOpen) {
    SendQueuedSendData();
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
      if (HasNegotiationCompleted()) {
        if (!IsConnectedToDataSession()) {
          ConnectToDataSession();
        }
        if (was_ever_writable_) {
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
      if (IsConnectedToDataSession()) {
        DisconnectFromDataSession();
      }
      if (HasNegotiationCompleted()) {
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

void DataChannel::ConnectToDataSession() {
  ASSERT(session_->data_channel() != NULL);
  if (!session_->data_channel()) {
    LOG(LS_ERROR) << "The DataEngine does not exist.";
    return;
  }

  data_session_ = session_->data_channel();
  data_session_->SignalReadyToSendData.connect(this,
                                               &DataChannel::OnChannelReady);
  data_session_->SignalDataReceived.connect(this, &DataChannel::OnDataReceived);
}

void DataChannel::DisconnectFromDataSession() {
  data_session_->SignalReadyToSendData.disconnect(this);
  data_session_->SignalDataReceived.disconnect(this);
  data_session_ = NULL;
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

void DataChannel::SendQueuedSendData() {
  if (!was_ever_writable_) {
    return;
  }

  while (!queued_send_data_.empty()) {
    DataBuffer* buffer = queued_send_data_.front();
    cricket::SendDataResult send_result;
    if (!InternalSendWithoutQueueing(*buffer, &send_result)) {
      LOG(LS_WARNING) << "SendQueuedSendData aborted due to send_result "
                      << send_result;
      break;
    }
    queued_send_data_.pop_front();
    delete buffer;
  }
}

void DataChannel::ClearQueuedSendData() {
  while (!queued_received_data_.empty()) {
    DataBuffer* buffer = queued_received_data_.front();
    queued_received_data_.pop();
    delete buffer;
  }
}

bool DataChannel::InternalSendWithoutQueueing(
    const DataBuffer& buffer, cricket::SendDataResult* send_result) {
  cricket::SendDataParams send_params;

  send_params.ssrc = send_ssrc_;
  if (session_->data_channel_type() == cricket::DCT_SCTP) {
    send_params.ordered = config_.ordered;
    send_params.max_rtx_count = config_.maxRetransmits;
    send_params.max_rtx_ms = config_.maxRetransmitTime;
  }
  send_params.type = buffer.binary ? cricket::DMT_BINARY : cricket::DMT_TEXT;

  return session_->data_channel()->SendData(send_params, buffer.data,
                                            send_result);
}

bool DataChannel::QueueSendData(const DataBuffer& buffer) {
  if (queued_send_data_.size() > kMaxQueuedSendDataPackets) {
    LOG(LS_ERROR) << "Can't buffer any more data in the data channel.";
    return false;
  }
  queued_send_data_.push_back(new DataBuffer(buffer));
  return true;
}

}  // namespace webrtc
