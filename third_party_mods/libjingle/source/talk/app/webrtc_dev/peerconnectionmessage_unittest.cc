/*
 * libjingle
 * Copyright 2011, Google Inc.
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

#include "gtest/gtest.h"
#include "talk/app/webrtc_dev/peerconnectionmessage.h"
#include "talk/base/logging.h"
#include "talk/base/scoped_ptr.h"
#include "talk/session/phone/channelmanager.h"

using webrtc::PeerConnectionMessage;

static const char kStreamLabel1[] = "local_stream_1";
static const char kAudioTrackLabel1[] = "local_audio_1";
static const char kVideoTrackLabel1[] = "local_video_1";
static const char kVideoTrackLabel2[] = "local_video_2";

static const char kStreamLabel2[] = "local_stream_2";
static const char kAudioTrackLabel2[] = "local_audio_2";
static const char kVideoTrackLabel3[] = "local_video_3";

class PeerConnectionMessageTest: public testing::Test {
 public:
  PeerConnectionMessageTest() {
    channel_manager_.reset(new cricket::ChannelManager(
        talk_base::Thread::Current()));
    EXPECT_TRUE(channel_manager_->Init());
    session_description_factory_.reset(
        new cricket::MediaSessionDescriptionFactory(channel_manager_.get()));
    options_.AddStream(cricket::MEDIA_TYPE_AUDIO, kAudioTrackLabel1,
                       kStreamLabel1);
    options_.AddStream(cricket::MEDIA_TYPE_VIDEO, kVideoTrackLabel1,
                       kStreamLabel1);
    options_.AddStream(cricket::MEDIA_TYPE_VIDEO, kVideoTrackLabel2,
                       kStreamLabel1);

    // kStreamLabel2 with 1 audio track and 1 video track
    options_.AddStream(cricket::MEDIA_TYPE_AUDIO, kAudioTrackLabel2,
                       kStreamLabel2);
    options_.AddStream(cricket::MEDIA_TYPE_VIDEO, kVideoTrackLabel3,
                       kStreamLabel2);

    int port = 1234;
    talk_base::SocketAddress address("127.0.0.1", port++);
    cricket::Candidate candidate1("video_rtcp", "udp", address, 1,
        "user_video_rtcp", "password_video_rtcp", "local", "eth0", 0);
    address.SetPort(port++);
    cricket::Candidate candidate2("video_rtp", "udp", address, 1,
        "user_video_rtp", "password_video_rtp", "local", "eth0", 0);
    address.SetPort(port++);
    cricket::Candidate candidate3("rtp", "udp", address, 1,
        "user_rtp", "password_rtp", "local", "eth0", 0);
    address.SetPort(port++);
    cricket::Candidate candidate4("rtcp", "udp", address, 1,
        "user_rtcp", "password_rtcp", "local", "eth0", 0);

    candidates_.push_back(candidate1);
    candidates_.push_back(candidate2);
    candidates_.push_back(candidate3);
    candidates_.push_back(candidate4);
  }

 protected:
  talk_base::scoped_ptr<cricket::ChannelManager> channel_manager_;
  talk_base::scoped_ptr<cricket::MediaSessionDescriptionFactory>
      session_description_factory_;
  cricket::MediaSessionOptions options_;
  cricket::Candidates candidates_;
};

TEST_F(PeerConnectionMessageTest, Serialize) {
  std::string message;
  talk_base::scoped_ptr<PeerConnectionMessage> pc_message;

  // Offer
  talk_base::scoped_ptr<const cricket::SessionDescription> offer(
      session_description_factory_->CreateOffer(options_));
  pc_message.reset(PeerConnectionMessage::Create(PeerConnectionMessage::kOffer,
      offer.get(), candidates_));
  message = pc_message->Serialize();
  LOG(LS_INFO) << message;

  // Answer
  talk_base::scoped_ptr<const cricket::SessionDescription> answer(
      session_description_factory_->CreateAnswer(offer.get(), options_));
  pc_message.reset(PeerConnectionMessage::Create(PeerConnectionMessage::kAnswer,
      answer.get(), candidates_));
  message = pc_message->Serialize();
  LOG(LS_INFO) << message;

  // Error
  pc_message.reset(PeerConnectionMessage::CreateErrorMessage(
      PeerConnectionMessage::kParseError));
  message = pc_message->Serialize();
  LOG(LS_INFO) << message;

  // TODO(ronghuawu): Verify the serialized message.
}

TEST_F(PeerConnectionMessageTest, Deserialize) {
  std::string message_ref;
  std::string message_result;
  talk_base::scoped_ptr<PeerConnectionMessage> pc_message;
  talk_base::scoped_ptr<cricket::SessionDescription> offer(
      session_description_factory_->CreateOffer(options_));
  talk_base::scoped_ptr<cricket::SessionDescription> answer(
      session_description_factory_->CreateAnswer(offer.get(), options_));

  // Offer
  pc_message.reset(PeerConnectionMessage::Create(PeerConnectionMessage::kOffer,
      offer.get(), candidates_));
  message_ref = pc_message->Serialize();
  LOG(LS_INFO) << "The reference message: " << message_ref;

  // Deserialize Offer
  pc_message.reset(PeerConnectionMessage::Create(message_ref));
  message_result = pc_message->Serialize();
  LOG(LS_INFO) << "The result message: " << message_result;
  EXPECT_EQ(message_ref, message_result);

  // Answer
  pc_message.reset(PeerConnectionMessage::Create(PeerConnectionMessage::kAnswer,
      answer.get(), candidates_));
  message_ref = pc_message->Serialize();
  LOG(LS_INFO) << "The reference message: " << message_ref;

  // Deserialize Answer
  pc_message.reset(PeerConnectionMessage::Create(message_ref));
  message_result = pc_message->Serialize();
  LOG(LS_INFO) << "The result message: " << message_result;
  EXPECT_EQ(message_ref, message_result);

  // Error
  pc_message.reset(PeerConnectionMessage::CreateErrorMessage(
      PeerConnectionMessage::kParseError));
  message_ref = pc_message->Serialize();
  LOG(LS_INFO) << "The reference message: " << message_ref;

  // Deserialize Error
  pc_message.reset(PeerConnectionMessage::Create(message_ref));
  message_result = pc_message->Serialize();
  LOG(LS_INFO) << "The result message: " << message_result;
  EXPECT_EQ(message_ref, message_result);
}
