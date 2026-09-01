/*
 *  Copyright (c) 2026 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS. All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "rtc_tools/roq/call_loopback.h"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <span>
#include <string>
#include <thread>
#include <utility>
#include <vector>

#include "api/audio/audio_frame.h"
#include "api/audio/audio_mixer.h"
#include "api/audio_codecs/audio_format.h"
#include "api/audio_codecs/builtin_audio_decoder_factory.h"
#include "api/audio_codecs/builtin_audio_encoder_factory.h"
#include "api/call/audio_sink.h"
#include "api/call/transport.h"
#include "api/environment/environment.h"
#include "api/environment/environment_factory.h"
#include "api/scoped_refptr.h"
#include "api/task_queue/pending_task_safety_flag.h"
#include "api/task_queue/task_queue_base.h"
#include "api/test/video/function_video_decoder_factory.h"
#include "api/test/video/function_video_encoder_factory.h"
#include "api/units/timestamp.h"
#include "api/video/builtin_video_bitrate_allocator_factory.h"
#include "api/video/i420_buffer.h"
#include "api/video/video_bitrate_allocator_factory.h"
#include "api/video/video_frame.h"
#include "api/video/video_sink_interface.h"
#include "api/video_codecs/h264_profile_level_id.h"
#include "api/video_codecs/sdp_video_format.h"
#include "audio/audio_state.h"
#include "call/audio_receive_stream.h"
#include "call/audio_send_stream.h"
#include "call/call.h"
#include "call/call_config.h"
#include "call/packet_receiver.h"
#include "call/video_receive_stream.h"
#include "call/video_send_stream.h"
#include "modules/audio_device/include/test_audio_device.h"
#include "modules/audio_mixer/audio_mixer_impl.h"
#include "modules/rtp_rtcp/include/rtp_rtcp_defines.h"
#include "modules/rtp_rtcp/source/rtp_packet_received.h"
#include "modules/video_coding/codecs/h264/include/h264.h"
#include "rtc_base/copy_on_write_buffer.h"
#include "rtc_base/network/sent_packet.h"
#include "rtc_base/task_queue_for_test.h"
#include "rtc_base/thread.h"
#include "test/encoder_settings.h"
#include "test/frame_forwarder.h"
#include "video/config/video_encoder_config.h"

namespace webrtc::roq {
namespace {

constexpr int kVideoWidth = 640;
constexpr int kVideoHeight = 360;
constexpr int kVideoFps = 30;
constexpr int kAudioSampleRate = 48000;
constexpr size_t kAudioChannels = 2;
constexpr int kAudioPayloadType = 111;
constexpr int kVideoPayloadType = 96;
constexpr uint32_t kAudioSsrc = 11111111;
constexpr uint32_t kVideoSsrc = 22222222;

class CountingVideoRenderer final : public VideoSinkInterface<VideoFrame> {
 public:
  void OnFrame(const VideoFrame& frame) override {
    width_.store(frame.width());
    height_.store(frame.height());
    frames_.fetch_add(1);
  }

  size_t frames() const { return frames_.load(); }
  int width() const { return width_.load(); }
  int height() const { return height_.load(); }

 private:
  std::atomic<size_t> frames_ = 0;
  std::atomic<int> width_ = 0;
  std::atomic<int> height_ = 0;
};

class CountingAudioRenderer final : public TestAudioDeviceModule::Renderer {
 public:
  int SamplingFrequency() const override { return kAudioSampleRate; }
  int NumChannels() const override { return kAudioChannels; }

  bool Render(std::span<const int16_t> data) override {
    samples_per_channel_.fetch_add(data.size() / kAudioChannels);
    return true;
  }

  size_t samples_per_channel() const { return samples_per_channel_.load(); }

 private:
  std::atomic<size_t> samples_per_channel_ = 0;
};

class LoopbackTransport final : public Transport {
 public:
  LoopbackTransport(const Environment& env,
                    TaskQueueBase* network_queue,
                    Call* sending_call,
                    PacketReceiver* receiver,
                    MediaType media_type)
      : env_(env),
        network_queue_(network_queue),
        sending_call_(sending_call),
        receiver_(receiver),
        media_type_(media_type),
        safety_(PendingTaskSafetyFlag::CreateDetached()) {}

  bool SendRtp(std::span<const uint8_t> packet,
               const PacketOptions& options) override {
    ++rtp_packets_;
    network_queue_->PostTask(SafeTask(
        safety_, [this, bytes = CopyOnWriteBuffer(packet), options]() mutable {
          if (sending_call_ && options.packet_id >= 0) {
            SentPacketInfo sent(options.packet_id,
                                env_.clock().TimeInMilliseconds());
            sent.info.included_in_feedback = options.included_in_feedback;
            sent.info.included_in_allocation = options.included_in_allocation;
            sent.info.packet_size_bytes = bytes.size();
            sent.info.packet_type = PacketType::kData;
            sending_call_->OnSentPacket(sent);
          }
          RtpPacketReceived parsed(nullptr, env_.clock().CurrentTime());
          if (!parsed.Parse(std::move(bytes))) {
            ++parse_errors_;
            return;
          }
          if (media_type_ == MediaType::VIDEO) {
            parsed.set_payload_type_frequency(kVideoPayloadTypeFrequency);
          }
          receiver_->DeliverRtpPacket(
              media_type_, std::move(parsed),
              [](const RtpPacketReceived&) { return false; });
        }));
    return true;
  }

  bool SendRtcp(std::span<const uint8_t> packet,
                const PacketOptions&) override {
    ++rtcp_packets_;
    network_queue_->PostTask(
        SafeTask(safety_, [this, bytes = CopyOnWriteBuffer(packet)]() mutable {
          receiver_->DeliverRtcpPacket(std::move(bytes));
        }));
    return true;
  }

  void Stop() { safety_->SetNotAlive(); }
  size_t rtp_packets() const { return rtp_packets_.load(); }
  size_t rtcp_packets() const { return rtcp_packets_.load(); }
  size_t parse_errors() const { return parse_errors_.load(); }

 private:
  const Environment env_;
  TaskQueueBase* const network_queue_;
  Call* const sending_call_;
  PacketReceiver* const receiver_;
  const MediaType media_type_;
  const scoped_refptr<PendingTaskSafetyFlag> safety_;
  std::atomic<size_t> rtp_packets_ = 0;
  std::atomic<size_t> rtcp_packets_ = 0;
  std::atomic<size_t> parse_errors_ = 0;
};

SdpVideoFormat H264Format() {
  return CreateH264Format(H264Profile::kProfileConstrainedBaseline,
                          H264Level::kLevel3_1, "1");
}

}  // namespace

CallLoopbackResult RunCallLoopback(double duration_seconds) {
  CallLoopbackResult result;
  if (duration_seconds <= 0) {
    result.error = "loopback duration must be positive";
    return result;
  }

  const Environment env = CreateEnvironment();
  std::unique_ptr<Thread> network_thread = Thread::CreateWithSocketServer();
  if (!network_thread->Start()) {
    result.error = "failed to start loopback worker thread";
    return result;
  }

  auto audio_renderer = std::make_unique<CountingAudioRenderer>();
  CountingAudioRenderer* audio_renderer_ptr = audio_renderer.get();
  CountingVideoRenderer video_renderer;
  scoped_refptr<AudioDeviceModule> sender_audio_device;
  scoped_refptr<AudioDeviceModule> receiver_audio_device;
  scoped_refptr<AudioState> sender_audio_state;
  scoped_refptr<AudioState> receiver_audio_state;
  std::unique_ptr<Call> sender_call;
  std::unique_ptr<Call> receiver_call;
  std::unique_ptr<LoopbackTransport> audio_forward;
  std::unique_ptr<LoopbackTransport> video_forward;
  std::unique_ptr<LoopbackTransport> audio_reverse;
  std::unique_ptr<LoopbackTransport> video_reverse;
  AudioSendStream* audio_send_stream = nullptr;
  AudioReceiveStreamInterface* audio_receive_stream = nullptr;
  VideoSendStream* video_send_stream = nullptr;
  VideoReceiveStreamInterface* video_receive_stream = nullptr;
  test::FrameForwarder video_source;
  std::unique_ptr<test::FunctionVideoEncoderFactory> video_encoder_factory;
  std::unique_ptr<test::FunctionVideoDecoderFactory> video_decoder_factory;
  std::unique_ptr<VideoBitrateAllocatorFactory> bitrate_allocator_factory;
  scoped_refptr<AudioEncoderFactory> audio_encoder_factory;
  scoped_refptr<AudioDecoderFactory> audio_decoder_factory;

  network_thread->BlockingCall([&] {
    sender_audio_device = TestAudioDeviceModule::Create(
        env,
        TestAudioDeviceModule::CreatePulsedNoiseCapturer(
            12000, kAudioSampleRate, kAudioChannels),
        nullptr);
    receiver_audio_device =
        TestAudioDeviceModule::Create(env, nullptr, std::move(audio_renderer));
    if (!sender_audio_device || !receiver_audio_device ||
        sender_audio_device->Init() != 0 ||
        receiver_audio_device->Init() != 0) {
      result.error = "failed to initialize test audio devices";
      return;
    }

    AudioState::Config sender_audio_config;
    sender_audio_config.audio_mixer = AudioMixerImpl::Create();
    sender_audio_config.audio_device_module = sender_audio_device;
    sender_audio_state = AudioState::Create(sender_audio_config);
    sender_audio_device->RegisterAudioCallback(
        sender_audio_state->audio_transport());

    AudioState::Config receiver_audio_config;
    receiver_audio_config.audio_mixer = AudioMixerImpl::Create();
    receiver_audio_config.audio_device_module = receiver_audio_device;
    receiver_audio_state = AudioState::Create(receiver_audio_config);
    receiver_audio_device->RegisterAudioCallback(
        receiver_audio_state->audio_transport());

    CallConfig sender_config =
        CallConfig::CreateWithJoinedWorkerAndNetworkQueue(env,
                                                          network_thread.get());
    sender_config.audio_state = sender_audio_state;
    sender_config.bitrate_config.start_bitrate_bps = 1000000;
    CallConfig receiver_config =
        CallConfig::CreateWithJoinedWorkerAndNetworkQueue(env,
                                                          network_thread.get());
    receiver_config.audio_state = receiver_audio_state;
    sender_call = Call::Create(std::move(sender_config));
    receiver_call = Call::Create(std::move(receiver_config));

    audio_forward = std::make_unique<LoopbackTransport>(
        env, network_thread.get(), sender_call.get(), receiver_call->Receiver(),
        MediaType::AUDIO);
    video_forward = std::make_unique<LoopbackTransport>(
        env, network_thread.get(), sender_call.get(), receiver_call->Receiver(),
        MediaType::VIDEO);
    audio_reverse = std::make_unique<LoopbackTransport>(
        env, network_thread.get(), receiver_call.get(), sender_call->Receiver(),
        MediaType::AUDIO);
    video_reverse = std::make_unique<LoopbackTransport>(
        env, network_thread.get(), receiver_call.get(), sender_call->Receiver(),
        MediaType::VIDEO);

    audio_encoder_factory = CreateBuiltinAudioEncoderFactory();
    audio_decoder_factory = CreateBuiltinAudioDecoderFactory();
    AudioSendStream::Config audio_send_config(audio_forward.get());
    audio_send_config.rtp.ssrc = kAudioSsrc;
    audio_send_config.rtp.c_name = "roq-call-loopback";
    audio_send_config.rtcp_report_interval_ms = 500;
    audio_send_config.send_codec_spec = AudioSendStream::Config::SendCodecSpec(
        kAudioPayloadType,
        {"opus", kAudioSampleRate, kAudioChannels, {{"stereo", "1"}}});
    audio_send_config.min_bitrate_bps = 32000;
    audio_send_config.max_bitrate_bps = 64000;
    audio_send_config.encoder_factory = audio_encoder_factory;
    audio_send_stream = sender_call->CreateAudioSendStream(audio_send_config);

    AudioReceiveStreamInterface::Config audio_receive_config;
    audio_receive_config.rtcp_send_transport = audio_reverse.get();
    audio_receive_config.rtp.remote_ssrc = kAudioSsrc;
    audio_receive_config.decoder_factory = audio_decoder_factory;
    audio_receive_config.decoder_map = {
        {kAudioPayloadType,
         {"opus", kAudioSampleRate, kAudioChannels, {{"stereo", "1"}}}}};
    audio_receive_stream = receiver_call->CreateAudioReceiveStream(
        std::move(audio_receive_config));

    const SdpVideoFormat h264_format = H264Format();
    video_encoder_factory = std::make_unique<test::FunctionVideoEncoderFactory>(
        [](const Environment& codec_env, const SdpVideoFormat& format) {
          return CreateH264Encoder(codec_env,
                                   H264EncoderSettings::Parse(format));
        });
    video_decoder_factory = std::make_unique<test::FunctionVideoDecoderFactory>(
        []() { return H264Decoder::Create(); },
        std::vector<SdpVideoFormat>{h264_format});
    bitrate_allocator_factory = CreateBuiltinVideoBitrateAllocatorFactory();

    VideoSendStream::Config video_send_config(video_forward.get());
    video_send_config.rtp.ssrcs.push_back(kVideoSsrc);
    video_send_config.rtp.payload_name = "H264";
    video_send_config.rtp.payload_type = kVideoPayloadType;
    video_send_config.rtp.max_packet_size = 1200;
    video_send_config.rtcp_report_interval_ms = 500;
    video_send_config.encoder_settings.encoder_factory =
        video_encoder_factory.get();
    video_send_config.encoder_settings.bitrate_allocator_factory =
        bitrate_allocator_factory.get();
    VideoEncoderConfig video_encoder_config;
    test::FillEncoderConfiguration(kVideoCodecH264, 1, &video_encoder_config);
    video_encoder_config.video_format = h264_format;
    video_encoder_config.max_bitrate_bps = 1000000;
    video_encoder_config.simulcast_layers[0].min_bitrate_bps = 200000;
    video_encoder_config.simulcast_layers[0].target_bitrate_bps = 600000;
    video_encoder_config.simulcast_layers[0].max_bitrate_bps = 1000000;
    video_send_stream = sender_call->CreateVideoSendStream(
        video_send_config.Copy(), video_encoder_config.Copy());

    VideoReceiveStreamInterface::Config video_receive_config(
        video_reverse.get(), video_decoder_factory.get());
    video_receive_config.rtp.remote_ssrc = kVideoSsrc;
    video_receive_config.rtp.rtcp_mode = RtcpMode::kCompound;
    video_receive_config.renderer = &video_renderer;
    video_receive_config.decoders.emplace_back(h264_format, kVideoPayloadType);
    video_receive_stream = receiver_call->CreateVideoReceiveStream(
        std::move(video_receive_config));

    sender_call->SignalChannelNetworkState(MediaType::AUDIO, kNetworkUp);
    sender_call->SignalChannelNetworkState(MediaType::VIDEO, kNetworkUp);
    receiver_call->SignalChannelNetworkState(MediaType::AUDIO, kNetworkUp);
    receiver_call->SignalChannelNetworkState(MediaType::VIDEO, kNetworkUp);
    video_send_stream->SetSource(&video_source,
                                 DegradationPreference::MAINTAIN_FRAMERATE);
    audio_receive_stream->Start();
    video_receive_stream->Start();
    audio_send_stream->Start();
    video_send_stream->Start();
  });
  if (!result.error.empty()) {
    network_thread->Stop();
    return result;
  }

  const size_t video_frame_count =
      static_cast<size_t>(duration_seconds * kVideoFps);
  const auto start = std::chrono::steady_clock::now();
  for (size_t frame_index = 0; frame_index < video_frame_count; ++frame_index) {
    const auto target =
        start + std::chrono::microseconds(frame_index * 1000000 / kVideoFps);
    std::this_thread::sleep_until(target);
    scoped_refptr<I420Buffer> buffer =
        I420Buffer::Create(kVideoWidth, kVideoHeight);
    I420Buffer::SetBlack(buffer.get());
    video_source.IncomingCapturedFrame(
        VideoFrame::Builder()
            .set_video_frame_buffer(buffer)
            .set_timestamp_ms(env.clock().TimeInMilliseconds())
            .set_rotation(kVideoRotation_0)
            .build());
  }
  std::this_thread::sleep_until(
      start + std::chrono::microseconds(
                  static_cast<int64_t>(duration_seconds * 1000000)));
  for (int drain = 0;
       drain < 100 && (video_renderer.frames() < 60 ||
                       audio_renderer_ptr->samples_per_channel() < 96000);
       ++drain) {
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
  }

  network_thread->BlockingCall([&] {
    video_send_stream->SetSource(nullptr,
                                 DegradationPreference::MAINTAIN_FRAMERATE);
    audio_send_stream->Stop();
    video_send_stream->Stop();
    audio_receive_stream->Stop();
    video_receive_stream->Stop();

    result.sent_audio_rtp = audio_forward->rtp_packets();
    result.sent_video_rtp = video_forward->rtp_packets();
    const AudioReceiveStreamInterface::Stats audio_stats =
        audio_receive_stream->GetStats();
    result.received_audio_rtp = audio_stats.packets_received;
    result.audio_decoding_normal = audio_stats.decoding_normal;
    result.forward_rtcp =
        audio_forward->rtcp_packets() + video_forward->rtcp_packets();
    result.reverse_rtcp =
        audio_reverse->rtcp_packets() + video_reverse->rtcp_packets();
    result.decoded_video_frames = video_renderer.frames();
    result.decoded_audio_samples_per_channel =
        audio_renderer_ptr->samples_per_channel();
    if (audio_forward->parse_errors() + video_forward->parse_errors() +
            audio_reverse->parse_errors() + video_reverse->parse_errors() !=
        0) {
      result.error = "loopback transport failed to parse RTP";
    }

    audio_forward->Stop();
    video_forward->Stop();
    audio_reverse->Stop();
    video_reverse->Stop();

    sender_call->DestroyAudioSendStream(audio_send_stream);
    sender_call->DestroyVideoSendStream(video_send_stream);
    receiver_call->DestroyAudioReceiveStream(audio_receive_stream);
    receiver_call->DestroyVideoReceiveStream(video_receive_stream);
    sender_call.reset();
    receiver_call.reset();
    audio_forward.reset();
    video_forward.reset();
    audio_reverse.reset();
    video_reverse.reset();
    sender_audio_device->RegisterAudioCallback(nullptr);
    receiver_audio_device->RegisterAudioCallback(nullptr);
    sender_audio_device->Terminate();
    receiver_audio_device->Terminate();
    sender_audio_state = nullptr;
    receiver_audio_state = nullptr;
    sender_audio_device = nullptr;
    receiver_audio_device = nullptr;
    result.clean_shutdown = true;
  });
  network_thread->Stop();

  if (result.error.empty() && (video_renderer.width() != kVideoWidth ||
                               video_renderer.height() != kVideoHeight)) {
    result.error = "decoded video dimensions do not match source";
  }
  if (result.error.empty() &&
      (result.sent_video_rtp == 0 || result.sent_audio_rtp == 0 ||
       result.received_audio_rtp == 0 || result.audio_decoding_normal == 0 ||
       result.decoded_video_frames < 60 ||
       result.decoded_audio_samples_per_channel < 96000 ||
       result.forward_rtcp == 0 || result.reverse_rtcp == 0 ||
       !result.clean_shutdown)) {
    result.error = "Call loopback did not meet the P07 media contract";
  }
  return result;
}

}  // namespace webrtc::roq
