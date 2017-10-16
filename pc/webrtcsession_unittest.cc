/*
 *  Copyright 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <memory>
#include <utility>
#include <vector>

#include "api/fakemetricsobserver.h"
#include "api/jsepicecandidate.h"
#include "api/jsepsessiondescription.h"
#include "media/base/fakemediaengine.h"
#include "media/base/fakevideorenderer.h"
#include "media/base/mediachannel.h"
#include "media/engine/fakewebrtccall.h"
#include "media/sctp/sctptransportinternal.h"
#include "p2p/base/packettransportinternal.h"
#include "p2p/base/stunserver.h"
#include "p2p/base/teststunserver.h"
#include "p2p/base/testturnserver.h"
#include "p2p/client/basicportallocator.h"
#include "pc/audiotrack.h"
#include "pc/channelmanager.h"
#include "pc/mediasession.h"
#include "pc/peerconnection.h"
#include "pc/sctputils.h"
#include "pc/test/fakertccertificategenerator.h"
#include "pc/videotrack.h"
#include "pc/webrtcsession.h"
#include "pc/webrtcsessiondescriptionfactory.h"
#include "rtc_base/checks.h"
#include "rtc_base/fakenetwork.h"
#include "rtc_base/firewallsocketserver.h"
#include "rtc_base/gunit.h"
#include "rtc_base/logging.h"
#include "rtc_base/stringutils.h"
#include "rtc_base/virtualsocketserver.h"

using cricket::FakeVoiceMediaChannel;
using cricket::TransportInfo;
using rtc::SocketAddress;
using rtc::Thread;
using webrtc::CreateSessionDescription;
using webrtc::CreateSessionDescriptionObserver;
using webrtc::CreateSessionDescriptionRequest;
using webrtc::DataChannel;
using webrtc::FakeMetricsObserver;
using webrtc::IceCandidateCollection;
using webrtc::InternalDataChannelInit;
using webrtc::JsepIceCandidate;
using webrtc::JsepSessionDescription;
using webrtc::PeerConnectionFactoryInterface;
using webrtc::PeerConnectionInterface;
using webrtc::SessionDescriptionInterface;
using webrtc::SessionStats;
using webrtc::StreamCollection;
using webrtc::WebRtcSession;
using webrtc::kBundleWithoutRtcpMux;
using webrtc::kCreateChannelFailed;
using webrtc::kInvalidSdp;
using webrtc::kMlineMismatchInAnswer;
using webrtc::kPushDownTDFailed;
using webrtc::kSdpWithoutIceUfragPwd;
using webrtc::kSdpWithoutDtlsFingerprint;
using webrtc::kSdpWithoutSdesCrypto;
using webrtc::kSessionError;
using webrtc::kSessionErrorDesc;
using webrtc::kMaxUnsignalledRecvStreams;

typedef PeerConnectionInterface::RTCOfferAnswerOptions RTCOfferAnswerOptions;

static const int kClientAddrPort = 0;
static const char kClientAddrHost1[] = "11.11.11.11";
static const char kStunAddrHost[] = "99.99.99.1";

static const char kSessionVersion[] = "1";

// Media index of candidates belonging to the first media content.
static const int kMediaContentIndex0 = 0;
static const char kMediaContentName0[] = "audio";

// Media index of candidates belonging to the second media content.
static const int kMediaContentIndex1 = 1;
static const char kMediaContentName1[] = "video";

static const int kDefaultTimeout = 10000;  // 10 seconds.
static const int kIceCandidatesTimeout = 10000;

static const char kSdpWithRtx[] =
    "v=0\r\n"
    "o=- 4104004319237231850 2 IN IP4 127.0.0.1\r\n"
    "s=-\r\n"
    "t=0 0\r\n"
    "a=msid-semantic: WMS stream1\r\n"
    "m=video 9 RTP/SAVPF 0 96\r\n"
    "c=IN IP4 0.0.0.0\r\n"
    "a=rtcp:9 IN IP4 0.0.0.0\r\n"
    "a=ice-ufrag:CerjGp19G7wpXwl7\r\n"
    "a=ice-pwd:cMvOlFvQ6ochez1ZOoC2uBEC\r\n"
    "a=mid:video\r\n"
    "a=sendrecv\r\n"
    "a=rtcp-mux\r\n"
    "a=crypto:1 AES_CM_128_HMAC_SHA1_80 "
    "inline:5/4N5CDvMiyDArHtBByUM71VIkguH17ZNoX60GrA\r\n"
    "a=rtpmap:0 fake_video_codec/90000\r\n"
    "a=rtpmap:96 rtx/90000\r\n"
    "a=fmtp:96 apt=0\r\n";

static const char kStream1[] = "stream1";
static const char kVideoTrack1[] = "video1";
static const char kAudioTrack1[] = "audio1";

static const char kStream2[] = "stream2";
static const char kVideoTrack2[] = "video2";
static const char kAudioTrack2[] = "audio2";

static constexpr bool kStopped = true;
static constexpr bool kActive = false;

enum RTCCertificateGenerationMethod { ALREADY_GENERATED, DTLS_IDENTITY_STORE };

class MockIceObserver : public webrtc::IceObserver {
 public:
  MockIceObserver()
      : oncandidatesready_(false),
        ice_connection_state_(PeerConnectionInterface::kIceConnectionNew),
        ice_gathering_state_(PeerConnectionInterface::kIceGatheringNew) {
  }

  virtual ~MockIceObserver() = default;

  void OnIceConnectionStateChange(
      PeerConnectionInterface::IceConnectionState new_state) override {
    ice_connection_state_ = new_state;
    ice_connection_state_history_.push_back(new_state);
  }
  void OnIceGatheringChange(
      PeerConnectionInterface::IceGatheringState new_state) override {
    // We can never transition back to "new".
    EXPECT_NE(PeerConnectionInterface::kIceGatheringNew, new_state);
    ice_gathering_state_ = new_state;
    oncandidatesready_ =
        new_state == PeerConnectionInterface::kIceGatheringComplete;
  }

  // Found a new candidate.
  void OnIceCandidate(
      std::unique_ptr<webrtc::IceCandidateInterface> candidate) override {
    switch (candidate->sdp_mline_index()) {
      case kMediaContentIndex0:
        mline_0_candidates_.push_back(candidate->candidate());
        break;
      case kMediaContentIndex1:
        mline_1_candidates_.push_back(candidate->candidate());
        break;
      default:
        RTC_NOTREACHED();
    }

    // The ICE gathering state should always be Gathering when a candidate is
    // received (or possibly Completed in the case of the final candidate).
    EXPECT_NE(PeerConnectionInterface::kIceGatheringNew, ice_gathering_state_);
  }

  // Some local candidates are removed.
  void OnIceCandidatesRemoved(
      const std::vector<cricket::Candidate>& candidates) override {
    num_candidates_removed_ += candidates.size();
  }

  bool oncandidatesready_;
  std::vector<cricket::Candidate> mline_0_candidates_;
  std::vector<cricket::Candidate> mline_1_candidates_;
  PeerConnectionInterface::IceConnectionState ice_connection_state_;
  PeerConnectionInterface::IceGatheringState ice_gathering_state_;
  std::vector<PeerConnectionInterface::IceConnectionState>
      ice_connection_state_history_;
  size_t num_candidates_removed_ = 0;
};

// Used for tests in this file to verify that WebRtcSession responds to signals
// from the SctpTransport correctly, and calls Start with the correct
// local/remote ports.
class FakeSctpTransport : public cricket::SctpTransportInternal {
 public:
  void SetTransportChannel(rtc::PacketTransportInternal* channel) override {}
  bool Start(int local_port, int remote_port) override {
    local_port_ = local_port;
    remote_port_ = remote_port;
    return true;
  }
  bool OpenStream(int sid) override { return true; }
  bool ResetStream(int sid) override { return true; }
  bool SendData(const cricket::SendDataParams& params,
                const rtc::CopyOnWriteBuffer& payload,
                cricket::SendDataResult* result = nullptr) override {
    return true;
  }
  bool ReadyToSendData() override { return true; }
  void set_debug_name_for_testing(const char* debug_name) override {}

  int local_port() const { return local_port_; }
  int remote_port() const { return remote_port_; }

 private:
  int local_port_ = -1;
  int remote_port_ = -1;
};

class FakeSctpTransportFactory : public cricket::SctpTransportInternalFactory {
 public:
  std::unique_ptr<cricket::SctpTransportInternal> CreateSctpTransport(
      rtc::PacketTransportInternal*) override {
    last_fake_sctp_transport_ = new FakeSctpTransport();
    return std::unique_ptr<cricket::SctpTransportInternal>(
        last_fake_sctp_transport_);
  }

  FakeSctpTransport* last_fake_sctp_transport() {
    return last_fake_sctp_transport_;
  }

 private:
  FakeSctpTransport* last_fake_sctp_transport_ = nullptr;
};

class WebRtcSessionForTest : public webrtc::WebRtcSession {
 public:
  WebRtcSessionForTest(
      webrtc::Call* fake_call,
      cricket::ChannelManager* channel_manager,
      const cricket::MediaConfig& media_config,
      webrtc::RtcEventLog* event_log,
      rtc::Thread* network_thread,
      rtc::Thread* worker_thread,
      rtc::Thread* signaling_thread,
      cricket::PortAllocator* port_allocator,
      webrtc::IceObserver* ice_observer,
      std::unique_ptr<cricket::TransportController> transport_controller,
      std::unique_ptr<FakeSctpTransportFactory> sctp_factory)
      : WebRtcSession(fake_call, channel_manager, media_config, event_log,
                      network_thread,
                      worker_thread,
                      signaling_thread,
                      port_allocator,
                      std::move(transport_controller),
                      std::move(sctp_factory)) {
    RegisterIceObserver(ice_observer);
  }
  virtual ~WebRtcSessionForTest() {}

  // Note that these methods are only safe to use if the signaling thread
  // is the same as the worker thread
  rtc::PacketTransportInternal* voice_rtp_transport_channel() {
    return rtp_transport_channel(voice_channel());
  }

  rtc::PacketTransportInternal* voice_rtcp_transport_channel() {
    return rtcp_transport_channel(voice_channel());
  }

  rtc::PacketTransportInternal* video_rtp_transport_channel() {
    return rtp_transport_channel(video_channel());
  }

  rtc::PacketTransportInternal* video_rtcp_transport_channel() {
    return rtcp_transport_channel(video_channel());
  }

 private:
  rtc::PacketTransportInternal* rtp_transport_channel(
      cricket::BaseChannel* ch) {
    if (!ch) {
      return nullptr;
    }
    return ch->rtp_dtls_transport();
  }

  rtc::PacketTransportInternal* rtcp_transport_channel(
      cricket::BaseChannel* ch) {
    if (!ch) {
      return nullptr;
    }
    return ch->rtcp_dtls_transport();
  }
};

class WebRtcSessionCreateSDPObserverForTest
    : public rtc::RefCountedObject<CreateSessionDescriptionObserver> {
 public:
  enum State {
    kInit,
    kFailed,
    kSucceeded,
  };
  WebRtcSessionCreateSDPObserverForTest() : state_(kInit) {}

  // CreateSessionDescriptionObserver implementation.
  virtual void OnSuccess(SessionDescriptionInterface* desc) {
    description_.reset(desc);
    state_ = kSucceeded;
  }
  virtual void OnFailure(const std::string& error) {
    state_ = kFailed;
  }

  SessionDescriptionInterface* description() { return description_.get(); }

  SessionDescriptionInterface* ReleaseDescription() {
    return description_.release();
  }

  State state() const { return state_; }

 protected:
  ~WebRtcSessionCreateSDPObserverForTest() {}

 private:
  std::unique_ptr<SessionDescriptionInterface> description_;
  State state_;
};

class WebRtcSessionTest
    : public testing::TestWithParam<RTCCertificateGenerationMethod>,
      public sigslot::has_slots<> {
 protected:
  // TODO Investigate why ChannelManager crashes, if it's created
  // after stun_server.
  WebRtcSessionTest()
      : vss_(new rtc::VirtualSocketServer()),
        fss_(new rtc::FirewallSocketServer(vss_.get())),
        thread_(fss_.get()),
        media_engine_(new cricket::FakeMediaEngine()),
        data_engine_(new cricket::FakeDataEngine()),
        channel_manager_(new cricket::ChannelManager(
            std::unique_ptr<cricket::MediaEngineInterface>(media_engine_),
            std::unique_ptr<cricket::DataEngineInterface>(data_engine_),
            rtc::Thread::Current())),
        fake_call_(webrtc::Call::Config(&event_log_)),
        tdesc_factory_(new cricket::TransportDescriptionFactory()),
        desc_factory_(
            new cricket::MediaSessionDescriptionFactory(channel_manager_.get(),
                                                        tdesc_factory_.get())),
        stun_socket_addr_(
            rtc::SocketAddress(kStunAddrHost, cricket::STUN_SERVER_PORT)),
        stun_server_(cricket::TestStunServer::Create(Thread::Current(),
                                                     stun_socket_addr_)),
        metrics_observer_(new rtc::RefCountedObject<FakeMetricsObserver>()) {
    cricket::ServerAddresses stun_servers;
    stun_servers.insert(stun_socket_addr_);
    allocator_.reset(new cricket::BasicPortAllocator(
        &network_manager_,
        stun_servers,
        SocketAddress(), SocketAddress(), SocketAddress()));
    allocator_->set_flags(cricket::PORTALLOCATOR_DISABLE_TCP |
                          cricket::PORTALLOCATOR_DISABLE_RELAY);
    EXPECT_TRUE(channel_manager_->Init());
    allocator_->set_step_delay(cricket::kMinimumStepDelay);
  }

  void AddInterface(const SocketAddress& addr) {
    network_manager_.AddInterface(addr);
  }

  // If |cert_generator| != null or |rtc_configuration| contains |certificates|
  // then DTLS will be enabled unless explicitly disabled by |rtc_configuration|
  // options. When DTLS is enabled a certificate will be used if provided,
  // otherwise one will be generated using the |cert_generator|.
  void Init(
      std::unique_ptr<rtc::RTCCertificateGeneratorInterface> cert_generator,
      PeerConnectionInterface::RtcpMuxPolicy rtcp_mux_policy,
      const rtc::CryptoOptions& crypto_options) {
    ASSERT_TRUE(session_.get() == NULL);
    fake_sctp_transport_factory_ = new FakeSctpTransportFactory();
    session_.reset(new WebRtcSessionForTest(&fake_call_,
        channel_manager_.get(), cricket::MediaConfig(), &event_log_,
        rtc::Thread::Current(), rtc::Thread::Current(),
        rtc::Thread::Current(), allocator_.get(), &observer_,
        std::unique_ptr<cricket::TransportController>(
            new cricket::TransportController(
                rtc::Thread::Current(), rtc::Thread::Current(),
                allocator_.get(),
                /*redetermine_role_on_ice_restart=*/true, crypto_options)),
        std::unique_ptr<FakeSctpTransportFactory>(
            fake_sctp_transport_factory_)));
    session_->SignalDataChannelOpenMessage.connect(
        this, &WebRtcSessionTest::OnDataChannelOpenMessage);

    configuration_.rtcp_mux_policy = rtcp_mux_policy;
    EXPECT_EQ(PeerConnectionInterface::kIceConnectionNew,
        observer_.ice_connection_state_);
    EXPECT_EQ(PeerConnectionInterface::kIceGatheringNew,
        observer_.ice_gathering_state_);

    EXPECT_TRUE(session_->Initialize(options_, std::move(cert_generator),
                                     configuration_));
    session_->set_metrics_observer(metrics_observer_);
    crypto_options_ = crypto_options;
  }

  void OnDataChannelOpenMessage(const std::string& label,
                                const InternalDataChannelInit& config) {
    last_data_channel_label_ = label;
    last_data_channel_config_ = config;
  }

  void Init() {
    Init(nullptr, PeerConnectionInterface::kRtcpMuxPolicyNegotiate,
         rtc::CryptoOptions());
  }

  void InitWithBundlePolicy(
      PeerConnectionInterface::BundlePolicy bundle_policy) {
    configuration_.bundle_policy = bundle_policy;
    Init();
  }

  void InitWithRtcpMuxPolicy(
      PeerConnectionInterface::RtcpMuxPolicy rtcp_mux_policy) {
    PeerConnectionInterface::RTCConfiguration configuration;
    Init(nullptr, rtcp_mux_policy, rtc::CryptoOptions());
  }

  // Successfully init with DTLS; with a certificate generated and supplied or
  // with a store that generates it for us.
  void InitWithDtls(RTCCertificateGenerationMethod cert_gen_method) {
    std::unique_ptr<FakeRTCCertificateGenerator> cert_generator;
    if (cert_gen_method == ALREADY_GENERATED) {
      configuration_.certificates.push_back(
          FakeRTCCertificateGenerator::GenerateCertificate());
    } else if (cert_gen_method == DTLS_IDENTITY_STORE) {
      cert_generator.reset(new FakeRTCCertificateGenerator());
      cert_generator->set_should_fail(false);
    } else {
      RTC_CHECK(false);
    }
    Init(std::move(cert_generator),
         PeerConnectionInterface::kRtcpMuxPolicyNegotiate,
         rtc::CryptoOptions());
  }

  // The following convenience functions can be applied for both local side and
  // remote side. The flags can be overwritten for different use cases.
  void SendAudioVideoStream1() {
    send_stream_1_ = true;
    send_stream_2_ = false;
    local_send_audio_ = true;
    local_send_video_ = true;
    remote_send_audio_ = true;
    remote_send_video_ = true;
  }

  void SendAudioVideoStream2() {
    send_stream_1_ = false;
    send_stream_2_ = true;
    local_send_audio_ = true;
    local_send_video_ = true;
    remote_send_audio_ = true;
    remote_send_video_ = true;
  }

  void SendAudioVideoStream1And2() {
    send_stream_1_ = true;
    send_stream_2_ = true;
    local_send_audio_ = true;
    local_send_video_ = true;
    remote_send_audio_ = true;
    remote_send_video_ = true;
  }

  void SendNothing() {
    send_stream_1_ = false;
    send_stream_2_ = false;
    local_send_audio_ = false;
    local_send_video_ = false;
    remote_send_audio_ = false;
    remote_send_video_ = false;
  }

  void SendAudioOnlyStream2() {
    send_stream_1_ = false;
    send_stream_2_ = true;
    local_send_audio_ = true;
    local_send_video_ = false;
    remote_send_audio_ = true;
    remote_send_video_ = false;
  }

  void SendVideoOnlyStream2() {
    send_stream_1_ = false;
    send_stream_2_ = true;
    local_send_audio_ = false;
    local_send_video_ = true;
    remote_send_audio_ = false;
    remote_send_video_ = true;
  }

  // Helper function used to add a specific media section to the
  // |session_options|.
  void AddMediaSection(cricket::MediaType type,
                       const std::string& mid,
                       cricket::MediaContentDirection direction,
                       bool stopped,
                       cricket::MediaSessionOptions* opts) {
    opts->media_description_options.push_back(cricket::MediaDescriptionOptions(
        type, mid,
        cricket::RtpTransceiverDirection::FromMediaContentDirection(direction),
        stopped));
  }

  // Add the media sections to the options from |offered_media_sections_| when
  // creating an answer or a new offer.
  // This duplicates a lot of logic from PeerConnection but this can be fixed
  // when PeerConnection and WebRtcSession are merged.
  void AddExistingMediaSectionsAndSendersToOptions(
      cricket::MediaSessionOptions* session_options,
      bool send_audio,
      bool recv_audio,
      bool send_video,
      bool recv_video) {
    int num_sim_layer = 1;
    for (auto media_description_options : offered_media_sections_) {
      if (media_description_options.type == cricket::MEDIA_TYPE_AUDIO) {
        bool stopped = !send_audio && !recv_audio;
        auto media_desc_options = cricket::MediaDescriptionOptions(
            cricket::MEDIA_TYPE_AUDIO, media_description_options.mid,
            cricket::RtpTransceiverDirection(send_audio, recv_audio), stopped);
        if (send_stream_1_ && send_audio) {
          media_desc_options.AddAudioSender(kAudioTrack1, {kStream1});
        }
        if (send_stream_2_ && send_audio) {
          media_desc_options.AddAudioSender(kAudioTrack2, {kStream2});
        }
        session_options->media_description_options.push_back(
            media_desc_options);
      } else if (media_description_options.type == cricket::MEDIA_TYPE_VIDEO) {
        bool stopped = !send_video && !recv_video;
        auto media_desc_options = cricket::MediaDescriptionOptions(
            cricket::MEDIA_TYPE_VIDEO, media_description_options.mid,
            cricket::RtpTransceiverDirection(send_video, recv_video), stopped);
        if (send_stream_1_ && send_video) {
          media_desc_options.AddVideoSender(kVideoTrack1, {kStream1},
                                            num_sim_layer);
        }
        if (send_stream_2_ && send_video) {
          media_desc_options.AddVideoSender(kVideoTrack2, {kStream2},
                                            num_sim_layer);
        }
        session_options->media_description_options.push_back(
            media_desc_options);
      } else if (media_description_options.type == cricket::MEDIA_TYPE_DATA) {
        session_options->media_description_options.push_back(
            cricket::MediaDescriptionOptions(
                cricket::MEDIA_TYPE_DATA, media_description_options.mid,
                // Direction for data sections is meaningless, but legacy
                // endpoints might expect sendrecv.
                cricket::RtpTransceiverDirection(true, true), false));
      } else {
        RTC_NOTREACHED();
      }
    }
  }

  // Add the existing media sections first and then add new media sections if
  // needed.
  void AddMediaSectionsAndSendersToOptions(
      cricket::MediaSessionOptions* session_options,
      bool send_audio,
      bool recv_audio,
      bool send_video,
      bool recv_video) {
    AddExistingMediaSectionsAndSendersToOptions(
        session_options, send_audio, recv_audio, send_video, recv_video);

    if (!session_options->has_audio() && (send_audio || recv_audio)) {
      cricket::MediaDescriptionOptions media_desc_options =
          cricket::MediaDescriptionOptions(
              cricket::MEDIA_TYPE_AUDIO, cricket::CN_AUDIO,
              cricket::RtpTransceiverDirection(send_audio, recv_audio),
              kActive);
      if (send_stream_1_ && send_audio) {
        media_desc_options.AddAudioSender(kAudioTrack1, {kStream1});
      }
      if (send_stream_2_ && send_audio) {
        media_desc_options.AddAudioSender(kAudioTrack2, {kStream2});
      }
      session_options->media_description_options.push_back(media_desc_options);
      offered_media_sections_.push_back(media_desc_options);
    }

    if (!session_options->has_video() && (send_video || recv_video)) {
      cricket::MediaDescriptionOptions media_desc_options =
          cricket::MediaDescriptionOptions(
              cricket::MEDIA_TYPE_VIDEO, cricket::CN_VIDEO,
              cricket::RtpTransceiverDirection(send_video, recv_video),
              kActive);
      int num_sim_layer = 1;
      if (send_stream_1_ && send_video) {
        media_desc_options.AddVideoSender(kVideoTrack1, {kStream1},
                                          num_sim_layer);
      }
      if (send_stream_2_ && send_video) {
        media_desc_options.AddVideoSender(kVideoTrack2, {kStream2},
                                          num_sim_layer);
      }
      session_options->media_description_options.push_back(media_desc_options);
      offered_media_sections_.push_back(media_desc_options);
    }

    if (!session_options->has_data() &&
        (data_channel_ ||
         session_options->data_channel_type != cricket::DCT_NONE)) {
      cricket::MediaDescriptionOptions media_desc_options =
          cricket::MediaDescriptionOptions(
              cricket::MEDIA_TYPE_DATA, cricket::CN_DATA,
              cricket::RtpTransceiverDirection(true, true), kActive);
      if (session_options->data_channel_type == cricket::DCT_RTP) {
        media_desc_options.AddRtpDataChannel(data_channel_->label(),
                                             data_channel_->label());
      }
      session_options->media_description_options.push_back(media_desc_options);
      offered_media_sections_.push_back(media_desc_options);
    }
  }

  void GetOptionsForOffer(
      const PeerConnectionInterface::RTCOfferAnswerOptions& rtc_options,
      cricket::MediaSessionOptions* session_options) {
    ExtractSharedMediaSessionOptions(rtc_options, session_options);

    // |recv_X| is true by default if |offer_to_receive_X| is undefined.
    bool recv_audio = rtc_options.offer_to_receive_audio != 0;
    bool recv_video = rtc_options.offer_to_receive_video != 0;

    AddMediaSectionsAndSendersToOptions(session_options, local_send_audio_,
                                        recv_audio, local_send_video_,
                                        recv_video);
    session_options->bundle_enabled =
        session_options->bundle_enabled &&
        (session_options->has_audio() || session_options->has_video() ||
         session_options->has_data());

    session_options->crypto_options = crypto_options_;
  }

  void GetOptionsForAnswer(cricket::MediaSessionOptions* session_options) {
    AddExistingMediaSectionsAndSendersToOptions(
        session_options, local_send_audio_, local_recv_audio_,
        local_send_video_, local_recv_video_);

    session_options->bundle_enabled =
        session_options->bundle_enabled &&
        (session_options->has_audio() || session_options->has_video() ||
         session_options->has_data());

    if (session_->data_channel_type() != cricket::DCT_RTP) {
      session_options->data_channel_type = session_->data_channel_type();
    }

    session_options->crypto_options = crypto_options_;
  }

  void GetOptionsForRemoteAnswer(
      cricket::MediaSessionOptions* session_options) {
    bool recv_audio = local_send_audio_ || remote_recv_audio_;
    bool recv_video = local_send_video_ || remote_recv_video_;
    bool send_audio = false;
    bool send_video = false;

    AddExistingMediaSectionsAndSendersToOptions(
        session_options, send_audio, recv_audio, send_video, recv_video);

    session_options->bundle_enabled =
        session_options->bundle_enabled &&
        (session_options->has_audio() || session_options->has_video() ||
         session_options->has_data());

    if (session_->data_channel_type() != cricket::DCT_RTP) {
      session_options->data_channel_type = session_->data_channel_type();
    }

    session_options->crypto_options = crypto_options_;
  }

  void GetOptionsForAudioOnlyRemoteOffer(
      cricket::MediaSessionOptions* session_options) {
    remote_recv_audio_ = true;
    remote_recv_video_ = false;
    GetOptionsForRemoteOffer(session_options);
  }

  void GetOptionsForRemoteOffer(cricket::MediaSessionOptions* session_options) {
    AddMediaSectionsAndSendersToOptions(session_options, remote_send_audio_,
                                        remote_recv_audio_, remote_send_video_,
                                        remote_recv_video_);
    session_options->bundle_enabled =
        (session_options->has_audio() || session_options->has_video() ||
         session_options->has_data());

    if (session_->data_channel_type() != cricket::DCT_RTP) {
      session_options->data_channel_type = session_->data_channel_type();
    }

    session_options->crypto_options = crypto_options_;
  }

  // Creates a local offer and applies it. Starts ICE.
  // Call SendAudioVideoStreamX() before this function
  // to decide which streams to create.
  void InitiateCall() {
    SessionDescriptionInterface* offer = CreateOffer();
    SetLocalDescriptionWithoutError(offer);
    EXPECT_TRUE_WAIT(PeerConnectionInterface::kIceGatheringNew !=
        observer_.ice_gathering_state_,
        kIceCandidatesTimeout);
  }

  SessionDescriptionInterface* CreateOffer() {
    PeerConnectionInterface::RTCOfferAnswerOptions options;
    options.offer_to_receive_audio =
        RTCOfferAnswerOptions::kOfferToReceiveMediaTrue;
    return CreateOffer(options);
  }

  SessionDescriptionInterface* CreateOffer(
      const PeerConnectionInterface::RTCOfferAnswerOptions options) {
    rtc::scoped_refptr<WebRtcSessionCreateSDPObserverForTest>
        observer = new WebRtcSessionCreateSDPObserverForTest();
    cricket::MediaSessionOptions session_options;
    GetOptionsForOffer(options, &session_options);
    session_->CreateOffer(observer, options, session_options);
    EXPECT_TRUE_WAIT(
        observer->state() != WebRtcSessionCreateSDPObserverForTest::kInit,
        2000);
    return observer->ReleaseDescription();
  }

  SessionDescriptionInterface* CreateAnswer(
      const cricket::MediaSessionOptions& options) {
    rtc::scoped_refptr<WebRtcSessionCreateSDPObserverForTest> observer
        = new WebRtcSessionCreateSDPObserverForTest();
    cricket::MediaSessionOptions session_options = options;
    GetOptionsForAnswer(&session_options);
    session_->CreateAnswer(observer, session_options);
    EXPECT_TRUE_WAIT(
        observer->state() != WebRtcSessionCreateSDPObserverForTest::kInit,
        2000);
    return observer->ReleaseDescription();
  }

  SessionDescriptionInterface* CreateAnswer() {
    cricket::MediaSessionOptions options;
    options.bundle_enabled = true;
    return CreateAnswer(options);
  }

  // Set the internal fake description factories to do DTLS-SRTP.
  void SetFactoryDtlsSrtp() {
    desc_factory_->set_secure(cricket::SEC_DISABLED);
    std::string identity_name = "WebRTC" +
        rtc::ToString(rtc::CreateRandomId());
    // Confirmed to work with KT_RSA and KT_ECDSA.
    tdesc_factory_->set_certificate(
        rtc::RTCCertificate::Create(std::unique_ptr<rtc::SSLIdentity>(
            rtc::SSLIdentity::Generate(identity_name, rtc::KT_DEFAULT))));
    tdesc_factory_->set_secure(cricket::SEC_REQUIRED);
  }

  // Compares ufrag/password only for the specified |media_type|.
  bool IceUfragPwdEqual(const cricket::SessionDescription* desc1,
                        const cricket::SessionDescription* desc2,
                        cricket::MediaType media_type) {
    if (desc1->contents().size() != desc2->contents().size()) {
      return false;
    }

    const cricket::ContentInfo* cinfo =
        cricket::GetFirstMediaContent(desc1->contents(), media_type);
    const cricket::TransportDescription* transport_desc1 =
        desc1->GetTransportDescriptionByName(cinfo->name);
    const cricket::TransportDescription* transport_desc2 =
        desc2->GetTransportDescriptionByName(cinfo->name);
    if (!transport_desc1 || !transport_desc2) {
      return false;
    }
    if (transport_desc1->ice_pwd != transport_desc2->ice_pwd ||
        transport_desc1->ice_ufrag != transport_desc2->ice_ufrag) {
      return false;
    }
    return true;
  }

  // Sets ufrag/pwd for specified |media_type|.
  void SetIceUfragPwd(SessionDescriptionInterface* current_desc,
                      cricket::MediaType media_type,
                      const std::string& ufrag,
                      const std::string& pwd) {
    cricket::SessionDescription* desc = current_desc->description();
    const cricket::ContentInfo* cinfo =
        cricket::GetFirstMediaContent(desc->contents(), media_type);
    TransportInfo* transport_info = desc->GetTransportInfoByName(cinfo->name);
    cricket::TransportDescription* transport_desc =
        &transport_info->description;
    transport_desc->ice_ufrag = ufrag;
    transport_desc->ice_pwd = pwd;
  }

  // Creates a remote offer and and applies it as a remote description,
  // creates a local answer and applies is as a local description.
  // Call SendAudioVideoStreamX() before this function
  // to decide which local and remote streams to create.
  void CreateAndSetRemoteOfferAndLocalAnswer() {
    SessionDescriptionInterface* offer = CreateRemoteOffer();
    SetRemoteDescriptionWithoutError(offer);
    SessionDescriptionInterface* answer = CreateAnswer();
    SetLocalDescriptionWithoutError(answer);
  }
  void SetLocalDescriptionWithoutError(SessionDescriptionInterface* desc) {
    ASSERT_TRUE(session_->SetLocalDescription(desc, nullptr));
    session_->MaybeStartGathering();
  }
  void SetLocalDescriptionExpectState(SessionDescriptionInterface* desc,
                                      WebRtcSession::State expected_state) {
    SetLocalDescriptionWithoutError(desc);
    EXPECT_EQ(expected_state, session_->state());
  }
  void SetLocalDescriptionExpectError(const std::string& action,
                                      const std::string& expected_error,
                                      SessionDescriptionInterface* desc) {
    std::string error;
    EXPECT_FALSE(session_->SetLocalDescription(desc, &error));
    std::string sdp_type = "local ";
    sdp_type.append(action);
    EXPECT_NE(std::string::npos, error.find(sdp_type));
    EXPECT_NE(std::string::npos, error.find(expected_error));
  }
  void SetLocalDescriptionOfferExpectError(const std::string& expected_error,
                                           SessionDescriptionInterface* desc) {
    SetLocalDescriptionExpectError(SessionDescriptionInterface::kOffer,
                                   expected_error, desc);
  }
  void SetLocalDescriptionAnswerExpectError(const std::string& expected_error,
                                            SessionDescriptionInterface* desc) {
    SetLocalDescriptionExpectError(SessionDescriptionInterface::kAnswer,
                                   expected_error, desc);
  }
  void SetRemoteDescriptionWithoutError(SessionDescriptionInterface* desc) {
    ASSERT_TRUE(session_->SetRemoteDescription(desc, nullptr));
  }
  void SetRemoteDescriptionExpectState(SessionDescriptionInterface* desc,
                                       WebRtcSession::State expected_state) {
    SetRemoteDescriptionWithoutError(desc);
    EXPECT_EQ(expected_state, session_->state());
  }
  void SetRemoteDescriptionExpectError(const std::string& action,
                                       const std::string& expected_error,
                                       SessionDescriptionInterface* desc) {
    std::string error;
    EXPECT_FALSE(session_->SetRemoteDescription(desc, &error));
    std::string sdp_type = "remote ";
    sdp_type.append(action);
    EXPECT_NE(std::string::npos, error.find(sdp_type));
    EXPECT_NE(std::string::npos, error.find(expected_error));
  }
  void SetRemoteDescriptionOfferExpectError(
      const std::string& expected_error, SessionDescriptionInterface* desc) {
    SetRemoteDescriptionExpectError(SessionDescriptionInterface::kOffer,
                                    expected_error, desc);
  }
  void SetRemoteDescriptionAnswerExpectError(
      const std::string& expected_error, SessionDescriptionInterface* desc) {
    SetRemoteDescriptionExpectError(SessionDescriptionInterface::kAnswer,
                                    expected_error, desc);
  }

  JsepSessionDescription* CreateRemoteOfferWithVersion(
        cricket::MediaSessionOptions options,
        cricket::SecurePolicy secure_policy,
        const std::string& session_version,
        const SessionDescriptionInterface* current_desc) {
    std::string session_id = rtc::ToString(rtc::CreateRandomId64());
    const cricket::SessionDescription* cricket_desc = NULL;
    if (current_desc) {
      cricket_desc = current_desc->description();
      session_id = current_desc->session_id();
    }

    desc_factory_->set_secure(secure_policy);
    JsepSessionDescription* offer(
        new JsepSessionDescription(JsepSessionDescription::kOffer));
    if (!offer->Initialize(desc_factory_->CreateOffer(options, cricket_desc),
                           session_id, session_version)) {
      delete offer;
      offer = NULL;
    }
    return offer;
  }
  JsepSessionDescription* CreateRemoteOffer(
      cricket::MediaSessionOptions options) {
    return CreateRemoteOfferWithVersion(options, cricket::SEC_ENABLED,
                                        kSessionVersion, NULL);
  }
  JsepSessionDescription* CreateRemoteOffer(
      cricket::MediaSessionOptions options, cricket::SecurePolicy sdes_policy) {
    return CreateRemoteOfferWithVersion(
        options, sdes_policy, kSessionVersion, NULL);
  }
  JsepSessionDescription* CreateRemoteOffer(
      cricket::MediaSessionOptions options,
      const SessionDescriptionInterface* current_desc) {
    return CreateRemoteOfferWithVersion(options, cricket::SEC_ENABLED,
                                        kSessionVersion, current_desc);
  }

  SessionDescriptionInterface* CreateRemoteOfferWithSctpPort(
      const char* sctp_stream_name,
      int new_port,
      cricket::MediaSessionOptions options) {
    options.data_channel_type = cricket::DCT_SCTP;
    GetOptionsForRemoteOffer(&options);
    return ChangeSDPSctpPort(new_port, CreateRemoteOffer(options));
  }

  // Takes ownership of offer_basis (and deletes it).
  SessionDescriptionInterface* ChangeSDPSctpPort(
      int new_port,
      webrtc::SessionDescriptionInterface* offer_basis) {
    // Stringify the input SDP, swap the 5000 for 'new_port' and create a new
    // SessionDescription from the mutated string.
    const char* default_port_str = "5000";
    char new_port_str[16];
    rtc::sprintfn(new_port_str, sizeof(new_port_str), "%d", new_port);
    std::string offer_str;
    offer_basis->ToString(&offer_str);
    rtc::replace_substrs(default_port_str, strlen(default_port_str),
                               new_port_str, strlen(new_port_str),
                               &offer_str);
    SessionDescriptionInterface* offer =
        CreateSessionDescription(offer_basis->type(), offer_str, nullptr);
    delete offer_basis;
    return offer;
  }

  // Create a remote offer. Call SendAudioVideoStreamX()
  // before this function to decide which streams to create.
  JsepSessionDescription* CreateRemoteOffer() {
    cricket::MediaSessionOptions options;
    GetOptionsForRemoteOffer(&options);
    return CreateRemoteOffer(options, session_->remote_description());
  }

  JsepSessionDescription* CreateRemoteAnswer(
      const SessionDescriptionInterface* offer,
      cricket::MediaSessionOptions options,
      cricket::SecurePolicy policy) {
    desc_factory_->set_secure(policy);
    const std::string session_id =
        rtc::ToString(rtc::CreateRandomId64());
    JsepSessionDescription* answer(
        new JsepSessionDescription(JsepSessionDescription::kAnswer));
    if (!answer->Initialize(desc_factory_->CreateAnswer(offer->description(),
                                                        options, NULL),
                            session_id, kSessionVersion)) {
      delete answer;
      answer = NULL;
    }
    return answer;
  }

  JsepSessionDescription* CreateRemoteAnswer(
      const SessionDescriptionInterface* offer,
      cricket::MediaSessionOptions options) {
      return CreateRemoteAnswer(offer, options, cricket::SEC_REQUIRED);
  }

  // Creates an answer session description.
  // Call SendAudioVideoStreamX() before this function
  // to decide which streams to create.
  JsepSessionDescription* CreateRemoteAnswer(
      const SessionDescriptionInterface* offer) {
    cricket::MediaSessionOptions options;
    GetOptionsForAnswer(&options);
    options.bundle_enabled = true;
    return CreateRemoteAnswer(offer, options, cricket::SEC_REQUIRED);
  }

  void TestSessionCandidatesWithBundleRtcpMux(bool bundle, bool rtcp_mux) {
    AddInterface(rtc::SocketAddress(kClientAddrHost1, kClientAddrPort));
    Init();
    SendAudioVideoStream1();

    PeerConnectionInterface::RTCOfferAnswerOptions options;
    options.use_rtp_mux = bundle;

    SessionDescriptionInterface* offer = CreateOffer(options);
    // SetLocalDescription and SetRemoteDescriptions takes ownership of offer
    // and answer.
    SetLocalDescriptionWithoutError(offer);

    std::unique_ptr<SessionDescriptionInterface> answer(
        CreateRemoteAnswer(session_->local_description()));
    std::string sdp;
    EXPECT_TRUE(answer->ToString(&sdp));

    size_t expected_candidate_num = 2;
    if (!rtcp_mux) {
      // If rtcp_mux is enabled we should expect 4 candidates - host and srflex
      // for rtp and rtcp.
      expected_candidate_num = 4;
      // Disable rtcp-mux from the answer
      const std::string kRtcpMux = "a=rtcp-mux";
      const std::string kXRtcpMux = "a=xrtcp-mux";
      rtc::replace_substrs(kRtcpMux.c_str(), kRtcpMux.length(),
                                 kXRtcpMux.c_str(), kXRtcpMux.length(),
                                 &sdp);
    }

    SessionDescriptionInterface* new_answer = CreateSessionDescription(
        JsepSessionDescription::kAnswer, sdp, NULL);

    // SetRemoteDescription to enable rtcp mux.
    SetRemoteDescriptionWithoutError(new_answer);
    EXPECT_TRUE_WAIT(observer_.oncandidatesready_, kIceCandidatesTimeout);
    EXPECT_EQ(expected_candidate_num, observer_.mline_0_candidates_.size());
    if (bundle) {
      EXPECT_EQ(0, observer_.mline_1_candidates_.size());
    } else {
      EXPECT_EQ(expected_candidate_num, observer_.mline_1_candidates_.size());
    }
  }

  bool ContainsVideoCodecWithName(const SessionDescriptionInterface* desc,
                                  const std::string& codec_name) {
    for (const auto& content : desc->description()->contents()) {
      if (static_cast<cricket::MediaContentDescription*>(content.description)
              ->type() == cricket::MEDIA_TYPE_VIDEO) {
        const auto* mdesc =
            static_cast<cricket::VideoContentDescription*>(content.description);
        for (const auto& codec : mdesc->codecs()) {
          if (codec.name == codec_name) {
            return true;
          }
        }
      }
    }
    return false;
  }

  // The method sets up a call from the session to itself, in a loopback
  // arrangement.  It also uses a firewall rule to create a temporary
  // disconnection, and then a permanent disconnection.
  // This code is placed in a method so that it can be invoked
  // by multiple tests with different allocators (e.g. with and without BUNDLE).
  // While running the call, this method also checks if the session goes through
  // the correct sequence of ICE states when a connection is established,
  // broken, and re-established.
  // The Connection state should go:
  // New -> Checking -> (Connected) -> Completed -> Disconnected -> Completed
  //     -> Failed.
  // The Gathering state should go: New -> Gathering -> Completed.

  void SetupLoopbackCall() {
    Init();
    SendAudioVideoStream1();
    SessionDescriptionInterface* offer = CreateOffer();

    EXPECT_EQ(PeerConnectionInterface::kIceGatheringNew,
              observer_.ice_gathering_state_);
    SetLocalDescriptionWithoutError(offer);
    EXPECT_EQ(PeerConnectionInterface::kIceConnectionNew,
              observer_.ice_connection_state_);
    EXPECT_EQ_WAIT(PeerConnectionInterface::kIceGatheringGathering,
                   observer_.ice_gathering_state_, kIceCandidatesTimeout);
    EXPECT_TRUE_WAIT(observer_.oncandidatesready_, kIceCandidatesTimeout);
    EXPECT_EQ_WAIT(PeerConnectionInterface::kIceGatheringComplete,
                   observer_.ice_gathering_state_, kIceCandidatesTimeout);

    std::string sdp;
    offer->ToString(&sdp);
    SessionDescriptionInterface* desc = webrtc::CreateSessionDescription(
        JsepSessionDescription::kAnswer, sdp, nullptr);
    ASSERT_TRUE(desc != NULL);
    SetRemoteDescriptionWithoutError(desc);

    EXPECT_EQ_WAIT(PeerConnectionInterface::kIceConnectionChecking,
                   observer_.ice_connection_state_, kIceCandidatesTimeout);

    // The ice connection state is "Connected" too briefly to catch in a test.
    EXPECT_EQ_WAIT(PeerConnectionInterface::kIceConnectionCompleted,
                   observer_.ice_connection_state_, kIceCandidatesTimeout);
  }

  void TestPacketOptions() {
    AddInterface(rtc::SocketAddress(kClientAddrHost1, kClientAddrPort));

    SetupLoopbackCall();

    // Wait for channel to be ready for sending.
    EXPECT_TRUE_WAIT(media_engine_->GetVideoChannel(0)->sending(), 100);
    uint8_t test_packet[15] = {0};
    rtc::PacketOptions options;
    options.packet_id = 10;
    media_engine_->GetVideoChannel(0)
        ->SendRtp(test_packet, sizeof(test_packet), options);

    const int kPacketTimeout = 2000;
    EXPECT_EQ_WAIT(10, fake_call_.last_sent_nonnegative_packet_id(),
                   kPacketTimeout);
    EXPECT_GT(fake_call_.last_sent_packet().send_time_ms, -1);
  }

  // Adds CN codecs to FakeMediaEngine and MediaDescriptionFactory.
  void AddCNCodecs() {
    const cricket::AudioCodec kCNCodec1(102, "CN", 8000, 0, 1);
    const cricket::AudioCodec kCNCodec2(103, "CN", 16000, 0, 1);

    // Add kCNCodec for dtmf test.
    std::vector<cricket::AudioCodec> codecs =
        media_engine_->audio_send_codecs();
    codecs.push_back(kCNCodec1);
    codecs.push_back(kCNCodec2);
    media_engine_->SetAudioCodecs(codecs);
    desc_factory_->set_audio_codecs(codecs, codecs);
  }

  bool VerifyNoCNCodecs(const cricket::ContentInfo* content) {
    const cricket::ContentDescription* description = content->description;
    RTC_CHECK(description != NULL);
    const cricket::AudioContentDescription* audio_content_desc =
        static_cast<const cricket::AudioContentDescription*>(description);
    RTC_CHECK(audio_content_desc != NULL);
    for (size_t i = 0; i < audio_content_desc->codecs().size(); ++i) {
      if (audio_content_desc->codecs()[i].name == "CN")
        return false;
    }
    return true;
  }

  void CreateDataChannel() {
    webrtc::InternalDataChannelInit dci;
    RTC_CHECK(session_.get());
    dci.reliable = session_->data_channel_type() == cricket::DCT_SCTP;
    data_channel_ = DataChannel::Create(
        session_.get(), session_->data_channel_type(), "datachannel", dci);
  }

  void SetLocalDescriptionWithDataChannel() {
    CreateDataChannel();
    SessionDescriptionInterface* offer = CreateOffer();
    SetLocalDescriptionWithoutError(offer);
  }

  webrtc::RtcEventLogNullImpl event_log_;
  std::unique_ptr<rtc::VirtualSocketServer> vss_;
  std::unique_ptr<rtc::FirewallSocketServer> fss_;
  rtc::AutoSocketServerThread thread_;
  // |media_engine_| and |data_engine_| are actually owned by
  // |channel_manager_|.
  cricket::FakeMediaEngine* media_engine_;
  cricket::FakeDataEngine* data_engine_;
  // Actually owned by session_.
  FakeSctpTransportFactory* fake_sctp_transport_factory_ = nullptr;
  std::unique_ptr<cricket::ChannelManager> channel_manager_;
  cricket::FakeCall fake_call_;
  std::unique_ptr<cricket::TransportDescriptionFactory> tdesc_factory_;
  std::unique_ptr<cricket::MediaSessionDescriptionFactory> desc_factory_;
  rtc::SocketAddress stun_socket_addr_;
  std::unique_ptr<cricket::TestStunServer> stun_server_;
  rtc::FakeNetworkManager network_manager_;
  std::unique_ptr<cricket::BasicPortAllocator> allocator_;
  PeerConnectionFactoryInterface::Options options_;
  PeerConnectionInterface::RTCConfiguration configuration_;
  std::unique_ptr<WebRtcSessionForTest> session_;
  MockIceObserver observer_;
  cricket::FakeVideoMediaChannel* video_channel_;
  cricket::FakeVoiceMediaChannel* voice_channel_;
  rtc::scoped_refptr<FakeMetricsObserver> metrics_observer_;
  // The following flags affect options created for CreateOffer/CreateAnswer.
  bool send_stream_1_ = false;
  bool send_stream_2_ = false;
  bool local_send_audio_ = false;
  bool local_send_video_ = false;
  bool local_recv_audio_ = true;
  bool local_recv_video_ = true;
  bool remote_send_audio_ = false;
  bool remote_send_video_ = false;
  bool remote_recv_audio_ = true;
  bool remote_recv_video_ = true;
  std::vector<cricket::MediaDescriptionOptions> offered_media_sections_;
  rtc::scoped_refptr<DataChannel> data_channel_;
  // Last values received from data channel creation signal.
  std::string last_data_channel_label_;
  InternalDataChannelInit last_data_channel_config_;
  rtc::CryptoOptions crypto_options_;
};

