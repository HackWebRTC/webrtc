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

#ifndef TALK_APP_WEBRTC_DATACHANNEL_H_
#define TALK_APP_WEBRTC_DATACHANNEL_H_

#include <string>
#include <queue>

#include "talk/app/webrtc/datachannelinterface.h"
#include "talk/app/webrtc/proxy.h"
#include "talk/base/scoped_ref_ptr.h"
#include "talk/base/sigslot.h"
#include "talk/session/media/channel.h"

namespace webrtc {

class WebRtcSession;

// DataChannel is a an implementation of the DataChannelInterface based on
// libjingle's data engine. It provides an implementation of unreliable data
// channels. Currently this class is specifically designed to use RtpDataEngine,
// and will changed to use SCTP in the future.

// DataChannel states:
// kConnecting: The channel has been created but SSRC for sending and receiving
//              has not yet been set and the transport might not yet be ready.
// kOpen: The channel have a local SSRC set by a call to UpdateSendSsrc
//        and a remote SSRC set by call to UpdateReceiveSsrc and the transport
//        has been writable once.
// kClosing: DataChannelInterface::Close has been called or UpdateReceiveSsrc
//           has been called with SSRC==0
// kClosed: Both UpdateReceiveSsrc and UpdateSendSsrc has been called with
//          SSRC==0.
class DataChannel : public DataChannelInterface,
                    public sigslot::has_slots<> {
 public:
  static talk_base::scoped_refptr<DataChannel> Create(
      WebRtcSession* session,
      const std::string& label,
      const DataChannelInit* config);

  virtual void RegisterObserver(DataChannelObserver* observer);
  virtual void UnregisterObserver();

  virtual std::string label() const { return label_; }
  virtual bool reliable() const;
  virtual bool ordered() const { return config_.ordered; }
  virtual uint16 maxRetransmitTime() const {
    return config_.maxRetransmitTime;
  }
  virtual uint16 maxRetransmits() const {
    return config_.maxRetransmits;
  }
  virtual std::string protocol() const { return config_.protocol; }
  virtual bool negotiated() const { return config_.negotiated; }
  virtual int id() const { return config_.id; }
  virtual uint64 buffered_amount() const;
  virtual void Close();
  virtual DataState state() const { return state_; }
  virtual bool Send(const DataBuffer& buffer);
  // Send a control message right now, or queue for later.
  virtual bool SendControl(const talk_base::Buffer* buffer);
  void ConnectToDataSession();

  // Set the SSRC this channel should use to receive data from the
  // underlying data engine.
  void SetReceiveSsrc(uint32 receive_ssrc);
  // The remote peer request that this channel should be closed.
  void RemotePeerRequestClose();

  // Set the SSRC this channel should use to send data on the
  // underlying data engine. |send_ssrc| == 0 means that the channel is no
  // longer part of the session negotiation.
  void SetSendSsrc(uint32 send_ssrc);

  // Called if the underlying data engine is closing.
  void OnDataEngineClose();

  // Called when the channel's ready to use.  That can happen when the
  // underlying DataMediaChannel becomes ready, or when this channel is a new
  // stream on an existing DataMediaChannel, and we've finished negotiation.
  void OnChannelReady(bool writable);
 protected:
  DataChannel(WebRtcSession* session, const std::string& label);
  virtual ~DataChannel();

  bool Init(const DataChannelInit* config);
  bool HasNegotiationCompleted();

  // Sigslots from cricket::DataChannel
  void OnDataReceived(cricket::DataChannel* channel,
                      const cricket::ReceiveDataParams& params,
                      const talk_base::Buffer& payload);

 private:
  void DoClose();
  void UpdateState();
  void SetState(DataState state);
  void DisconnectFromDataSession();
  bool IsConnectedToDataSession() { return data_session_ != NULL; }
  void DeliverQueuedControlData();
  void QueueControl(const talk_base::Buffer* buffer);
  void ClearQueuedControlData();
  void DeliverQueuedReceivedData();
  void ClearQueuedReceivedData();
  void DeliverQueuedSendData();
  void ClearQueuedSendData();
  bool InternalSendWithoutQueueing(const DataBuffer& buffer,
                                   cricket::SendDataResult* send_result);
  bool QueueSendData(const DataBuffer& buffer);

  std::string label_;
  DataChannelInit config_;
  DataChannelObserver* observer_;
  DataState state_;
  bool was_ever_writable_;
  WebRtcSession* session_;
  cricket::DataChannel* data_session_;
  bool send_ssrc_set_;
  uint32 send_ssrc_;
  bool receive_ssrc_set_;
  uint32 receive_ssrc_;
  // Control messages that always have to get sent out before any queued
  // data.
  std::queue<const talk_base::Buffer*> queued_control_data_;
  std::queue<DataBuffer*> queued_received_data_;
  std::deque<DataBuffer*> queued_send_data_;
};

class DataChannelFactory {
 public:
  virtual talk_base::scoped_refptr<DataChannel> CreateDataChannel(
      const std::string& label,
      const DataChannelInit* config) = 0;

 protected:
  virtual ~DataChannelFactory() {}
};

// Define proxy for DataChannelInterface.
BEGIN_PROXY_MAP(DataChannel)
  PROXY_METHOD1(void, RegisterObserver, DataChannelObserver*)
  PROXY_METHOD0(void, UnregisterObserver)
  PROXY_CONSTMETHOD0(std::string, label)
  PROXY_CONSTMETHOD0(bool, reliable)
  PROXY_CONSTMETHOD0(bool, ordered)
  PROXY_CONSTMETHOD0(uint16, maxRetransmitTime)
  PROXY_CONSTMETHOD0(uint16, maxRetransmits)
  PROXY_CONSTMETHOD0(std::string, protocol)
  PROXY_CONSTMETHOD0(bool, negotiated)
  PROXY_CONSTMETHOD0(int, id)
  PROXY_CONSTMETHOD0(DataState, state)
  PROXY_CONSTMETHOD0(uint64, buffered_amount)
  PROXY_METHOD0(void, Close)
  PROXY_METHOD1(bool, Send, const DataBuffer&)
END_PROXY()

}  // namespace webrtc

#endif  // TALK_APP_WEBRTC_DATACHANNEL_H_
