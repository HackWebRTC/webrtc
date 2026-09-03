/*
 *  Copyright (c) 2026 The WebRTC project authors. All Rights Reserved.
 */

#include "api/roq/roq_media_session.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <utility>
#include <vector>

#include "api/audio/audio_frame.h"
#include "api/audio/audio_mixer.h"
#include "api/audio_codecs/audio_format.h"
#include "api/environment/environment.h"
#include "api/roq/quic_network_controller.h"
#include "api/task_queue/pending_task_safety_flag.h"
#include "api/video/builtin_video_bitrate_allocator_factory.h"
#include "api/video/video_bitrate_allocator_factory.h"
#include "api/video/video_frame.h"
#include "api/video/video_source_interface.h"
#include "api/video_codecs/sdp_video_format.h"
#include "audio/audio_state.h"
#include "call/audio_receive_stream.h"
#include "call/audio_send_stream.h"
#include "call/call.h"
#include "call/call_config.h"
#include "call/packet_receiver.h"
#include "call/rtp_transport_controller_send_interface.h"
#include "call/video_receive_stream.h"
#include "call/video_send_stream.h"
#include "modules/audio_mixer/audio_mixer_impl.h"
#include "modules/rtp_rtcp/source/rtp_packet_received.h"
#include "rtc_base/copy_on_write_buffer.h"
#include "rtc_base/network/sent_packet.h"
#include "rtc_base/network_route.h"
#include "rtc_base/synchronization/mutex.h"
#include "rtc_base/thread.h"
#include "video/config/video_encoder_config.h"

namespace webrtc::roq {
namespace {

class PushVideoSource final : public VideoSourceInterface<VideoFrame> {
 public:
  void Push(const VideoFrame& frame) {
    MutexLock lock(&mutex_);
    if (sink_)
      sink_->OnFrame(frame);
  }

 private:
  void AddOrUpdateSink(VideoSinkInterface<VideoFrame>* sink,
                       const VideoSinkWants&) override {
    MutexLock lock(&mutex_);
    sink_ = sink;
  }
  void RemoveSink(VideoSinkInterface<VideoFrame>* sink) override {
    MutexLock lock(&mutex_);
    if (sink_ == sink)
      sink_ = nullptr;
  }

  Mutex mutex_;
  VideoSinkInterface<VideoFrame>* sink_ RTC_GUARDED_BY(mutex_) = nullptr;
};

SdpVideoFormat H264Format() {
  return SdpVideoFormat("H264", {{"profile-level-id", "42e02a"},
                                 {"level-asymmetry-allowed", "1"},
                                 {"packetization-mode", "1"}});
}

class RoqMediaSessionImpl final : public RoqMediaSession {
 public:
  RoqMediaSessionImpl(RoqMediaSessionConfig config, const Environment& env)
      : config_(std::move(config)), env_(env) {}

  ~RoqMediaSessionImpl() override { Stop(); }

  bool Start(std::string* error) override {
    if (started_) {
      *error = "media session already started";
      return false;
    }
    worker_ = Thread::CreateWithSocketServer();
    if (!worker_->Start()) {
      *error = "failed to start media worker thread";
      return false;
    }
    worker_->BlockingCall([&] { Setup(error); });
    if (!error->empty()) {
      worker_->Stop();
      worker_.reset();
      return false;
    }
    started_ = true;
    return true;
  }

  void Stop() override {
    if (!started_)
      return;
    worker_->BlockingCall([this] { TearDown(); });
    worker_->Stop();
    worker_.reset();
    started_ = false;
  }

  void SendVideoFrame(scoped_refptr<I420BufferInterface> buffer,
                      int64_t pts_us) override {
    if (!video_timestamp_base_us_) {
      video_timestamp_base_us_ = env_.clock().TimeInMicroseconds() - pts_us;
    }
    video_source_.Push(VideoFrame::Builder()
                           .set_video_frame_buffer(std::move(buffer))
                           .set_timestamp_us(*video_timestamp_base_us_ + pts_us)
                           .set_ntp_time_ms(1 + pts_us / 1000)
                           .set_rotation(kVideoRotation_0)
                           .build());
  }