TEST_F(WebRtcSessionTest, TestSessionCandidates) {
  TestSessionCandidatesWithBundleRtcpMux(false, false);
}

// Below test cases (TestSessionCandidatesWith*) verify the candidates gathered
// with rtcp-mux and/or bundle.
TEST_F(WebRtcSessionTest, TestSessionCandidatesWithRtcpMux) {
  TestSessionCandidatesWithBundleRtcpMux(false, true);
}

TEST_F(WebRtcSessionTest, TestSessionCandidatesWithBundleRtcpMux) {
  TestSessionCandidatesWithBundleRtcpMux(true, true);
}

TEST_F(WebRtcSessionTest, SetSdpFailedOnInvalidSdp) {
  Init();
  SessionDescriptionInterface* offer = NULL;
  // Since |offer| is NULL, there's no way to tell if it's an offer or answer.
  std::string unknown_action;
  SetLocalDescriptionExpectError(unknown_action, kInvalidSdp, offer);
  SetRemoteDescriptionExpectError(unknown_action, kInvalidSdp, offer);
}

// Test creating offers and receive answers and make sure the
// media engine creates the expected send and receive streams.
TEST_F(WebRtcSessionTest, TestCreateSdesOfferReceiveSdesAnswer) {
  Init();
  SendAudioVideoStream1();
  SessionDescriptionInterface* offer = CreateOffer();
  const std::string session_id_orig = offer->session_id();
  const std::string session_version_orig = offer->session_version();
  SetLocalDescriptionWithoutError(offer);

  SendAudioVideoStream2();
  SessionDescriptionInterface* answer =
      CreateRemoteAnswer(session_->local_description());
  SetRemoteDescriptionWithoutError(answer);

  video_channel_ = media_engine_->GetVideoChannel(0);
  voice_channel_ = media_engine_->GetVoiceChannel(0);

  ASSERT_EQ(1u, video_channel_->recv_streams().size());
  EXPECT_TRUE(kVideoTrack2 == video_channel_->recv_streams()[0].id);

  ASSERT_EQ(1u, voice_channel_->recv_streams().size());
  EXPECT_TRUE(kAudioTrack2 == voice_channel_->recv_streams()[0].id);

  ASSERT_EQ(1u, video_channel_->send_streams().size());
  EXPECT_TRUE(kVideoTrack1 == video_channel_->send_streams()[0].id);
  ASSERT_EQ(1u, voice_channel_->send_streams().size());
  EXPECT_TRUE(kAudioTrack1 == voice_channel_->send_streams()[0].id);

  // Create new offer without send streams.
  SendNothing();
  offer = CreateOffer();

  // Verify the session id is the same and the session version is
  // increased.
  EXPECT_EQ(session_id_orig, offer->session_id());
  EXPECT_LT(rtc::FromString<uint64_t>(session_version_orig),
            rtc::FromString<uint64_t>(offer->session_version()));

  SetLocalDescriptionWithoutError(offer);
  EXPECT_EQ(0u, video_channel_->send_streams().size());
  EXPECT_EQ(0u, voice_channel_->send_streams().size());

  SendAudioVideoStream2();
  answer = CreateRemoteAnswer(session_->local_description());
  SetRemoteDescriptionWithoutError(answer);

  // Make sure the receive streams have not changed.
  ASSERT_EQ(1u, video_channel_->recv_streams().size());
  EXPECT_TRUE(kVideoTrack2 == video_channel_->recv_streams()[0].id);
  ASSERT_EQ(1u, voice_channel_->recv_streams().size());
  EXPECT_TRUE(kAudioTrack2 == voice_channel_->recv_streams()[0].id);
}

