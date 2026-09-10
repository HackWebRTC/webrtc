/*
 *  Copyright (c) 2026 The WebRTC project authors. All Rights Reserved.
 */

#include "api/roq/roq_media_engine.h"

#include <map>
#include <memory>
#include <optional>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "api/audio/audio_frame.h"
#include "api/audio/builtin_audio_processing_builder.h"
#include "api/roq/quic_network_controller.h"
#include "api/task_queue/pending_task_safety_flag.h"
#include "api/video/builtin_video_bitrate_allocator_factory.h"
#include "api/video/video_bitrate_allocator_factory.h"
#include "api/video/video_source_interface.h"
#include "api/video_codecs/sdp_video_format.h"
#include "audio/audio_receive_stream.h"
#include "audio/audio_state.h"
#include "call/audio_receive_stream.h"
#include "call/audio_send_stream.h"
#include "call/call.h"
#include "call/call_config.h"
#include "call/packet_receiver.h"
#include "call/video_receive_stream.h"
#include "call/video_send_stream.h"
#include "modules/audio_mixer/audio_mixer_impl.h"
#include "modules/rtp_rtcp/source/rtcp_packet/common_header.h"
#include "modules/rtp_rtcp/source/rtcp_packet/nack.h"
#include "modules/rtp_rtcp/source/rtp_packet_received.h"
#include "rtc_base/copy_on_write_buffer.h"
#include "rtc_base/network/sent_packet.h"
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
  return SdpVideoFormat("H264", {{"profile-level-id", "42e034"},
                                 {"level-asymmetry-allowed", "1"},
                                 {"packetization-mode", "1"}});
}

uint32_t CountNackPackets(std::span<const uint8_t> packet) {
  uint32_t count = 0;
  while (!packet.empty()) {
    rtcp::CommonHeader header;
    if (!header.Parse(packet.data(), packet.size()))
      return 0;
    if (header.type() == rtcp::Nack::kPacketType &&
        header.fmt() == rtcp::Nack::kFeedbackMessageType) {
      ++count;
    }
    packet = packet.subspan(header.packet_size());
  }
  return count;
}

struct Binding {
  RoqMediaBindingConfig config;
  AudioReceiveStreamInterface* audio = nullptr;
  VideoReceiveStreamInterface* video = nullptr;
  AudioSendStream* audio_send = nullptr;
  VideoSendStream* video_send = nullptr;
  std::unique_ptr<PushVideoSource> video_source;
  std::optional<int64_t> video_timestamp_base_us;
  bool audio_nack_enabled = false;
};

class RoqMediaEngineImpl final : public RoqMediaEngine {
 public:
  RoqMediaEngineImpl(RoqMediaEngineConfig config, const Environment& env)
      : config_(std::move(config)), env_(env) {}
  ~RoqMediaEngineImpl() override { Stop(); }

  bool Start(std::string* error) override {
    if (started_) {
      *error = "media engine already started";
      return false;
    }
    if (!config_.audio_device_module ||
        !config_.audio_device_module->Initialized()) {
      *error = "audio device must be initialized by the caller";
      return false;
    }
    worker_ = Thread::CreateWithSocketServer();
    if (!worker_->Start()) {
      *error = "failed to start media worker thread";
      return false;
    }
    safety_->SetAlive();
    worker_->BlockingCall([this] {
      AudioState::Config audio;
      audio.audio_mixer = AudioMixerImpl::Create();
      audio.audio_device_module = config_.audio_device_module;
      if (config_.use_audio_device_recording) {
        AudioProcessing::Config processing;
        processing.echo_canceller.enabled = true;
        processing.noise_suppression.enabled = true;
        processing.gain_controller1.enabled = true;
        audio.audio_processing =
            BuiltinAudioProcessingBuilder(processing).Build(env_);
      }
      audio_state_ = AudioState::Create(audio);
      config_.audio_device_module->RegisterAudioCallback(
          audio_state_->audio_transport());
      if (!config_.use_audio_device_playout)
        audio_state_->SetPlayout(false);
      CallConfig call_config =
          CallConfig::CreateWithJoinedWorkerAndNetworkQueue(env_,
                                                            worker_.get());
      call_config.audio_state = audio_state_;
      call_config.bitrate_config.min_bitrate_bps = config_.send_min_bitrate_bps;
      call_config.bitrate_config.start_bitrate_bps =
          config_.send_start_bitrate_bps;
      call_config.bitrate_config.max_bitrate_bps = config_.send_max_bitrate_bps;
      if (config_.network_stats_provider) {
        call_config.per_call_network_controller_factory =
            std::make_unique<QuicNetworkControllerFactory>(
                config_.network_stats_provider,
                [this](const QuicNetworkControlState& state) {
                  network_control_updates_.fetch_add(1);
                  controller_target_rate_bps_.store(state.target_rate.bps());
                  controller_pacer_rate_bps_.store(state.pacer_rate.bps());
                  controller_padding_rate_bps_.store(state.padding_rate.bps());
                  controller_probe_cluster_count_.store(
                      state.probe_cluster_count);
                  controller_has_congestion_window_.store(
                      state.has_congestion_window);
                  if (config_.network_control_observer)
                    config_.network_control_observer(state);
                },
                DataRate::BitsPerSec(config_.send_min_bitrate_bps),
                DataRate::BitsPerSec(config_.send_start_bitrate_bps),
                config_.send_max_bitrate_bps > 0
                    ? DataRate::BitsPerSec(config_.send_max_bitrate_bps)
                    : DataRate::PlusInfinity());
      }
      call_ = Call::Create(std::move(call_config));
      bitrate_allocator_factory_ = CreateBuiltinVideoBitrateAllocatorFactory();
      call_->SignalChannelNetworkState(MediaType::AUDIO, kNetworkUp);
      call_->SignalChannelNetworkState(MediaType::VIDEO, kNetworkUp);
      audio_state_->SetRecording(false);
    });
    started_ = true;
    return true;
  }