  void SendAudioBlock(std::span<const int16_t> samples,
                      uint32_t rtp_timestamp) override {
    if (!audio_send_stream_ || samples.size() != 960)
      return;
    auto frame = std::make_unique<AudioFrame>();
    frame->UpdateFrame(rtp_timestamp, samples.data(), 480, 48000,
                       AudioFrame::kNormalSpeech, AudioFrame::kVadUnknown, 2);
    audio_send_stream_->SendAudioData(std::move(frame));
  }

  void DeliverRtp(bool audio, std::span<const uint8_t> packet) override {
    if (!worker_ || !call_)
      return;
    worker_->PostTask(SafeTask(
        safety_, [this, audio, bytes = CopyOnWriteBuffer(packet)]() mutable {
          RtpPacketReceived parsed(nullptr, env_.clock().CurrentTime());
          if (!parsed.Parse(std::move(bytes)))
            return;
          if (!audio)
            parsed.set_payload_type_frequency(kVideoPayloadTypeFrequency);
          call_->Receiver()->DeliverRtpPacket(
              audio ? MediaType::AUDIO : MediaType::VIDEO, std::move(parsed),
              [](const RtpPacketReceived&) { return false; });
        }));
  }

  void DeliverRtcp(std::span<const uint8_t> packet) override {
    if (!worker_ || !call_)
      return;
    worker_->PostTask(
        SafeTask(safety_, [this, bytes = CopyOnWriteBuffer(packet)]() mutable {
          call_->Receiver()->DeliverRtcpPacket(std::move(bytes));
        }));
  }

  void NotifyPacketAccepted(const PacketOptions& options,
                            size_t packet_size) override {
    if (!worker_ || !call_ || options.packet_id < 0)
      return;
    auto notify = [this, options, packet_size] {
      SentPacketInfo sent(options.packet_id, env_.clock().TimeInMilliseconds());
      sent.info.included_in_feedback = options.included_in_feedback;
      sent.info.included_in_allocation = options.included_in_allocation;
      sent.info.packet_size_bytes = packet_size;
      sent.info.packet_type = PacketType::kData;
      call_->OnSentPacket(sent);
    };
    if (worker_->IsCurrent()) {
      notify();
    } else {
      worker_->PostTask(SafeTask(safety_, std::move(notify)));
    }
  }

  void GenerateSenderKeyFrame() override {
    if (!worker_ || !video_send_stream_)
      return;
    worker_->PostTask(SafeTask(
        safety_, [this] { video_send_stream_->GenerateKeyFrame({}); }));
  }

  void RequestReceiverKeyFrame() override {
    if (!worker_ || !video_receive_stream_)
      return;
    worker_->PostTask(SafeTask(
        safety_, [this] { video_receive_stream_->GenerateKeyFrame(); }));
  }

  void UpdateMaxRtpPacketSize(size_t max_rtp_packet_size) override {
    if (!worker_ || !call_)
      return;
    auto update = [this, max_rtp_packet_size] {
      NetworkRoute route;
      route.connected = true;
      route.packet_overhead =
          static_cast<int>(1500 - std::min<size_t>(1500, max_rtp_packet_size));
      call_->GetTransportControllerSend()->OnNetworkRouteChanged("roq", route);
    };
    if (worker_->IsCurrent()) {
      update();
    } else {
      worker_->BlockingCall(std::move(update));
    }
  }

  RoqMediaSessionStats stats() override {
    RoqMediaSessionStats snapshot = final_stats_;
    if (!worker_ || !call_)
      return snapshot;
    worker_->BlockingCall([&] { snapshot = SnapshotStats(); });
    return snapshot;
  }