// Test receiving offers and creating answers and make sure the
// media engine creates the expected send and receive streams.
TEST_F(WebRtcSessionTest, TestReceiveSdesOfferCreateSdesAnswer) {
  Init();
  SendAudioVideoStream2();
  SessionDescriptionInterface* offer = CreateOffer();
  SetRemoteDescriptionWithoutError(offer);

  SendAudioVideoStream1();
  SessionDescriptionInterface* answer = CreateAnswer();
  SetLocalDescriptionWithoutError(answer);

  const std::string session_id_orig = answer->session_id();
  const std::string session_version_orig = answer->session_version();

  video_channel_ = media_engine_->GetVideoChannel(0);
  voice_channel_ = media_engine_->GetVoiceChannel(0);

  ASSERT_TRUE(video_channel_);
  ASSERT_TRUE(voice_channel_);
  ASSERT_EQ(1u, video_channel_->recv_streams().size());
  EXPECT_TRUE(kVideoTrack2 == video_channel_->recv_streams()[0].id);

  ASSERT_EQ(1u, voice_channel_->recv_streams().size());
  EXPECT_TRUE(kAudioTrack2 == voice_channel_->recv_streams()[0].id);

  ASSERT_EQ(1u, video_channel_->send_streams().size());
  EXPECT_TRUE(kVideoTrack1 == video_channel_->send_streams()[0].id);
  ASSERT_EQ(1u, voice_channel_->send_streams().size());
  EXPECT_TRUE(kAudioTrack1 == voice_channel_->send_streams()[0].id);

  SendAudioVideoStream1And2();
  offer = CreateOffer();
  SetRemoteDescriptionWithoutError(offer);

  // Answer by turning off all send streams.
  SendNothing();
  answer = CreateAnswer();

  // Verify the session id is the same and the session version is
  // increased.
  EXPECT_EQ(session_id_orig, answer->session_id());
  EXPECT_LT(rtc::FromString<uint64_t>(session_version_orig),
            rtc::FromString<uint64_t>(answer->session_version()));
  SetLocalDescriptionWithoutError(answer);

  ASSERT_EQ(2u, video_channel_->recv_streams().size());
  EXPECT_TRUE(kVideoTrack1 == video_channel_->recv_streams()[0].id);
  EXPECT_TRUE(kVideoTrack2 == video_channel_->recv_streams()[1].id);
  ASSERT_EQ(2u, voice_channel_->recv_streams().size());
  EXPECT_TRUE(kAudioTrack1 == voice_channel_->recv_streams()[0].id);
  EXPECT_TRUE(kAudioTrack2 == voice_channel_->recv_streams()[1].id);

  // Make sure we have no send streams.
  EXPECT_EQ(0u, video_channel_->send_streams().size());
  EXPECT_EQ(0u, voice_channel_->send_streams().size());
}

