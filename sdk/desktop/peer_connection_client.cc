#include "sdk/desktop/peer_connection_client.h"

#include <algorithm>

#include "api/audio_codecs/builtin_audio_decoder_factory.h"
#include "api/audio_codecs/builtin_audio_encoder_factory.h"
#include "api/create_peerconnection_factory.h"
#include "api/rtp_transceiver_interface.h"
#include "api/task_queue/default_task_queue_factory.h"
#include "api/video_codecs/builtin_video_decoder_factory.h"
#include "api/video_codecs/builtin_video_encoder_factory.h"
#include "modules/audio_device/include/audio_device.h"
#include "modules/transit_media/audio_device_module.h"
#include "rtc_base/critical_section.h"
#include "rtc_base/thread.h"
#include "sdk/desktop/camera_capturer_track_source.h"
#include "sdk/desktop/desktop_capturer_track_source.h"
#if !defined(DISABLE_TRANSIT_MEDIA)
#include "sdk/desktop/file_capturer_track_source.h"
#include "sdk/desktop/file_video_capturer.h"
#endif
#include "sdk/desktop/legacy/owned_factory_and_threads.h"
#if defined(WEBRTC_WIN)
#include "sdk/desktop/win32_video_composer.h"
#include "sdk/desktop/win32_video_renderer.h"
#endif

namespace AvConf {

int constexpr PeerConnectionClient::DIR_INACTIVE;
int constexpr PeerConnectionClient::DIR_RECV_ONLY;
int constexpr PeerConnectionClient::DIR_SEND_ONLY;
int constexpr PeerConnectionClient::DIR_SEND_RECV;
std::string const PeerConnectionClient::K_AUDIO_TRACK_ID = {"CFAMSa0"};
std::string const PeerConnectionClient::K_VIDEO_TRACK_ID = {"CFAMSv0"};
int constexpr PeerConnectionClient::K_BPS_IN_KBPS;
int constexpr PeerConnectionClient::CAPTURER_TYPE_CAMERA;
int constexpr PeerConnectionClient::CAPTURER_TYPE_SCREEN;
int constexpr PeerConnectionClient::CAPTURER_TYPE_FILE;

static webrtc::legacy::OwnedFactoryAndThreads* g_factory_ = nullptr;
#if defined(WEBRTC_WIN)
static Win32VideoComposer* g_composer_ = nullptr;
#endif

static rtc::CriticalSection g_global_state_lock_;
static rtc::scoped_refptr<webrtc::AudioSourceInterface> g_local_audio_source_ =
    nullptr;
static rtc::scoped_refptr<webrtc::AudioTrackInterface> g_local_audio_track_ =
    nullptr;
static rtc::scoped_refptr<webrtc::VideoTrackInterface> g_local_video_track_ =
    nullptr;
#if defined(WEBRTC_WIN)
std::vector<Win32VideoRenderer*> g_local_video_renderers_;
#endif

class CreateSdpObserver : public webrtc::CreateSessionDescriptionObserver {
 public:
  CreateSdpObserver(PeerConnectionClient* client) : client_(client) {}

  void OnSuccess(webrtc::SessionDescriptionInterface* desc) override {
    client_->OnCreateSuccess(desc);
  }

  void OnFailure(webrtc::RTCError error) override {
    client_->OnCreateFailure(error);
  }

 private:
  PeerConnectionClient* client_;
};

class SetSdpObserver : public webrtc::SetSessionDescriptionObserver {
 public:
  SetSdpObserver(PeerConnectionClient* client) : client_(client) {}

  void OnSuccess() override { client_->OnSetSuccess(); }

  void OnFailure(webrtc::RTCError error) override {
    client_->OnSetFailure(error);
  }

 private:
  PeerConnectionClient* client_;
};

class GetStatsObserver : public webrtc::RTCStatsCollectorCallback {
 public:
  GetStatsObserver(PeerConnectionClient* client) : client_(client) {}

  void OnStatsDelivered(
      const rtc::scoped_refptr<const webrtc::RTCStatsReport>& report) override {
    client_->OnGetStatsSuccess(report);
  }