 private:
  void Setup(std::string* error) {
    if (!config_.audio_device_module || !config_.network_stats_provider ||
        ((config_.send_audio || config_.receive_audio) &&
         !config_.audio_transport) ||
        ((config_.send_video || config_.receive_video) &&
         !config_.video_transport) ||
        (config_.receive_video && !config_.video_sink) ||
        (config_.send_audio && !config_.audio_encoder_factory) ||
        (config_.receive_audio && !config_.audio_decoder_factory) ||
        (config_.send_video && !config_.video_encoder_factory) ||
        (config_.receive_video && !config_.video_decoder_factory)) {
      *error = "incomplete RoQ media session configuration";
      return;
    }
    if (!config_.audio_device_module->Initialized()) {
      *error = "audio device must be initialized by the caller";
      return;
    }
    AudioState::Config audio_state_config;
    audio_state_config.audio_mixer = AudioMixerImpl::Create();
    audio_state_config.audio_device_module = config_.audio_device_module;
    audio_state_ = AudioState::Create(audio_state_config);
    config_.audio_device_module->RegisterAudioCallback(
        audio_state_->audio_transport());
    if (!config_.send_audio)
      audio_state_->SetPlayout(false);

    CallConfig call_config =
        CallConfig::CreateWithJoinedWorkerAndNetworkQueue(env_, worker_.get());
    call_config.audio_state = audio_state_;
    call_config.bitrate_config.min_bitrate_bps = 256'000;
    call_config.bitrate_config.start_bitrate_bps = 1'000'000;
    call_config.bitrate_config.max_bitrate_bps = 4'128'000;
    call_config.per_call_network_controller_factory =
        std::make_unique<QuicNetworkControllerFactory>(
            config_.network_stats_provider,
            [this](const QuicNetworkControlState& state) {
              if (config_.network_control_observer)
                config_.network_control_observer(state);
              const RoqSessionStats network = config_.network_stats_provider();
              if (network.early_data_fallback &&
                  !early_data_recovery_requested_ && video_send_stream_) {
                early_data_recovery_requested_ = true;
                ++early_data_recovery_keyframes_;
                video_send_stream_->GenerateKeyFrame({});
              }
            });
    call_ = Call::Create(std::move(call_config));
    if (config_.send_audio)
      audio_state_->SetRecording(false);

    bitrate_allocator_factory_ = CreateBuiltinVideoBitrateAllocatorFactory();

    if (config_.send_audio) {
      AudioSendStream::Config send(config_.audio_transport);
      send.rtp.ssrc = config_.local_audio_ssrc;
      send.rtp.c_name = "roq-media";
      send.rtcp_report_interval_ms = 500;
      CodecParameterMap opus_parameters = {{"stereo", "1"}};
      opus_parameters["useinbandfec"] = "1";
      send.send_codec_spec = AudioSendStream::Config::SendCodecSpec(
          config_.audio_payload_type, {"opus", 48000, 2, opus_parameters});
      send.send_codec_spec->target_bitrate_bps = config_.audio_bitrate_bps;
      send.min_bitrate_bps = config_.audio_bitrate_bps;
      send.max_bitrate_bps = config_.audio_bitrate_bps;
      send.include_in_congestion_control_allocation = true;
      send.encoder_factory = config_.audio_encoder_factory;
      audio_send_stream_ = call_->CreateAudioSendStream(send);
    }
    if (config_.receive_audio) {
      AudioReceiveStreamInterface::Config receive;
      receive.rtcp_send_transport = config_.audio_transport;
      receive.rtp.remote_ssrc = config_.remote_audio_ssrc;
      receive.sync_group = config_.sync_group;
      receive.decoder_factory = config_.audio_decoder_factory;
      CodecParameterMap opus_parameters = {{"stereo", "1"}};
      opus_parameters["useinbandfec"] = "1";
      receive.decoder_map = {
          {config_.audio_payload_type, {"opus", 48000, 2, opus_parameters}}};
      audio_receive_stream_ =
          call_->CreateAudioReceiveStream(std::move(receive));
      if (config_.audio_sink)
        audio_receive_stream_->SetSink(config_.audio_sink);
    }
    const SdpVideoFormat h264 = H264Format();
    if (config_.send_video) {
      VideoSendStream::Config send(config_.video_transport);
      send.rtp.ssrcs = {config_.local_video_ssrc};
      send.rtp.payload_name = "H264";
      send.rtp.payload_type = config_.video_payload_type;
      send.rtp.nack.rtp_history_ms = 1000;
      send.rtp.rtx.payload_type = config_.video_rtx_payload_type;
      send.rtp.rtx.ssrcs = {config_.local_video_rtx_ssrc};
      send.rtp.max_packet_size = config_.max_rtp_packet_size;
      send.rtcp_report_interval_ms = 500;
      send.encoder_settings.encoder_factory = config_.video_encoder_factory;
      send.encoder_settings.bitrate_allocator_factory =
          bitrate_allocator_factory_.get();
      VideoEncoderConfig encoder;
      encoder.codec_type = kVideoCodecH264;
      encoder.video_format = h264;
      encoder.number_of_streams = 1;
      encoder.max_bitrate_bps = config_.video_bitrate_bps;
      encoder.frame_drop_enabled = true;
      encoder.simulcast_layers.resize(1);
      auto& layer = encoder.simulcast_layers[0];
      layer.active = true;
      layer.min_bitrate_bps = 128'000;
      layer.target_bitrate_bps = 872'000;
      layer.max_bitrate_bps = config_.video_bitrate_bps;
      layer.max_framerate = 60;
      layer.scale_resolution_down_by = 1.0;
      layer.max_qp = 56;
      layer.num_temporal_layers = 1;
      video_send_stream_ =
          call_->CreateVideoSendStream(send.Copy(), encoder.Copy());
      video_send_stream_->SetSource(&video_source_,
                                    DegradationPreference::MAINTAIN_FRAMERATE);
    }
    if (config_.receive_video) {
      VideoReceiveStreamInterface::Config receive(
          config_.video_transport, config_.video_decoder_factory);
      receive.rtp.remote_ssrc = config_.remote_video_ssrc;
      receive.rtp.nack.rtp_history_ms = 1000;
      receive.rtp.rtx_ssrc = config_.remote_video_rtx_ssrc;
      receive.rtp.rtx_associated_payload_types[config_.video_rtx_payload_type] =
          config_.video_payload_type;
      receive.rtp.rtcp_mode = RtcpMode::kCompound;
      receive.rtp.keyframe_method =
          config_.keyframe_request_method == RoqKeyFrameRequestMethod::kPli
              ? KeyFrameReqMethod::kPliRtcp
              : KeyFrameReqMethod::kFirRtcp;
      receive.sync_group = config_.sync_group;
      receive.renderer = config_.video_sink;
      receive.decoders.emplace_back(h264, config_.video_payload_type);
      video_receive_stream_ =
          call_->CreateVideoReceiveStream(std::move(receive));
    }

    call_->SignalChannelNetworkState(MediaType::AUDIO, kNetworkUp);
    call_->SignalChannelNetworkState(MediaType::VIDEO, kNetworkUp);
    if (audio_receive_stream_)
      audio_receive_stream_->Start();
    if (video_receive_stream_)
      video_receive_stream_->Start();
    if (audio_send_stream_)
      audio_send_stream_->Start();
    if (video_send_stream_)
      video_send_stream_->Start();
  }

