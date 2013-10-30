/*
 * libjingle
 * Copyright 2013, Google Inc.
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

class FakeDataChannelProvider : public webrtc::DataChannelProviderInterface {
 public:
  FakeDataChannelProvider()
      : id_allocation_should_fail_(false),
        send_blocked_(false),
        transport_available_(true) {}
  virtual ~FakeDataChannelProvider() {}

  virtual bool SendData(const cricket::SendDataParams& params,
                        const talk_base::Buffer& payload,
                        cricket::SendDataResult* result) OVERRIDE {
    if (send_blocked_) {
      *result = cricket::SDR_BLOCK;
      return false;
    }
    last_send_data_params_ = params;
    return true;
  }
  virtual bool ConnectDataChannel(webrtc::DataChannel* data_channel) OVERRIDE {
    ASSERT(connected_channels_.find(data_channel) == connected_channels_.end());
    if (!transport_available_) {
      return false;
    }
    LOG(LS_INFO) << "DataChannel connected " << data_channel;
    connected_channels_.insert(data_channel);
    return true;
  }
  virtual void DisconnectDataChannel(
      webrtc::DataChannel* data_channel) OVERRIDE {
    ASSERT(connected_channels_.find(data_channel) != connected_channels_.end());
    LOG(LS_INFO) << "DataChannel disconnected " << data_channel;
    connected_channels_.erase(data_channel);
  }
  virtual void AddRtpDataStream(uint32 send_ssrc, uint32 recv_ssrc) OVERRIDE {
    send_ssrcs_.insert(send_ssrc);
    recv_ssrcs_.insert(recv_ssrc);
  }
  virtual void AddSctpDataStream(uint32 sid) OVERRIDE {
    AddRtpDataStream(sid, sid);
  }
  virtual void RemoveRtpDataStream(
      uint32 send_ssrc, uint32 recv_ssrc) OVERRIDE {
    send_ssrcs_.erase(send_ssrc);
    recv_ssrcs_.erase(recv_ssrc);
  }
  virtual void RemoveSctpDataStream(uint32 sid) OVERRIDE {
    RemoveRtpDataStream(sid, sid);
  }

  void set_send_blocked(bool blocked) {
    send_blocked_ = blocked;
  }
  cricket::SendDataParams last_send_data_params() const {
    return last_send_data_params_;
  }
  void set_id_allocaiton_should_fail(bool fail) {
    id_allocation_should_fail_ = fail;
  }
  void set_transport_available(bool available) {
    transport_available_ = available;
  }
  bool IsConnected(webrtc::DataChannel* data_channel) const {
    return connected_channels_.find(data_channel) != connected_channels_.end();
  }
  bool IsSendStreamAdded(uint32 stream) const {
    return send_ssrcs_.find(stream) != send_ssrcs_.end();
  }
  bool IsRecvStreamAdded(uint32 stream) const {
    return recv_ssrcs_.find(stream) != recv_ssrcs_.end();
  }

 private:
  cricket::SendDataParams last_send_data_params_;
  bool id_allocation_should_fail_;
  bool send_blocked_;
  bool transport_available_;
  std::set<webrtc::DataChannel*> connected_channels_;
  std::set<uint32> send_ssrcs_;
  std::set<uint32> recv_ssrcs_;
};