TEST_F(WebRtcSessionTest, SetLocalSdpFailedOnCreateChannel) {
  Init();
  media_engine_->set_fail_create_channel(true);

  SessionDescriptionInterface* offer = CreateOffer();
  ASSERT_TRUE(offer != NULL);
  // SetRemoteDescription and SetLocalDescription will take the ownership of
  // the offer.
  SetRemoteDescriptionOfferExpectError(kCreateChannelFailed, offer);
  offer = CreateOffer();
  ASSERT_TRUE(offer != NULL);
  SetLocalDescriptionOfferExpectError(kCreateChannelFailed, offer);
}

// Test that we can create and set an answer correctly when different
// SSL roles have been negotiated for different transports.
// See: https://bugs.chromium.org/p/webrtc/issues/detail?id=4525
TEST_P(WebRtcSessionTest, TestCreateAnswerWithDifferentSslRoles) {
  SendAudioVideoStream1();
  InitWithDtls(GetParam());
  SetFactoryDtlsSrtp();

  SessionDescriptionInterface* offer = CreateOffer();
  SetLocalDescriptionWithoutError(offer);

  cricket::MediaSessionOptions options;
  GetOptionsForAnswer(&options);

  // First, negotiate different SSL roles.
  SessionDescriptionInterface* answer =
      CreateRemoteAnswer(offer, options, cricket::SEC_DISABLED);
  TransportInfo* audio_transport_info =
      answer->description()->GetTransportInfoByName("audio");
  audio_transport_info->description.connection_role =
      cricket::CONNECTIONROLE_ACTIVE;
  TransportInfo* video_transport_info =
      answer->description()->GetTransportInfoByName("video");
  video_transport_info->description.connection_role =
      cricket::CONNECTIONROLE_PASSIVE;
  SetRemoteDescriptionWithoutError(answer);

  // Now create an offer in the reverse direction, and ensure the initial
  // offerer responds with an answer with correct SSL roles.
  offer = CreateRemoteOfferWithVersion(options, cricket::SEC_DISABLED,
                                       kSessionVersion,
                                       session_->remote_description());
  SetRemoteDescriptionWithoutError(offer);

  cricket::MediaSessionOptions answer_options;
  answer_options.bundle_enabled = true;
  answer = CreateAnswer(answer_options);
  audio_transport_info = answer->description()->GetTransportInfoByName("audio");
  EXPECT_EQ(cricket::CONNECTIONROLE_PASSIVE,
            audio_transport_info->description.connection_role);
  video_transport_info = answer->description()->GetTransportInfoByName("video");
  EXPECT_EQ(cricket::CONNECTIONROLE_ACTIVE,
            video_transport_info->description.connection_role);
  SetLocalDescriptionWithoutError(answer);

  // Lastly, start BUNDLE-ing on "audio", expecting that the "passive" role of
  // audio is transferred over to video in the answer that completes the BUNDLE
  // negotiation.
  options.bundle_enabled = true;
  offer = CreateRemoteOfferWithVersion(options, cricket::SEC_DISABLED,
                                       kSessionVersion,
                                       session_->remote_description());
  SetRemoteDescriptionWithoutError(offer);
  answer = CreateAnswer(answer_options);
  audio_transport_info = answer->description()->GetTransportInfoByName("audio");
  EXPECT_EQ(cricket::CONNECTIONROLE_PASSIVE,
            audio_transport_info->description.connection_role);
  video_transport_info = answer->description()->GetTransportInfoByName("video");
  EXPECT_EQ(cricket::CONNECTIONROLE_PASSIVE,
            video_transport_info->description.connection_role);
  SetLocalDescriptionWithoutError(answer);
}

TEST_F(WebRtcSessionTest, TestSetLocalOfferTwice) {
  Init();
  SendNothing();
  // SetLocalDescription take ownership of offer.
  SessionDescriptionInterface* offer = CreateOffer();
  SetLocalDescriptionWithoutError(offer);

  // SetLocalDescription take ownership of offer.
  SessionDescriptionInterface* offer2 = CreateOffer();
  SetLocalDescriptionWithoutError(offer2);
}

TEST_F(WebRtcSessionTest, TestSetRemoteOfferTwice) {
  Init();
  SendNothing();
  // SetLocalDescription take ownership of offer.
  SessionDescriptionInterface* offer = CreateOffer();
  SetRemoteDescriptionWithoutError(offer);

  SessionDescriptionInterface* offer2 = CreateOffer();
  SetRemoteDescriptionWithoutError(offer2);
}

TEST_F(WebRtcSessionTest, TestSetLocalAndRemoteOffer) {
  Init();
  SendNothing();
  SessionDescriptionInterface* offer = CreateOffer();
  SetLocalDescriptionWithoutError(offer);
  offer = CreateOffer();
  SetRemoteDescriptionOfferExpectError("Called in wrong state: STATE_SENTOFFER",
                                       offer);
}

TEST_F(WebRtcSessionTest, TestSetRemoteAndLocalOffer) {
  Init();
  SendNothing();
  SessionDescriptionInterface* offer = CreateOffer();
  SetRemoteDescriptionWithoutError(offer);
  offer = CreateOffer();
  SetLocalDescriptionOfferExpectError(
      "Called in wrong state: STATE_RECEIVEDOFFER", offer);
}

TEST_F(WebRtcSessionTest, TestSetLocalPrAnswer) {
  Init();
  SendNothing();
  SessionDescriptionInterface* offer = CreateRemoteOffer();
  SetRemoteDescriptionExpectState(offer, WebRtcSession::STATE_RECEIVEDOFFER);

  JsepSessionDescription* pranswer =
      static_cast<JsepSessionDescription*>(CreateAnswer());
  pranswer->set_type(SessionDescriptionInterface::kPrAnswer);
  SetLocalDescriptionExpectState(pranswer, WebRtcSession::STATE_SENTPRANSWER);

  SendAudioVideoStream1();
  JsepSessionDescription* pranswer2 =
      static_cast<JsepSessionDescription*>(CreateAnswer());
  pranswer2->set_type(SessionDescriptionInterface::kPrAnswer);

  SetLocalDescriptionExpectState(pranswer2, WebRtcSession::STATE_SENTPRANSWER);

  SendAudioVideoStream2();
  SessionDescriptionInterface* answer = CreateAnswer();
  SetLocalDescriptionExpectState(answer, WebRtcSession::STATE_INPROGRESS);
}

TEST_F(WebRtcSessionTest, TestSetRemotePrAnswer) {
  Init();
  SendNothing();
  SessionDescriptionInterface* offer = CreateOffer();
  SetLocalDescriptionExpectState(offer, WebRtcSession::STATE_SENTOFFER);

  JsepSessionDescription* pranswer =
      CreateRemoteAnswer(session_->local_description());
  pranswer->set_type(SessionDescriptionInterface::kPrAnswer);

  SetRemoteDescriptionExpectState(pranswer,
                                  WebRtcSession::STATE_RECEIVEDPRANSWER);

  SendAudioVideoStream1();
  JsepSessionDescription* pranswer2 =
      CreateRemoteAnswer(session_->local_description());
  pranswer2->set_type(SessionDescriptionInterface::kPrAnswer);

  SetRemoteDescriptionExpectState(pranswer2,
                                  WebRtcSession::STATE_RECEIVEDPRANSWER);

  SendAudioVideoStream2();
  SessionDescriptionInterface* answer =
      CreateRemoteAnswer(session_->local_description());
  SetRemoteDescriptionExpectState(answer, WebRtcSession::STATE_INPROGRESS);
}