  void Stop() override {
    if (!started_)
      return;
    safety_->SetNotAlive();
    worker_->BlockingCall([this] {
      for (auto& [id, binding] : bindings_)
        Destroy(binding);
      bindings_.clear();
      ssrcs_.clear();
      call_.reset();
      config_.audio_device_module->RegisterAudioCallback(nullptr);
      audio_state_ = nullptr;
    });
    worker_->Stop();
    worker_.reset();
    started_ = false;
  }

  bool AddReceiveBinding(RoqMediaBindingConfig config,
                         std::string* error) override {
    if (!CanAdd(config, "receive", error))
      return false;
    Binding binding{.config = config};
    worker_->BlockingCall([&] {
      if (config.kind == RoqMediaKind::kAudio) {
        if (config_.use_audio_device_playout && !HasAudioReceiveBinding() &&
            (config_.audio_device_module->SetPlayoutDevice(0) != 0 ||
             config_.audio_device_module->InitSpeaker() != 0)) {
          return;
        }
        AudioReceiveStreamInterface::Config receive;
        receive.rtcp_send_transport = config.transport;
        receive.rtp.remote_ssrc = config.primary_ssrc;
        receive.sync_group = config.sync_group;
        receive.rtp.nack.rtp_history_ms = config.enable_nack ? 1000 : 0;
        receive.decoder_factory = config_.audio_decoder_factory;
        CodecParameterMap opus_parameters = {{"stereo", "1"}};
        opus_parameters["useinbandfec"] = "1";
        opus_parameters["usedtx"] = "1";
        receive.decoder_map = {
            {config.payload_type, {"red", 48000, 2, {{"", "112/112"}}}},
            {112, {"opus", 48000, 2, opus_parameters}}};
        binding.audio = call_->CreateAudioReceiveStream(std::move(receive));
        binding.audio_nack_enabled = config.enable_nack;
        if (config.audio_sink)
          binding.audio->SetSink(config.audio_sink);
        binding.audio->Start();
        if (config_.use_audio_device_playout &&
            !config_.audio_device_module->Playing()) {
          Destroy(binding);
          return;
        }
      } else {
        VideoReceiveStreamInterface::Config receive(
            config.transport, config_.video_decoder_factory);
        receive.rtp.remote_ssrc = config.primary_ssrc;
        receive.sync_group = config.sync_group;
        receive.rtp.nack.rtp_history_ms = 1000;
        receive.rtp.rtx_ssrc = config.rtx_ssrc;
        if (config.rtx_ssrc != 0) {
          receive.rtp.rtx_associated_payload_types[config.rtx_payload_type] =
              config.payload_type;
        }
        receive.rtp.rtcp_mode = RtcpMode::kCompound;
        receive.rtp.keyframe_method =
            config.keyframe_request_method == RoqKeyFrameRequestMethod::kPli
                ? KeyFrameReqMethod::kPliRtcp
                : KeyFrameReqMethod::kFirRtcp;
        receive.renderer = config.video_sink;
        receive.frame_ready_for_decode_callback =
            config.video_frame_ready_for_decode;
        receive.decoders.emplace_back(
            SdpVideoFormat("H264", {{"profile-level-id", "42e034"},
                                    {"level-asymmetry-allowed", "1"},
                                    {"packetization-mode", "1"}}),
            config.payload_type);
        binding.video = call_->CreateVideoReceiveStream(std::move(receive));
        binding.video->Start();
      }
    });
    if (config.kind == RoqMediaKind::kAudio && !binding.audio) {
      *error = "failed to start audio playout device";
      return false;
    }
    Install(std::move(config), std::move(binding));
    error->clear();
    return true;
  }

