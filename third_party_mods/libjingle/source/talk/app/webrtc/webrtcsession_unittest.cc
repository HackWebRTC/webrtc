/*
 * libjingle
 * Copyright 2004--2011, Google Inc.
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

#include <stdio.h>

#include <list>

#include "base/gunit.h"
#include "base/helpers.h"
#include "talk/app/webrtc/webrtcsession.h"
#include "talk/base/fakenetwork.h"
#include "talk/base/scoped_ptr.h"
#include "talk/base/thread.h"
#include "talk/p2p/base/portallocator.h"
#include "talk/p2p/base/sessiondescription.h"
#include "talk/p2p/client/fakeportallocator.h"
#include "talk/session/phone/fakesession.h"
#include "talk/session/phone/mediasessionclient.h"

cricket::VideoContentDescription* CopyVideoContentDescription(
    const cricket::VideoContentDescription* video_description) {
  cricket::VideoContentDescription* new_video_description =
      new cricket::VideoContentDescription();
  cricket::VideoCodecs::const_iterator iter =
      video_description->codecs().begin();
  for (; iter != video_description->codecs().end(); iter++) {
    new_video_description->AddCodec(*iter);
  }
  new_video_description->SortCodecs();
  return new_video_description;
}

cricket::AudioContentDescription* CopyAudioContentDescription(
    const cricket::AudioContentDescription* audio_description) {
  cricket::AudioContentDescription* new_audio_description =
      new cricket::AudioContentDescription();
  cricket::AudioCodecs::const_iterator iter =
      audio_description->codecs().begin();
  for (; iter != audio_description->codecs().end(); iter++) {
    new_audio_description->AddCodec(*iter);
  }
  new_audio_description->SortCodecs();
  return new_audio_description;
}

const cricket::ContentDescription* CopyContentDescription(
    const cricket::ContentDescription* original) {
  const cricket::MediaContentDescription* media =
      static_cast<const cricket::MediaContentDescription*>(original);
  const cricket::ContentDescription* new_content_description = NULL;
  if (media->type() == cricket::MEDIA_TYPE_VIDEO) {
    const cricket::VideoContentDescription* video_description =
        static_cast<const cricket::VideoContentDescription*>(original);
    new_content_description = static_cast<const cricket::ContentDescription*>
        (CopyVideoContentDescription(video_description));
  } else if (media->type() == cricket::MEDIA_TYPE_AUDIO) {
    const cricket::AudioContentDescription* audio_description =
        static_cast<const cricket::AudioContentDescription*>(original);
    new_content_description = static_cast<const cricket::ContentDescription*>
        (CopyAudioContentDescription(audio_description));
  } else {
    return NULL;
  }
  return new_content_description;
}

cricket::ContentInfos CopyContentInfos(const cricket::ContentInfos& original) {
  cricket::ContentInfos new_content_infos;
  for (cricket::ContentInfos::const_iterator iter = original.begin();
       iter != original.end(); iter++) {
    cricket::ContentInfo info;
    info.name = (*iter).name;
    info.type = (*iter).type;
    info.description = CopyContentDescription((*iter).description);
  }
  return new_content_infos;
}

cricket::SessionDescription* CopySessionDescription(
    const cricket::SessionDescription* original) {
  const cricket::ContentInfos& content_infos = original->contents();
  cricket::ContentInfos new_content_infos = CopyContentInfos(content_infos);
  return new cricket::SessionDescription(new_content_infos);
}

bool GenerateFakeSessionDescription(bool video,
    cricket::SessionDescription** incoming_sdp) {
  *incoming_sdp = new cricket::SessionDescription();
  if (*incoming_sdp == NULL)
    return false;
  const std::string name = video ? std::string(cricket::CN_VIDEO) :
                                   std::string(cricket::CN_AUDIO);
  cricket::ContentDescription* description = NULL;
  if (video) {
    cricket::VideoContentDescription* video_dsc =
        new cricket::VideoContentDescription;
    video_dsc->SortCodecs();
    description = static_cast<cricket::ContentDescription*>(video_dsc);
  } else {
    cricket::AudioContentDescription* audio_dsc =
        new cricket::AudioContentDescription();
    audio_dsc->SortCodecs();
    description = static_cast<cricket::ContentDescription*>(audio_dsc);
  }

  // Cannot fail.
  (*incoming_sdp)->AddContent(name, cricket::NS_JINGLE_RTP, description);
  return true;
}

void GenerateFakeCandidate(bool video,
                           std::vector<cricket::Candidate>* candidates) {
  // Next add a candidate.
  // int port_index = 0;
  std::string port_index_as_string("0");

  cricket::Candidate candidate;
  candidate.set_name("rtp");
  candidate.set_protocol("udp");
  talk_base::SocketAddress address("127.0.0.1", 1234);
  candidate.set_address(address);
  candidate.set_preference(1);
  candidate.set_username("username" + port_index_as_string);
  candidate.set_password(port_index_as_string);
  candidate.set_type("local");
  candidate.set_network_name("network");
  candidate.set_generation(0);

  candidates->push_back(candidate);
}


bool GenerateFakeSession(bool video, cricket::SessionDescription** incoming_sdp,
                         std::vector<cricket::Candidate>* candidates) {
  if (!GenerateFakeSessionDescription(video, incoming_sdp)) {
    return false;
  }

  GenerateFakeCandidate(video, candidates);
  return true;
}

class OnSignalImpl
    : public sigslot::has_slots<> {
 public:
  enum CallbackId {
    kNone,
    kOnAddStream,
    kOnRemoveStream,
    kOnRtcMediaChannelCreated,
    kOnLocalDescription,
    kOnFailedCall,
  };
  OnSignalImpl()
      : callback_ids_(),
        last_stream_id_(""),
        last_was_video_(false),
        last_description_ptr_(NULL),
        last_candidates_() {
  }
  virtual ~OnSignalImpl() {
    delete last_description_ptr_;
    last_description_ptr_ = NULL;
  }

  void OnAddStream(const std::string& stream_id, bool video) {
    callback_ids_.push_back(kOnAddStream);
    last_stream_id_ = stream_id;
    last_was_video_ = video;
  }
  void OnRemoveStream(const std::string& stream_id, bool video) {
    callback_ids_.push_back(kOnRemoveStream);
    last_stream_id_ = stream_id;
    last_was_video_ = video;
  }
  void OnRtcMediaChannelCreated(const std::string& stream_id,
                                        bool video) {
    callback_ids_.push_back(kOnRtcMediaChannelCreated);
    last_stream_id_ = stream_id;
    last_was_video_ = video;
  }
  void OnLocalDescription(
      const cricket::SessionDescription* desc,
      const std::vector<cricket::Candidate>& candidates) {
    callback_ids_.push_back(kOnLocalDescription);
    delete last_description_ptr_;
    last_description_ptr_ = CopySessionDescription(desc);
    last_candidates_.clear();
    last_candidates_.insert(last_candidates_.end(), candidates.begin(),
                            candidates.end());
  }
  cricket::SessionDescription* GetLocalDescription(
      std::vector<cricket::Candidate>* candidates) {
    if (last_candidates_.empty()) {
      return NULL;
    }
    if (last_description_ptr_ == NULL) {
      return NULL;
    }
    candidates->insert(candidates->end(), last_candidates_.begin(),
                       last_candidates_.end());
    return CopySessionDescription(last_description_ptr_);
  }

  void OnFailedCall() {
    callback_ids_.push_back(kOnFailedCall);
  }

  CallbackId PopOldestCallback() {
    if (callback_ids_.empty()) {
      return kNone;
    }
    const CallbackId return_value = callback_ids_.front();
    callback_ids_.pop_front();
    return return_value;
  }

  CallbackId PeekOldestCallback() {
    if (callback_ids_.empty()) {
      return kNone;
    }
    const CallbackId return_value = callback_ids_.front();
    return return_value;
  }

  void Reset() {
    callback_ids_.clear();
    last_stream_id_ = "";
    last_was_video_ = false;
    delete last_description_ptr_;
    last_description_ptr_ = NULL;
    last_candidates_.clear();
  }

 protected:
  std::list<CallbackId> callback_ids_;

  std::string last_stream_id_;
  bool last_was_video_;
  cricket::SessionDescription* last_description_ptr_;
  std::vector<cricket::Candidate> last_candidates_;
};

class WebRtcSessionTest : public OnSignalImpl {
 public:
  static WebRtcSessionTest* CreateWebRtcSessionTest(bool receiving) {
    WebRtcSessionTest* return_value =
        new WebRtcSessionTest();
    if (return_value == NULL) {
      return NULL;
    }
    if (!return_value->Init(receiving)) {
      delete return_value;
      return NULL;
    }
    return return_value;
  }

  bool WaitForCallback(CallbackId id, int timeout_ms) {
    bool success = false;
    for (int ms = 0; ms < timeout_ms; ms++) {
      const CallbackId peek_id = PeekOldestCallback();
      if (peek_id == id) {
        PopOldestCallback();
        success = true;
        break;
      } else if (peek_id != kNone) {
        success = false;
        break;
      }
      talk_base::Thread::Current()->ProcessMessages(1);
    }
    return success;
  }

  bool Init(bool receiving) {
    if (signaling_thread_ != NULL)
        return false;
    signaling_thread_ = talk_base::Thread::Current();
    receiving_ = receiving;

    if (worker_thread_!= NULL)
        return false;
    worker_thread_ = talk_base::Thread::Current();

    cricket::FakePortAllocator* fake_port_allocator =
        new cricket::FakePortAllocator(worker_thread_, NULL);

    allocator_ = static_cast<cricket::PortAllocator*>(fake_port_allocator);

    channel_manager_ = new cricket::ChannelManager(worker_thread_);
    if (!channel_manager_->Init())
      return false;

    talk_base::CreateRandomString(8, &id_);

    session_ = new webrtc::WebRtcSession(
        id_, receiving_ , allocator_,
        channel_manager_,
        signaling_thread_);
    session_->SignalAddStream.connect(
        static_cast<OnSignalImpl*> (this),
        &OnSignalImpl::OnAddStream);
    session_->SignalRemoveStream.connect(
        static_cast<OnSignalImpl*> (this),
        &OnSignalImpl::OnRemoveStream);
    session_->SignalRtcMediaChannelCreated.connect(
        static_cast<OnSignalImpl*> (this),
        &OnSignalImpl::OnRtcMediaChannelCreated);
    session_->SignalLocalDescription.connect(
        static_cast<OnSignalImpl*> (this),
        &OnSignalImpl::OnLocalDescription);
    session_->SignalFailedCall.connect(
        static_cast<OnSignalImpl*> (this),
        &OnSignalImpl::OnFailedCall);

    return true;
  }

  void Terminate() {
    delete session_;
    session_ = NULL;
    delete channel_manager_;
    channel_manager_ = NULL;
    delete allocator_;
    allocator_ = NULL;
  }

  ~WebRtcSessionTest() {
    Terminate();
  }

  // All session APIs must be called from the signaling thread.
  bool CallInitiate() {
    return session_->Initiate();
  }

  bool CallConnect() {
    if (!session_->Connect())
      return false;
    // This callback does not happen with FakeTransport!
    if (!WaitForCallback(kOnLocalDescription, 1000)) {
      return false;
    }
    return true;
  }

  bool CallOnRemoteDescription(
      cricket::SessionDescription* description,
      std::vector<cricket::Candidate> candidates) {
    if (!session_->OnRemoteDescription(description, candidates)) {
      return false;
    }
    return true;
  }

  bool CallOnInitiateMessage() {
    cricket::SessionDescription* description = NULL;
    std::vector<cricket::Candidate> candidates;

    if (!GenerateFakeSession(false, &description, &candidates)) {
      return false;
    }
    if (!session_->OnInitiateMessage(description, candidates)) {
      delete description;
      return false;
    }
    return true;
  }

  bool CallCreateVoiceChannel(const std::string& stream_id) {
    if (!session_->CreateVoiceChannel(stream_id)) {
      return false;
    }
    if (!WaitForCallback(kOnRtcMediaChannelCreated, 1000)) {
      return false;
    }
    return true;
  }

  bool CallCreateVideoChannel(const std::string& stream_id) {
    if (!session_->CreateVideoChannel(stream_id)) {
      return false;
    }
    if (!WaitForCallback(kOnRtcMediaChannelCreated, 1000)) {
      return false;
    }
    return true;
  }

  bool CallRemoveStream(const std::string& stream_id) {
    return session_->RemoveStream(stream_id);
  }

  void CallRemoveAllStreams() {
    session_->RemoveAllStreams();
  }

  bool CallHasStream(const std::string& label) {
    return session_->HasStream(label);
  }

  bool CallHasStream(bool video) {
    return session_->HasStream(video);
  }

  bool CallHasAudioStream() {
    return session_->HasAudioStream();
  }

  bool CallHasVideoStream() {
    return session_->HasVideoStream();
  }

  bool CallSetVideoRenderer(const std::string& stream_id,
                            cricket::VideoRenderer* renderer) {
    return session_->SetVideoRenderer(stream_id, renderer);
  }

  const std::vector<cricket::Candidate>& CallLocalCandidates() {
    return session_->local_candidates();
  }

 private:
  WebRtcSessionTest()
      : session_(NULL),
        id_(),
        receiving_(false),
        allocator_(NULL),
        channel_manager_(NULL),
        worker_thread_(NULL),
        signaling_thread_(NULL) {
  }

  webrtc::WebRtcSession* session_;
  std::string id_;
  bool receiving_;

  cricket::PortAllocator* allocator_;

  cricket::ChannelManager* channel_manager_;

  talk_base::Thread* worker_thread_;
  talk_base::Thread* signaling_thread_;
};

bool CallbackReceived(WebRtcSessionTest* session, int timeout) {
  talk_base::Thread::SleepMs(timeout);
  const OnSignalImpl::CallbackId peek_id =
      session->PeekOldestCallback();
  return peek_id != OnSignalImpl::kNone;
}

void SleepMs(int timeout_ms) {
  talk_base::Thread::SleepMs(timeout_ms);
}

TEST(WebRtcSessionTest, InitializationReceiveSanity) {
  const bool kReceiving = true;
  talk_base::scoped_ptr<WebRtcSessionTest> my_session;
  my_session.reset(WebRtcSessionTest::CreateWebRtcSessionTest(kReceiving));

  ASSERT_TRUE(my_session.get() != NULL);
  ASSERT_TRUE(my_session->CallInitiate());

  // Should return false because no stream has been set up yet.
  EXPECT_FALSE(my_session->CallConnect());
  const bool kVideo = true;
  EXPECT_FALSE(my_session->CallHasStream(kVideo));
  EXPECT_FALSE(my_session->CallHasStream(!kVideo));

  EXPECT_EQ(OnSignalImpl::kNone,
            my_session->PopOldestCallback());
}

TEST(WebRtcSessionTest, AudioSendReceiveCallSetUp) {
  const bool kReceiving = false;
  talk_base::scoped_ptr<WebRtcSessionTest> my_session;
  my_session.reset(WebRtcSessionTest::CreateWebRtcSessionTest(kReceiving));

  ASSERT_TRUE(my_session.get() != NULL);
  ASSERT_TRUE(my_session->CallInitiate());

  ASSERT_TRUE(my_session->CallCreateVoiceChannel("Audio"));
  ASSERT_TRUE(my_session->CallConnect());

  std::vector<cricket::Candidate> candidates;
  cricket::SessionDescription* local_session = my_session->GetLocalDescription(
      &candidates);
  ASSERT_FALSE(candidates.empty());
  ASSERT_FALSE(local_session == NULL);
  if (!my_session->CallOnRemoteDescription(local_session, candidates)) {
      delete local_session;
      FAIL();
  }

  // All callbacks should be caught by my session. Assert it.
  ASSERT_FALSE(CallbackReceived(my_session.get(), 1000));
}

TEST(WebRtcSessionTest, VideoSendCallSetUp) {
  const bool kReceiving = false;
  talk_base::scoped_ptr<WebRtcSessionTest> my_session;
  my_session.reset(WebRtcSessionTest::CreateWebRtcSessionTest(kReceiving));

  ASSERT_TRUE(my_session.get() != NULL);
  ASSERT_TRUE(my_session->CallInitiate());

  ASSERT_TRUE(my_session->CallCreateVideoChannel("Video"));
  ASSERT_TRUE(my_session->CallConnect());

  std::vector<cricket::Candidate> candidates;
  cricket::SessionDescription* local_session = my_session->GetLocalDescription(
      &candidates);
  ASSERT_FALSE(candidates.empty());
  ASSERT_FALSE(local_session == NULL);

  if (!my_session->CallOnRemoteDescription(local_session, candidates)) {
      delete local_session;
      FAIL();
  }

  // All callbacks should be caught by my session. Assert it.
  ASSERT_FALSE(CallbackReceived(my_session.get(), 1000));
}

// TODO(ronghuawu): Add tests for incoming calls.