TEST_F(WebRtcSessionTest, TestSetLocalAnswerWithoutOffer) {
  Init();
  SendNothing();
  std::unique_ptr<SessionDescriptionInterface> offer(CreateOffer());

  SessionDescriptionInterface* answer =
      CreateRemoteAnswer(offer.get());
  SetLocalDescriptionAnswerExpectError("Called in wrong state: STATE_INIT",
                                       answer);
}

TEST_F(WebRtcSessionTest, TestSetRemoteAnswerWithoutOffer) {
  Init();
  SendNothing();
  std::unique_ptr<SessionDescriptionInterface> offer(CreateOffer());

  SessionDescriptionInterface* answer =
      CreateRemoteAnswer(offer.get());
  SetRemoteDescriptionAnswerExpectError(
      "Called in wrong state: STATE_INIT", answer);
}

// Verifies TransportProxy and media channels are created with content names
// present in the SessionDescription.
TEST_F(WebRtcSessionTest, TestChannelCreationsWithContentNames) {
  Init();
  SendAudioVideoStream1();
  std::unique_ptr<SessionDescriptionInterface> offer(CreateOffer());

  // CreateOffer creates session description with the content names "audio" and
  // "video". Goal is to modify these content names and verify transport
  // channels
  // in the WebRtcSession, as channels are created with the content names
  // present in SDP.
  std::string sdp;
  EXPECT_TRUE(offer->ToString(&sdp));

  SessionDescriptionInterface* modified_offer =
      CreateSessionDescription(JsepSessionDescription::kOffer, sdp, NULL);

  SetRemoteDescriptionWithoutError(modified_offer);

  cricket::MediaSessionOptions answer_options;
  answer_options.bundle_enabled = false;
  SessionDescriptionInterface* answer = CreateAnswer(answer_options);
  SetLocalDescriptionWithoutError(answer);

  rtc::PacketTransportInternal* voice_transport_channel =
      session_->voice_rtp_transport_channel();
  EXPECT_TRUE(voice_transport_channel != NULL);
  EXPECT_EQ(voice_transport_channel->debug_name(),
            "audio " + std::to_string(cricket::ICE_CANDIDATE_COMPONENT_RTP));
  rtc::PacketTransportInternal* video_transport_channel =
      session_->video_rtp_transport_channel();
  ASSERT_TRUE(video_transport_channel != NULL);
  EXPECT_EQ(video_transport_channel->debug_name(),
            "video " + std::to_string(cricket::ICE_CANDIDATE_COMPONENT_RTP));
  EXPECT_TRUE((video_channel_ = media_engine_->GetVideoChannel(0)) != NULL);
  EXPECT_TRUE((voice_channel_ = media_engine_->GetVoiceChannel(0)) != NULL);
}

// Test that an offer contains the correct media content descriptions based on
// the send streams when no constraints have been set.
TEST_F(WebRtcSessionTest, CreateOfferWithoutConstraintsOrStreams) {
  Init();
  std::unique_ptr<SessionDescriptionInterface> offer(CreateOffer());

  ASSERT_TRUE(offer != NULL);
  const cricket::ContentInfo* content =
      cricket::GetFirstAudioContent(offer->description());
  ASSERT_TRUE(content != NULL);
  EXPECT_EQ(
      cricket::MD_RECVONLY,
      static_cast<const cricket::AudioContentDescription*>(content->description)
          ->direction());
  content = cricket::GetFirstVideoContent(offer->description());
  ASSERT_TRUE(content != NULL);
  EXPECT_EQ(
      cricket::MD_RECVONLY,
      static_cast<const cricket::VideoContentDescription*>(content->description)
          ->direction());
}

// Test that an offer contains the correct media content descriptions based on
// the send streams when no constraints have been set.
TEST_F(WebRtcSessionTest, CreateOfferWithoutConstraints) {
  Init();
  // Test Audio only offer.
  SendAudioOnlyStream2();
  std::unique_ptr<SessionDescriptionInterface> offer(CreateOffer());

  const cricket::ContentInfo* content =
      cricket::GetFirstAudioContent(offer->description());
  ASSERT_TRUE(content != NULL);
  EXPECT_EQ(
      cricket::MD_SENDRECV,
      static_cast<const cricket::AudioContentDescription*>(content->description)
          ->direction());
  content = cricket::GetFirstVideoContent(offer->description());
  ASSERT_TRUE(content != NULL);
  EXPECT_EQ(
      cricket::MD_RECVONLY,
      static_cast<const cricket::VideoContentDescription*>(content->description)
          ->direction());

  // Test Audio / Video offer.
  SendAudioVideoStream1();
  offer.reset(CreateOffer());
  content = cricket::GetFirstAudioContent(offer->description());
  ASSERT_TRUE(content != NULL);
  EXPECT_EQ(
      cricket::MD_SENDRECV,
      static_cast<const cricket::AudioContentDescription*>(content->description)
          ->direction());

  content = cricket::GetFirstVideoContent(offer->description());
  ASSERT_TRUE(content != NULL);
  EXPECT_EQ(
      cricket::MD_SENDRECV,
      static_cast<const cricket::VideoContentDescription*>(content->description)
          ->direction());
}

// Test that an offer contains no media content descriptions if
// kOfferToReceiveVideo and kOfferToReceiveAudio constraints are set to false.
TEST_F(WebRtcSessionTest, CreateOfferWithConstraintsWithoutStreams) {
  Init();
  PeerConnectionInterface::RTCOfferAnswerOptions options;
  options.offer_to_receive_audio = 0;
  options.offer_to_receive_video = 0;

  std::unique_ptr<SessionDescriptionInterface> offer(CreateOffer(options));

  ASSERT_TRUE(offer != NULL);
  const cricket::ContentInfo* content =
      cricket::GetFirstAudioContent(offer->description());
  EXPECT_TRUE(content == NULL);
  content = cricket::GetFirstVideoContent(offer->description());
  EXPECT_TRUE(content == NULL);
}

// Test that an offer contains only audio media content descriptions if
// kOfferToReceiveAudio constraints are set to true.
TEST_F(WebRtcSessionTest, CreateAudioOnlyOfferWithConstraints) {
  Init();
  PeerConnectionInterface::RTCOfferAnswerOptions options;
  options.offer_to_receive_audio =
      RTCOfferAnswerOptions::kOfferToReceiveMediaTrue;
  options.offer_to_receive_video = 0;

  std::unique_ptr<SessionDescriptionInterface> offer(CreateOffer(options));

  const cricket::ContentInfo* content =
      cricket::GetFirstAudioContent(offer->description());
  EXPECT_TRUE(content != NULL);
  content = cricket::GetFirstVideoContent(offer->description());
  EXPECT_TRUE(content == NULL);
}

// Test that an offer contains audio and video media content descriptions if
// kOfferToReceiveAudio and kOfferToReceiveVideo constraints are set to true.
TEST_F(WebRtcSessionTest, CreateOfferWithConstraints) {
  Init();
  // Test Audio / Video offer.
  PeerConnectionInterface::RTCOfferAnswerOptions options;
  options.offer_to_receive_audio =
      RTCOfferAnswerOptions::kOfferToReceiveMediaTrue;
  options.offer_to_receive_video =
      RTCOfferAnswerOptions::kOfferToReceiveMediaTrue;

  std::unique_ptr<SessionDescriptionInterface> offer(CreateOffer(options));

  const cricket::ContentInfo* content =
      cricket::GetFirstAudioContent(offer->description());
  EXPECT_TRUE(content != NULL);

  content = cricket::GetFirstVideoContent(offer->description());
  EXPECT_TRUE(content != NULL);

  // Sets constraints to false and verifies that audio/video contents are
  // removed.
  options.offer_to_receive_audio = 0;
  options.offer_to_receive_video = 0;
  // Remove the media sections added in previous offer.
  offered_media_sections_.clear();
  offer.reset(CreateOffer(options));

  content = cricket::GetFirstAudioContent(offer->description());
  EXPECT_TRUE(content == NULL);
  content = cricket::GetFirstVideoContent(offer->description());
  EXPECT_TRUE(content == NULL);
}

// Test that an answer can not be created if the last remote description is not
// an offer.
TEST_F(WebRtcSessionTest, CreateAnswerWithoutAnOffer) {
  Init();
  SessionDescriptionInterface* offer = CreateOffer();
  SetLocalDescriptionWithoutError(offer);
  SessionDescriptionInterface* answer = CreateRemoteAnswer(offer);
  SetRemoteDescriptionWithoutError(answer);
  EXPECT_TRUE(CreateAnswer() == NULL);
}

// Test that an answer contains the correct media content descriptions when no
// constraints have been set.
TEST_F(WebRtcSessionTest, CreateAnswerWithoutConstraintsOrStreams) {
  Init();
  // Create a remote offer with audio and video content.
  std::unique_ptr<JsepSessionDescription> offer(CreateRemoteOffer());
  SetRemoteDescriptionWithoutError(offer.release());
  std::unique_ptr<SessionDescriptionInterface> answer(CreateAnswer());
  const cricket::ContentInfo* content =
      cricket::GetFirstAudioContent(answer->description());
  ASSERT_TRUE(content != NULL);
  EXPECT_FALSE(content->rejected);

  content = cricket::GetFirstVideoContent(answer->description());
  ASSERT_TRUE(content != NULL);
  EXPECT_FALSE(content->rejected);
}

// Test that an answer contains the correct media content descriptions when no
// constraints have been set and the offer only contain audio.
TEST_F(WebRtcSessionTest, CreateAudioAnswerWithoutConstraintsOrStreams) {
  Init();
  // Create a remote offer with audio only.
  cricket::MediaSessionOptions options;
  GetOptionsForAudioOnlyRemoteOffer(&options);

  std::unique_ptr<JsepSessionDescription> offer(CreateRemoteOffer(options));
  ASSERT_TRUE(cricket::GetFirstVideoContent(offer->description()) == NULL);
  ASSERT_TRUE(cricket::GetFirstAudioContent(offer->description()) != NULL);

  SetRemoteDescriptionWithoutError(offer.release());
  std::unique_ptr<SessionDescriptionInterface> answer(CreateAnswer());
  const cricket::ContentInfo* content =
      cricket::GetFirstAudioContent(answer->description());
  ASSERT_TRUE(content != NULL);
  EXPECT_FALSE(content->rejected);

  EXPECT_TRUE(cricket::GetFirstVideoContent(answer->description()) == NULL);
}

// Test that an answer contains the correct media content descriptions when no
// constraints have been set.
TEST_F(WebRtcSessionTest, CreateAnswerWithoutConstraints) {
  Init();
  // Create a remote offer with audio and video content.
  std::unique_ptr<JsepSessionDescription> offer(CreateRemoteOffer());
  SetRemoteDescriptionWithoutError(offer.release());
  // Test with a stream with tracks.
  SendAudioVideoStream1();
  std::unique_ptr<SessionDescriptionInterface> answer(CreateAnswer());
  const cricket::ContentInfo* content =
      cricket::GetFirstAudioContent(answer->description());
  ASSERT_TRUE(content != NULL);
  EXPECT_FALSE(content->rejected);

  content = cricket::GetFirstVideoContent(answer->description());
  ASSERT_TRUE(content != NULL);
  EXPECT_FALSE(content->rejected);
}

// Test that an answer contains the correct media content descriptions when
// constraints have been set but no stream is sent.
TEST_F(WebRtcSessionTest, CreateAnswerWithConstraintsWithoutStreams) {
  Init();
  // Create a remote offer with audio and video content.
  std::unique_ptr<JsepSessionDescription> offer(CreateRemoteOffer());
  SetRemoteDescriptionWithoutError(offer.release());

  cricket::MediaSessionOptions session_options;
  remote_send_audio_ = false;
  remote_send_video_ = false;
  local_recv_audio_ = false;
  local_recv_video_ = false;
  std::unique_ptr<SessionDescriptionInterface> answer(
      CreateAnswer(session_options));

  const cricket::ContentInfo* content =
      cricket::GetFirstAudioContent(answer->description());
  ASSERT_TRUE(content != NULL);
  EXPECT_TRUE(content->rejected);

  content = cricket::GetFirstVideoContent(answer->description());
  ASSERT_TRUE(content != NULL);
  EXPECT_TRUE(content->rejected);
}

// Test that an answer contains the correct media content descriptions when
// constraints have been set and streams are sent.
TEST_F(WebRtcSessionTest, CreateAnswerWithConstraints) {
  Init();
  // Create a remote offer with audio and video content.
  std::unique_ptr<JsepSessionDescription> offer(CreateRemoteOffer());
  SetRemoteDescriptionWithoutError(offer.release());

  cricket::MediaSessionOptions options;
  // Test with a stream with tracks.
  SendAudioVideoStream1();
  std::unique_ptr<SessionDescriptionInterface> answer(CreateAnswer(options));

  // TODO(perkj): Should the direction be set to SEND_ONLY?
  const cricket::ContentInfo* content =
      cricket::GetFirstAudioContent(answer->description());
  ASSERT_TRUE(content != NULL);
  EXPECT_FALSE(content->rejected);

  // TODO(perkj): Should the direction be set to SEND_ONLY?
  content = cricket::GetFirstVideoContent(answer->description());
  ASSERT_TRUE(content != NULL);
  EXPECT_FALSE(content->rejected);
}

TEST_F(WebRtcSessionTest, CreateOfferWithoutCNCodecs) {
  AddCNCodecs();
  Init();
  PeerConnectionInterface::RTCOfferAnswerOptions options;
  options.offer_to_receive_audio =
      RTCOfferAnswerOptions::kOfferToReceiveMediaTrue;
  options.voice_activity_detection = false;

  std::unique_ptr<SessionDescriptionInterface> offer(CreateOffer(options));

  const cricket::ContentInfo* content =
      cricket::GetFirstAudioContent(offer->description());
  EXPECT_TRUE(content != NULL);
  EXPECT_TRUE(VerifyNoCNCodecs(content));
}

TEST_F(WebRtcSessionTest, CreateAnswerWithoutCNCodecs) {
  AddCNCodecs();
  Init();
  // Create a remote offer with audio and video content.
  std::unique_ptr<JsepSessionDescription> offer(CreateRemoteOffer());
  SetRemoteDescriptionWithoutError(offer.release());

  cricket::MediaSessionOptions options;
  options.vad_enabled = false;
  std::unique_ptr<SessionDescriptionInterface> answer(CreateAnswer(options));
  const cricket::ContentInfo* content =
      cricket::GetFirstAudioContent(answer->description());
  ASSERT_TRUE(content != NULL);
  EXPECT_TRUE(VerifyNoCNCodecs(content));
}