  void TearDown() {
    safety_->SetNotAlive();
    if (video_send_stream_) {
      video_send_stream_->SetSource(nullptr,
                                    DegradationPreference::MAINTAIN_FRAMERATE);
      video_send_stream_->Stop();
    }
    if (audio_send_stream_)
      audio_send_stream_->Stop();
    if (video_receive_stream_)
      video_receive_stream_->Stop();
    if (audio_receive_stream_)
      audio_receive_stream_->Stop();
    final_stats_ = SnapshotStats();
    if (audio_send_stream_)
      call_->DestroyAudioSendStream(audio_send_stream_);
    if (video_send_stream_)
      call_->DestroyVideoSendStream(video_send_stream_);
    if (audio_receive_stream_)
      call_->DestroyAudioReceiveStream(audio_receive_stream_);
    if (video_receive_stream_)
      call_->DestroyVideoReceiveStream(video_receive_stream_);
    audio_send_stream_ = nullptr;
    video_send_stream_ = nullptr;
    audio_receive_stream_ = nullptr;
    video_receive_stream_ = nullptr;
    call_.reset();
    config_.audio_device_module->RegisterAudioCallback(nullptr);
    audio_state_ = nullptr;
  }

  RoqMediaSessionStats SnapshotStats() {
    RoqMediaSessionStats snapshot;
    if (call_) {
      snapshot.call_target_rate_bps = call_->GetStats().send_bandwidth_bps;
      const std::optional<PacerConfig> pacer_config =
          call_->GetTransportControllerSend()->GetPacerConfig();
      if (pacer_config) {
        snapshot.pacer_rate_bps = pacer_config->data_rate().bps();
        snapshot.padding_rate_bps = pacer_config->pad_rate().bps();
      }
    }
    if (audio_send_stream_) {
      const auto audio = audio_send_stream_->GetStats();
      snapshot.sent_audio_packets = audio.packets_sent;
      snapshot.audio_target_bitrate_bps =
          static_cast<int>(audio.target_bitrate_bps);
      snapshot.opus_inband_fec = true;
    }
    if (video_send_stream_) {
      const auto video = video_send_stream_->GetStats();
      snapshot.sent_video_frames = video.frames_sent;
      snapshot.video_target_bitrate_bps = video.target_media_bitrate_bps;
      snapshot.video_media_bitrate_bps = video.media_bitrate_bps;
      snapshot.video_frames_encoded = video.frames_encoded;
      snapshot.video_frames_dropped =
          video.frames_dropped_by_capturer +
          video.frames_dropped_by_bad_timestamp +
          video.frames_dropped_by_encoder_queue +
          video.frames_dropped_by_rate_limiter +
          video.frames_dropped_by_congestion_window +
          video.frames_dropped_by_encoder;
      for (const auto& [ssrc, stream] : video.substreams) {
        snapshot.nack_received += stream.rtcp_packet_type_counts.nack_packets;
        snapshot.retransmitted_packets_sent +=
            stream.rtp_stats.retransmitted.packets;
        snapshot.retransmitted_bytes_sent +=
            stream.rtp_stats.retransmitted.TotalBytes();
      }
    }
    if (audio_receive_stream_) {
      const auto audio = audio_receive_stream_->GetStats();
      snapshot.received_audio_packets = audio.packets_received;
      snapshot.received_audio_samples = audio.total_samples_received;
      snapshot.audio_fec_packets_received = audio.fec_packets_received;
      snapshot.audio_fec_packets_discarded = audio.fec_packets_discarded;
      snapshot.audio_secondary_decoded_rate = audio.secondary_decoded_rate;
      snapshot.audio_decoding_normal = audio.decoding_normal;
    }
    if (video_receive_stream_) {
      const auto video = video_receive_stream_->GetStats();
      snapshot.decoded_video_frames = video.frames_decoded;
      snapshot.rendered_video_frames = video.frames_rendered;
      snapshot.key_frames_decoded = video.key_frames_decoded;
      snapshot.pli_sent = video.rtcp_packet_type_counts.pli_packets;
      snapshot.fir_sent = video.rtcp_packet_type_counts.fir_packets;
      snapshot.nack_sent = video.rtcp_packet_type_counts.nack_packets;
    }
    snapshot.early_data_recovery_keyframes = early_data_recovery_keyframes_;
    return snapshot;
  }