  bool AddSendBinding(RoqMediaBindingConfig config,
                      std::string* error) override {
    if (!CanAdd(config, "send", error))
      return false;
    Binding binding{.config = config};
    worker_->BlockingCall([&] {
      if (config.kind == RoqMediaKind::kAudio) {
        if (config.audio_bitrate_bps <= 0 ||
            config.video_min_bitrate_bps != 0 ||
            config.video_start_bitrate_bps != 0 ||
            config.video_max_bitrate_bps != 0) {
          return;
        }
        if (config.audio_input_mode == RoqAudioInputMode::kDevice &&
            !HasDeviceAudioSendBinding() &&
            (config_.audio_device_module->SetRecordingDevice(0) != 0 ||
             config_.audio_device_module->InitMicrophone() != 0)) {
          return;
        }
        AudioSendStream::Config send(config.transport);
        send.rtp.ssrc = config.primary_ssrc;
        send.rtp.c_name = "roq-media";
        send.rtcp_report_interval_ms = 500;
        CodecParameterMap opus_parameters = {{"stereo", "1"}};
        opus_parameters["useinbandfec"] = "1";
        opus_parameters["usedtx"] = "1";
        send.send_codec_spec = AudioSendStream::Config::SendCodecSpec(
            112, {"opus", 48000, 2, opus_parameters});
        send.send_codec_spec->red_payload_type = config.payload_type;
        send.send_codec_spec->nack_enabled = true;
        send.send_codec_spec->target_bitrate_bps = config.audio_bitrate_bps;
        send.min_bitrate_bps = config.audio_bitrate_bps;
        send.max_bitrate_bps = config.audio_bitrate_bps;
        send.include_in_congestion_control_allocation = true;
        send.encoder_factory = config_.audio_encoder_factory;
        binding.audio_send = call_->CreateAudioSendStream(send);
        binding.audio_nack_enabled = true;
        binding.audio_send->Start();
        if (config.audio_input_mode == RoqAudioInputMode::kDevice)
          audio_state_->SetRecording(true);
        if (config.audio_input_mode == RoqAudioInputMode::kDevice &&
            !config_.audio_device_module->Recording()) {
          Destroy(binding);
          return;
        }
      } else {
        if (config.audio_bitrate_bps != 0 ||
            config.video_min_bitrate_bps <= 0 ||
            config.video_start_bitrate_bps < config.video_min_bitrate_bps ||
            config.video_max_bitrate_bps < config.video_start_bitrate_bps) {
          return;
        }
        VideoSendStream::Config send(config.transport);
        send.rtp.ssrcs = {config.primary_ssrc};
        send.rtp.payload_name = "H264";
        send.rtp.payload_type = config.payload_type;
        send.rtp.nack.rtp_history_ms = 1000;
        send.rtp.rtx.payload_type = config.rtx_payload_type;
        send.rtp.rtx.ssrcs = {config.rtx_ssrc};
        send.rtp.max_packet_size = config_.max_rtp_packet_size;
        send.encoder_settings.encoder_factory = config_.video_encoder_factory;
        send.encoder_settings.bitrate_allocator_factory =
            bitrate_allocator_factory_.get();
        VideoEncoderConfig encoder;
        encoder.codec_type = kVideoCodecH264;
        encoder.video_format = H264Format();
        encoder.number_of_streams = 1;
        encoder.max_bitrate_bps = config.video_max_bitrate_bps;
        encoder.bitrate_priority = 1.0;
        encoder.frame_drop_enabled = true;
        encoder.simulcast_layers.resize(1);
        encoder.simulcast_layers[0].active = true;
        encoder.simulcast_layers[0].min_bitrate_bps =
            config.video_min_bitrate_bps;
        encoder.simulcast_layers[0].target_bitrate_bps =
            config.video_start_bitrate_bps;
        encoder.simulcast_layers[0].max_bitrate_bps =
            config.video_max_bitrate_bps;
        encoder.simulcast_layers[0].bitrate_priority = 1.0;
        encoder.simulcast_layers[0].max_framerate = 60;
        encoder.simulcast_layers[0].scale_resolution_down_by = 1.0;
        binding.video_source = std::make_unique<PushVideoSource>();
        binding.video_send =
            call_->CreateVideoSendStream(send.Copy(), encoder.Copy());
        binding.video_send->SetSource(
            binding.video_source.get(),
            DegradationPreference::MAINTAIN_FRAMERATE);
        binding.video_send->Start();
      }
    });
    if ((config.kind == RoqMediaKind::kAudio && !binding.audio_send) ||
        (config.kind == RoqMediaKind::kVideo && !binding.video_send)) {
      *error = "invalid publication bitrate config";
      return false;
    }
    Install(std::move(config), std::move(binding));
    error->clear();
    return true;
  }