// This test verifies the call setup when remote answer with audio only and
// later updates with video.
TEST_F(WebRtcSessionTest, TestAVOfferWithAudioOnlyAnswer) {
  Init();
  EXPECT_TRUE(media_engine_->GetVideoChannel(0) == NULL);
  EXPECT_TRUE(media_engine_->GetVoiceChannel(0) == NULL);

  SendAudioVideoStream1();
  SessionDescriptionInterface* offer = CreateOffer();

  cricket::MediaSessionOptions options;
  AddMediaSection(cricket::MEDIA_TYPE_AUDIO, cricket::CN_AUDIO,
                  cricket::MD_RECVONLY, kActive, &options);
  AddMediaSection(cricket::MEDIA_TYPE_VIDEO, cricket::CN_VIDEO,
                  cricket::MD_INACTIVE, kStopped, &options);
  local_recv_video_ = false;
  SessionDescriptionInterface* answer = CreateRemoteAnswer(offer, options);

  // SetLocalDescription and SetRemoteDescriptions takes ownership of offer
  // and answer;
  SetLocalDescriptionWithoutError(offer);
  SetRemoteDescriptionWithoutError(answer);

  video_channel_ = media_engine_->GetVideoChannel(0);
  voice_channel_ = media_engine_->GetVoiceChannel(0);

  ASSERT_TRUE(video_channel_ == nullptr);

  ASSERT_EQ(0u, voice_channel_->recv_streams().size());
  ASSERT_EQ(1u, voice_channel_->send_streams().size());
  EXPECT_EQ(kAudioTrack1, voice_channel_->send_streams()[0].id);

  // Let the remote end update the session descriptions, with Audio and Video.
  SendAudioVideoStream2();
  local_recv_video_ = true;
  CreateAndSetRemoteOfferAndLocalAnswer();

  video_channel_ = media_engine_->GetVideoChannel(0);
  voice_channel_ = media_engine_->GetVoiceChannel(0);

  ASSERT_TRUE(video_channel_ != nullptr);
  ASSERT_TRUE(voice_channel_ != nullptr);

  ASSERT_EQ(1u, video_channel_->recv_streams().size());
  ASSERT_EQ(1u, video_channel_->send_streams().size());
  EXPECT_EQ(kVideoTrack2, video_channel_->recv_streams()[0].id);
  EXPECT_EQ(kVideoTrack2, video_channel_->send_streams()[0].id);
  ASSERT_EQ(1u, voice_channel_->recv_streams().size());
  ASSERT_EQ(1u, voice_channel_->send_streams().size());
  EXPECT_EQ(kAudioTrack2, voice_channel_->recv_streams()[0].id);
  EXPECT_EQ(kAudioTrack2, voice_channel_->send_streams()[0].id);

  // Change session back to audio only.
  // The remote side doesn't send and recv video.
  SendAudioOnlyStream2();
  remote_recv_video_ = false;
  CreateAndSetRemoteOfferAndLocalAnswer();

  video_channel_ = media_engine_->GetVideoChannel(0);
  voice_channel_ = media_engine_->GetVoiceChannel(0);

  // The audio is expected to be rejected.
  EXPECT_TRUE(video_channel_ == nullptr);

  ASSERT_EQ(1u, voice_channel_->recv_streams().size());
  EXPECT_EQ(kAudioTrack2, voice_channel_->recv_streams()[0].id);
  ASSERT_EQ(1u, voice_channel_->send_streams().size());
  EXPECT_EQ(kAudioTrack2, voice_channel_->send_streams()[0].id);
}

// This test verifies the call setup when remote answer with video only and
// later updates with audio.
TEST_F(WebRtcSessionTest, TestAVOfferWithVideoOnlyAnswer) {
  Init();
  EXPECT_TRUE(media_engine_->GetVideoChannel(0) == NULL);
  EXPECT_TRUE(media_engine_->GetVoiceChannel(0) == NULL);
  SendAudioVideoStream1();
  SessionDescriptionInterface* offer = CreateOffer();

  cricket::MediaSessionOptions options;
  AddMediaSection(cricket::MEDIA_TYPE_AUDIO, cricket::CN_AUDIO,
                  cricket::MD_INACTIVE, kStopped, &options);
  AddMediaSection(cricket::MEDIA_TYPE_VIDEO, cricket::CN_VIDEO,
                  cricket::MD_RECVONLY, kActive, &options);
  local_recv_audio_ = false;
  SessionDescriptionInterface* answer =
      CreateRemoteAnswer(offer, options, cricket::SEC_ENABLED);

  // SetLocalDescription and SetRemoteDescriptions takes ownership of offer
  // and answer.
  SetLocalDescriptionWithoutError(offer);
  SetRemoteDescriptionWithoutError(answer);

  video_channel_ = media_engine_->GetVideoChannel(0);
  voice_channel_ = media_engine_->GetVoiceChannel(0);

  ASSERT_TRUE(voice_channel_ == NULL);
  ASSERT_TRUE(video_channel_ != NULL);

  EXPECT_EQ(0u, video_channel_->recv_streams().size());
  ASSERT_EQ(1u, video_channel_->send_streams().size());
  EXPECT_EQ(kVideoTrack1, video_channel_->send_streams()[0].id);

  // Update the session descriptions, with Audio and Video.
  SendAudioVideoStream2();
  local_recv_audio_ = true;
  SessionDescriptionInterface* offer2 = CreateRemoteOffer();
  SetRemoteDescriptionWithoutError(offer2);
  cricket::MediaSessionOptions answer_options;
  // Disable the bundling here. If the media is bundled on audio
  // transport, then we can't reject the audio because switching the bundled
  // transport is not currently supported.
  // (https://bugs.chromium.org/p/webrtc/issues/detail?id=6704)
  answer_options.bundle_enabled = false;
  SessionDescriptionInterface* answer2 = CreateAnswer(answer_options);
  SetLocalDescriptionWithoutError(answer2);

  voice_channel_ = media_engine_->GetVoiceChannel(0);

  ASSERT_TRUE(voice_channel_ != NULL);
  ASSERT_EQ(1u, voice_channel_->recv_streams().size());
  ASSERT_EQ(1u, voice_channel_->send_streams().size());
  EXPECT_EQ(kAudioTrack2, voice_channel_->recv_streams()[0].id);
  EXPECT_EQ(kAudioTrack2, voice_channel_->send_streams()[0].id);

  // Change session back to video only.
  // The remote side doesn't send and recv audio.
  SendVideoOnlyStream2();
  remote_recv_audio_ = false;
  SessionDescriptionInterface* offer3 = CreateRemoteOffer();
  SetRemoteDescriptionWithoutError(offer3);
  SessionDescriptionInterface* answer3 = CreateAnswer(answer_options);
  SetLocalDescriptionWithoutError(answer3);

  video_channel_ = media_engine_->GetVideoChannel(0);
  voice_channel_ = media_engine_->GetVoiceChannel(0);

  // The video is expected to be rejected.
  EXPECT_TRUE(voice_channel_ == nullptr);

  ASSERT_EQ(1u, video_channel_->recv_streams().size());
  EXPECT_EQ(kVideoTrack2, video_channel_->recv_streams()[0].id);
  ASSERT_EQ(1u, video_channel_->send_streams().size());
  EXPECT_EQ(kVideoTrack2, video_channel_->send_streams()[0].id);
}

// Test that candidates sent to the "video" transport do not get pushed down to
// the "audio" transport channel when bundling.
TEST_F(WebRtcSessionTest, TestIgnoreCandidatesForUnusedTransportWhenBundling) {
  AddInterface(rtc::SocketAddress(kClientAddrHost1, kClientAddrPort));

  InitWithBundlePolicy(PeerConnectionInterface::kBundlePolicyBalanced);
  SendAudioVideoStream1();

  cricket::MediaSessionOptions offer_options;
  GetOptionsForRemoteOffer(&offer_options);
  offer_options.bundle_enabled = true;

  SessionDescriptionInterface* offer = CreateRemoteOffer(offer_options);
  SetRemoteDescriptionWithoutError(offer);

  cricket::MediaSessionOptions answer_options;
  answer_options.bundle_enabled = true;
  SessionDescriptionInterface* answer = CreateAnswer(answer_options);
  SetLocalDescriptionWithoutError(answer);

  EXPECT_EQ(session_->voice_rtp_transport_channel(),
            session_->video_rtp_transport_channel());

  cricket::BaseChannel* voice_channel = session_->voice_channel();
  ASSERT_TRUE(voice_channel != NULL);

  // Checks if one of the transport channels contains a connection using a given
  // port.
  auto connection_with_remote_port = [this](int port) {
    std::unique_ptr<webrtc::SessionStats> stats = session_->GetStats_s();
    for (auto& kv : stats->transport_stats) {
      for (auto& chan_stat : kv.second.channel_stats) {
        for (auto& conn_info : chan_stat.connection_infos) {
          if (conn_info.remote_candidate.address().port() == port) {
            return true;
          }
        }
      }
    }
    return false;
  };

  EXPECT_FALSE(connection_with_remote_port(5000));
  EXPECT_FALSE(connection_with_remote_port(5001));
  EXPECT_FALSE(connection_with_remote_port(6000));

  // The way the *_WAIT checks work is they only wait if the condition fails,
  // which does not help in the case where state is not changing. This is
  // problematic in this test since we want to verify that adding a video
  // candidate does _not_ change state. So we interleave candidates and assume
  // that messages are executed in the order they were posted.

  // First audio candidate.
  cricket::Candidate candidate0;
  candidate0.set_address(rtc::SocketAddress("1.1.1.1", 5000));
  candidate0.set_component(1);
  candidate0.set_protocol("udp");
  candidate0.set_type("local");
  JsepIceCandidate ice_candidate0(kMediaContentName0, kMediaContentIndex0,
                                  candidate0);
  EXPECT_TRUE(session_->ProcessIceMessage(&ice_candidate0));

  // Video candidate.
  cricket::Candidate candidate1;
  candidate1.set_address(rtc::SocketAddress("1.1.1.1", 6000));
  candidate1.set_component(1);
  candidate1.set_protocol("udp");
  candidate1.set_type("local");
  JsepIceCandidate ice_candidate1(kMediaContentName1, kMediaContentIndex1,
                                  candidate1);
  EXPECT_TRUE(session_->ProcessIceMessage(&ice_candidate1));

  // Second audio candidate.
  cricket::Candidate candidate2;
  candidate2.set_address(rtc::SocketAddress("1.1.1.1", 5001));
  candidate2.set_component(1);
  candidate2.set_protocol("udp");
  candidate2.set_type("local");
  JsepIceCandidate ice_candidate2(kMediaContentName0, kMediaContentIndex0,
                                  candidate2);
  EXPECT_TRUE(session_->ProcessIceMessage(&ice_candidate2));

  EXPECT_TRUE_WAIT(connection_with_remote_port(5000), 1000);
  EXPECT_TRUE_WAIT(connection_with_remote_port(5001), 1000);

  // No need here for a _WAIT check since we are checking that state hasn't
  // changed: if this is false we would be doing waits for nothing and if this
  // is true then there will be no messages processed anyways.
  EXPECT_FALSE(connection_with_remote_port(6000));
}

// kBundlePolicyBalanced BUNDLE policy and answer contains BUNDLE.
TEST_F(WebRtcSessionTest, TestBalancedBundleInAnswer) {
  InitWithBundlePolicy(PeerConnectionInterface::kBundlePolicyBalanced);
  SendAudioVideoStream1();

  PeerConnectionInterface::RTCOfferAnswerOptions options;
  options.use_rtp_mux = true;

  SessionDescriptionInterface* offer = CreateOffer(options);
  SetLocalDescriptionWithoutError(offer);

  EXPECT_NE(session_->voice_rtp_transport_channel(),
            session_->video_rtp_transport_channel());

  SendAudioVideoStream2();
  SessionDescriptionInterface* answer =
      CreateRemoteAnswer(session_->local_description());
  SetRemoteDescriptionWithoutError(answer);

  EXPECT_EQ(session_->voice_rtp_transport_channel(),
            session_->video_rtp_transport_channel());
}

// kBundlePolicyBalanced BUNDLE policy but no BUNDLE in the answer.
TEST_F(WebRtcSessionTest, TestBalancedNoBundleInAnswer) {
  InitWithBundlePolicy(PeerConnectionInterface::kBundlePolicyBalanced);
  SendAudioVideoStream1();

  PeerConnectionInterface::RTCOfferAnswerOptions options;
  options.use_rtp_mux = true;

  SessionDescriptionInterface* offer = CreateOffer(options);
  SetLocalDescriptionWithoutError(offer);

  EXPECT_NE(session_->voice_rtp_transport_channel(),
            session_->video_rtp_transport_channel());

  SendAudioVideoStream2();

  // Remove BUNDLE from the answer.
  std::unique_ptr<SessionDescriptionInterface> answer(
      CreateRemoteAnswer(session_->local_description()));
  cricket::SessionDescription* answer_copy = answer->description()->Copy();
  answer_copy->RemoveGroupByName(cricket::GROUP_TYPE_BUNDLE);
  JsepSessionDescription* modified_answer =
      new JsepSessionDescription(JsepSessionDescription::kAnswer);
  modified_answer->Initialize(answer_copy, "1", "1");
  SetRemoteDescriptionWithoutError(modified_answer);  //

  EXPECT_NE(session_->voice_rtp_transport_channel(),
            session_->video_rtp_transport_channel());
}

// kBundlePolicyMaxBundle policy with BUNDLE in the answer.
TEST_F(WebRtcSessionTest, TestMaxBundleBundleInAnswer) {
  InitWithBundlePolicy(PeerConnectionInterface::kBundlePolicyMaxBundle);
  SendAudioVideoStream1();

  PeerConnectionInterface::RTCOfferAnswerOptions options;
  options.use_rtp_mux = true;

  SessionDescriptionInterface* offer = CreateOffer(options);
  SetLocalDescriptionWithoutError(offer);

  EXPECT_EQ(session_->voice_rtp_transport_channel(),
            session_->video_rtp_transport_channel());

  SendAudioVideoStream2();
  SessionDescriptionInterface* answer =
      CreateRemoteAnswer(session_->local_description());
  SetRemoteDescriptionWithoutError(answer);

  EXPECT_EQ(session_->voice_rtp_transport_channel(),
            session_->video_rtp_transport_channel());
}

// kBundlePolicyMaxBundle policy with BUNDLE in the answer, but no
// audio content in the answer.
TEST_F(WebRtcSessionTest, TestMaxBundleRejectAudio) {
  InitWithBundlePolicy(PeerConnectionInterface::kBundlePolicyMaxBundle);
  SendAudioVideoStream1();

  PeerConnectionInterface::RTCOfferAnswerOptions options;
  options.use_rtp_mux = true;

  SessionDescriptionInterface* offer = CreateOffer(options);
  SetLocalDescriptionWithoutError(offer);

  EXPECT_EQ(session_->voice_rtp_transport_channel(),
            session_->video_rtp_transport_channel());

  SendVideoOnlyStream2();
  local_send_audio_ = false;
  remote_recv_audio_ = false;
  cricket::MediaSessionOptions recv_options;
  GetOptionsForRemoteAnswer(&recv_options);
  SessionDescriptionInterface* answer =
      CreateRemoteAnswer(session_->local_description(), recv_options);
  SetRemoteDescriptionWithoutError(answer);

  EXPECT_TRUE(nullptr == session_->voice_channel());
  EXPECT_TRUE(nullptr != session_->video_rtp_transport_channel());

  session_->Close();
  EXPECT_TRUE(nullptr == session_->voice_rtp_transport_channel());
  EXPECT_TRUE(nullptr == session_->voice_rtcp_transport_channel());
  EXPECT_TRUE(nullptr == session_->video_rtp_transport_channel());
  EXPECT_TRUE(nullptr == session_->video_rtcp_transport_channel());
}

// kBundlePolicyMaxBundle policy but no BUNDLE in the answer.
TEST_F(WebRtcSessionTest, TestMaxBundleNoBundleInAnswer) {
  InitWithBundlePolicy(PeerConnectionInterface::kBundlePolicyMaxBundle);
  SendAudioVideoStream1();

  PeerConnectionInterface::RTCOfferAnswerOptions options;
  options.use_rtp_mux = true;

  SessionDescriptionInterface* offer = CreateOffer(options);
  SetLocalDescriptionWithoutError(offer);

  EXPECT_EQ(session_->voice_rtp_transport_channel(),
            session_->video_rtp_transport_channel());

  SendAudioVideoStream2();

  // Remove BUNDLE from the answer.
  std::unique_ptr<SessionDescriptionInterface> answer(
      CreateRemoteAnswer(session_->local_description()));
  cricket::SessionDescription* answer_copy = answer->description()->Copy();
  answer_copy->RemoveGroupByName(cricket::GROUP_TYPE_BUNDLE);
  JsepSessionDescription* modified_answer =
      new JsepSessionDescription(JsepSessionDescription::kAnswer);
  modified_answer->Initialize(answer_copy, "1", "1");
  SetRemoteDescriptionWithoutError(modified_answer);

  EXPECT_EQ(session_->voice_rtp_transport_channel(),
            session_->video_rtp_transport_channel());
}