  const RoqMediaSessionConfig config_;
  const Environment env_;
  std::unique_ptr<Thread> worker_;
  scoped_refptr<PendingTaskSafetyFlag> safety_ =
      PendingTaskSafetyFlag::CreateDetached();
  scoped_refptr<AudioState> audio_state_;
  std::unique_ptr<Call> call_;
  AudioSendStream* audio_send_stream_ = nullptr;
  AudioReceiveStreamInterface* audio_receive_stream_ = nullptr;
  VideoSendStream* video_send_stream_ = nullptr;
  VideoReceiveStreamInterface* video_receive_stream_ = nullptr;
  PushVideoSource video_source_;
  std::unique_ptr<VideoBitrateAllocatorFactory> bitrate_allocator_factory_;
  bool started_ = false;
  RoqMediaSessionStats final_stats_;
  std::optional<int64_t> video_timestamp_base_us_;
  bool early_data_recovery_requested_ = false;
  uint32_t early_data_recovery_keyframes_ = 0;
};

}  // namespace

std::unique_ptr<RoqMediaSession> RoqMediaSession::Create(
    RoqMediaSessionConfig config,
    const Environment& env,
    std::string* error) {
  if (!error)
    return nullptr;
  if (!config.send_audio && !config.send_video && !config.receive_audio &&
      !config.receive_video) {
    *error = "media session must send or receive at least one stream";
    return nullptr;
  }
  return std::make_unique<RoqMediaSessionImpl>(std::move(config), env);
}

}  // namespace webrtc::roq
