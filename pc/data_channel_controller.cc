/*
 *  Copyright 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

// This file contains the implementation of the class
// webrtc::PeerConnection::DataChannelController.
//
// The intent is that this should be webrtc::DataChannelController, but
// as a migration stage, it is simpler to have it as an inner class,
// declared in the header file pc/peer_connection.h

#include "pc/peer_connection.h"
#include "pc/sctp_utils.h"

namespace webrtc {

bool PeerConnection::DataChannelController::SendData(
    const cricket::SendDataParams& params,
    const rtc::CopyOnWriteBuffer& payload,
    cricket::SendDataResult* result) {
  // RTC_DCHECK_RUN_ON(signaling_thread());
  if (data_channel_transport()) {
    SendDataParams send_params;
    send_params.type = ToWebrtcDataMessageType(params.type);
    send_params.ordered = params.ordered;
    if (params.max_rtx_count >= 0) {
      send_params.max_rtx_count = params.max_rtx_count;
    } else if (params.max_rtx_ms >= 0) {
      send_params.max_rtx_ms = params.max_rtx_ms;
    }

    RTCError error = network_thread()->Invoke<RTCError>(
        RTC_FROM_HERE, [this, params, send_params, payload] {
          return data_channel_transport()->SendData(params.sid, send_params,
                                                    payload);
        });

    if (error.ok()) {
      *result = cricket::SendDataResult::SDR_SUCCESS;
      return true;
    } else if (error.type() == RTCErrorType::RESOURCE_EXHAUSTED) {
      // SCTP transport uses RESOURCE_EXHAUSTED when it's blocked.
      // TODO(mellem):  Stop using RTCError here and get rid of the mapping.
      *result = cricket::SendDataResult::SDR_BLOCK;
      return false;
    }
    *result = cricket::SendDataResult::SDR_ERROR;
    return false;
  } else if (rtp_data_channel()) {
    return rtp_data_channel()->SendData(params, payload, result);
  }
  RTC_LOG(LS_ERROR) << "SendData called before transport is ready";
  return false;
}

bool PeerConnection::DataChannelController::ConnectDataChannel(
    DataChannel* webrtc_data_channel) {
  RTC_DCHECK_RUN_ON(signaling_thread());
  if (!rtp_data_channel() && !data_channel_transport()) {
    // Don't log an error here, because DataChannels are expected to call
    // ConnectDataChannel in this state. It's the only way to initially tell
    // whether or not the underlying transport is ready.
    return false;
  }
  if (data_channel_transport()) {
    SignalDataChannelTransportWritable_s.connect(webrtc_data_channel,
                                                 &DataChannel::OnChannelReady);
    SignalDataChannelTransportReceivedData_s.connect(
        webrtc_data_channel, &DataChannel::OnDataReceived);
    SignalDataChannelTransportChannelClosing_s.connect(
        webrtc_data_channel, &DataChannel::OnClosingProcedureStartedRemotely);
    SignalDataChannelTransportChannelClosed_s.connect(
        webrtc_data_channel, &DataChannel::OnClosingProcedureComplete);
  }
  if (rtp_data_channel()) {
    rtp_data_channel()->SignalReadyToSendData.connect(
        webrtc_data_channel, &DataChannel::OnChannelReady);
    rtp_data_channel()->SignalDataReceived.connect(
        webrtc_data_channel, &DataChannel::OnDataReceived);
  }
  return true;
}

void PeerConnection::DataChannelController::DisconnectDataChannel(
    DataChannel* webrtc_data_channel) {
  RTC_DCHECK_RUN_ON(signaling_thread());
  if (!rtp_data_channel() && !data_channel_transport()) {
    RTC_LOG(LS_ERROR)
        << "DisconnectDataChannel called when rtp_data_channel_ and "
           "sctp_transport_ are NULL.";
    return;
  }
  if (data_channel_transport()) {
    SignalDataChannelTransportWritable_s.disconnect(webrtc_data_channel);
    SignalDataChannelTransportReceivedData_s.disconnect(webrtc_data_channel);
    SignalDataChannelTransportChannelClosing_s.disconnect(webrtc_data_channel);
    SignalDataChannelTransportChannelClosed_s.disconnect(webrtc_data_channel);
  }
  if (rtp_data_channel()) {
    rtp_data_channel()->SignalReadyToSendData.disconnect(webrtc_data_channel);
    rtp_data_channel()->SignalDataReceived.disconnect(webrtc_data_channel);
  }
}

void PeerConnection::DataChannelController::AddSctpDataStream(int sid) {
  if (data_channel_transport()) {
    network_thread()->Invoke<void>(RTC_FROM_HERE, [this, sid] {
      if (data_channel_transport()) {
        data_channel_transport()->OpenChannel(sid);
      }
    });
  }
}

void PeerConnection::DataChannelController::RemoveSctpDataStream(int sid) {
  if (data_channel_transport()) {
    network_thread()->Invoke<void>(RTC_FROM_HERE, [this, sid] {
      if (data_channel_transport()) {
        data_channel_transport()->CloseChannel(sid);
      }
    });
  }
}

bool PeerConnection::DataChannelController::ReadyToSendData() const {
  RTC_DCHECK_RUN_ON(signaling_thread());
  return (rtp_data_channel() && rtp_data_channel()->ready_to_send_data()) ||
         (data_channel_transport() && data_channel_transport_ready_to_send_);
}

void PeerConnection::DataChannelController::OnDataReceived(
    int channel_id,
    DataMessageType type,
    const rtc::CopyOnWriteBuffer& buffer) {
  RTC_DCHECK_RUN_ON(network_thread());
  cricket::ReceiveDataParams params;
  params.sid = channel_id;
  params.type = ToCricketDataMessageType(type);
  data_channel_transport_invoker_->AsyncInvoke<void>(
      RTC_FROM_HERE, signaling_thread(), [this, params, buffer] {
        RTC_DCHECK_RUN_ON(signaling_thread());
        if (!HandleOpenMessage_s(params, buffer)) {
          SignalDataChannelTransportReceivedData_s(params, buffer);
        }
      });
}

void PeerConnection::DataChannelController::OnChannelClosing(int channel_id) {
  RTC_DCHECK_RUN_ON(network_thread());
  data_channel_transport_invoker_->AsyncInvoke<void>(
      RTC_FROM_HERE, signaling_thread(), [this, channel_id] {
        RTC_DCHECK_RUN_ON(signaling_thread());
        SignalDataChannelTransportChannelClosing_s(channel_id);
      });
}

void PeerConnection::DataChannelController::OnChannelClosed(int channel_id) {
  RTC_DCHECK_RUN_ON(network_thread());
  data_channel_transport_invoker_->AsyncInvoke<void>(
      RTC_FROM_HERE, signaling_thread(), [this, channel_id] {
        RTC_DCHECK_RUN_ON(signaling_thread());
        SignalDataChannelTransportChannelClosed_s(channel_id);
      });
}

void PeerConnection::DataChannelController::OnReadyToSend() {
  RTC_DCHECK_RUN_ON(network_thread());
  data_channel_transport_invoker_->AsyncInvoke<void>(
      RTC_FROM_HERE, signaling_thread(), [this] {
        RTC_DCHECK_RUN_ON(signaling_thread());
        data_channel_transport_ready_to_send_ = true;
        SignalDataChannelTransportWritable_s(
            data_channel_transport_ready_to_send_);
      });
}

void PeerConnection::DataChannelController::SetupDataChannelTransport_n() {
  RTC_DCHECK_RUN_ON(network_thread());
  data_channel_transport_invoker_ = std::make_unique<rtc::AsyncInvoker>();
}

void PeerConnection::DataChannelController::TeardownDataChannelTransport_n() {
  RTC_DCHECK_RUN_ON(network_thread());
  data_channel_transport_invoker_ = nullptr;
  if (data_channel_transport()) {
    data_channel_transport()->SetDataSink(nullptr);
  }
  set_data_channel_transport(nullptr);
}

void PeerConnection::DataChannelController::OnTransportChanged(
    DataChannelTransportInterface* new_data_channel_transport) {
  RTC_DCHECK_RUN_ON(network_thread());
  if (data_channel_transport() &&
      data_channel_transport() != new_data_channel_transport) {
    // Changed which data channel transport is used for |sctp_mid_| (eg. now
    // it's bundled).
    data_channel_transport()->SetDataSink(nullptr);
    set_data_channel_transport(new_data_channel_transport);
    if (new_data_channel_transport) {
      new_data_channel_transport->SetDataSink(this);

      // There's a new data channel transport.  This needs to be signaled to the
      // |sctp_data_channels_| so that they can reopen and reconnect.  This is
      // necessary when bundling is applied.
      data_channel_transport_invoker_->AsyncInvoke<void>(
          RTC_FROM_HERE, signaling_thread(), [this] {
            RTC_DCHECK_RUN_ON(pc_->signaling_thread());
            for (auto channel : pc_->sctp_data_channels_) {
              channel->OnTransportChannelCreated();
            }
          });
    }
  }
}

bool PeerConnection::DataChannelController::HandleOpenMessage_s(
    const cricket::ReceiveDataParams& params,
    const rtc::CopyOnWriteBuffer& buffer) {
  if (params.type == cricket::DMT_CONTROL && IsOpenMessage(buffer)) {
    // Received OPEN message; parse and signal that a new data channel should
    // be created.
    std::string label;
    InternalDataChannelInit config;
    config.id = params.ssrc;
    if (!ParseDataChannelOpenMessage(buffer, &label, &config)) {
      RTC_LOG(LS_WARNING) << "Failed to parse the OPEN message for ssrc "
                          << params.ssrc;
      return true;
    }
    config.open_handshake_role = InternalDataChannelInit::kAcker;
    OnDataChannelOpenMessage(label, config);
    return true;
  }
  return false;
}

void PeerConnection::DataChannelController::OnDataChannelOpenMessage(
    const std::string& label,
    const InternalDataChannelInit& config) {
  rtc::scoped_refptr<DataChannel> channel(
      InternalCreateDataChannel(label, &config));
  if (!channel.get()) {
    RTC_LOG(LS_ERROR) << "Failed to create DataChannel from the OPEN message.";
    return;
  }

  rtc::scoped_refptr<DataChannelInterface> proxy_channel =
      DataChannelProxy::Create(signaling_thread(), channel);
  {
    RTC_DCHECK_RUN_ON(pc_->signaling_thread());
    pc_->Observer()->OnDataChannel(std::move(proxy_channel));
    pc_->NoteUsageEvent(UsageEvent::DATA_ADDED);
  }
}

rtc::scoped_refptr<DataChannel>
PeerConnection::DataChannelController::InternalCreateDataChannel(
    const std::string& label,
    const InternalDataChannelInit* config) {
  RTC_DCHECK_RUN_ON(pc_->signaling_thread());
  if (pc_->IsClosed()) {
    return nullptr;
  }
  if (pc_->data_channel_type() == cricket::DCT_NONE) {
    RTC_LOG(LS_ERROR)
        << "InternalCreateDataChannel: Data is not supported in this call.";
    return nullptr;
  }
  InternalDataChannelInit new_config =
      config ? (*config) : InternalDataChannelInit();
  if (DataChannel::IsSctpLike(pc_->data_channel_type_)) {
    if (new_config.id < 0) {
      rtc::SSLRole role;
      if ((pc_->GetSctpSslRole(&role)) &&
          !sid_allocator_.AllocateSid(role, &new_config.id)) {
        RTC_LOG(LS_ERROR)
            << "No id can be allocated for the SCTP data channel.";
        return nullptr;
      }
    } else if (!sid_allocator_.ReserveSid(new_config.id)) {
      RTC_LOG(LS_ERROR) << "Failed to create a SCTP data channel "
                           "because the id is already in use or out of range.";
      return nullptr;
    }
  }

  rtc::scoped_refptr<DataChannel> channel(
      DataChannel::Create(this, pc_->data_channel_type(), label, new_config));
  if (!channel) {
    sid_allocator_.ReleaseSid(new_config.id);
    return nullptr;
  }

  if (channel->data_channel_type() == cricket::DCT_RTP) {
    if (pc_->rtp_data_channels_.find(channel->label()) !=
        pc_->rtp_data_channels_.end()) {
      RTC_LOG(LS_ERROR) << "DataChannel with label " << channel->label()
                        << " already exists.";
      return nullptr;
    }
    pc_->rtp_data_channels_[channel->label()] = channel;
  } else {
    RTC_DCHECK(DataChannel::IsSctpLike(pc_->data_channel_type_));
    pc_->sctp_data_channels_.push_back(channel);
    channel->SignalClosed.connect(pc_,
                                  &PeerConnection::OnSctpDataChannelClosed);
  }

  pc_->SignalDataChannelCreated_(channel.get());
  return channel;
}

void PeerConnection::DataChannelController::AllocateSctpSids(
    rtc::SSLRole role) {
  RTC_DCHECK_RUN_ON(pc_->signaling_thread());
  std::vector<rtc::scoped_refptr<DataChannel>> channels_to_close;
  for (const auto& channel : pc_->sctp_data_channels_) {
    if (channel->id() < 0) {
      int sid;
      if (!sid_allocator_.AllocateSid(role, &sid)) {
        RTC_LOG(LS_ERROR) << "Failed to allocate SCTP sid, closing channel.";
        channels_to_close.push_back(channel);
        continue;
      }
      channel->SetSctpSid(sid);
    }
  }
  // Since closing modifies the list of channels, we have to do the actual
  // closing outside the loop.
  for (const auto& channel : channels_to_close) {
    channel->CloseAbruptly();
  }
}

void PeerConnection::DataChannelController::OnSctpDataChannelClosed(
    DataChannel* channel) {
  RTC_DCHECK_RUN_ON(pc_->signaling_thread());
  for (auto it = pc_->sctp_data_channels_.begin();
       it != pc_->sctp_data_channels_.end(); ++it) {
    if (it->get() == channel) {
      if (channel->id() >= 0) {
        // After the closing procedure is done, it's safe to use this ID for
        // another data channel.
        sid_allocator_.ReleaseSid(channel->id());
      }
      // Since this method is triggered by a signal from the DataChannel,
      // we can't free it directly here; we need to free it asynchronously.
      pc_->sctp_data_channels_to_free_.push_back(*it);
      pc_->sctp_data_channels_.erase(it);
      pc_->SignalFreeDataChannels();
      return;
    }
  }
}

}  // namespace webrtc