// kBundlePolicyMaxBundle policy with BUNDLE in the remote offer.
TEST_F(WebRtcSessionTest, TestMaxBundleBundleInRemoteOffer) {
  InitWithBundlePolicy(PeerConnectionInterface::kBundlePolicyMaxBundle);
  SendAudioVideoStream1();

  SessionDescriptionInterface* offer = CreateRemoteOffer();
  SetRemoteDescriptionWithoutError(offer);

  EXPECT_EQ(session_->voice_rtp_transport_channel(),
            session_->video_rtp_transport_channel());

  SendAudioVideoStream2();
  SessionDescriptionInterface* answer = CreateAnswer();
  SetLocalDescriptionWithoutError(answer);

  EXPECT_EQ(session_->voice_rtp_transport_channel(),
            session_->video_rtp_transport_channel());
}

// kBundlePolicyMaxBundle policy but no BUNDLE in the remote offer.
TEST_F(WebRtcSessionTest, TestMaxBundleNoBundleInRemoteOffer) {
  InitWithBundlePolicy(PeerConnectionInterface::kBundlePolicyMaxBundle);
  SendAudioVideoStream1();

  // Remove BUNDLE from the offer.
  std::unique_ptr<SessionDescriptionInterface> offer(CreateRemoteOffer());
  cricket::SessionDescription* offer_copy = offer->description()->Copy();
  offer_copy->RemoveGroupByName(cricket::GROUP_TYPE_BUNDLE);
  JsepSessionDescription* modified_offer =
      new JsepSessionDescription(JsepSessionDescription::kOffer);
  modified_offer->Initialize(offer_copy, "1", "1");

  // Expect an error when applying the remote description
  SetRemoteDescriptionExpectError(JsepSessionDescription::kOffer,
                                  kCreateChannelFailed, modified_offer);
}

// kBundlePolicyMaxCompat bundle policy and answer contains BUNDLE.
TEST_F(WebRtcSessionTest, TestMaxCompatBundleInAnswer) {
  InitWithBundlePolicy(PeerConnectionInterface::kBundlePolicyMaxCompat);
  SendAudioVideoStream1();

  PeerConnectionInterface::RTCOfferAnswerOptions rtc_options;
  rtc_options.use_rtp_mux = true;

  SessionDescriptionInterface* offer = CreateOffer(rtc_options);
  SetLocalDescriptionWithoutError(offer);

  EXPECT_NE(session_->voice_rtp_transport_channel(),
            session_->video_rtp_transport_channel());

  SendAudioVideoStream2();
  SessionDescriptionInterface* answer =
      CreateRemoteAnswer(session_->local_description());
  SetRemoteDescriptionWithoutError(answer);

  // This should lead to an audio-only call but isn't implemented
  // correctly yet.
  EXPECT_EQ(session_->voice_rtp_transport_channel(),
            session_->video_rtp_transport_channel());
}

// kBundlePolicyMaxCompat BUNDLE policy but no BUNDLE in the answer.
TEST_F(WebRtcSessionTest, TestMaxCompatNoBundleInAnswer) {
  InitWithBundlePolicy(PeerConnectionInterface::kBundlePolicyMaxCompat);
  SendAudioVideoStream1();
  PeerConnectionInterface::RTCOfferAnswerOptions options;
  options.use_rtp_mux = true;

  SessionDescriptionInterface* offer = CreateOffer(options);
  SetLocalDescriptionWithoutError(offer);

  EXPECT_NE(session_->voice_rtp_transport_channel(),
            session_->video_rtp_transport_channel());

  SendAudioVideoStream2();

  // Remove BUNDLE from the answer.
  std::unique_ptr<SessionDescriptionInterface> answer(
      CreateRemoteAnswer(session_->local_description()));
  cricket::SessionDescription* answer_copy = answer->description()->Copy();
  answer_copy->RemoveGroupByName(cricket::GROUP_TYPE_BUNDLE);
  JsepSessionDescription* modified_answer =
      new JsepSessionDescription(JsepSessionDescription::kAnswer);
  modified_answer->Initialize(answer_copy, "1", "1");
  SetRemoteDescriptionWithoutError(modified_answer);  //

  EXPECT_NE(session_->voice_rtp_transport_channel(),
            session_->video_rtp_transport_channel());
}

// kBundlePolicyMaxbundle and then we call SetRemoteDescription first.
TEST_F(WebRtcSessionTest, TestMaxBundleWithSetRemoteDescriptionFirst) {
  InitWithBundlePolicy(PeerConnectionInterface::kBundlePolicyMaxBundle);
  SendAudioVideoStream1();

  PeerConnectionInterface::RTCOfferAnswerOptions options;
  options.use_rtp_mux = true;

  SessionDescriptionInterface* offer = CreateOffer(options);
  SetRemoteDescriptionWithoutError(offer);

  EXPECT_EQ(session_->voice_rtp_transport_channel(),
            session_->video_rtp_transport_channel());
}

// Adding a new channel to a BUNDLE which is already connected should directly
// assign the bundle transport to the channel, without first setting a
// disconnected non-bundle transport and then replacing it. The application
// should not receive any changes in the ICE state.
TEST_F(WebRtcSessionTest, TestAddChannelToConnectedBundle) {
  AddInterface(rtc::SocketAddress(kClientAddrHost1, kClientAddrPort));
  // Both BUNDLE and RTCP-mux need to be enabled for the ICE state to remain
  // connected. Disabling either of these two means that we need to wait for the
  // answer to find out if more transports are needed.
  configuration_.bundle_policy =
      PeerConnectionInterface::kBundlePolicyMaxBundle;
  options_.disable_encryption = true;
  InitWithRtcpMuxPolicy(PeerConnectionInterface::kRtcpMuxPolicyRequire);

  // Negotiate an audio channel with MAX_BUNDLE enabled.
  SendAudioOnlyStream2();
  SessionDescriptionInterface* offer = CreateOffer();
  SetLocalDescriptionWithoutError(offer);
  EXPECT_EQ_WAIT(PeerConnectionInterface::kIceGatheringComplete,
                 observer_.ice_gathering_state_, kIceCandidatesTimeout);
  std::string sdp;
  offer->ToString(&sdp);
  SessionDescriptionInterface* answer = webrtc::CreateSessionDescription(
      JsepSessionDescription::kAnswer, sdp, nullptr);
  ASSERT_TRUE(answer != NULL);
  SetRemoteDescriptionWithoutError(answer);

  // Wait for the ICE state to stabilize.
  EXPECT_EQ_WAIT(PeerConnectionInterface::kIceConnectionCompleted,
                 observer_.ice_connection_state_, kIceCandidatesTimeout);
  observer_.ice_connection_state_history_.clear();

  // Now add a video channel which should be using the same bundle transport.
  SendAudioVideoStream2();
  offer = CreateOffer();
  offer->ToString(&sdp);
  SetLocalDescriptionWithoutError(offer);
  answer = webrtc::CreateSessionDescription(JsepSessionDescription::kAnswer,
                                            sdp, nullptr);
  ASSERT_TRUE(answer != NULL);
  SetRemoteDescriptionWithoutError(answer);

  // Wait for ICE state to stabilize
  rtc::Thread::Current()->ProcessMessages(0);
  EXPECT_EQ_WAIT(PeerConnectionInterface::kIceConnectionCompleted,
                 observer_.ice_connection_state_, kIceCandidatesTimeout);

  // No ICE state changes are expected to happen.
  EXPECT_EQ(0, observer_.ice_connection_state_history_.size());
}

TEST_F(WebRtcSessionTest, TestRequireRtcpMux) {
  InitWithRtcpMuxPolicy(PeerConnectionInterface::kRtcpMuxPolicyRequire);
  SendAudioVideoStream1();

  PeerConnectionInterface::RTCOfferAnswerOptions options;
  SessionDescriptionInterface* offer = CreateOffer(options);
  SetLocalDescriptionWithoutError(offer);

  EXPECT_TRUE(session_->voice_rtcp_transport_channel() == NULL);
  EXPECT_TRUE(session_->video_rtcp_transport_channel() == NULL);

  SendAudioVideoStream2();
  SessionDescriptionInterface* answer =
      CreateRemoteAnswer(session_->local_description());
  SetRemoteDescriptionWithoutError(answer);

  EXPECT_TRUE(session_->voice_rtcp_transport_channel() == NULL);
  EXPECT_TRUE(session_->video_rtcp_transport_channel() == NULL);
}

TEST_F(WebRtcSessionTest, TestNegotiateRtcpMux) {
  InitWithRtcpMuxPolicy(PeerConnectionInterface::kRtcpMuxPolicyNegotiate);
  SendAudioVideoStream1();

  PeerConnectionInterface::RTCOfferAnswerOptions options;
  SessionDescriptionInterface* offer = CreateOffer(options);
  SetLocalDescriptionWithoutError(offer);

  EXPECT_TRUE(session_->voice_rtcp_transport_channel() != NULL);
  EXPECT_TRUE(session_->video_rtcp_transport_channel() != NULL);

  SendAudioVideoStream2();
  SessionDescriptionInterface* answer =
      CreateRemoteAnswer(session_->local_description());
  SetRemoteDescriptionWithoutError(answer);

  EXPECT_TRUE(session_->voice_rtcp_transport_channel() == NULL);
  EXPECT_TRUE(session_->video_rtcp_transport_channel() == NULL);
}

// This test verifies that SetLocalDescription and SetRemoteDescription fails
// if BUNDLE is enabled but rtcp-mux is disabled in m-lines.
TEST_F(WebRtcSessionTest, TestDisabledRtcpMuxWithBundleEnabled) {
  Init();
  SendAudioVideoStream1();

  PeerConnectionInterface::RTCOfferAnswerOptions options;
  options.use_rtp_mux = true;

  SessionDescriptionInterface* offer = CreateOffer(options);
  std::string offer_str;
  offer->ToString(&offer_str);
  // Disable rtcp-mux
  const std::string rtcp_mux = "rtcp-mux";
  const std::string xrtcp_mux = "xrtcp-mux";
  rtc::replace_substrs(rtcp_mux.c_str(), rtcp_mux.length(),
                             xrtcp_mux.c_str(), xrtcp_mux.length(),
                             &offer_str);
  SessionDescriptionInterface* local_offer = CreateSessionDescription(
      SessionDescriptionInterface::kOffer, offer_str, nullptr);
  ASSERT_TRUE(local_offer);
  SetLocalDescriptionOfferExpectError(kBundleWithoutRtcpMux, local_offer);

  SessionDescriptionInterface* remote_offer = CreateSessionDescription(
      SessionDescriptionInterface::kOffer, offer_str, nullptr);
  ASSERT_TRUE(remote_offer);
  SetRemoteDescriptionOfferExpectError(kBundleWithoutRtcpMux, remote_offer);

  // Trying unmodified SDP.
  SetLocalDescriptionWithoutError(offer);
}

// This test verifies the |initial_offerer| flag when session initiates the
// call.
TEST_F(WebRtcSessionTest, TestInitiatorFlagAsOriginator) {
  Init();
  EXPECT_FALSE(session_->initial_offerer());
  SessionDescriptionInterface* offer = CreateOffer();
  SessionDescriptionInterface* answer = CreateRemoteAnswer(offer);
  SetLocalDescriptionWithoutError(offer);
  EXPECT_TRUE(session_->initial_offerer());
  SetRemoteDescriptionWithoutError(answer);
  EXPECT_TRUE(session_->initial_offerer());
}

// This test verifies the |initial_offerer| flag when session receives the call.
TEST_F(WebRtcSessionTest, TestInitiatorFlagAsReceiver) {
  Init();
  EXPECT_FALSE(session_->initial_offerer());
  SessionDescriptionInterface* offer = CreateRemoteOffer();
  SetRemoteDescriptionWithoutError(offer);
  SessionDescriptionInterface* answer = CreateAnswer();

  EXPECT_FALSE(session_->initial_offerer());
  SetLocalDescriptionWithoutError(answer);
  EXPECT_FALSE(session_->initial_offerer());
}

// Verifing local offer and remote answer have matching m-lines as per RFC 3264.
TEST_F(WebRtcSessionTest, TestIncorrectMLinesInRemoteAnswer) {
  Init();
  SendAudioVideoStream1();
  SessionDescriptionInterface* offer = CreateOffer();
  SetLocalDescriptionWithoutError(offer);
  std::unique_ptr<SessionDescriptionInterface> answer(
      CreateRemoteAnswer(session_->local_description()));

  cricket::SessionDescription* answer_copy = answer->description()->Copy();
  answer_copy->RemoveContentByName("video");
  JsepSessionDescription* modified_answer =
      new JsepSessionDescription(JsepSessionDescription::kAnswer);

  EXPECT_TRUE(modified_answer->Initialize(answer_copy,
                                          answer->session_id(),
                                          answer->session_version()));
  SetRemoteDescriptionAnswerExpectError(kMlineMismatchInAnswer,
                                        modified_answer);

  // Different content names.
  std::string sdp;
  EXPECT_TRUE(answer->ToString(&sdp));
  const std::string kAudioMid = "a=mid:audio";
  const std::string kAudioMidReplaceStr = "a=mid:audio_content_name";
  rtc::replace_substrs(kAudioMid.c_str(), kAudioMid.length(),
                             kAudioMidReplaceStr.c_str(),
                             kAudioMidReplaceStr.length(),
                             &sdp);
  SessionDescriptionInterface* modified_answer1 =
      CreateSessionDescription(JsepSessionDescription::kAnswer, sdp, NULL);
  SetRemoteDescriptionAnswerExpectError(kMlineMismatchInAnswer,
                                        modified_answer1);

  // Different media types.
  EXPECT_TRUE(answer->ToString(&sdp));
  const std::string kAudioMline = "m=audio";
  const std::string kAudioMlineReplaceStr = "m=video";
  rtc::replace_substrs(kAudioMline.c_str(), kAudioMline.length(),
                             kAudioMlineReplaceStr.c_str(),
                             kAudioMlineReplaceStr.length(),
                             &sdp);
  SessionDescriptionInterface* modified_answer2 =
      CreateSessionDescription(JsepSessionDescription::kAnswer, sdp, NULL);
  SetRemoteDescriptionAnswerExpectError(kMlineMismatchInAnswer,
                                        modified_answer2);

  SetRemoteDescriptionWithoutError(answer.release());
}

// Verifying remote offer and local answer have matching m-lines as per
// RFC 3264.
TEST_F(WebRtcSessionTest, TestIncorrectMLinesInLocalAnswer) {
  Init();
  SendAudioVideoStream1();
  SessionDescriptionInterface* offer = CreateRemoteOffer();
  SetRemoteDescriptionWithoutError(offer);
  SessionDescriptionInterface* answer = CreateAnswer();

  cricket::SessionDescription* answer_copy = answer->description()->Copy();
  answer_copy->RemoveContentByName("video");
  JsepSessionDescription* modified_answer =
      new JsepSessionDescription(JsepSessionDescription::kAnswer);

  EXPECT_TRUE(modified_answer->Initialize(answer_copy,
                                          answer->session_id(),
                                          answer->session_version()));
  SetLocalDescriptionAnswerExpectError(kMlineMismatchInAnswer, modified_answer);
  SetLocalDescriptionWithoutError(answer);
}