  bool RemoveBinding(const std::string& binding_id) override {
    auto it = bindings_.find(binding_id);
    if (it == bindings_.end())
      return false;
    terminal_stats_[binding_id] = ReadBindingStats(it->second);
    worker_->BlockingCall([&] { Destroy(it->second); });
    ssrcs_.erase(it->second.config.primary_ssrc);
    ssrcs_.erase(it->second.config.rtx_ssrc);
    bindings_.erase(it);
    worker_->BlockingCall([this] { UpdateAudioDeviceState(); });
    return true;
  }

  bool SuspendReceiveBinding(const std::string& binding_id) override {
    auto it = bindings_.find(binding_id);
    if (it == bindings_.end() || (!it->second.audio && !it->second.video))
      return false;
    terminal_stats_[binding_id] = ReadBindingStats(it->second);
    worker_->BlockingCall([&] {
      if (it->second.audio) {
        it->second.audio->Stop();
        call_->DestroyAudioReceiveStream(it->second.audio);
        it->second.audio = nullptr;
      }
      if (it->second.video) {
        it->second.video->Stop();
        call_->DestroyVideoReceiveStream(it->second.video);
        it->second.video = nullptr;
      }
    });
    UpdateAudioDeviceState();
    return true;
  }

  bool ResumeReceiveBinding(const std::string& binding_id,
                            std::string* error) override {
    auto it = bindings_.find(binding_id);
    if (it == bindings_.end())
      return false;
    if (it->second.audio || it->second.video) {
      error->clear();
      return true;
    }
    Binding replacement{.config = it->second.config};
    worker_->BlockingCall([&] {
      const RoqMediaBindingConfig& config = replacement.config;
      if (config.kind == RoqMediaKind::kAudio) {
        AudioReceiveStreamInterface::Config receive;
        receive.rtcp_send_transport = config.transport;
        receive.rtp.remote_ssrc = config.primary_ssrc;
        receive.sync_group = config.sync_group;
        receive.rtp.nack.rtp_history_ms = config.enable_nack ? 1000 : 0;
        receive.decoder_factory = config_.audio_decoder_factory;
        CodecParameterMap opus_parameters = {{"stereo", "1"}};
        opus_parameters["useinbandfec"] = "1";
        opus_parameters["usedtx"] = "1";
        receive.decoder_map = {
            {config.payload_type, {"red", 48000, 2, {{"", "112/112"}}}},
            {112, {"opus", 48000, 2, opus_parameters}}};
        replacement.audio =
            call_->CreateAudioReceiveStream(std::move(receive));
        replacement.audio_nack_enabled = config.enable_nack;
        if (config.audio_sink)
          replacement.audio->SetSink(config.audio_sink);
        replacement.audio->Start();
      } else {
        VideoReceiveStreamInterface::Config receive(
            config.transport, config_.video_decoder_factory);
        receive.rtp.remote_ssrc = config.primary_ssrc;
        receive.sync_group = config.sync_group;
        receive.rtp.nack.rtp_history_ms = config.enable_nack ? 1000 : 0;
        receive.rtp.rtx_ssrc = config.rtx_ssrc;
        if (config.rtx_ssrc != 0)
          receive.rtp.rtx_associated_payload_types[config.rtx_payload_type] =
              config.payload_type;
        receive.rtp.rtcp_mode = RtcpMode::kCompound;
        receive.rtp.keyframe_method =
            config.keyframe_request_method == RoqKeyFrameRequestMethod::kPli
                ? KeyFrameReqMethod::kPliRtcp
                : KeyFrameReqMethod::kFirRtcp;
        receive.renderer = config.video_sink;
        receive.frame_ready_for_decode_callback =
            config.video_frame_ready_for_decode;
        receive.decoders.emplace_back(
            SdpVideoFormat("H264", {{"profile-level-id", "42e034"},
                                    {"level-asymmetry-allowed", "1"},
                                    {"packetization-mode", "1"}}),
            config.payload_type);
        replacement.video =
            call_->CreateVideoReceiveStream(std::move(receive));
        replacement.video->Start();
      }
    });
    if (!replacement.audio && !replacement.video) {
      *error = "failed to resume receive binding";
      return false;
    }
    it->second = std::move(replacement);
    terminal_stats_.erase(binding_id);
    error->clear();
    return true;
  }

