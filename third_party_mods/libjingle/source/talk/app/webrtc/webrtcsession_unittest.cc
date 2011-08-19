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

template<typename T>
struct ReturnValue : public talk_base::MessageData {
  ReturnValue() : return_value_() {}
  T return_value_;
};

typedef ReturnValue<bool> ReturnBool;
typedef ReturnValue<const std::vector<cricket::Candidate>*>
ReturnCandidates;

template <typename T>
class PassArgument : public talk_base::MessageData {
 public:
  explicit PassArgument(const T& argument) : argument_(argument) {}
  const T& argument() { return argument_; }

 protected:
  T argument_;
};

typedef PassArgument<bool> PassBool;
typedef PassArgument<cricket::BaseSession::Error> PassError;
typedef PassArgument<std::pair<cricket::VoiceChannel*, std::string> >
    PassVoiceChannelString;
typedef PassArgument<std::pair<cricket::VideoChannel*, std::string> >
    PassVideoChannelString;

template <typename T>
class ReturnBoolPassArgument : public talk_base::MessageData {
 public:
  explicit ReturnBoolPassArgument(const T& argument)
      : argument_(argument) { return_value_ = false; }
  const T& argument() { return argument_; }
  bool return_value_;

 protected:
  T argument_;
};

typedef ReturnBoolPassArgument<std::pair<std::string, bool> >
    ReturnBoolPassStringBool;
typedef ReturnBoolPassArgument<std::string> ReturnBoolPassString;
typedef ReturnBoolPassArgument<bool> ReturnBoolPassBool;
typedef ReturnBoolPassArgument<
    std::pair<std::string, cricket::VideoRenderer*> >
        ReturnBoolPassStringVideoRenderer;

class WebRtcSessionExtendedForTest : public webrtc::WebRtcSession {
 public:
  WebRtcSessionExtendedForTest(const std::string& id,
                                   const std::string& direction,
                                   cricket::PortAllocator* allocator,
                                   cricket::ChannelManager* channelmgr,
                                   talk_base::Thread* signaling_thread)
      : WebRtcSession(id, direction, allocator, channelmgr, signaling_thread),
        worker_thread_(channelmgr->worker_thread()) {
  }
 private:
  virtual cricket::Transport* CreateTransport() {
    ASSERT(signaling_thread()->IsCurrent());
    return static_cast<cricket::Transport*> (new cricket::FakeTransport(
        signaling_thread(),
        worker_thread_));
  }
  talk_base::Thread* worker_thread_;
};