TEST_F(WebRtcSessionTest, TestSessionContentError) {
  Init();
  SendAudioVideoStream1();
  SessionDescriptionInterface* offer = CreateOffer();
  const std::string session_id_orig = offer->session_id();
  const std::string session_version_orig = offer->session_version();
  SetLocalDescriptionWithoutError(offer);

  video_channel_ = media_engine_->GetVideoChannel(0);
  video_channel_->set_fail_set_send_codecs(true);

  SessionDescriptionInterface* answer =
      CreateRemoteAnswer(session_->local_description());
  SetRemoteDescriptionAnswerExpectError("ERROR_CONTENT", answer);

  // Test that after a content error, setting any description will
  // result in an error.
  video_channel_->set_fail_set_send_codecs(false);
  answer = CreateRemoteAnswer(session_->local_description());
  SetRemoteDescriptionExpectError("", "ERROR_CONTENT", answer);
  offer = CreateRemoteOffer();
  SetLocalDescriptionExpectError("", "ERROR_CONTENT", offer);
}

TEST_F(WebRtcSessionTest, TestRtpDataChannel) {
  configuration_.enable_rtp_data_channel = true;
  Init();
  SetLocalDescriptionWithDataChannel();
  ASSERT_TRUE(data_engine_);
  EXPECT_NE(nullptr, data_engine_->GetChannel(0));
}

TEST_P(WebRtcSessionTest, TestRtpDataChannelConstraintTakesPrecedence) {
  configuration_.enable_rtp_data_channel = true;
  options_.disable_sctp_data_channels = false;

  InitWithDtls(GetParam());

  SetLocalDescriptionWithDataChannel();
  EXPECT_NE(nullptr, data_engine_->GetChannel(0));
}

// Test that sctp_content_name/sctp_transport_name (used for stats) are correct
// before and after BUNDLE is negotiated.
TEST_P(WebRtcSessionTest, SctpContentAndTransportName) {
  SetFactoryDtlsSrtp();
  InitWithDtls(GetParam());

  // Initially these fields should be empty.
  EXPECT_FALSE(session_->sctp_content_name());
  EXPECT_FALSE(session_->sctp_transport_name());

  // Create offer with audio/video/data.
  // Default bundle policy is "balanced", so data should be using its own
  // transport.
  SendAudioVideoStream1();
  CreateDataChannel();
  InitiateCall();
  ASSERT_TRUE(session_->sctp_content_name());
  ASSERT_TRUE(session_->sctp_transport_name());
  EXPECT_EQ("data", *session_->sctp_content_name());
  EXPECT_EQ("data", *session_->sctp_transport_name());

  // Create answer that finishes BUNDLE negotiation, which means everything
  // should be bundled on the first transport (audio).
  cricket::MediaSessionOptions answer_options;
  answer_options.bundle_enabled = true;
  answer_options.data_channel_type = cricket::DCT_SCTP;
  GetOptionsForAnswer(&answer_options);
  SetRemoteDescriptionWithoutError(CreateRemoteAnswer(
      session_->local_description(), answer_options, cricket::SEC_DISABLED));
  ASSERT_TRUE(session_->sctp_content_name());
  ASSERT_TRUE(session_->sctp_transport_name());
  EXPECT_EQ("data", *session_->sctp_content_name());
  EXPECT_EQ("audio", *session_->sctp_transport_name());
}

TEST_P(WebRtcSessionTest, TestCreateOfferWithSctpEnabledWithoutStreams) {
  InitWithDtls(GetParam());

  std::unique_ptr<SessionDescriptionInterface> offer(CreateOffer());
  EXPECT_TRUE(offer->description()->GetContentByName("data") == NULL);
  EXPECT_TRUE(offer->description()->GetTransportInfoByName("data") == NULL);
}

TEST_P(WebRtcSessionTest, TestCreateAnswerWithSctpInOfferAndNoStreams) {
  SetFactoryDtlsSrtp();
  InitWithDtls(GetParam());

  // Create remote offer with SCTP.
  cricket::MediaSessionOptions options;
  options.data_channel_type = cricket::DCT_SCTP;
  GetOptionsForRemoteOffer(&options);
  JsepSessionDescription* offer =
      CreateRemoteOffer(options, cricket::SEC_DISABLED);
  SetRemoteDescriptionWithoutError(offer);

  // Verifies the answer contains SCTP.
  std::unique_ptr<SessionDescriptionInterface> answer(CreateAnswer());
  EXPECT_TRUE(answer != NULL);
  EXPECT_TRUE(answer->description()->GetContentByName("data") != NULL);
  EXPECT_TRUE(answer->description()->GetTransportInfoByName("data") != NULL);
}

// Test that if DTLS is disabled, we don't end up with an SctpTransport
// created (or an RtpDataChannel).
TEST_P(WebRtcSessionTest, TestSctpDataChannelWithoutDtls) {
  configuration_.enable_dtls_srtp = rtc::Optional<bool>(false);
  InitWithDtls(GetParam());

  SetLocalDescriptionWithDataChannel();
  EXPECT_EQ(nullptr, data_engine_->GetChannel(0));
  EXPECT_EQ(nullptr, fake_sctp_transport_factory_->last_fake_sctp_transport());
}

// Test that if DTLS is enabled, we end up with an SctpTransport created
// (and not an RtpDataChannel).
TEST_P(WebRtcSessionTest, TestSctpDataChannelWithDtls) {
  InitWithDtls(GetParam());

  SetLocalDescriptionWithDataChannel();
  EXPECT_EQ(nullptr, data_engine_->GetChannel(0));
  EXPECT_NE(nullptr, fake_sctp_transport_factory_->last_fake_sctp_transport());
}

// Test that if SCTP is disabled, we don't end up with an SctpTransport
// created (or an RtpDataChannel).
TEST_P(WebRtcSessionTest, TestDisableSctpDataChannels) {
  options_.disable_sctp_data_channels = true;
  InitWithDtls(GetParam());

  SetLocalDescriptionWithDataChannel();
  EXPECT_EQ(nullptr, data_engine_->GetChannel(0));
  EXPECT_EQ(nullptr, fake_sctp_transport_factory_->last_fake_sctp_transport());
}

TEST_P(WebRtcSessionTest, TestSctpDataChannelSendPortParsing) {
  const int new_send_port = 9998;
  const int new_recv_port = 7775;

  InitWithDtls(GetParam());
  SetFactoryDtlsSrtp();

  // By default, don't actually add the codecs to desc_factory_; they don't
  // actually get serialized for SCTP in BuildMediaDescription().  Instead,
  // let the session description get parsed.  That'll get the proper codecs
  // into the stream.
  cricket::MediaSessionOptions options;
  SessionDescriptionInterface* offer =
      CreateRemoteOfferWithSctpPort("stream1", new_send_port, options);

  // SetRemoteDescription will take the ownership of the offer.
  SetRemoteDescriptionWithoutError(offer);

  SessionDescriptionInterface* answer =
      ChangeSDPSctpPort(new_recv_port, CreateAnswer());
  ASSERT_TRUE(answer != NULL);

  // Now set the local description, which'll take ownership of the answer.
  SetLocalDescriptionWithoutError(answer);

  // TEST PLAN: Set the port number to something new, set it in the SDP,
  // and pass it all the way down.
  EXPECT_EQ(nullptr, data_engine_->GetChannel(0));
  CreateDataChannel();
  ASSERT_NE(nullptr, fake_sctp_transport_factory_->last_fake_sctp_transport());
  EXPECT_EQ(
      new_recv_port,
      fake_sctp_transport_factory_->last_fake_sctp_transport()->local_port());
  EXPECT_EQ(
      new_send_port,
      fake_sctp_transport_factory_->last_fake_sctp_transport()->remote_port());
}

// Verifies that when a session's SctpTransport receives an OPEN message,
// WebRtcSession signals the SctpTransport creation request with the expected
// config.
TEST_P(WebRtcSessionTest, TestSctpDataChannelOpenMessage) {
  InitWithDtls(GetParam());

  SetLocalDescriptionWithDataChannel();
  EXPECT_EQ(nullptr, data_engine_->GetChannel(0));
  ASSERT_NE(nullptr, fake_sctp_transport_factory_->last_fake_sctp_transport());

  // Make the fake SCTP transport pretend it received an OPEN message.
  webrtc::DataChannelInit config;
  config.id = 1;
  rtc::CopyOnWriteBuffer payload;
  webrtc::WriteDataChannelOpenMessage("a", config, &payload);
  cricket::ReceiveDataParams params;
  params.ssrc = config.id;
  params.type = cricket::DMT_CONTROL;
  fake_sctp_transport_factory_->last_fake_sctp_transport()->SignalDataReceived(
      params, payload);

  EXPECT_EQ_WAIT("a", last_data_channel_label_, kDefaultTimeout);
  EXPECT_EQ(config.id, last_data_channel_config_.id);
  EXPECT_FALSE(last_data_channel_config_.negotiated);
  EXPECT_EQ(webrtc::InternalDataChannelInit::kAcker,
            last_data_channel_config_.open_handshake_role);
}

TEST_F(WebRtcSessionTest, TestCombinedAudioVideoBweConstraint) {
  configuration_.combined_audio_video_bwe = rtc::Optional<bool>(true);
  Init();
  SendAudioVideoStream1();
  SessionDescriptionInterface* offer = CreateOffer();

  SetLocalDescriptionWithoutError(offer);

  voice_channel_ = media_engine_->GetVoiceChannel(0);

  ASSERT_TRUE(voice_channel_ != NULL);
  const cricket::AudioOptions& audio_options = voice_channel_->options();
  EXPECT_EQ(rtc::Optional<bool>(true), audio_options.combined_audio_video_bwe);
}

#ifdef HAVE_QUIC
TEST_P(WebRtcSessionTest, TestNegotiateQuic) {
  configuration_.enable_quic = true;
  InitWithDtls(GetParam());
  EXPECT_TRUE(session_->data_channel_type() == cricket::DCT_QUIC);
  SessionDescriptionInterface* offer = CreateOffer();
  ASSERT_TRUE(offer);
  ASSERT_TRUE(offer->description());
  SetLocalDescriptionWithoutError(offer);
  cricket::MediaSessionOptions options;
  GetOptionsForAnswer(&options);
  SessionDescriptionInterface* answer =
      CreateRemoteAnswer(offer, options, cricket::SEC_DISABLED);
  ASSERT_TRUE(answer);
  ASSERT_TRUE(answer->description());
  SetRemoteDescriptionWithoutError(answer);
}
#endif  // HAVE_QUIC

// Tests that RTX codec is removed from the answer when it isn't supported
// by local side.
TEST_F(WebRtcSessionTest, TestRtxRemovedByCreateAnswer) {
  Init();
  // Send video only to match the |kSdpWithRtx|.
  SendVideoOnlyStream2();
  std::string offer_sdp(kSdpWithRtx);

  SessionDescriptionInterface* offer =
      CreateSessionDescription(JsepSessionDescription::kOffer, offer_sdp, NULL);
  EXPECT_TRUE(offer->ToString(&offer_sdp));

  // Offer SDP contains the RTX codec.
  EXPECT_TRUE(ContainsVideoCodecWithName(offer, "rtx"));
  SetRemoteDescriptionWithoutError(offer);

  // |offered_media_sections_| is used when creating answer.
  offered_media_sections_.push_back(cricket::MediaDescriptionOptions(
      cricket::MEDIA_TYPE_VIDEO, cricket::CN_VIDEO,
      cricket::RtpTransceiverDirection(true, true), false));
  // Don't create media section for audio in the answer.
  SessionDescriptionInterface* answer = CreateAnswer();
  // Answer SDP does not contain the RTX codec.
  EXPECT_FALSE(ContainsVideoCodecWithName(answer, "rtx"));
  SetLocalDescriptionWithoutError(answer);
}

// This verifies that the voice channel after bundle has both options from video
// and voice channels.
TEST_F(WebRtcSessionTest, TestSetSocketOptionBeforeBundle) {
  InitWithBundlePolicy(PeerConnectionInterface::kBundlePolicyBalanced);
  SendAudioVideoStream1();

  PeerConnectionInterface::RTCOfferAnswerOptions options;
  options.use_rtp_mux = true;

  SessionDescriptionInterface* offer = CreateOffer(options);
  SetLocalDescriptionWithoutError(offer);

  session_->video_channel()->SetOption(cricket::BaseChannel::ST_RTP,
                                       rtc::Socket::Option::OPT_SNDBUF, 4000);

  session_->voice_channel()->SetOption(cricket::BaseChannel::ST_RTP,
                                       rtc::Socket::Option::OPT_RCVBUF, 8000);

  int option_val;
  EXPECT_TRUE(session_->video_rtp_transport_channel()->GetOption(
      rtc::Socket::Option::OPT_SNDBUF, &option_val));
  EXPECT_EQ(4000, option_val);
  EXPECT_FALSE(session_->voice_rtp_transport_channel()->GetOption(
      rtc::Socket::Option::OPT_SNDBUF, &option_val));

  EXPECT_TRUE(session_->voice_rtp_transport_channel()->GetOption(
      rtc::Socket::Option::OPT_RCVBUF, &option_val));
  EXPECT_EQ(8000, option_val);
  EXPECT_FALSE(session_->video_rtp_transport_channel()->GetOption(
      rtc::Socket::Option::OPT_RCVBUF, &option_val));

  EXPECT_NE(session_->voice_rtp_transport_channel(),
            session_->video_rtp_transport_channel());

  SendAudioVideoStream2();
  SessionDescriptionInterface* answer =
      CreateRemoteAnswer(session_->local_description());
  SetRemoteDescriptionWithoutError(answer);

  EXPECT_TRUE(session_->voice_rtp_transport_channel()->GetOption(
      rtc::Socket::Option::OPT_SNDBUF, &option_val));
  EXPECT_EQ(4000, option_val);

  EXPECT_TRUE(session_->voice_rtp_transport_channel()->GetOption(
      rtc::Socket::Option::OPT_RCVBUF, &option_val));
  EXPECT_EQ(8000, option_val);
}

// Test creating a session, request multiple offers, destroy the session
// and make sure we got success/failure callbacks for all of the requests.
// Background: crbug.com/507307
TEST_F(WebRtcSessionTest, CreateOffersAndShutdown) {
  Init();

  rtc::scoped_refptr<WebRtcSessionCreateSDPObserverForTest> observers[100];
  PeerConnectionInterface::RTCOfferAnswerOptions options;
  options.offer_to_receive_audio =
      RTCOfferAnswerOptions::kOfferToReceiveMediaTrue;
  cricket::MediaSessionOptions session_options;
  GetOptionsForOffer(options, &session_options);
  for (auto& o : observers) {
    o = new WebRtcSessionCreateSDPObserverForTest();
    session_->CreateOffer(o, options, session_options);
  }

  session_.reset();

  for (auto& o : observers) {
    // We expect to have received a notification now even if the session was
    // terminated.  The offer creation may or may not have succeeded, but we
    // must have received a notification which, so the only invalid state
    // is kInit.
    EXPECT_NE(WebRtcSessionCreateSDPObserverForTest::kInit, o->state());
  }
}

TEST_F(WebRtcSessionTest, TestPacketOptionsAndOnPacketSent) {
  TestPacketOptions();
}

// TODO(bemasc): Add a TestIceStatesBundle with BUNDLE enabled.  That test
// currently fails because upon disconnection and reconnection OnIceComplete is
// called more than once without returning to IceGatheringGathering.

INSTANTIATE_TEST_CASE_P(WebRtcSessionTests,
                        WebRtcSessionTest,
                        testing::Values(ALREADY_GENERATED,
                                        DTLS_IDENTITY_STORE));