  void UpdateSendBitrateConstraints(int min_bitrate_bps,
                                    int start_bitrate_bps,
                                    int max_bitrate_bps) override {
    if (!worker_ || !call_ || min_bitrate_bps <= 0 ||
        start_bitrate_bps < min_bitrate_bps ||
        max_bitrate_bps < start_bitrate_bps) {
      return;
    }
    worker_->PostTask([this, min_bitrate_bps, start_bitrate_bps,
                       max_bitrate_bps] {
      BitrateConstraints constraints;
      constraints.min_bitrate_bps = min_bitrate_bps;
      constraints.start_bitrate_bps = start_bitrate_bps;
      constraints.max_bitrate_bps = max_bitrate_bps;
      call_->GetTransportControllerSend()->SetSdpBitrateParameters(constraints);
    });
  }

  void UpdateMaxRtpPacketSize(size_t max_rtp_packet_size) override {
    if (!worker_ || !call_ || max_rtp_packet_size < 100 ||
        max_rtp_packet_size > 1500) {
      return;
    }
    max_rtp_packet_size =
        std::min(max_rtp_packet_size, config_.max_rtp_packet_size);
    auto update = [this, max_rtp_packet_size] {
      NetworkRoute route;
      route.connected = true;
      route.packet_overhead = static_cast<int>(1500 - max_rtp_packet_size);
      call_->GetTransportControllerSend()->OnNetworkRouteChanged("roq", route);
    };
    if (worker_->IsCurrent()) {
      update();
    } else {
      worker_->BlockingCall(std::move(update));
    }
  }

  bool HasBinding(const std::string& binding_id) const override {
    return bindings_.contains(binding_id);
  }
  size_t binding_count() const override { return bindings_.size(); }

  void SendVideoFrame(const std::string& binding_id,
                      scoped_refptr<I420BufferInterface> buffer,
                      int64_t pts_us) override {
    auto it = bindings_.find(binding_id);
    if (it == bindings_.end() || !it->second.video_source)
      return;
    if (!it->second.video_timestamp_base_us) {
      it->second.video_timestamp_base_us =
          env_.clock().TimeInMicroseconds() - pts_us;
    }
    it->second.video_source->Push(
        VideoFrame::Builder()
            .set_video_frame_buffer(std::move(buffer))
            .set_timestamp_us(*it->second.video_timestamp_base_us + pts_us)
            .set_ntp_time_ms(1 + pts_us / 1000)
            .set_rotation(kVideoRotation_0)
            .build());
  }

  void SendAudioBlock(const std::string& binding_id,
                      std::span<const int16_t> samples,
                      uint32_t rtp_timestamp) override {
    auto it = bindings_.find(binding_id);
    if (it == bindings_.end() || !it->second.audio_send ||
        samples.size() != 960) {
      return;
    }
    auto frame = std::make_unique<AudioFrame>();
    frame->UpdateFrame(rtp_timestamp, samples.data(), 480, 48000,
                       AudioFrame::kNormalSpeech, AudioFrame::kVadUnknown, 2);
    it->second.audio_send->SendAudioData(std::move(frame));
  }

  void DeliverRtp(const std::string& binding_id,
                  std::span<const uint8_t> packet) override {
    auto it = bindings_.find(binding_id);
    if (it == bindings_.end() || !worker_ || !call_)
      return;
    const MediaType media_type = it->second.config.kind == RoqMediaKind::kAudio
                                     ? MediaType::AUDIO
                                     : MediaType::VIDEO;
    worker_->PostTask(
        [this, media_type, bytes = CopyOnWriteBuffer(packet)]() mutable {
          RtpPacketReceived parsed(nullptr, env_.clock().CurrentTime());
          if (!parsed.Parse(std::move(bytes)))
            return;
          if (media_type == MediaType::VIDEO)
            parsed.set_payload_type_frequency(90000);
          call_->Receiver()->DeliverRtpPacket(
              media_type, std::move(parsed),
              [](const RtpPacketReceived&) { return false; });
        });
  }

