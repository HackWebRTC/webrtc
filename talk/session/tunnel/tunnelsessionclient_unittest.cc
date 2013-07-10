/*
 * libjingle
 * Copyright 2010, Google Inc.
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
#include "talk/base/gunit.h"
#include "talk/base/messagehandler.h"
#include "talk/base/scoped_ptr.h"
#include "talk/base/stream.h"
#include "talk/base/thread.h"
#include "talk/base/timeutils.h"
#include "talk/p2p/base/sessionmanager.h"
#include "talk/p2p/base/transport.h"
#include "talk/p2p/client/fakeportallocator.h"
#include "talk/session/tunnel/tunnelsessionclient.h"

static const int kTimeoutMs = 10000;
static const int kBlockSize = 4096;
static const buzz::Jid kLocalJid("local@localhost");
static const buzz::Jid kRemoteJid("remote@localhost");

// This test fixture creates the necessary plumbing to create and run
// two TunnelSessionClients that talk to each other.
class TunnelSessionClientTest : public testing::Test,
                                public talk_base::MessageHandler,
                                public sigslot::has_slots<> {
 public:
  TunnelSessionClientTest()
      : local_pa_(talk_base::Thread::Current(), NULL),
        remote_pa_(talk_base::Thread::Current(), NULL),
        local_sm_(&local_pa_, talk_base::Thread::Current()),
        remote_sm_(&remote_pa_, talk_base::Thread::Current()),
        local_client_(kLocalJid, &local_sm_),
        remote_client_(kRemoteJid, &remote_sm_),
        done_(false) {
    local_sm_.SignalRequestSignaling.connect(this,
        &TunnelSessionClientTest::OnLocalRequestSignaling);
    local_sm_.SignalOutgoingMessage.connect(this,
        &TunnelSessionClientTest::OnOutgoingMessage);
    remote_sm_.SignalRequestSignaling.connect(this,
        &TunnelSessionClientTest::OnRemoteRequestSignaling);
    remote_sm_.SignalOutgoingMessage.connect(this,
        &TunnelSessionClientTest::OnOutgoingMessage);
    remote_client_.SignalIncomingTunnel.connect(this,
        &TunnelSessionClientTest::OnIncomingTunnel);
  }

  // Transfer the desired amount of data from the local to the remote client.
  void TestTransfer(int size) {
    // Create some dummy data to send.
    send_stream_.ReserveSize(size);
    for (int i = 0; i < size; ++i) {
      char ch = static_cast<char>(i);
      send_stream_.Write(&ch, 1, NULL, NULL);
    }
    send_stream_.Rewind();
    // Prepare the receive stream.
    recv_stream_.ReserveSize(size);
    // Create the tunnel and set things in motion.
    local_tunnel_.reset(local_client_.CreateTunnel(kRemoteJid, "test"));
    local_tunnel_->SignalEvent.connect(this,
        &TunnelSessionClientTest::OnStreamEvent);
    EXPECT_TRUE_WAIT(done_, kTimeoutMs);
    // Make sure we received the right data.
    EXPECT_EQ(0, memcmp(send_stream_.GetBuffer(),
                        recv_stream_.GetBuffer(), size));
  }

 private:
  enum { MSG_LSIGNAL, MSG_RSIGNAL };

  // There's no SessionManager* argument in this callback, so we need 2 of them.
  void OnLocalRequestSignaling() {
    local_sm_.OnSignalingReady();
  }
  void OnRemoteRequestSignaling() {
    remote_sm_.OnSignalingReady();
  }

  // Post a message, to avoid problems with directly connecting the callbacks.
  void OnOutgoingMessage(cricket::SessionManager* manager,
                         const buzz::XmlElement* stanza) {
    if (manager == &local_sm_) {
      talk_base::Thread::Current()->Post(this, MSG_LSIGNAL,
          talk_base::WrapMessageData(*stanza));
    } else if (manager == &remote_sm_) {
      talk_base::Thread::Current()->Post(this, MSG_RSIGNAL,
          talk_base::WrapMessageData(*stanza));
    }
  }

  // Need to add a "from=" attribute (normally added by the server)
  // Then route the incoming signaling message to the "other" session manager.
  virtual void OnMessage(talk_base::Message* message) {
    talk_base::TypedMessageData<buzz::XmlElement>* data =
        static_cast<talk_base::TypedMessageData<buzz::XmlElement>*>(
            message->pdata);
    bool response = data->data().Attr(buzz::QN_TYPE) == buzz::STR_RESULT;
    if (message->message_id == MSG_RSIGNAL) {
      data->data().AddAttr(buzz::QN_FROM, remote_client_.jid().Str());
      if (!response) {
        local_sm_.OnIncomingMessage(&data->data());
      } else {
        local_sm_.OnIncomingResponse(NULL, &data->data());
      }
    } else if (message->message_id == MSG_LSIGNAL) {
      data->data().AddAttr(buzz::QN_FROM, local_client_.jid().Str());
      if (!response) {
        remote_sm_.OnIncomingMessage(&data->data());
      } else {
        remote_sm_.OnIncomingResponse(NULL, &data->data());
      }
    }
    delete data;
  }

  // Accept the tunnel when it arrives and wire up the stream.
  void OnIncomingTunnel(cricket::TunnelSessionClient* client,
                        buzz::Jid jid, std::string description,
                        cricket::Session* session) {
    remote_tunnel_.reset(remote_client_.AcceptTunnel(session));
    remote_tunnel_->SignalEvent.connect(this,
        &TunnelSessionClientTest::OnStreamEvent);
  }

  // Send from send_stream_ as long as we're not flow-controlled.
  // Read bytes out into recv_stream_ as they arrive.
  // End the test when we are notified that the local side has closed the
  // tunnel. All data has been read out at this point.
  void OnStreamEvent(talk_base::StreamInterface* stream, int events,
                     int error) {
    if (events & talk_base::SE_READ) {
      if (stream == remote_tunnel_.get()) {
        ReadData();
      }
    }
    if (events & talk_base::SE_WRITE) {
      if (stream == local_tunnel_.get()) {
        bool done = false;
        WriteData(&done);
        if (done) {
          local_tunnel_->Close();
        }
      }
    }
    if (events & talk_base::SE_CLOSE) {
      if (stream == remote_tunnel_.get()) {
        remote_tunnel_->Close();
        done_ = true;
      }
    }
  }

  // Spool from the tunnel into recv_stream.
  // Flow() doesn't work here because it won't write if the read blocks.
  void ReadData() {
    char block[kBlockSize];
    size_t read, position;
    talk_base::StreamResult res;
    while ((res = remote_tunnel_->Read(block, sizeof(block), &read, NULL)) ==
        talk_base::SR_SUCCESS) {
      recv_stream_.Write(block, read, NULL, NULL);
    }
    ASSERT(res != talk_base::SR_EOS);
    recv_stream_.GetPosition(&position);
    LOG(LS_VERBOSE) << "Recv position: " << position;
  }
  // Spool from send_stream into the tunnel. Back up if we get flow controlled.
  void WriteData(bool* done) {
    char block[kBlockSize];
    size_t leftover = 0, position;
    talk_base::StreamResult res = talk_base::Flow(&send_stream_,
        block, sizeof(block), local_tunnel_.get(), &leftover);
    if (res == talk_base::SR_BLOCK) {
      send_stream_.GetPosition(&position);
      send_stream_.SetPosition(position - leftover);
      LOG(LS_VERBOSE) << "Send position: " << position - leftover;
      *done = false;
    } else if (res == talk_base::SR_SUCCESS) {
      *done = true;
    } else {
      ASSERT(false);  // shouldn't happen
    }
  }

 private:
  cricket::FakePortAllocator local_pa_;
  cricket::FakePortAllocator remote_pa_;
  cricket::SessionManager local_sm_;
  cricket::SessionManager remote_sm_;
  cricket::TunnelSessionClient local_client_;
  cricket::TunnelSessionClient remote_client_;
  talk_base::scoped_ptr<talk_base::StreamInterface> local_tunnel_;
  talk_base::scoped_ptr<talk_base::StreamInterface> remote_tunnel_;
  talk_base::MemoryStream send_stream_;
  talk_base::MemoryStream recv_stream_;
  bool done_;
};

// Test the normal case of sending data from one side to the other.
TEST_F(TunnelSessionClientTest, TestTransfer) {
  TestTransfer(1000000);
}