 private:
  PeerConnectionClient* client_;
};

PeerConnectionClient::PeerConnectionClient(
    const std::string& peer_uid,
    int dir,
    bool has_video,
    const std::shared_ptr<PeerConnectionClientCallback>& callback,
    int video_max_bitrate,
    int video_max_frame_rate)
    : peer_uid_(peer_uid),
      dir_(dir),
      has_video_(has_video),
      callback_(callback),
      video_max_bitrate_(video_max_bitrate),
      video_max_frame_rate_(video_max_frame_rate),
      is_initiator_(false),
      peer_connection_(nullptr),
      remote_audio_track_(nullptr),
      remote_video_track_(nullptr),
      candidates_drained_(false) {}

PeerConnectionClient::~PeerConnectionClient() {}

int PeerConnectionClient::CreatePeerConnectionFactory(void* hwnd,
                                                      int dummy_audio_device,
                                                      int transit_video) {
  rtc::CritScope cs(&g_global_state_lock_);
  RTC_LOG(LS_INFO) << "createPeerConnectionFactory, ver " PC_CLIENT_VERSION;
  if (g_factory_) {
    RTC_LOG(LS_INFO) << "createPeerConnectionFactory: already created";
    return 0;
  }

  std::unique_ptr<rtc::Thread> network_thread =
      rtc::Thread::CreateWithSocketServer();
  network_thread->SetName("network_thread", nullptr);
  RTC_CHECK(network_thread->Start()) << "Failed to start thread";

  std::unique_ptr<rtc::Thread> worker_thread = rtc::Thread::Create();
  worker_thread->SetName("worker_thread", nullptr);
  RTC_CHECK(worker_thread->Start()) << "Failed to start thread";

  std::unique_ptr<rtc::Thread> signaling_thread = rtc::Thread::Create();
  signaling_thread->SetName("signaling_thread", NULL);
  RTC_CHECK(signaling_thread->Start()) << "Failed to start thread";

  rtc::scoped_refptr<webrtc::AudioDeviceModule> adm = nullptr;
#if !defined(DISABLE_TRANSIT_MEDIA)
  if (dummy_audio_device) {
    adm = webrtc::CreateMuteAudioDeviceModule();
  }
#endif

  rtc::scoped_refptr<webrtc::PeerConnectionFactoryInterface> factory(
      webrtc::CreatePeerConnectionFactory(
          network_thread.get(), worker_thread.get(), signaling_thread.get(),
          adm, webrtc::CreateBuiltinAudioEncoderFactory(),
          webrtc::CreateBuiltinAudioDecoderFactory(),
          webrtc::CreateBuiltinVideoEncoderFactory(transit_video),
          webrtc::CreateBuiltinVideoDecoderFactory(transit_video),
          nullptr /* audio_mixer */, nullptr /* audio_processing */));
  g_factory_ = new webrtc::legacy::OwnedFactoryAndThreads(
      std::move(network_thread), std::move(worker_thread),
      std::move(signaling_thread), nullptr, factory.release());

#if defined(WEBRTC_WIN)
  if (g_composer_) {
    g_composer_->UpdateHwnd(hwnd);
  } else {
    g_composer_ = new Win32VideoComposer(hwnd, 40);
  }
#endif

  RTC_LOG(LS_INFO) << "createPeerConnectionFactory success";
  return g_factory_ ? 0 : -1;
}

int PeerConnectionClient::CreateLocalTracks(
    webrtc::VideoTrackSource* video_source) {
  rtc::CritScope cs(&g_global_state_lock_);
  RTC_LOG(LS_INFO) << "createLocalTracks, source " << (void*)video_source;
  if (!g_factory_) {
    RTC_LOG(LS_ERROR) << "createLocalTracks error: no factory";
    return -1;
  }

  g_local_video_track_ =
      g_factory_->factory()->CreateVideoTrack(K_VIDEO_TRACK_ID, video_source);
  cricket::AudioOptions options;
  // options.delay_agnostic_aec = true;
  g_local_audio_source_ = g_factory_->factory()->CreateAudioSource(options);
  g_local_audio_track_ = g_factory_->factory()->CreateAudioTrack(
      K_AUDIO_TRACK_ID, g_local_audio_source_);

  RTC_LOG(LS_INFO) << "createLocalTracks success";
  return 0;
}

void PeerConnectionClient::DestroyLocalTracks() {
  rtc::CritScope cs(&g_global_state_lock_);
  RTC_LOG(LS_INFO) << "destroyLocalTracks";
  // g_local_audio_source_->Release();
  g_local_audio_source_ = nullptr;
  // g_local_audio_track_->Release();
  g_local_audio_track_ = nullptr;
  // g_local_video_track_->Release();
  g_local_video_track_ = nullptr;
#if defined(WEBRTC_WIN)
  // should stop before release VideoTrack
  for (auto renderer : g_local_video_renderers_) {
    renderer->StopRendering();
    if (g_composer_) {
      g_composer_->RemoveRenderer(renderer);
    }
  }
  g_local_video_renderers_.clear();
  // release composer or composer thread would cause crash at
  // Win32VideoComposer destructor.
  /*if (g_composer_) {
    g_composer_->Stop();
    delete g_composer_;
    g_composer_ = nullptr;
  }*/
#endif
  RTC_LOG(LS_INFO) << "destroyLocalTracks success";
}

webrtc::VideoTrackSource* PeerConnectionClient::CreateVideoCapturer(
    int type,
    int width,
    int height,
    int frame_rate,
    const char* extra_param) {
  RTC_LOG(LS_INFO) << "CreateVideoCapturer, type " << type << ", width "
                   << width << ", height " << height << ", fps " << frame_rate
                   << ", extra_param " << extra_param;
  rtc::scoped_refptr<webrtc::VideoTrackSource> video_capturer = nullptr;
  switch (type) {
    case CAPTURER_TYPE_SCREEN:
      video_capturer =
          DesktopCapturerTrackSource::Create(width, height, frame_rate);
      break;
    case CAPTURER_TYPE_FILE:
#if !defined(DISABLE_TRANSIT_MEDIA)
      video_capturer =
          FileCapturerTrackSource::Create(width, height, extra_param);
      break;
#endif
    case CAPTURER_TYPE_CAMERA:
    default:
      video_capturer =
          CameraCapturerTrackSource::Create(width, height, frame_rate);
      break;
  }
  RTC_LOG(LS_INFO) << "CreateVideoCapturer success "
                   << (void*)video_capturer.get();
  return video_capturer.release();
}

void PeerConnectionClient::StartVideoCapturer(void* video_capturer, int type) {
  RTC_LOG(LS_INFO) << "startVideoCapturer " << video_capturer << ", type "
                   << type;
  if (!video_capturer) {
    return;
  }
  switch (type) {
    case CAPTURER_TYPE_SCREEN:
      // already started on create
      break;
    case CAPTURER_TYPE_FILE:
#if !defined(DISABLE_TRANSIT_MEDIA)
      reinterpret_cast<FileCapturerTrackSource*>(video_capturer)->Start();
#endif
      break;
    case CAPTURER_TYPE_CAMERA:
    default:
      reinterpret_cast<CameraCapturerTrackSource*>(video_capturer)->Start();
      break;
  }
  RTC_LOG(LS_INFO) << "startVideoCapturer success";
}

void PeerConnectionClient::StopVideoCapturer(void* video_capturer, int type) {
  RTC_LOG(LS_INFO) << "stopVideoCapturer " << video_capturer << ", type "
                   << type;
  if (!video_capturer) {
    return;
  }
  switch (type) {
    case CAPTURER_TYPE_SCREEN:
      // will destroy on destroy
      break;
    case CAPTURER_TYPE_FILE:
#if !defined(DISABLE_TRANSIT_MEDIA)
      reinterpret_cast<FileCapturerTrackSource*>(video_capturer)->Stop();
#endif
      break;
    case CAPTURER_TYPE_CAMERA:
    default:
      reinterpret_cast<CameraCapturerTrackSource*>(video_capturer)->Stop();
      break;
  }
  RTC_LOG(LS_INFO) << "stopVideoCapturer success";
}

void PeerConnectionClient::AdaptVideoCapturerOutputFormat(void* video_capturer,
                                                          int type,
                                                          int width,
                                                          int height,
                                                          int fps) {
  RTC_LOG(LS_INFO) << "adaptVideoCapturerOutputFormat " << video_capturer
                   << ", type " << type << ", width " << width << ", height "
                   << height << ", fps " << fps;
  if (!video_capturer) {
    return;
  }
  switch (type) {
    case CAPTURER_TYPE_SCREEN:
      break;
    case CAPTURER_TYPE_CAMERA:
    default:
      reinterpret_cast<CameraCapturerTrackSource*>(video_capturer)
          ->AdaptOutputFormat(width, height, fps);
      break;
  }
  RTC_LOG(LS_INFO) << "adaptVideoCapturerOutputFormat success";
}

void PeerConnectionClient::DestroyVideoCapturer(void* video_capturer,
                                                int type) {
  RTC_LOG(LS_INFO) << "destroyVideoCapturer " << video_capturer << ", type "
                   << type;
  if (video_capturer) {
    delete reinterpret_cast<webrtc::VideoTrackSource*>(video_capturer);
  }
  RTC_LOG(LS_INFO) << "destroyVideoCapturer success";
}

#if defined(WEBRTC_WIN)
void PeerConnectionClient::AddLocalRenderer(Win32VideoRenderer* renderer) {
  rtc::CritScope cs(&g_global_state_lock_);
  if (g_local_video_track_) {
    renderer->RenderTrack(g_local_video_track_.get());
    g_local_video_renderers_.push_back(renderer);
    if (g_composer_) {
      g_composer_->AddRenderer(renderer);
    }
  }
}

void PeerConnectionClient::RemoveLocalRenderer(Win32VideoRenderer* renderer) {
  rtc::CritScope cs(&g_global_state_lock_);
  auto it = std::find(g_local_video_renderers_.begin(),
                      g_local_video_renderers_.end(), renderer);
  if (it != g_local_video_renderers_.end()) {
    g_local_video_renderers_.erase(it);
  }
  renderer->StopRendering();
  if (g_composer_) {
    g_composer_->RemoveRenderer(renderer);
  }
}
#endif

void PeerConnectionClient::LogLongMessage(std::string* message) {
  if (message->size() < 4000) {
    RTC_LOG(LS_INFO) << *message;
  } else {
    // MarsXLog limit single log length to 4096
    size_t write = 0;
    while (write < message->size()) {
      size_t end_index = std::min(message->size(), write + 4000);
      RTC_LOG(LS_INFO) << message->substr(write, end_index);
      write = end_index;
    }
  }
}

void PeerConnectionClient::CreatePeerConnection(
    const std::vector<webrtc::PeerConnectionInterface::IceServer>&
        ice_servers) {
  RTC_LOG(LS_INFO) << "createPeerConnection";
  if (!g_factory_) {
    RTC_LOG(LS_ERROR) << "createPeerConnection error: no factory";
    reportError(PeerConnectionClientCallback::ERR_NO_FACTORY);
    return;
  }
  if (send() &&
      (!g_local_audio_track_ || (has_video_ && !g_local_video_track_))) {
    RTC_LOG(LS_ERROR) << "createPeerConnection error: no sending track";
    reportError(PeerConnectionClientCallback::ERR_NO_SENDING_TRACK);
    return;
  }

  webrtc::PeerConnectionInterface::RTCConfiguration config;
  config.sdp_semantics = webrtc::SdpSemantics::kUnifiedPlan;
  config.enable_dtls_srtp = true;
  config.servers = ice_servers;

  peer_connection_ = g_factory_->factory()->CreatePeerConnection(
      config, nullptr, nullptr, this);
  if (!peer_connection_) {
    RTC_LOG(LS_ERROR) << "createPeerConnection error: create pc fail";
    reportError(PeerConnectionClientCallback::ERR_CREATE_PC_FAIL);
    return;
  }

  // use addTransceiver API on answer end seems only get recvonly answer,
  // so let's stay at addTrack API for now.
  peer_connection_->AddTrack(g_local_audio_track_, {peer_uid_});
  if (g_local_video_track_) {
    peer_connection_->AddTrack(g_local_video_track_, {peer_uid_});
  }

  /*if (send()) {
    webrtc::RtpTransceiverInit init;
    init.direction = receive() ? webrtc::RtpTransceiverDirection::kSendRecv
                               : webrtc::RtpTransceiverDirection::kSendOnly;
    init.stream_ids = {peer_uid_};
    peer_connection_->AddTransceiver(g_local_audio_track_, init);
    if (g_local_video_track_) {
      peer_connection_->AddTransceiver(g_local_video_track_, init);
    }
  } else if (receive()) {
    webrtc::RtpTransceiverInit init;
    init.direction = webrtc::RtpTransceiverDirection::kRecvOnly;
    init.stream_ids = {peer_uid_};
    peer_connection_->AddTransceiver(cricket::MediaType::MEDIA_TYPE_AUDIO,
                                     init);
    if (has_video_) {
      peer_connection_->AddTransceiver(cricket::MediaType::MEDIA_TYPE_VIDEO,
                                       init);
    }
  }*/

  RTC_LOG(LS_INFO) << "createPeerConnection success";
}

void PeerConnectionClient::setVideoMaxBitrate() {
  if (video_max_bitrate_ <= 0 || video_max_frame_rate_ <= 0) {
      return;
  }
  for (auto& sender : peer_connection_->GetSenders()) {
    if (sender->media_type() == cricket::MediaType::MEDIA_TYPE_VIDEO) {
      webrtc::RtpParameters params = sender->GetParameters();
      for (auto& encoding : params.encodings) {
        encoding.max_bitrate_bps = video_max_bitrate_ * K_BPS_IN_KBPS;
        encoding.max_framerate = video_max_frame_rate_;
      }
      sender->SetParameters(params);
    }
  }
}

void PeerConnectionClient::GetStats() {
  if (peer_connection_) {
    peer_connection_->GetStats(
        new rtc::RefCountedObject<GetStatsObserver>(this));
  }
}

void PeerConnectionClient::OnGetStatsSuccess(
    const rtc::scoped_refptr<const webrtc::RTCStatsReport>& report) {
  callback_->OnPeerConnectionstatsReady(peer_uid_, report);
}

void PeerConnectionClient::SetAudioSendingEnabled(bool enable) {
  if (g_local_audio_track_) {
    g_local_audio_track_->set_enabled(enable);
  }
}

void PeerConnectionClient::SetVideoSendingEnabled(bool enable) {
  // TODO: Hijack
  if (g_local_video_track_) {
    g_local_video_track_->set_enabled(enable);
  }
}

void PeerConnectionClient::SetAudioReceivingEnabled(bool enable) {
  if (remote_audio_track_) {
    remote_audio_track_->set_enabled(enable);
  }
}

void PeerConnectionClient::SetVideoReceivingEnabled(bool enable) {
  if (remote_video_track_) {
    remote_video_track_->set_enabled(enable);
  }
}

void PeerConnectionClient::CreateOffer() {
  RTC_LOG(LS_INFO) << "createOffer";
  if (!peer_connection_) {
    RTC_LOG(LS_ERROR) << "createOffer error, no PC";
    return;
  }
  is_initiator_ = true;
  peer_connection_->CreateOffer(
      new rtc::RefCountedObject<CreateSdpObserver>(this),
      defaultSdpOptions());
  RTC_LOG(LS_INFO) << "createOffer success";
}

webrtc::PeerConnectionInterface::RTCOfferAnswerOptions PeerConnectionClient::defaultSdpOptions() {
  webrtc::PeerConnectionInterface::RTCOfferAnswerOptions options;
  // use addTransceiver API on answer end seems only get recvonly answer,
  // so let's stay at addTrack API for now (which needs OfferToReceiveAudio).
  if (receive()) {
    options.offer_to_receive_video = 1;
    options.offer_to_receive_audio = 1;
  }
  return options;
}

void PeerConnectionClient::CreateAnswer() {
  RTC_LOG(LS_INFO) << "createAnswer";
  if (!peer_connection_) {
    RTC_LOG(LS_ERROR) << "createAnswer error, no PC";
    return;
  }
  is_initiator_ = false;
  peer_connection_->CreateAnswer(
      new rtc::RefCountedObject<CreateSdpObserver>(this),
      defaultSdpOptions());
  RTC_LOG(LS_INFO) << "createAnswer success";
}

void PeerConnectionClient::AddIceCandidate(
    const webrtc::IceCandidateInterface* candidate) {
  RTC_LOG(LS_INFO) << "addIceCandidate";
  if (!peer_connection_) {
    RTC_LOG(LS_ERROR) << "addIceCandidate error, no PC";
    return;
  }

  if (!candidates_drained_) {
    queued_candidates_.push_back(candidate);
  } else if (!peer_connection_->AddIceCandidate(candidate)) {
    RTC_LOG(LS_ERROR)
        << "addIceCandidate: failed to apply the received candidate";
  }
}

void PeerConnectionClient::RemoveIceCandidates(
    const std::vector<webrtc::IceCandidateInterface*>& candidates) {}

void PeerConnectionClient::SetRemoteDescription(
    webrtc::SessionDescriptionInterface* desc) {
  RTC_LOG(LS_INFO) << "SetRemoteDescription";
  if (!peer_connection_) {
    RTC_LOG(LS_ERROR) << "SetRemoteDescription error, no PC";
    return;
  }
  peer_connection_->SetRemoteDescription(
      new rtc::RefCountedObject<SetSdpObserver>(this), desc);
  getRemoteTracks();

  RTC_LOG(LS_INFO) << "setRemoteDescription success";
}

#if defined(WEBRTC_WIN)
void PeerConnectionClient::AddRemoteRenderer(Win32VideoRenderer* renderer) {
  remote_track_renderers_.push_back(renderer);
  if (remote_video_track_) {
    renderer->RenderTrack(reinterpret_cast<webrtc::VideoTrackInterface*>(
        remote_video_track_.get()));
    if (g_composer_) {
      g_composer_->AddRenderer(renderer);
    }
  }
}

void PeerConnectionClient::RemoveRemoteRenderer(Win32VideoRenderer* renderer) {
  auto it = std::find(remote_track_renderers_.begin(),
                      remote_track_renderers_.end(), renderer);
  if (it != remote_track_renderers_.end()) {
    remote_track_renderers_.erase(it);
  }
  renderer->StopRendering();
  if (g_composer_) {
    g_composer_->RemoveRenderer(renderer);
  }
}
#endif

void PeerConnectionClient::Close() {
#if defined(WEBRTC_WIN)
  if (receive() && g_composer_) {
    for (auto renderer : remote_track_renderers_) {
      renderer->StopRendering();
      g_composer_->RemoveRenderer(renderer);
    }
    remote_track_renderers_.clear();
  }
#endif
  peer_connection_ = nullptr;
}

void PeerConnectionClient::OnCreateSuccess(
    webrtc::SessionDescriptionInterface* desc) {
  std::string sdp;
  if (!desc->ToString(&sdp)) {
    RTC_LOG(LS_ERROR) << "onCreateSuccess error: serialize sdp fail";
    return;
  }
  std::string log("onCreateSuccess\n");
  log.append(sdp);
  LogLongMessage(&log);

  std::string refined_sdp = callback_->OnPreferCodecs(peer_uid_, sdp);
  std::string log2("refined sdp\n");
  log2.append(refined_sdp);
  LogLongMessage(&log2);

  std::unique_ptr<webrtc::SessionDescriptionInterface> new_sdp =
      webrtc::CreateSessionDescription(desc->GetType(), refined_sdp);
  local_sdp_ = webrtc::CreateSessionDescription(desc->GetType(), refined_sdp);
  peer_connection_->SetLocalDescription(
      new rtc::RefCountedObject<SetSdpObserver>(this), new_sdp.release());
}

void PeerConnectionClient::OnCreateFailure(const webrtc::RTCError& error) {
  RTC_LOG(LS_ERROR) << "onCreateFailure " << error.message();
  reportError(PeerConnectionClientCallback::ERR_CREATE_SDP_FAIL);
}

void PeerConnectionClient::OnSetSuccess() {
  RTC_LOG(LS_INFO) << "onSetSuccess";
  if (is_initiator_) {
    if (peer_connection_->remote_description() == nullptr) {
      // set offer success
      RTC_LOG(LS_INFO) << "Local SDP set successfully";
      callback_->OnLocalDescription(peer_uid_, *local_sdp_);
      setVideoMaxBitrate();
    } else {
      RTC_LOG(LS_INFO) << "Remote SDP set successfully";
      drainCandidates();
    }
  } else {
    if (peer_connection_->local_description() != nullptr) {
      // set answer success
      RTC_LOG(LS_INFO) << "Local SDP set successfully";
      callback_->OnLocalDescription(peer_uid_, *local_sdp_);
      setVideoMaxBitrate();
      drainCandidates();
    } else {
      RTC_LOG(LS_INFO) << "Remote SDP set successfully";
    }
  }
}

void PeerConnectionClient::OnSetFailure(const webrtc::RTCError& error) {
  RTC_LOG(LS_INFO) << "onSetFailure " << error.message();
  reportError(PeerConnectionClientCallback::ERR_SET_SDP_FAIL);
}

void PeerConnectionClient::OnSignalingChange(
    webrtc::PeerConnectionInterface::SignalingState new_state) {}
void PeerConnectionClient::OnAddTrack(
    rtc::scoped_refptr<webrtc::RtpReceiverInterface> receiver,
    const std::vector<rtc::scoped_refptr<webrtc::MediaStreamInterface>>&
        streams) {}
void PeerConnectionClient::OnRemoveTrack(
    rtc::scoped_refptr<webrtc::RtpReceiverInterface> receiver) {}
void PeerConnectionClient::OnDataChannel(
    rtc::scoped_refptr<webrtc::DataChannelInterface> channel) {}
void PeerConnectionClient::OnRenegotiationNeeded() {}
void PeerConnectionClient::OnIceConnectionChange(
    webrtc::PeerConnectionInterface::IceConnectionState new_state) {
  switch (new_state) {
    case webrtc::PeerConnectionInterface::IceConnectionState::
        kIceConnectionConnected:
      callback_->OnIceConnected(peer_uid_);
      break;
    case webrtc::PeerConnectionInterface::IceConnectionState::
        kIceConnectionDisconnected:
      callback_->OnIceDisconnected(peer_uid_);
      break;
    case webrtc::PeerConnectionInterface::IceConnectionState::
        kIceConnectionFailed:
      reportError(PeerConnectionClientCallback::ERR_ICE_FAIL);
      break;
    default:
      break;
  }
}

void PeerConnectionClient::OnIceGatheringChange(
    webrtc::PeerConnectionInterface::IceGatheringState new_state) {}

void PeerConnectionClient::OnIceCandidate(
    const webrtc::IceCandidateInterface* candidate) {
  callback_->OnIceCandidate(peer_uid_, *candidate);
}

void PeerConnectionClient::OnIceConnectionReceivingChange(bool receiving) {}

void PeerConnectionClient::reportError(int code) {
  callback_->OnError(peer_uid_, code);
}

void PeerConnectionClient::getRemoteTracks() {
  if (remote_video_track_ || !receive()) {
    return;
  }
  for (auto& transceiver : peer_connection_->GetTransceivers()) {
    if (transceiver->media_type() == cricket::MediaType::MEDIA_TYPE_VIDEO) {
      remote_video_track_ = transceiver->receiver()->track();
#if defined(WEBRTC_WIN)
      for (auto renderer : remote_track_renderers_) {
        renderer->RenderTrack(reinterpret_cast<webrtc::VideoTrackInterface*>(
            remote_video_track_.get()));
        if (g_composer_) {
          g_composer_->AddRenderer(renderer);
        }
      }
#endif
    } else if (transceiver->media_type() ==
               cricket::MediaType::MEDIA_TYPE_AUDIO) {
      // todo this would cause crash when PCClient is destroyed
      // remote_audio_track_ = transceiver->receiver()->track();
    }
  }
}

void PeerConnectionClient::drainCandidates() {
  if (!candidates_drained_) {
    candidates_drained_ = true;
    for (const auto candidate : queued_candidates_) {
      if (!peer_connection_->AddIceCandidate(candidate)) {
        RTC_LOG(LS_ERROR)
            << "drainCandidates: failed to apply the received candidate";
      }
    }
    queued_candidates_.clear();
  }
}

bool PeerConnectionClient::send() {
  return dir_ == DIR_SEND_ONLY || dir_ == DIR_SEND_RECV;
}

bool PeerConnectionClient::receive() {
  return dir_ == DIR_RECV_ONLY || dir_ == DIR_SEND_RECV;
}

}  // namespace AvConf