class WebRtcSessionTest : public OnSignalImpl,
                          public talk_base::MessageHandler {
 public:
  enum FunctionCallId {
    kCallInitiate,
    kCallConnect,
    kCallOnRemoteDescription,
    kCallOnInitiateMessage,
    kCallMuted,
    kCallCameraMuted,
    kCallCreateVoiceChannel,
    kCallCreateVideoChannel,
    kCallRemoveStream,
    kCallRemoveAllStreams,
    kCallHasStreamString,
    kCallHasStreamBool,
    kCallHasAudioStream,
    kCallHasVideoStream,
    kCallSetVideoRenderer,
    kCallLocalCandidates
  };
  enum {kInit = kCallLocalCandidates + 1};
  enum {kTerminate = kInit + 1};

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

  std::string DirectionAsString() {
    // Direction is either "r"=incoming or "s"=outgoing.
    return (receiving_) ? "r" : "s";
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
      talk_base::Thread::SleepMs(1);
    }
    return success;
  }

  bool Init(bool receiving) {
    if (signaling_thread_ != NULL)
        return false;
    signaling_thread_ = new talk_base::Thread();

    if (!signaling_thread_->SetName("signaling_thread test", this)) {
      return false;
    }
    if (!signaling_thread_->Start()) {
      return false;
    }
    receiving_ = receiving;

    ReturnBool return_value;
    signaling_thread_->Send(this, kInit, &return_value);
    return return_value.return_value_;
  }

  void Init_s(talk_base::Message* message) {
    ReturnBool* return_value = reinterpret_cast<ReturnBool*>(message->pdata);
    return_value->return_value_ = false;

    ASSERT_TRUE(worker_thread_ == NULL);
    worker_thread_ = new talk_base::Thread();

    if (!worker_thread_->SetName("worker thread test", this))
      return;

    if (!worker_thread_->Start())
      return;

    cricket::FakePortAllocator* fake_port_allocator =
        new cricket::FakePortAllocator(worker_thread_, NULL);
    fake_port_allocator->set_flags(cricket::PORTALLOCATOR_DISABLE_STUN |
                                   cricket::PORTALLOCATOR_DISABLE_RELAY |
                                   cricket::PORTALLOCATOR_DISABLE_TCP);

    allocator_ = static_cast<cricket::PortAllocator*>(fake_port_allocator);

    channel_manager_ = new cricket::ChannelManager(worker_thread_);
    if (!channel_manager_->Init())
      return;

    talk_base::CreateRandomString(8, &id_);

    session_ = new webrtc::WebRtcSession(
        id_, DirectionAsString() , allocator_,
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

    return_value->return_value_ = true;
    return;
  }

  void Terminate_s() {
    delete session_;
    delete channel_manager_;
    delete allocator_;
  }

  ~WebRtcSessionTest() {
    if (signaling_thread_ != NULL) {
      signaling_thread_->Send(this, kTerminate, NULL);
      signaling_thread_->Stop();
      signaling_thread_->Clear(NULL);
      delete signaling_thread_;
    }
    if (worker_thread_ != NULL) {
      worker_thread_->Stop();
      worker_thread_->Clear(NULL);
      delete worker_thread_;
    }
  }

  // All session APIs must be called from the signaling thread.
  bool CallInitiate() {
    ReturnBool return_value;
    signaling_thread_->Send(this, kCallInitiate, &return_value);
    return return_value.return_value_;
  }

  bool CallConnect() {
    ReturnBool return_value;
    signaling_thread_->Send(this, kCallConnect, &return_value);
    // This callback does not happen with FakeTransport!
    if (!WaitForCallback(kOnLocalDescription, 1000)) {
      return false;
    }
    return return_value.return_value_;
  }

  bool CallOnRemoteDescription() {
    ReturnBool return_value;
    signaling_thread_->Send(this, kCallOnRemoteDescription, &return_value);
    return return_value.return_value_;
  }

  bool CallOnInitiateMessage() {
    ReturnBool return_value;
    signaling_thread_->Send(this, kCallOnInitiateMessage, &return_value);
    return return_value.return_value_;
  }

  bool CallCreateVoiceChannel(const std::string& stream_id) {
    ReturnBoolPassString return_value(stream_id);
    signaling_thread_->Send(this, kCallCreateVoiceChannel, &return_value);
    if (!WaitForCallback(kOnRtcMediaChannelCreated, 1000)) {
      return false;
    }
    return return_value.return_value_;
  }

  bool CallCreateVideoChannel(const std::string& stream_id) {
    ReturnBoolPassString return_value(stream_id);
    signaling_thread_->Send(this, kCallCreateVideoChannel, &return_value);
    return return_value.return_value_;
  }

  bool CallRemoveStream(const std::string& stream_id) {
    ReturnBoolPassString return_value(stream_id);
    signaling_thread_->Send(this, kCallRemoveStream, &return_value);
    return return_value.return_value_;
  }

  void CallRemoveAllStreams() {
    signaling_thread_->Send(this, kCallRemoveAllStreams, NULL);
  }

  bool CallHasStream(const std::string& label) {
    ReturnBoolPassString return_value(label);
    signaling_thread_->Send(this, kCallHasStreamString, &return_value);
    return return_value.return_value_;
  }

  bool CallHasStream(bool video) {
    ReturnBoolPassBool return_value(video);
    signaling_thread_->Send(this, kCallHasStreamBool, &return_value);
    return return_value.return_value_;
  }

  bool CallHasAudioStream() {
    ReturnBool return_value;
    signaling_thread_->Send(this, kCallHasAudioStream, &return_value);
    return return_value.return_value_;
  }

  bool CallHasVideoStream() {
    ReturnBool return_value;
    signaling_thread_->Send(this, kCallHasVideoStream, &return_value);
    return return_value.return_value_;
  }

  bool CallSetVideoRenderer(const std::string& stream_id,
                            cricket::VideoRenderer* renderer) {
    ReturnBoolPassStringVideoRenderer return_value(std::make_pair(
        stream_id, renderer));
    signaling_thread_->Send(this, kCallSetVideoRenderer, &return_value);
    return return_value.return_value_;
  }

  const std::vector<cricket::Candidate>& CallLocalCandidates() {
    ReturnCandidates return_value;
    signaling_thread_->Send(this, kCallLocalCandidates, &return_value);
    EXPECT_TRUE(return_value.return_value_ != NULL);
    return *return_value.return_value_;
  }

  void Initiate_s(talk_base::Message* message) {
    ReturnBool* return_value = reinterpret_cast<ReturnBool*>(message->pdata);
    if (!session_->Initiate()) {
      return_value->return_value_ = false;
      return;
    }
    return_value->return_value_ = true;
  }

  void Connect_s(talk_base::Message* message) {
    ReturnBool* return_value = reinterpret_cast<ReturnBool*>(message->pdata);
    return_value->return_value_ = session_->Connect();
  }

  void OnRemoteDescription_s(talk_base::Message* message) {
    ReturnBool* return_value = reinterpret_cast<ReturnBool*>(message->pdata);
    return_value->return_value_ = false;
    std::vector<cricket::Candidate> candidates;
    cricket::SessionDescription* description = GetLocalDescription(&candidates);
    if (description == NULL) {
        return;
    }
    if (!session_->OnRemoteDescription(description, candidates)) {
      delete description;
      return;
    }
    return_value->return_value_ = true;
  }

  void OnInitiateMessage_s(talk_base::Message* message) {
    cricket::SessionDescription* description = NULL;
    std::vector<cricket::Candidate> candidates;

    ReturnBool* return_value = reinterpret_cast<ReturnBool*>(message->pdata);
    if (!GenerateFakeSession(false, &description, &candidates)) {
      return_value->return_value_ = false;
      return;
    }
    if (!session_->OnInitiateMessage(description, candidates)) {
      return_value->return_value_ = false;
      delete description;
      return;
    }
    return_value->return_value_ = true;
  }

  void Muted_s(talk_base::Message* message) {
    ReturnBool* return_value = reinterpret_cast<ReturnBool*>(message->pdata);
    return_value->return_value_ = session_->muted();
  }

  void CameraMuted_s(talk_base::Message* message) {
    ReturnBool* return_value = reinterpret_cast<ReturnBool*>(message->pdata);
    return_value->return_value_ = session_->camera_muted();
  }

  void CreateVoiceChannel_s(talk_base::Message* message) {
    ReturnBoolPassString* return_value =
        reinterpret_cast<ReturnBoolPassString*>(message->pdata);
    return_value->return_value_ = session_->CreateVoiceChannel(
        return_value->argument());
  }

  void CreateVideoChannel_s(talk_base::Message* message) {
    ReturnBoolPassString* return_value =
        reinterpret_cast<ReturnBoolPassString*>(message->pdata);
    return_value->return_value_ = session_->CreateVideoChannel(
        return_value->argument());
  }

  void RemoveStream_s(talk_base::Message* message) {
    ReturnBoolPassString* return_value =
        reinterpret_cast<ReturnBoolPassString*>(message->pdata);
    return_value->return_value_ = session_->RemoveStream(
        return_value->argument());
  }

  void RemoveAllStreams_s(talk_base::Message* message) {
    EXPECT_TRUE(message->pdata == NULL);
    session_->RemoveAllStreams();
  }

  void HasStreamString_s(talk_base::Message* message) {
    ReturnBoolPassString* return_value =
        reinterpret_cast<ReturnBoolPassString*>(message->pdata);
    return_value->return_value_ = session_->HasStream(return_value->argument());
  }

  void HasStreamBool_s(talk_base::Message* message) {
    ReturnBoolPassBool* return_value = reinterpret_cast<ReturnBoolPassBool*>(
        message->pdata);
    return_value->return_value_ = session_->HasStream(return_value->argument());
  }

  void HasAudioStream_s(talk_base::Message* message) {
    ReturnBool* return_value = reinterpret_cast<ReturnBool*>(message->pdata);
    return_value->return_value_ = session_->HasAudioStream();
  }

  void HasVideoStream_s(talk_base::Message* message) {
    ReturnBool* return_value = reinterpret_cast<ReturnBool*>(message->pdata);
    return_value->return_value_ = session_->HasVideoStream();
  }

  void SetVideoRenderer_s(talk_base::Message* message) {
    ReturnBoolPassStringVideoRenderer* return_value =
        reinterpret_cast<ReturnBoolPassStringVideoRenderer*>(message->pdata);
    return_value->return_value_ = session_->SetVideoRenderer(
        return_value->argument().first, return_value->argument().second);
  }

  void LocalCandidates_s(talk_base::Message* message) {
    ReturnCandidates* return_value =
        reinterpret_cast<ReturnCandidates*>(message->pdata);
    return_value->return_value_ = &session_->local_candidates();
  }

  void OnMessage(talk_base::Message* message) {
    if ((message->pdata == NULL) &&
        (message->message_id != kCallRemoveAllStreams) &&
        (message->message_id != kTerminate)) {
      ADD_FAILURE();
      return;
    }
    if (!signaling_thread_->IsCurrent()) {
      ADD_FAILURE();
      return;
    }

    switch (message->message_id) {
      case kCallInitiate:
        Initiate_s(message);
        return;
      case kCallConnect:
        Connect_s(message);
        return;
      case kCallOnRemoteDescription:
        OnRemoteDescription_s(message);
        return;
      case kCallOnInitiateMessage:
        OnInitiateMessage_s(message);
        return;
      case kCallMuted:
        Muted_s(message);
        return;
      case kCallCameraMuted:
        CameraMuted_s(message);
        return;
      case kCallCreateVoiceChannel:
        CreateVoiceChannel_s(message);
        return;
      case kCallCreateVideoChannel:
        CreateVideoChannel_s(message);
        return;
      case kCallRemoveStream:
        RemoveStream_s(message);
        return;
      case kCallRemoveAllStreams:
        RemoveAllStreams_s(message);
        return;
      case kCallHasStreamString:
        HasStreamString_s(message);
        return;
      case kCallHasStreamBool:
        HasStreamBool_s(message);
        return;
      case kCallHasAudioStream:
        HasAudioStream_s(message);
        return;
      case kCallHasVideoStream:
        HasVideoStream_s(message);
        return;
      case kCallSetVideoRenderer:
        SetVideoRenderer_s(message);
        return;
      case kCallLocalCandidates:
        LocalCandidates_s(message);
        return;
      case kInit:
        Init_s(message);
        return;
      case kTerminate:
        Terminate_s();
        return;
      default:
        ADD_FAILURE();
        return;
    }
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

// TODO(ronghuawu): Add tests for video calls and incoming calls.
TEST(WebRtcSessionTest, SendCallSetUp) {
  const bool kReceiving = false;
  talk_base::scoped_ptr<WebRtcSessionTest> my_session;
  my_session.reset(WebRtcSessionTest::CreateWebRtcSessionTest(kReceiving));

  ASSERT_TRUE(my_session.get() != NULL);
  ASSERT_TRUE(my_session->CallInitiate());

  ASSERT_TRUE(my_session->CallCreateVoiceChannel("Audio"));
  ASSERT_TRUE(my_session->CallConnect());

  ASSERT_TRUE(my_session->CallOnRemoteDescription());

  // All callbacks should be caught by my session. Assert it.
  ASSERT_FALSE(CallbackReceived(my_session.get(), 1000));
}