  void DeliverRtcp(const std::string& binding_id,
                   std::span<const uint8_t> packet) override {
    const auto binding = bindings_.find(binding_id);
    if (binding == bindings_.end() || !worker_ || !call_)
      return;
    const uint32_t nack_packets = CountNackPackets(packet);
    if (binding->second.config.kind == RoqMediaKind::kAudio)
      audio_nack_received_.fetch_add(nack_packets);
    else
      video_nack_received_.fetch_add(nack_packets);
    worker_->PostTask([this, bytes = CopyOnWriteBuffer(packet)]() mutable {
      call_->Receiver()->DeliverRtcpPacket(std::move(bytes));
    });
  }

  void GenerateSenderKeyFrame(const std::string& binding_id) override {
    auto it = bindings_.find(binding_id);
    if (it == bindings_.end() || !it->second.video_send)
      return;
    worker_->PostTask(
        [stream = it->second.video_send] { stream->GenerateKeyFrame({}); });
  }

  void RequestReceiverKeyFrame(const std::string& binding_id) override {
    auto it = bindings_.find(binding_id);
    if (it == bindings_.end() || !it->second.video)
      return;
    worker_->PostTask(
        [stream = it->second.video] { stream->GenerateKeyFrame(); });
  }

  RoqMediaEngineStats stats() override {
    RoqMediaEngineStats result;
    result.audio_nack_received = audio_nack_received_.load();
    result.video_nack_received = video_nack_received_.load();
    result.network_control_updates = network_control_updates_.load();
    result.controller_target_rate_bps = controller_target_rate_bps_.load();
    result.controller_pacer_rate_bps = controller_pacer_rate_bps_.load();
    result.controller_padding_rate_bps = controller_padding_rate_bps_.load();
    result.controller_probe_cluster_count =
        controller_probe_cluster_count_.load();
    result.controller_has_congestion_window =
        controller_has_congestion_window_.load();
    if (audio_state_ && audio_state_->audio_processing()) {
      const AudioProcessing::Config processing =
          audio_state_->audio_processing()->GetConfig();
      result.audio_processing_active = true;
      result.echo_cancellation_enabled = processing.echo_canceller.enabled;
      result.noise_suppression_enabled = processing.noise_suppression.enabled;
      result.automatic_gain_control_enabled =
          processing.gain_controller1.enabled ||
          processing.gain_controller2.enabled;
    }
    if (!worker_ || !call_)
      return result;
    worker_->BlockingCall([&] {
      result.call_target_rate_bps = call_->GetStats().send_bandwidth_bps;
      for (const auto& [id, binding] : bindings_) {
        if (binding.audio)
          result.audio_receive_nack_enabled =
              result.audio_receive_nack_enabled || binding.audio_nack_enabled;
        if (binding.audio_send)
          result.audio_send_nack_enabled =
              result.audio_send_nack_enabled || binding.audio_nack_enabled;
        if (binding.audio) {
          const auto audio = binding.audio->GetStats();
          result.received_audio_packets += audio.packets_received;
          result.received_audio_samples += audio.total_samples_received;
          result.audio_fec_packets_received += audio.fec_packets_received;
          result.audio_fec_packets_discarded += audio.fec_packets_discarded;
          result.audio_secondary_decoded_rate += audio.secondary_decoded_rate;
          result.audio_decoding_normal += audio.decoding_normal;
        }
        if (binding.video) {
          const auto video = binding.video->GetStats();
          result.decoded_video_frames += video.frames_decoded;
          result.pli_sent += video.rtcp_packet_type_counts.pli_packets;
          result.fir_sent += video.rtcp_packet_type_counts.fir_packets;
          result.nack_sent += video.rtcp_packet_type_counts.nack_packets;
        }
        if (binding.audio_send) {
          const auto audio = binding.audio_send->GetStats();
          result.sent_audio_packets += audio.packets_sent;
          result.audio_retransmitted_packets_sent +=
              audio.retransmitted_packets_sent;
          result.audio_target_bitrate_bps +=
              static_cast<int>(audio.target_bitrate_bps);
          result.opus_inband_fec = true;
        }
        if (binding.video_send) {
          const auto video = binding.video_send->GetStats();
          result.sent_video_frames += video.frames_sent;
          result.video_target_bitrate_bps += video.target_media_bitrate_bps;
          result.video_media_bitrate_bps += video.media_bitrate_bps;
          result.video_frames_encoded += video.frames_encoded;
          result.video_frames_dropped +=
              video.frames_dropped_by_capturer +
              video.frames_dropped_by_bad_timestamp +
              video.frames_dropped_by_encoder_queue +
              video.frames_dropped_by_rate_limiter +
              video.frames_dropped_by_congestion_window +
              video.frames_dropped_by_encoder;
          for (const auto& [ssrc, stream] : video.substreams) {
            result.nack_received += stream.rtcp_packet_type_counts.nack_packets;
            result.retransmitted_packets_sent +=
                stream.rtp_stats.retransmitted.packets;
            result.video_retransmitted_packets_sent +=
                stream.rtp_stats.retransmitted.packets;
            result.retransmitted_bytes_sent +=
                stream.rtp_stats.retransmitted.TotalBytes();
          }
        }
      }
      for (const auto& [id, terminal] : terminal_stats_) {
        result.sent_audio_packets += terminal.sent_audio_packets;
        result.sent_video_frames += terminal.sent_video_frames;
        result.received_audio_packets += terminal.received_audio_packets;
        result.decoded_video_frames += terminal.decoded_video_frames;
        result.pli_sent += terminal.pli_sent;
        result.fir_sent += terminal.fir_sent;
        result.nack_sent += terminal.nack_sent;
        result.nack_received += terminal.nack_received;
        result.retransmitted_packets_sent +=
            terminal.retransmitted_packets_sent;
        result.audio_retransmitted_packets_sent +=
            terminal.audio_retransmitted_packets_sent;
        result.video_retransmitted_packets_sent +=
            terminal.video_retransmitted_packets_sent;
      }
      result.nack_received =
          result.audio_nack_received + result.video_nack_received;
      result.retransmitted_packets_sent =
          result.audio_retransmitted_packets_sent +
          result.video_retransmitted_packets_sent;
    });
    return result;
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

  std::optional<RoqMediaBindingStats> binding_stats(
      const std::string& binding_id) override {
    auto it = bindings_.find(binding_id);
    if (it != bindings_.end())
      return ReadBindingStats(it->second);
    auto terminal = terminal_stats_.find(binding_id);
    return terminal == terminal_stats_.end()
               ? std::nullopt
               : std::optional<RoqMediaBindingStats>(terminal->second);
  }

 private:
  bool HasAudioReceiveBinding() const {
    for (const auto& [id, binding] : bindings_) {
      if (binding.audio)
        return true;
    }
    return false;
  }

  bool HasDeviceAudioSendBinding() const {
    for (const auto& [id, binding] : bindings_) {
      if (binding.audio_send &&
          binding.config.audio_input_mode == RoqAudioInputMode::kDevice) {
        return true;
      }
    }
    return false;
  }

  void UpdateAudioDeviceState() {
    const bool should_play =
        config_.use_audio_device_playout && HasAudioReceiveBinding();
    if (!should_play && config_.audio_device_module->Playing())
      config_.audio_device_module->StopPlayout();
    const bool should_record = HasDeviceAudioSendBinding();
    if (!should_record && config_.audio_device_module->Recording())
      config_.audio_device_module->StopRecording();
  }

  bool CanAdd(const RoqMediaBindingConfig& config,
              const char* direction,
              std::string* error) const {
    if (!started_ || config.binding_id.empty() || config.primary_ssrc == 0 ||
        !config.transport) {
      *error = std::string("invalid ") + direction + " binding";
      return false;
    }
    if (bindings_.contains(config.binding_id)) {
      *error = "binding ID is already active";
      return false;
    }
    if (ssrcs_.contains(config.primary_ssrc) ||
        (config.rtx_ssrc != 0 && ssrcs_.contains(config.rtx_ssrc))) {
      *error = "SSRC is already active";
      return false;
    }
    return true;
  }

  void Install(RoqMediaBindingConfig config, Binding binding) {
    terminal_stats_.erase(config.binding_id);
    ssrcs_.insert(config.primary_ssrc);
    if (config.rtx_ssrc != 0)
      ssrcs_.insert(config.rtx_ssrc);
    bindings_.emplace(config.binding_id, std::move(binding));
  }

  RoqMediaBindingStats ReadBindingStatsOnWorker(const Binding& binding) {
    RoqMediaBindingStats result;
    if (!worker_ || !call_)
      return result;
    if (binding.audio) {
      result.received_audio_packets = binding.audio->GetStats().packets_received;
    }
    if (binding.video) {
      const auto video = binding.video->GetStats();
      result.decoded_video_frames = video.frames_decoded;
      result.pli_sent = video.rtcp_packet_type_counts.pli_packets;
      result.fir_sent = video.rtcp_packet_type_counts.fir_packets;
      result.nack_sent = video.rtcp_packet_type_counts.nack_packets;
    }
    if (binding.audio_send) {
      const auto audio = binding.audio_send->GetStats();
      result.sent_audio_packets = audio.packets_sent;
      result.audio_retransmitted_packets_sent = audio.retransmitted_packets_sent;
    }
    if (binding.video_send) {
      const auto video = binding.video_send->GetStats();
      result.sent_video_frames = video.frames_sent;
      for (const auto& [ssrc, stream] : video.substreams) {
        result.nack_received += stream.rtcp_packet_type_counts.nack_packets;
        result.nack_requests_received +=
            stream.rtcp_packet_type_counts.nack_requests;
        result.retransmitted_packets_sent +=
            stream.rtp_stats.retransmitted.packets;
        result.video_retransmitted_packets_sent +=
            stream.rtp_stats.retransmitted.packets;
      }
    }
    return result;
  }

  RoqMediaBindingStats ReadBindingStats(const Binding& binding) {
    if (!worker_ || !call_)
      return {};
    if (worker_->IsCurrent())
      return ReadBindingStatsOnWorker(binding);
    return worker_->BlockingCall(
        [&] { return ReadBindingStatsOnWorker(binding); });
  }

  void Destroy(Binding& binding) {
    if (binding.audio) {
      binding.audio->Stop();
      call_->DestroyAudioReceiveStream(binding.audio);
      binding.audio = nullptr;
    }
    if (binding.video) {
      binding.video->Stop();
      call_->DestroyVideoReceiveStream(binding.video);
      binding.video = nullptr;
    }
    if (binding.video_send) {
      binding.video_send->SetSource(nullptr,
                                    DegradationPreference::MAINTAIN_FRAMERATE);
      binding.video_send->Stop();
      call_->DestroyVideoSendStream(binding.video_send);
      binding.video_send = nullptr;
    }
    if (binding.audio_send) {
      binding.audio_send->Stop();
      call_->DestroyAudioSendStream(binding.audio_send);
      binding.audio_send = nullptr;
    }
  }

  const RoqMediaEngineConfig config_;
  const Environment env_;
  std::unique_ptr<Thread> worker_;
  scoped_refptr<AudioState> audio_state_;
  std::unique_ptr<Call> call_;
  std::unique_ptr<VideoBitrateAllocatorFactory> bitrate_allocator_factory_;
  scoped_refptr<PendingTaskSafetyFlag> safety_ =
      PendingTaskSafetyFlag::CreateDetached();
  std::map<std::string, Binding> bindings_;
  std::map<std::string, RoqMediaBindingStats> terminal_stats_;
  std::set<uint32_t> ssrcs_;
  std::atomic<uint64_t> network_control_updates_ = 0;
  std::atomic<uint32_t> audio_nack_received_ = 0;
  std::atomic<uint32_t> video_nack_received_ = 0;
  std::atomic<int64_t> controller_target_rate_bps_ = 0;
  std::atomic<int64_t> controller_pacer_rate_bps_ = 0;
  std::atomic<int64_t> controller_padding_rate_bps_ = 0;
  std::atomic<size_t> controller_probe_cluster_count_ = 0;
  std::atomic<bool> controller_has_congestion_window_ = false;
  bool started_ = false;
};

}  // namespace

std::unique_ptr<RoqMediaEngine> RoqMediaEngine::Create(
    RoqMediaEngineConfig config,
    const Environment& env,
    std::string* error) {
  if (!error || !config.audio_device_module || !config.audio_decoder_factory ||
      !config.video_decoder_factory || config.max_rtp_packet_size < 100 ||
      config.max_rtp_packet_size > 1500) {
    if (error)
      *error = "incomplete RoQ media engine configuration";
    return nullptr;
  }
  return std::make_unique<RoqMediaEngineImpl>(std::move(config), env);
}

}  // namespace webrtc::roq
