/*
 *  Copyright (c) 2026 The WebRTC project authors. All Rights Reserved.
 */

#include "rtc_tools/roq/media_peer.h"

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <csignal>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <memory>
#include <mutex>
#include <span>
#include <string>
#include <thread>
#include <utility>
#include <vector>

#include "api/audio/audio_device.h"
#include "api/audio/create_audio_device_module.h"
#include "api/audio_codecs/builtin_audio_decoder_factory.h"
#include "api/audio_codecs/builtin_audio_encoder_factory.h"
#include "api/call/audio_sink.h"
#include "api/environment/environment.h"
#include "api/environment/environment_factory.h"
#include "api/file_capturer/file_media_capturer.h"
#include "api/roq/roq_media_session.h"
#include "api/roq/roq_session.h"
#include "api/roq/roq_transport.h"
#include "api/scoped_refptr.h"
#include "api/units/time_delta.h"
#include "api/video/i420_buffer.h"
#include "api/video/video_frame.h"
#include "api/video/video_sink_interface.h"
#include "api/video_codecs/builtin_video_decoder_factory.h"
#include "api/video_codecs/builtin_video_encoder_factory.h"
#include "modules/audio_device/include/test_audio_device.h"
#include "modules/roq/rtp_packet_metadata.h"
#include "modules/rtp_rtcp/source/rtcp_packet/common_header.h"
#include "rtc_tools/roq/manifest.h"
#include "rtc_tools/roq/test_roq_server.h"
#include "test/video_renderer.h"

namespace webrtc::roq {
namespace {

std::atomic<bool> g_stop_requested = false;
constexpr std::array<uint8_t, 16> kEarlyMediaAddressToken = {
    0x72, 0x6f, 0x71, 0x2d, 0x6d, 0x34, 0x2d, 0x74,
    0x6f, 0x6b, 0x65, 0x6e, 0x2d, 0x30, 0x30, 0x31,
};
constexpr std::array<uint8_t, 14> kEarlyMediaRequest = {
    'e', 'a', 'r', 'l', 'y', '-', 'm', 'e', 'd', 'i', 'a', '-', 'v', '1',
};
constexpr std::array<uint8_t, 14> kEarlyMediaReady = {
    'e', 'a', 'r', 'l', 'y', '-', 'r', 'e', 'a', 'd', 'y', '-', 'v', '1',
};

void RequestStop(int) {
  g_stop_requested.store(true);
}

class CountingVideoSink final : public VideoSinkInterface<VideoFrame> {
 public:
  void OnFrame(const VideoFrame& frame) override {
    int64_t unset_time = -1;
    first_frame_monotonic_us_.compare_exchange_strong(
        unset_time, std::chrono::duration_cast<std::chrono::microseconds>(
                        std::chrono::steady_clock::now().time_since_epoch())
                        .count());
    uint32_t unset = 0;
    first_timestamp_.compare_exchange_strong(unset, frame.rtp_timestamp());
    last_timestamp_.store(frame.rtp_timestamp());
    ++frames_;
  }
  size_t frames() const { return frames_.load(); }
  uint32_t last_timestamp() const { return last_timestamp_.load(); }
  uint32_t timestamp_span() const {
    return last_timestamp_.load() - first_timestamp_.load();
  }
  int64_t first_frame_monotonic_us() const {
    return first_frame_monotonic_us_.load();
  }

 private:
  std::atomic<size_t> frames_ = 0;
  std::atomic<uint32_t> last_timestamp_ = 0;
  std::atomic<uint32_t> first_timestamp_ = 0;
  std::atomic<int64_t> first_frame_monotonic_us_ = -1;
};

class FanoutVideoSink final : public VideoSinkInterface<VideoFrame> {
 public:
  FanoutVideoSink(VideoSinkInterface<VideoFrame>* first,
                  VideoSinkInterface<VideoFrame>* second)
      : first_(first), second_(second) {}
  void OnFrame(const VideoFrame& frame) override {
    first_->OnFrame(frame);
    second_->OnFrame(frame);
  }

 private:
  VideoSinkInterface<VideoFrame>* const first_;
  VideoSinkInterface<VideoFrame>* const second_;
};

class CountingAudioRenderer final : public TestAudioDeviceModule::Renderer {
 public:
  int SamplingFrequency() const override { return 48000; }
  int NumChannels() const override { return 2; }
  bool Render(std::span<const int16_t> data) override {
    samples_.fetch_add(data.size() / 2);
    return true;
  }
  size_t samples() const { return samples_.load(); }

 private:
  std::atomic<size_t> samples_ = 0;
};

class CountingAudioSink final : public AudioSinkInterface {
 public:
  void OnData(const Data& audio) override {
    samples_.fetch_add(audio.samples_per_channel);
    uint32_t unset = 0;
    first_timestamp_.compare_exchange_strong(unset, audio.timestamp);
    last_timestamp_.store(audio.timestamp);
  }
  size_t samples() const { return samples_.load(); }
  uint32_t last_timestamp() const { return last_timestamp_.load(); }
  uint32_t timestamp_span() const {
    return last_timestamp_.load() - first_timestamp_.load();
  }

 private:
  std::atomic<size_t> samples_ = 0;
  std::atomic<uint32_t> last_timestamp_ = 0;
  std::atomic<uint32_t> first_timestamp_ = 0;
};

class CapturingTransport final : public Transport {
 public:
  bool SendRtp(std::span<const uint8_t> packet, const PacketOptions&) override {
    std::lock_guard lock(mutex_);
    packets_.emplace_back(packet.begin(), packet.end());
    return true;
  }
  bool SendRtcp(std::span<const uint8_t>, const PacketOptions&) override {
    return true;
  }
  std::vector<std::vector<uint8_t>> packets() const {
    std::lock_guard lock(mutex_);
    return packets_;
  }

 private:
  mutable std::mutex mutex_;
  std::vector<std::vector<uint8_t>> packets_;
};

std::vector<std::vector<uint8_t>> CompleteKeyAccessUnit(
    const std::vector<std::vector<uint8_t>>& packets) {
  std::vector<std::vector<uint8_t>> access_unit;
  std::optional<uint32_t> timestamp;
  bool has_sps = false;
  bool has_pps = false;
  bool has_idr = false;
  for (const std::vector<uint8_t>& packet : packets) {
    RtpPacketMetadata metadata;
    if (!ParseH264RtpPacketMetadata(packet, &metadata))
      continue;
    if (!timestamp)
      timestamp = metadata.timestamp;
    if (metadata.timestamp != *timestamp)
      break;
    access_unit.push_back(packet);
    has_sps |= metadata.contains_sps;
    has_pps |= metadata.contains_pps;
    has_idr |= metadata.contains_idr;
    if (metadata.marker)
      return has_sps && has_pps && has_idr
                 ? access_unit
                 : std::vector<std::vector<uint8_t>>{};
  }
  return {};
}

class MediaEventLog {
 public:
  MediaEventLog(const std::string& path, std::string run_id, PeerRole role)
      : output_(path),
        run_id_(std::move(run_id)),
        process_(role == PeerRole::kClient ? "client" : "server") {}

  bool ok() const { return output_.good(); }

  void Write(const char* event, const char* kind, uint64_t value = 0) {
    const auto monotonic_us =
        std::chrono::duration_cast<std::chrono::microseconds>(
            std::chrono::steady_clock::now().time_since_epoch())
            .count();
    const auto utc_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                            std::chrono::system_clock::now().time_since_epoch())
                            .count();
    std::lock_guard lock(mutex_);
    output_ << "{\"schema_version\":1,\"run_id\":\"" << run_id_
            << "\",\"process\":\"" << process_ << "\",\"event\":\"" << event
            << "\",\"kind\":\"" << kind << "\",\"sequence\":" << sequence_++
            << ",\"value\":" << value << ",\"monotonic_us\":" << monotonic_us
            << ",\"utc_ms\":" << utc_ms << "}\n";
  }

  void WriteBudget(const QuicNetworkControlState& state) {
    const auto utc_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                            std::chrono::system_clock::now().time_since_epoch())
                            .count();
    std::lock_guard lock(mutex_);
    output_ << "{\"schema_version\":1,\"run_id\":\"" << run_id_
            << "\",\"process\":\"" << process_
            << "\",\"event\":\"quic_media_budget\","
            << "\"kind\":\"controller\",\"sequence\":" << sequence_++
            << ",\"monotonic_us\":" << state.at_time.us()
            << ",\"utc_ms\":" << utc_ms
            << ",\"acked_bytes\":" << state.acked_bytes
            << ",\"startup_complete\":"
            << (state.startup_complete ? "true" : "false")
            << ",\"delivery_rate_bytes_per_second\":"
            << state.delivery_rate_bytes_per_second
            << ",\"max_bandwidth_bytes_per_second\":"
            << state.max_bandwidth_bytes_per_second
            << ",\"rtt_us\":" << state.round_trip_time.us()
            << ",\"fresh\":" << (state.fresh ? "true" : "false")
            << ",\"raw_capacity_bps\":" << state.raw_capacity.bps()
            << ",\"overhead_bps\":" << state.overhead.bps()
            << ",\"recovery_reserve_bps\":" << state.recovery_reserve.bps()
            << ",\"safety_headroom_bps\":" << state.safety_headroom.bps()
            << ",\"usable_media_rate_bps\":" << state.usable_media_rate.bps()
            << ",\"target_rate_bps\":" << state.target_rate.bps()
            << ",\"pacer_rate_bps\":" << state.pacer_rate.bps()
            << ",\"padding_rate_bps\":" << state.padding_rate.bps()
            << ",\"probe_cluster_count\":" << state.probe_cluster_count
            << ",\"has_congestion_window\":"
            << (state.has_congestion_window ? "true" : "false") << "}\n";
  }

  void WriteMediaControl(const RoqMediaSessionStats& stats) {
    const auto monotonic_us =
        std::chrono::duration_cast<std::chrono::microseconds>(
            std::chrono::steady_clock::now().time_since_epoch())
            .count();
    const auto utc_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                            std::chrono::system_clock::now().time_since_epoch())
                            .count();
    std::lock_guard lock(mutex_);
    output_ << "{\"schema_version\":1,\"run_id\":\"" << run_id_
            << "\",\"process\":\"" << process_
            << "\",\"event\":\"media_control_state\","
            << "\"kind\":\"webrtc\",\"sequence\":" << sequence_++
            << ",\"monotonic_us\":" << monotonic_us << ",\"utc_ms\":" << utc_ms
            << ",\"video_encoder_target_bps\":"
            << stats.video_target_bitrate_bps
            << ",\"call_target_rate_bps\":" << stats.call_target_rate_bps
            << ",\"pacer_rate_bps\":" << stats.pacer_rate_bps
            << ",\"padding_rate_bps\":" << stats.padding_rate_bps << "}\n";
  }

 private:
  std::ofstream output_;
  std::string run_id_;
  std::string process_;
  std::mutex mutex_;
  uint64_t sequence_ = 0;
};

class PrewarmFileCapturerObserver final : public FileMediaCapturerObserver {
 public:
  void set_capturer(FileMediaCapturer* capturer) { capturer_ = capturer; }

  void OnVideoFrame(VideoFrame frame) override {
    if (!frame_) {
      frame_ = frame.video_frame_buffer()->ToI420();
      capturer_->Stop();
    }
  }
  void OnAudioBlock(FileMediaAudioBlock) override {}
  void OnLoopBoundary(FileMediaLoopBoundary) override {}
  void OnTerminal(FileMediaTerminal terminal) override {
    {
      std::lock_guard lock(mutex_);
      if (terminal.reason == FileMediaTerminalReason::kError)
        error_ = terminal.error.message;
      terminal_ = true;
    }
    condition_.notify_all();
  }

  bool Wait() {
    std::unique_lock lock(mutex_);
    return condition_.wait_for(lock, std::chrono::seconds(3),
                               [this] { return terminal_; });
  }
  scoped_refptr<I420BufferInterface> frame() const { return frame_; }
  const std::string& error() const { return error_; }

 private:
  FileMediaCapturer* capturer_ = nullptr;
  scoped_refptr<I420BufferInterface> frame_;
  std::mutex mutex_;
  std::condition_variable condition_;
  bool terminal_ = false;
  std::string error_;
};

class SendingFileCapturerObserver final : public FileMediaCapturerObserver {
 public:
  SendingFileCapturerObserver(RoqMediaSession* send,
                              MediaPeerResult* result,
                              MediaEventLog* event_log,
                              std::atomic<size_t>* loop_counter,
                              std::optional<double> duration_seconds,
                              RoqSession* network)
      : send_(send),
        result_(result),
        event_log_(event_log),
        loop_counter_(loop_counter),
        duration_us_(duration_seconds
                         ? static_cast<int64_t>(*duration_seconds * 1000000.0)
                         : std::numeric_limits<int64_t>::max()),
        network_(network) {}

  void set_capturer(FileMediaCapturer* capturer) { capturer_ = capturer; }

  void OnVideoFrame(VideoFrame frame) override {
    if (ShouldStop(frame.timestamp_us()))
      return;
    scoped_refptr<I420BufferInterface> buffer =
        frame.video_frame_buffer()->ToI420();
    if (!buffer) {
      error_ = "file capturer emitted non-I420 video";
      capturer_->Stop();
      return;
    }
    send_->SendVideoFrame(std::move(buffer), frame.timestamp_us());
    ++result_->sent_video_frames;
  }

  void OnAudioBlock(FileMediaAudioBlock block) override {
    if (ShouldStop(block.timestamp_us))
      return;
    send_->SendAudioBlock(
        block.samples, static_cast<uint32_t>(block.timestamp_us * 48 / 1000));
  }

  void OnLoopBoundary(FileMediaLoopBoundary boundary) override {
    result_->loops_started = boundary.cycle_index + 1;
    loop_counter_->store(result_->loops_started);
    event_log_->Write("loop_started", "media", result_->loops_started);
    send_->GenerateSenderKeyFrame();
  }

  void OnTerminal(FileMediaTerminal terminal) override {
    {
      std::lock_guard lock(mutex_);
      if (terminal.reason == FileMediaTerminalReason::kError && error_.empty())
        error_ = terminal.error.message;
      terminal_ = true;
    }
    condition_.notify_all();
  }

  void Wait() {
    std::unique_lock lock(mutex_);
    while (!terminal_) {
      condition_.wait_for(lock, std::chrono::milliseconds(20));
      if (!terminal_ &&
          (g_stop_requested.load() || !network_->error().empty() ||
           network_->stats().clean_shutdown)) {
        lock.unlock();
        capturer_->Stop();
        lock.lock();
      }
    }
  }

  const std::string& error() const { return error_; }

 private:
  bool ShouldStop(int64_t timestamp_us) {
    if (timestamp_us < duration_us_ && !g_stop_requested.load() &&
        network_->error().empty() && !network_->stats().clean_shutdown) {
      return false;
    }
    capturer_->Stop();
    return true;
  }

  RoqMediaSession* const send_;
  MediaPeerResult* const result_;
  MediaEventLog* const event_log_;
  std::atomic<size_t>* const loop_counter_;
  const int64_t duration_us_;
  RoqSession* const network_;
  FileMediaCapturer* capturer_ = nullptr;
  mutable std::mutex mutex_;
  std::condition_variable condition_;
  bool terminal_ = false;
  std::string error_;
};

struct MediaState {
  std::mutex mutex;
  RoqSession* network = nullptr;
  RoqMediaSession* send = nullptr;
  RoqMediaSession* receive = nullptr;
  const SessionManifest* manifest = nullptr;
  bool reflect = false;
  MediaEventLog* event_log = nullptr;
  bool drop_first_idr = false;
  bool first_idr_dropped = false;
  uint32_t buffered_video_timestamp = 0;
  bool has_buffered_video = false;
  std::vector<std::vector<uint8_t>> buffered_video_packets;
  bool buffered_video_is_idr = false;
  bool buffered_video_has_sps = false;
  bool buffered_video_has_pps = false;
  std::chrono::steady_clock::time_point idr_drop_time;
  std::atomic<size_t> reflected_audio = 0;
  std::atomic<size_t> reflected_video = 0;
  std::atomic<size_t> received_rtcp = 0;
  std::atomic<size_t> idr_access_units = 0;
  std::atomic<size_t> dropped_idr_access_units = 0;
  std::atomic<size_t> recovery_idr_access_units = 0;
  std::atomic<size_t> pli_packets = 0;
  std::atomic<size_t> fir_packets = 0;
  std::atomic<size_t> sender_reports = 0;
  std::atomic<size_t> receiver_reports = 0;
  std::atomic<int64_t> idr_recovery_ms = -1;
  std::atomic<bool> video_origin_set = false;
  std::atomic<bool> audio_origin_set = false;
  std::atomic<bool> audio_last_set = false;
  std::atomic<uint32_t> video_timestamp_origin = 0;
  std::atomic<uint32_t> audio_timestamp_origin = 0;
  std::atomic<uint32_t> audio_last_timestamp = 0;
  std::atomic<size_t> sent_video_rtp_packets = 0;
  std::atomic<size_t> sent_video_rtp_bytes = 0;
  std::atomic<size_t> sent_audio_rtp_bytes = 0;
  std::atomic<size_t> sent_rtx_packets = 0;
  std::atomic<size_t> received_rtx_packets = 0;
  std::atomic<size_t> nack_packets = 0;
  size_t drop_video_packet_after = 0;
  size_t drop_audio_every = 0;
  std::atomic<size_t> received_primary_audio_packets = 0;
  std::atomic<size_t> injected_audio_packet_drops = 0;
  std::atomic<size_t> received_primary_video_packets = 0;
  std::atomic<size_t> injected_video_packet_drops = 0;
  std::atomic<bool> first_media_received = false;
  std::atomic<bool> first_media_submitted = false;
};

uint32_t RtpTimestamp(std::span<const uint8_t> packet) {
  if (packet.size() < 8)
    return 0;
  return (static_cast<uint32_t>(packet[4]) << 24) |
         (static_cast<uint32_t>(packet[5]) << 16) |
         (static_cast<uint32_t>(packet[6]) << 8) | packet[7];
}

void RecordTimestampOrigin(std::atomic<bool>* is_set,
                           std::atomic<uint32_t>* timestamp,
                           std::span<const uint8_t> packet) {
  bool expected = false;
  if (is_set->compare_exchange_strong(expected, true))
    timestamp->store(RtpTimestamp(packet));
}

void CountFeedback(std::span<const uint8_t> packet, MediaState* state) {
  while (!packet.empty()) {
    rtcp::CommonHeader header;
    if (!header.Parse(packet.data(), packet.size()))
      return;
    if (header.type() == 206 && header.fmt() == 1)
      ++state->pli_packets;
    if (header.type() == 206 && header.fmt() == 4)
      ++state->fir_packets;
    if (header.type() == 205 && header.fmt() == 1)
      ++state->nack_packets;
    if (header.type() == 200)
      ++state->sender_reports;
    if (header.type() == 201)
      ++state->receiver_reports;
    if (header.packet_size() > packet.size())
      return;
    packet = packet.subspan(header.packet_size());
  }
}

struct RtcpRouting {
  bool to_sender = false;
  bool to_receiver = false;
};

RtcpRouting ClassifyRtcp(std::span<const uint8_t> packet) {
  RtcpRouting routing;
  while (!packet.empty()) {
    rtcp::CommonHeader header;
    if (!header.Parse(packet.data(), packet.size()))
      break;
    if (header.type() == 200)
      routing.to_receiver = true;
    if (header.type() == 201 || header.type() == 205 || header.type() == 206)
      routing.to_sender = true;
    if (header.packet_size() > packet.size())
      break;
    packet = packet.subspan(header.packet_size());
  }
  return routing;
}

RoqSendResult SendFixtureDatagram(RoqSession* session,
                                  uint64_t flow_id,
                                  std::span<const uint8_t> packet,
                                  RoqMediaType media_type,
                                  RoqDatagramPriority priority) {
  const Timestamp now = Clock::GetRealTimeClock()->CurrentTime();
  return session->SendMedia(flow_id, packet,
                            {.media_type = media_type,
                             .enqueue_time = now,
                             .deadline = now + TimeDelta::Seconds(1),
                             .priority = priority},
                            {});
}

void FlushReflectedVideo(MediaState* state) {
  if (!state->network || state->buffered_video_packets.empty())
    return;
  if (state->buffered_video_is_idr) {
    ++state->idr_access_units;
    if (!state->first_idr_dropped && state->drop_first_idr) {
      state->first_idr_dropped = true;
      state->idr_drop_time = std::chrono::steady_clock::now();
      ++state->dropped_idr_access_units;
      state->buffered_video_packets.clear();
      state->buffered_video_is_idr = false;
      state->buffered_video_has_sps = false;
      state->buffered_video_has_pps = false;
      state->has_buffered_video = false;
      return;
    }
    if (state->first_idr_dropped && state->buffered_video_has_sps &&
        state->buffered_video_has_pps) {
      ++state->recovery_idr_access_units;
      int64_t unset = -1;
      state->idr_recovery_ms.compare_exchange_strong(
          unset, std::chrono::duration_cast<std::chrono::milliseconds>(
                     std::chrono::steady_clock::now() - state->idr_drop_time)
                     .count());
    }
  }
  for (const std::vector<uint8_t>& packet : state->buffered_video_packets) {
    if (SendFixtureDatagram(state->network, state->manifest->video.flow_id,
                            packet, RoqMediaType::kVideo,
                            RoqDatagramPriority::kVideoKey) ==
        RoqSendResult::kQueued) {
      ++state->reflected_video;
    }
  }
  state->buffered_video_packets.clear();
  state->buffered_video_is_idr = false;
  state->buffered_video_has_sps = false;
  state->buffered_video_has_pps = false;
  state->has_buffered_video = false;
}

scoped_refptr<AudioDeviceModule> CreateTestAudioDevice(
    const Environment& env,
    std::unique_ptr<TestAudioDeviceModule::Renderer> renderer) {
  scoped_refptr<AudioDeviceModule> device =
      TestAudioDeviceModule::Create(env, nullptr, std::move(renderer));
  if (device && device->Init() != 0)
    return nullptr;
  return device;
}

scoped_refptr<AudioDeviceModule> CreatePlaybackAudioDevice(
    const Environment& env,
    std::string* error) {
  scoped_refptr<AudioDeviceModule> device =
      CreateAudioDeviceModule(env, AudioDeviceModule::kPlatformDefaultAudio);
  if (!device || device->Init() != 0 || device->SetPlayoutDevice(0) != 0 ||
      device->InitSpeaker() != 0) {
    *error = "failed to initialize macOS audio playout device";
    return nullptr;
  }
  bool stereo = false;
  if (device->StereoPlayoutIsAvailable(&stereo) != 0 ||
      device->SetStereoPlayout(stereo) != 0) {
    *error = "failed to configure macOS audio playout";
    return nullptr;
  }
  return device;
}

void RoutePacket(MediaState* state, RoqReceivedPacket received) {
  RoqMediaSession* send = nullptr;
  RoqMediaSession* receive = nullptr;
  RoqSession* network = nullptr;
  const SessionManifest* manifest = nullptr;
  bool reflect = false;
  {
    std::lock_guard lock(state->mutex);
    send = state->send;
    receive = state->receive;
    network = state->network;
    manifest = state->manifest;
    reflect = state->reflect;
  }
  if (!manifest)
    return;
  if (received.flow_id == manifest->audio.flow_id) {
    if (received.packet.size() < 2 ||
        (received.packet[1] & 0x7f) != manifest->audio.payload_type)
      return;
    const size_t received_primary = ++state->received_primary_audio_packets;
    if (state->drop_audio_every > 0 &&
        received_primary % state->drop_audio_every == 0) {
      ++state->injected_audio_packet_drops;
      if (state->event_log)
        state->event_log->Write("injected_audio_packet_drop", "audio",
                                received_primary);
      return;
    }
    RecordTimestampOrigin(&state->audio_origin_set,
                          &state->audio_timestamp_origin, received.packet);
    state->audio_last_timestamp.store(RtpTimestamp(received.packet));
    state->audio_last_set.store(true);
    if (state->event_log)
      state->event_log->Write("datagram_received", "audio");
    if (!state->first_media_received.exchange(true) && state->event_log)
      state->event_log->Write("first_media_datagram_decrypted", "audio");
    if (receive)
      receive->DeliverRtp(true, received.packet);
    if (reflect && network &&
        SendFixtureDatagram(
            network, received.flow_id, received.packet, RoqMediaType::kAudio,
            RoqDatagramPriority::kAudio) == RoqSendResult::kQueued)
      ++state->reflected_audio;
  } else if (received.flow_id == manifest->video.flow_id) {
    if (received.packet.size() < 2)
      return;
    const uint8_t payload_type = received.packet[1] & 0x7f;
    if (payload_type != manifest->video.payload_type &&
        payload_type != manifest->video_rtx.payload_type)
      return;
    if (payload_type == manifest->video.payload_type) {
      const size_t received_primary = ++state->received_primary_video_packets;
      if (state->drop_video_packet_after > 0 &&
          received_primary == state->drop_video_packet_after) {
        ++state->injected_video_packet_drops;
        if (state->event_log)
          state->event_log->Write("injected_video_packet_drop", "video",
                                  received_primary);
        return;
      }
      RecordTimestampOrigin(&state->video_origin_set,
                            &state->video_timestamp_origin, received.packet);
    } else {
      ++state->received_rtx_packets;
    }
    if (state->event_log)
      state->event_log->Write("datagram_received", "video");
    if (!state->first_media_received.exchange(true) && state->event_log)
      state->event_log->Write("first_media_datagram_decrypted", "video");
    if (receive)
      receive->DeliverRtp(false, received.packet);
    if (reflect && network) {
      if (!state->drop_first_idr) {
        if (SendFixtureDatagram(network, received.flow_id, received.packet,
                                RoqMediaType::kVideo,
                                RoqDatagramPriority::kVideoDelta) ==
            RoqSendResult::kQueued)
          ++state->reflected_video;
      } else {
        RtpPacketMetadata metadata;
        if (!ParseH264RtpPacketMetadata(received.packet, &metadata))
          return;
        if (state->has_buffered_video &&
            state->buffered_video_timestamp != metadata.timestamp) {
          FlushReflectedVideo(state);
        }
        state->has_buffered_video = true;
        state->buffered_video_timestamp = metadata.timestamp;
        state->buffered_video_is_idr |= metadata.contains_idr;
        state->buffered_video_has_sps |= metadata.contains_sps;
        state->buffered_video_has_pps |= metadata.contains_pps;
        state->buffered_video_packets.push_back(std::move(received.packet));
        if (metadata.marker)
          FlushReflectedVideo(state);
      }
    }
  } else if (received.flow_id == manifest->rtcp_flow_id) {
    if (state->event_log)
      state->event_log->Write("datagram_received", "rtcp");
    ++state->received_rtcp;
    CountFeedback(received.packet, state);
    const RtcpRouting routing = ClassifyRtcp(received.packet);
    if (send && routing.to_sender)
      send->DeliverRtcp(received.packet);
    if (receive && routing.to_receiver)
      receive->DeliverRtcp(received.packet);
    if (reflect && network)
      SendFixtureDatagram(network, received.flow_id, received.packet,
                          RoqMediaType::kRtcp, RoqDatagramPriority::kRtcp);
  }
}

RoqMediaSessionConfig BaseMediaConfig(
    const MediaPeerOptions& options,
    const SessionManifest& manifest,
    scoped_refptr<AudioDeviceModule> audio_device,
    AudioSinkInterface* audio_sink,
    VideoSinkInterface<VideoFrame>* video_sink,
    Transport* audio_transport,
    Transport* video_transport,
    VideoEncoderFactory* video_encoder_factory,
    VideoDecoderFactory* video_decoder_factory) {
  const bool independent_downlink = options.scenario == "early-media";
  const bool server = options.role == PeerRole::kServer;
  return {
      .local_audio_ssrc = independent_downlink && server
                              ? manifest.audio.server_ssrc
                              : manifest.audio.client_ssrc,
      .local_video_ssrc = independent_downlink && server
                              ? manifest.video.server_ssrc
                              : manifest.video.client_ssrc,
      .local_video_rtx_ssrc = independent_downlink && server
                                  ? manifest.video_rtx.server_ssrc
                                  : manifest.video_rtx.client_ssrc,
      .remote_audio_ssrc = independent_downlink && !server
                               ? manifest.audio.server_ssrc
                               : manifest.audio.client_ssrc,
      .remote_video_ssrc = independent_downlink && !server
                               ? manifest.video.server_ssrc
                               : manifest.video.client_ssrc,
      .remote_video_rtx_ssrc = independent_downlink && !server
                                   ? manifest.video_rtx.server_ssrc
                                   : manifest.video_rtx.client_ssrc,
      .audio_payload_type = static_cast<int>(manifest.audio.payload_type),
      .video_payload_type = static_cast<int>(manifest.video.payload_type),
      .video_rtx_payload_type =
          static_cast<int>(manifest.video_rtx.payload_type),
      .audio_bitrate_bps = options.audio_bitrate_bps,
      .video_bitrate_bps = options.video_bitrate_bps,
      .max_rtp_packet_size = independent_downlink ? 999u : 1460u,
      .audio_transport = audio_transport,
      .video_transport = video_transport,
      .audio_device_module = std::move(audio_device),
      .audio_encoder_factory = CreateBuiltinAudioEncoderFactory(),
      .audio_decoder_factory = CreateBuiltinAudioDecoderFactory(),
      .video_encoder_factory = video_encoder_factory,
      .video_decoder_factory = video_decoder_factory,
      .audio_sink = audio_sink,
      .video_sink = video_sink,
  };
}

}  // namespace

MediaPeerResult RunMediaPeer(const MediaPeerOptions& options) {
  MediaPeerResult result;
  g_stop_requested.store(false);
  std::signal(SIGINT, RequestStop);
  std::signal(SIGTERM, RequestStop);
  SessionManifest manifest;
  if (!LoadSessionManifest(options.manifest_path, &manifest, &result.error))
    return result;
  MediaEventLog event_log(options.event_log_path, options.run_id, options.role);
  if (!event_log.ok()) {
    result.error = "failed to open media peer event log";
    return result;
  }
  const Environment env = CreateEnvironment();
  scoped_refptr<I420BufferInterface> prewarmed_video_frame;
  if (options.scenario == "early-media") {
    PrewarmFileCapturerObserver prewarm;
    FileMediaCapturerCreateResult created = CreateFileMediaCapturer(
        {.input_path = options.input_path, .eof_mode = FileMediaEofMode::kStop},
        &prewarm);
    if (!created.ok()) {
      result.error = created.error.message;
      return result;
    }
    prewarm.set_capturer(created.capturer.get());
    FileMediaError start = created.capturer->Start();
    if (!start.ok() || !prewarm.Wait()) {
      result.error =
          start.ok() ? "file capturer prewarm timed out" : start.message;
      return result;
    }
    prewarmed_video_frame = prewarm.frame();
    if (!prewarmed_video_frame) {
      result.error = prewarm.error().empty() ? "failed to prewarm video frame"
                                             : prewarm.error();
      return result;
    }
  }
  CountingVideoSink counting_video_sink;
  std::unique_ptr<test::VideoRenderer> platform_video_sink;
  std::unique_ptr<FanoutVideoSink> fanout_video_sink;
  VideoSinkInterface<VideoFrame>* video_sink = &counting_video_sink;
  if (options.role == PeerRole::kClient && !options.no_render) {
    platform_video_sink.reset(test::VideoRenderer::CreatePlatformRenderer(
        "RoQ media loopback", 960, 540));
    if (!platform_video_sink) {
      result.error = "failed to initialize Cocoa video renderer";
      return result;
    }
    fanout_video_sink = std::make_unique<FanoutVideoSink>(
        &counting_video_sink, platform_video_sink.get());
    video_sink = fanout_video_sink.get();
  }
  CountingAudioSink audio_sink;
  auto audio_renderer = std::make_unique<CountingAudioRenderer>();
  scoped_refptr<AudioDeviceModule> audio_device =
      options.role == PeerRole::kClient && !options.no_render &&
              options.scenario == "media-loopback"
          ? CreatePlaybackAudioDevice(env, &result.error)
          : CreateTestAudioDevice(env, std::move(audio_renderer));
  if (!audio_device) {
    result.error = "failed to initialize test audio device";
    return result;
  }

  std::unique_ptr<VideoEncoderFactory> video_encoder_factory =
      CreateBuiltinVideoEncoderFactory();
  std::unique_ptr<VideoDecoderFactory> video_decoder_factory =
      CreateBuiltinVideoDecoderFactory();
  MediaState state;
  state.manifest = &manifest;
  state.event_log = &event_log;
  state.reflect =
      options.role == PeerRole::kServer && options.scenario == "media-loopback";
  state.drop_first_idr = options.drop_first_idr;
  state.drop_video_packet_after = options.drop_video_packet_after;
  state.drop_audio_every = options.drop_audio_every;
  const std::filesystem::path executable =
      std::filesystem::absolute(options.executable_path);
  std::vector<uint64_t> allowed_flow_ids = {
      manifest.audio.flow_id, manifest.video.flow_id, manifest.rtcp_flow_id};
  if (options.probe_datagram_boundary)
    allowed_flow_ids.push_back(63);
  RoqSessionConfig network_config = {
      .address = options.address,
      .alpn = manifest.alpn,
      .trusted_ca_file = (executable.parent_path() / "cert.crt").string(),
      .resumption_cache_directory = options.cache_directory,
      .allowed_flow_ids = std::move(allowed_flow_ids),
  };
  network_config.resumption_context.fill(options.context_byte);
  if (options.role == PeerRole::kClient && options.scenario == "early-media") {
    network_config.receive_flow_ids = {manifest.audio.flow_id,
                                       manifest.rtcp_flow_id};
  }
  std::atomic<bool> early_request_received = false;
  std::function<void()> early_request_handler;
  auto receive_callback = [&state](RoqReceivedPacket packet) {
    RoutePacket(&state, std::move(packet));
  };
  auto datagram_size_callback = [&state](size_t packet_size) {
    RoqMediaSession* send = nullptr;
    {
      std::lock_guard lock(state.mutex);
      send = state.send;
    }
    if (send)
      send->UpdateMaxRtpPacketSize(packet_size);
  };
  std::unique_ptr<RoqSession> network;
  TestRoqServer* test_server = nullptr;
  if (options.role == PeerRole::kClient) {
    network = RoqSession::Create(
        std::move(network_config), receive_callback, datagram_size_callback,
        [&state, &manifest](std::vector<uint8_t> message) {
          if (message != std::vector<uint8_t>(kEarlyMediaReady.begin(),
                                              kEarlyMediaReady.end()))
            return;
          RoqSession* session = nullptr;
          {
            std::lock_guard lock(state.mutex);
            session = state.network;
          }
          if (session) {
            session->UpdateReceiveFlowIds({manifest.audio.flow_id,
                                           manifest.video.flow_id,
                                           manifest.rtcp_flow_id});
          }
        });
  } else {
    std::array<uint8_t, 32> context = {};
    context.fill(options.context_byte);
    std::unique_ptr<TestRoqServer> server = CreateTestRoqServer(
        {.address = options.address,
         .alpn = manifest.alpn,
         .certificate_path = (executable.parent_path() / "cert.crt").string(),
         .private_key_path = (executable.parent_path() / "cert.key").string(),
         .resumption_context = context,
         .address_token = std::vector<uint8_t>(kEarlyMediaAddressToken.begin(),
                                               kEarlyMediaAddressToken.end()),
         .gate_media_until_reliable_message = options.scenario == "early-media",
         .replay_first_provisional = options.replay_first_provisional,
         .drop_all_provisional = options.drop_all_provisional,
         .allowed_flow_ids = network_config.allowed_flow_ids},
        receive_callback, datagram_size_callback,
        [&early_request_received, &early_request_handler,
         &event_log](std::vector<uint8_t> message) {
          if (message == std::vector<uint8_t>(kEarlyMediaRequest.begin(),
                                              kEarlyMediaRequest.end())) {
            early_request_received.store(true);
            event_log.Write("early_request_received", "control");
            if (early_request_handler)
              early_request_handler();
          }
        });
    test_server = server.get();
    network = std::move(server);
  }
  {
    std::lock_guard lock(state.mutex);
    state.network = network.get();
  }
  auto record_outgoing = [&state, &manifest, &options](
                             uint64_t flow_id, std::span<const uint8_t> packet,
                             const PacketOptions& packet_options) {
    if (packet.size() < 2)
      return;
    const uint8_t payload_type = packet[1] & 0x7f;
    bool media_rtp = false;
    if (flow_id == manifest.audio.flow_id &&
        payload_type == manifest.audio.payload_type) {
      media_rtp = true;
      RecordTimestampOrigin(&state.audio_origin_set,
                            &state.audio_timestamp_origin, packet);
      state.sent_audio_rtp_bytes.fetch_add(packet.size());
    } else if (flow_id == manifest.video.flow_id &&
               (payload_type == manifest.video.payload_type ||
                payload_type == manifest.video_rtx.payload_type)) {
      media_rtp = true;
      if (payload_type == manifest.video.payload_type) {
        RecordTimestampOrigin(&state.video_origin_set,
                              &state.video_timestamp_origin, packet);
        ++state.sent_video_rtp_packets;
      } else {
        ++state.sent_rtx_packets;
      }
      if (payload_type == manifest.video.payload_type)
        state.sent_video_rtp_bytes.fetch_add(packet.size());
    }
    RoqMediaSession* send_session = nullptr;
    {
      std::lock_guard lock(state.mutex);
      send_session = state.send;
    }
    if (send_session && media_rtp)
      send_session->NotifyPacketAccepted(packet_options, packet.size());
    if (media_rtp && options.role == PeerRole::kServer &&
        !state.first_media_submitted.exchange(true) && state.event_log) {
      state.event_log->Write("first_server_media_datagram_submitted", "media");
    }
  };
  const bool server_early_media =
      options.role == PeerRole::kServer && options.scenario == "early-media";
  const uint32_t local_audio_ssrc = server_early_media
                                        ? manifest.audio.server_ssrc
                                        : manifest.audio.client_ssrc;
  const uint32_t local_video_ssrc = server_early_media
                                        ? manifest.video.server_ssrc
                                        : manifest.video.client_ssrc;
  const uint32_t local_rtx_ssrc = server_early_media
                                      ? manifest.video_rtx.server_ssrc
                                      : manifest.video_rtx.client_ssrc;
  RoqPacketTransport audio_transport(
      {.session = network.get(),
       .media_type = RoqMediaType::kAudio,
       .rtp_flow_id = manifest.audio.flow_id,
       .rtcp_flow_id = manifest.rtcp_flow_id,
       .primary_payload_type =
           static_cast<uint8_t>(manifest.audio.payload_type),
       .primary_ssrc = local_audio_ssrc,
       .clock = &env.clock()},
      record_outgoing);
  RoqPacketTransport video_transport(
      {.session = network.get(),
       .media_type = RoqMediaType::kVideo,
       .rtp_flow_id = manifest.video.flow_id,
       .rtcp_flow_id = manifest.rtcp_flow_id,
       .primary_payload_type =
           static_cast<uint8_t>(manifest.video.payload_type),
       .primary_ssrc = local_video_ssrc,
       .rtx_payload_type =
           static_cast<uint8_t>(manifest.video_rtx.payload_type),
       .rtx_ssrc = local_rtx_ssrc,
       .clock = &env.clock()},
      record_outgoing);
  CapturingTransport server_capture_transport;
  std::unique_ptr<RoqMediaSession> receive;
  if ((options.role == PeerRole::kServer &&
       options.scenario != "early-media") ||
      options.scenario == "media-loopback" ||
      (options.role == PeerRole::kClient &&
       options.scenario == "early-media")) {
    RoqMediaSessionConfig receive_config = BaseMediaConfig(
        options, manifest, audio_device, &audio_sink, video_sink,
        &audio_transport, &video_transport, video_encoder_factory.get(),
        video_decoder_factory.get());
    receive_config.receive_audio = true;
    receive_config.receive_video = true;
    receive_config.network_stats_provider = [network = network.get()] {
      return network->stats();
    };
    receive_config.network_control_observer =
        [&event_log](const QuicNetworkControlState& state) {
          event_log.WriteBudget(state);
        };
    receive_config.keyframe_request_method =
        options.keyframe_request == "fir" ? RoqKeyFrameRequestMethod::kFir
                                          : RoqKeyFrameRequestMethod::kPli;
    receive =
        RoqMediaSession::Create(std::move(receive_config), env, &result.error);
    if (!receive || !receive->Start(&result.error))
      return result;
    {
      std::lock_guard lock(state.mutex);
      state.receive = receive.get();
    }
  }
  std::unique_ptr<RoqMediaSession> send;
  RoqMediaSessionStats send_stats;
  scoped_refptr<AudioDeviceModule> send_audio_device;
  std::atomic<bool> reporter_stop = false;
  std::atomic<bool> media_complete = false;
  std::atomic<size_t> loop_counter = 0;
  std::thread reporter;
  std::thread keyframe_requester;
  std::thread capacity_loader;
  std::chrono::steady_clock::time_point media_run_start;
  std::vector<std::vector<uint8_t>> preencoded_server_rtp;
  if (options.role == PeerRole::kClient || server_early_media) {
    send_audio_device = CreateTestAudioDevice(env, nullptr);
    RoqMediaSessionConfig send_config = BaseMediaConfig(
        options, manifest, send_audio_device, nullptr, nullptr,
        &audio_transport,
        server_early_media ? static_cast<Transport*>(&server_capture_transport)
                           : static_cast<Transport*>(&video_transport),
        video_encoder_factory.get(), video_decoder_factory.get());
    send_config.send_audio = !server_early_media;
    send_config.send_video = true;
    send_config.network_stats_provider = [network = network.get()] {
      return network->stats();
    };
    send_config.network_control_observer =
        [&event_log](const QuicNetworkControlState& state) {
          event_log.WriteBudget(state);
        };
    send = RoqMediaSession::Create(std::move(send_config), env, &result.error);
    if (!send || !send->Start(&result.error))
      return result;
    {
      std::lock_guard lock(state.mutex);
      state.send = send.get();
    }
  }
  if (options.scenario == "early-media") {
    send->GenerateSenderKeyFrame();
    send->SendVideoFrame(prewarmed_video_frame, 0);
    if (options.role == PeerRole::kClient) {
      std::array<int16_t, 960> silence = {};
      send->SendAudioBlock(silence, 0);
      if (!network->SendReplaySafeMessage(kEarlyMediaRequest)) {
        result.error = "failed to queue early-media request";
        return result;
      }
    }
    const auto encode_deadline =
        std::chrono::steady_clock::now() + std::chrono::seconds(2);
    while (send->stats().video_frames_encoded == 0 &&
           std::chrono::steady_clock::now() < encode_deadline) {
      std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    if (send->stats().video_frames_encoded == 0) {
      result.error = "failed to pre-encode early video frame";
      return result;
    }
    if (server_early_media) {
      const auto capture_deadline =
          std::chrono::steady_clock::now() + std::chrono::seconds(1);
      while (
          CompleteKeyAccessUnit(server_capture_transport.packets()).empty() &&
          std::chrono::steady_clock::now() < capture_deadline) {
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
      }
      preencoded_server_rtp =
          CompleteKeyAccessUnit(server_capture_transport.packets());
      if (preencoded_server_rtp.empty()) {
        result.error = "failed to capture pre-encoded server RTP";
        return result;
      }
      early_request_handler = [&] {
        for (const std::vector<uint8_t>& packet : preencoded_server_rtp)
          video_transport.SendRtp(packet, PacketOptions());
        network->SendReplaySafeMessage(kEarlyMediaReady);
      };
    }
  }
  if (!network->Start(&result.error) ||
      !network->WaitUntilReady(TimeDelta::Seconds(3))) {
    if (result.error.empty())
      result.error = network->error();
    return result;
  }
  if (options.role == PeerRole::kClient && options.scenario == "early-media")
    event_log.Write("client_initial_started", "quic");
  if (options.role == PeerRole::kClient && options.scenario == "early-media" &&
      network->stats().resumption_cache_hit) {
    std::vector<uint8_t> boundary_packet(999, 0);
    boundary_packet[0] = 0x80;
    boundary_packet[1] = 127;
    result.early_boundary_packet_accepted =
        SendFixtureDatagram(
            network.get(), 63, boundary_packet, RoqMediaType::kVideo,
            RoqDatagramPriority::kVideoDelta) == RoqSendResult::kQueued;
    boundary_packet.push_back(0);
    result.early_oversize_packet_rejected =
        SendFixtureDatagram(
            network.get(), 63, boundary_packet, RoqMediaType::kVideo,
            RoqDatagramPriority::kVideoDelta) == RoqSendResult::kOversize;
  }
  if (options.role == PeerRole::kServer) {
    std::cout << "{\"schema_version\":1,\"event\":\"ready\"}\n" << std::flush;
  }

  bool established = false;
  if (options.role == PeerRole::kServer) {
    while (!g_stop_requested.load() && network->error().empty()) {
      if (network->WaitUntilEstablished(TimeDelta::Millis(100))) {
        established = true;
        break;
      }
    }
  } else {
    established = network->WaitUntilEstablished(TimeDelta::Seconds(10));
  }
  if (!established) {
    const RoqSessionStats failed = network->stats();
    result.error =
        network->error().empty()
            ? "QUIC handshake timed out: sent_udp=" +
                  std::to_string(failed.sent_udp_packets) +
                  " received_udp=" + std::to_string(failed.received_udp_packets)
            : network->error();
    return result;
  }
  event_log.Write("session_established", "quic");
  if (options.role == PeerRole::kClient && options.capacity_load_bps > 0) {
    capacity_loader = std::thread([&] {
      constexpr size_t kPayloadSize = 1200;
      std::vector<uint8_t> payload(kPayloadSize, 0xa5);
      const auto interval = std::chrono::nanoseconds(
          kPayloadSize * 8'000'000'000ULL / options.capacity_load_bps);
      auto next_send = std::chrono::steady_clock::now();
      while (!media_complete.load() && !g_stop_requested.load()) {
        const Timestamp now = env.clock().CurrentTime();
        network->SendMedia(manifest.video.flow_id, payload,
                           {.media_type = RoqMediaType::kVideo,
                            .enqueue_time = now,
                            .deadline = now + TimeDelta::Millis(50),
                            .priority = RoqDatagramPriority::kVideoDelta},
                           {});
        next_send += interval;
        std::this_thread::sleep_until(next_send);
      }
    });
  }
  if (receive && options.request_keyframe_after_ms > 0) {
    keyframe_requester = std::thread([&] {
      std::this_thread::sleep_for(
          std::chrono::milliseconds(options.request_keyframe_after_ms));
      do {
        if (media_complete.load() || g_stop_requested.load())
          break;
        receive->RequestReceiverKeyFrame();
        event_log.Write("keyframe_requested", options.keyframe_request.c_str());
        if (options.request_keyframe_interval_ms <= 0)
          break;
        std::this_thread::sleep_for(
            std::chrono::milliseconds(options.request_keyframe_interval_ms));
      } while (!media_complete.load() && !g_stop_requested.load());
    });
  }
  const size_t negotiated_datagram_payload =
      network->stats().max_datagram_payload;
  if (send && negotiated_datagram_payload > 1) {
    send->UpdateMaxRtpPacketSize(negotiated_datagram_payload - 1);
  }
  if (options.role == PeerRole::kClient && options.probe_datagram_boundary &&
      negotiated_datagram_payload > 13) {
    std::vector<uint8_t> boundary_packet(negotiated_datagram_payload - 1, 0);
    boundary_packet[0] = 0x80;
    boundary_packet[1] = 127;
    boundary_packet[8] = 0x01;
    boundary_packet[9] = 0x53;
    boundary_packet[10] = 0x15;
    boundary_packet[11] = 0x8e;
    result.boundary_packet_accepted =
        SendFixtureDatagram(
            network.get(), 63, boundary_packet, RoqMediaType::kVideo,
            RoqDatagramPriority::kVideoDelta) == RoqSendResult::kQueued;
    boundary_packet.push_back(0);
    result.oversize_packet_rejected =
        SendFixtureDatagram(
            network.get(), 63, boundary_packet, RoqMediaType::kVideo,
            RoqDatagramPriority::kVideoDelta) == RoqSendResult::kOversize;
    event_log.Write("datagram_boundary_probe", "accepted",
                    result.boundary_packet_accepted);
    event_log.Write("datagram_boundary_probe", "oversize_rejected",
                    result.oversize_packet_rejected);
  }

  if (options.role == PeerRole::kClient) {
    media_run_start = std::chrono::steady_clock::now();
    reporter = std::thread([&] {
      size_t reports = 0;
      while (!reporter_stop.load()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        if (reporter_stop.load())
          break;
        const RoqMediaSessionStats sent_stats = send->stats();
        event_log.WriteMediaControl(sent_stats);
        if (options.no_render || ++reports % 10 != 0)
          continue;
        const RoqMediaSessionStats received_stats =
            receive ? receive->stats() : RoqMediaSessionStats{};
        const RoqSessionStats transport_stats = network->stats();
        std::cerr << "roq loop=" << loop_counter.load()
                  << " encoded_video=" << sent_stats.video_frames_encoded
                  << " decoded_video=" << received_stats.decoded_video_frames
                  << " audio_samples=" << received_stats.received_audio_samples
                  << " rtt_ms=" << transport_stats.rtt_ns / 1000000.0
                  << " drops="
                  << transport_stats.queue_full_drops +
                         transport_stats.oversize_drops
                  << '\n';
      }
    });
    std::thread mtu_shrinker;
    if (options.shrink_datagram_after_ms > 0 &&
        options.shrink_datagram_to > 1) {
      mtu_shrinker = std::thread([&] {
        std::this_thread::sleep_for(
            std::chrono::milliseconds(options.shrink_datagram_after_ms));
        network->UpdatePeerDatagramPayloadLimit(options.shrink_datagram_to);
        event_log.Write("datagram_limit_changed", "pmtu",
                        options.shrink_datagram_to);
      });
    }
    SendingFileCapturerObserver source(send.get(), &result, &event_log,
                                       &loop_counter, options.duration_seconds,
                                       network.get());
    FileMediaCapturerCreateResult created = CreateFileMediaCapturer(
        {.input_path = options.input_path, .eof_mode = FileMediaEofMode::kLoop},
        &source);
    if (!created.ok()) {
      result.error = created.error.message;
      return result;
    }
    source.set_capturer(created.capturer.get());
    FileMediaError source_start = created.capturer->Start();
    if (!source_start.ok()) {
      result.error = source_start.message;
      return result;
    }
    result.loops_started = 1;
    loop_counter.store(1);
    event_log.Write("loop_started", "media", 1);
    send->GenerateSenderKeyFrame();
    source.Wait();
    const double media_elapsed_seconds =
        std::max(0.001, std::chrono::duration<double>(
                            std::chrono::steady_clock::now() - media_run_start)
                            .count());
    media_complete.store(true);
    if (capacity_loader.joinable())
      capacity_loader.join();
    if (mtu_shrinker.joinable())
      mtu_shrinker.join();
    if (!source.error().empty()) {
      reporter_stop.store(true);
      if (reporter.joinable())
        reporter.join();
      result.error = source.error();
      return result;
    }
    const RoqMediaSessionStats active_send_stats = send->stats();
    const auto drain_deadline =
        std::chrono::steady_clock::now() + std::chrono::seconds(2);
    while (std::chrono::steady_clock::now() < drain_deadline)
      std::this_thread::sleep_for(std::chrono::milliseconds(10));
    send_stats = send->stats();
    result.sent_audio_packets = send_stats.sent_audio_packets;
    result.audio_target_bitrate_bps = send_stats.audio_target_bitrate_bps;
    result.video_target_bitrate_bps = send_stats.video_target_bitrate_bps;
    result.video_media_bitrate_bps = send_stats.video_media_bitrate_bps;
    result.video_frames_encoded = send_stats.video_frames_encoded;
    result.video_frames_dropped = send_stats.video_frames_dropped;
    result.sent_video_rtp_packets = state.sent_video_rtp_packets.load();
    result.sent_video_rtp_bytes = state.sent_video_rtp_bytes.load();
    result.sent_audio_rtp_bytes = state.sent_audio_rtp_bytes.load();
    result.video_media_bitrate_bps =
        std::max({result.video_media_bitrate_bps,
                  active_send_stats.video_media_bitrate_bps,
                  static_cast<int>(result.sent_video_rtp_bytes * 8 /
                                   media_elapsed_seconds)});
    reporter_stop.store(true);
    if (reporter.joinable())
      reporter.join();
    network->Close();
  }
  while (!network->WaitUntilClosed(TimeDelta::Millis(100))) {
    if (g_stop_requested.load()) {
      network->Close();
      if (!network->WaitUntilClosed(TimeDelta::Seconds(2)))
        break;
    }
  }
  media_complete.store(true);
  if (capacity_loader.joinable())
    capacity_loader.join();
  if (keyframe_requester.joinable())
    keyframe_requester.join();
  const RoqMediaSessionStats receive_stats =
      receive ? receive->stats() : RoqMediaSessionStats{};
  result.received_audio_packets = receive_stats.received_audio_packets;
  result.decoded_video_frames = receive_stats.decoded_video_frames;
  result.audio_decoding_normal = receive_stats.audio_decoding_normal;
  result.audio_fec_packets_received = receive_stats.audio_fec_packets_received;
  result.audio_fec_packets_discarded =
      receive_stats.audio_fec_packets_discarded;
  result.audio_secondary_decoded_rate =
      receive_stats.audio_secondary_decoded_rate;
  result.first_decoded_frame_monotonic_us =
      counting_video_sink.first_frame_monotonic_us();
  result.decoded_audio_samples_per_channel =
      receive_stats.received_audio_samples;
  if (counting_video_sink.frames() > 0 && state.video_origin_set.load()) {
    result.decoded_video_timestamp_span = counting_video_sink.last_timestamp() -
                                          state.video_timestamp_origin.load();
  }
  if (receive_stats.received_audio_packets > 0 &&
      state.audio_origin_set.load() && state.audio_last_set.load()) {
    result.decoded_audio_timestamp_span =
        state.audio_last_timestamp.load() - state.audio_timestamp_origin.load();
  }
  result.reflected_audio_packets = state.reflected_audio.load();
  result.reflected_video_packets = state.reflected_video.load();
  result.received_rtcp_packets = state.received_rtcp.load();
  result.idr_access_units = state.idr_access_units.load();
  result.dropped_idr_access_units = state.dropped_idr_access_units.load();
  result.recovery_idr_access_units = state.recovery_idr_access_units.load();
  result.pli_packets = state.pli_packets.load();
  result.fir_packets = state.fir_packets.load();
  result.sender_reports = state.sender_reports.load();
  result.receiver_reports = state.receiver_reports.load();
  result.idr_recovery_ms = state.idr_recovery_ms.load();
  {
    std::lock_guard lock(state.mutex);
    state.send = nullptr;
    state.receive = nullptr;
  }
  if (send)
    send->Stop();
  if (receive)
    receive->Stop();
  if (send_audio_device)
    send_audio_device->Terminate();
  audio_device->Terminate();
  const RoqSessionStats network_stats = network->stats();
  result.queue_full_drops = network_stats.queue_full_drops;
  result.oversize_drops = network_stats.oversize_drops;
  result.pmtu_purge_drops = network_stats.pmtu_purge_drops;
  result.expired_drops = network_stats.expired_drops;
  result.invalid_drops = network_stats.invalid_drops;
  result.quiche_slot_expired_drops = network_stats.quiche_slot_expired_drops;
  result.peak_queue_size = network_stats.peak_queue_size;
  result.max_queue_delay_us = network_stats.max_queue_delay_us;
  result.dropped_periodic_rtcp = network_stats.dropped_periodic_rtcp;
  result.dropped_video_delta = network_stats.dropped_video_delta;
  result.dropped_video_key = network_stats.dropped_video_key;
  result.dropped_audio = network_stats.dropped_audio;
  result.dropped_critical_rtcp = network_stats.dropped_critical_rtcp;
  result.max_datagram_payload = network_stats.max_datagram_payload;
  result.clean_shutdown = network_stats.clean_shutdown && result.error.empty();
  result.rtt_ns = network_stats.rtt_ns;
  result.max_rtt_ns = network_stats.max_rtt_ns;
  result.delivery_rate_bytes_per_second =
      network_stats.delivery_rate_bytes_per_second;
  result.max_bandwidth_bytes_per_second =
      network_stats.max_bandwidth_bytes_per_second;
  result.pmtu_bytes = network_stats.pmtu_bytes;
  result.dgram_lost = network_stats.dgram_lost;
  result.sent_udp_packets = network_stats.sent_udp_packets;
  result.received_udp_packets = network_stats.received_udp_packets;
  result.max_pacing_delay_ns = network_stats.max_pacing_delay_ns;
  result.resumption_cache_hit = network_stats.resumption_cache_hit;
  result.session_resumed = network_stats.session_resumed;
  result.early_data_attempted = network_stats.early_data_attempted;
  result.early_data_accepted = network_stats.early_data_accepted;
  result.early_data_fallback = network_stats.early_data_fallback;
  result.early_data_reason = network_stats.early_data_reason;
  result.early_datagrams_received = network_stats.early_datagrams_received;
  result.early_datagrams_sent = network_stats.early_datagrams_sent;
  result.early_reliable_messages_received =
      network_stats.early_reliable_messages_received;
  if (test_server) {
    const TestRoqServerStats stats = test_server->test_stats();
    result.zero_rtt_packets_received = stats.zero_rtt_packets_received;
    result.provisional_datagrams_received =
        stats.provisional_datagrams_received;
    result.provisional_datagrams_committed =
        stats.provisional_datagrams_committed;
    result.provisional_datagrams_dropped = stats.provisional_datagrams_dropped;
    result.provisional_datagrams_deduplicated =
        stats.provisional_datagrams_deduplicated;
    result.peer_address_validated = stats.peer_address_validated;
  }
  result.unknown_flow_buffered = network_stats.unknown_flow_buffered;
  result.unknown_flow_released = network_stats.unknown_flow_released;
  result.unknown_flow_dropped = network_stats.unknown_flow_dropped;
  result.early_rejection_queue_drops =
      network_stats.early_rejection_queue_drops;
  result.early_data_recovery_keyframes =
      send_stats.early_data_recovery_keyframes;
  result.sent_rtx_packets = state.sent_rtx_packets.load();
  result.received_rtx_packets = state.received_rtx_packets.load();
  result.nack_packets = state.nack_packets.load();
  result.retransmitted_packets_sent = send_stats.retransmitted_packets_sent;
  result.retransmitted_bytes_sent = send_stats.retransmitted_bytes_sent;
  result.opus_inband_fec = send_stats.opus_inband_fec;
  result.injected_video_packet_drops = state.injected_video_packet_drops.load();
  result.injected_audio_packet_drops = state.injected_audio_packet_drops.load();
  event_log.Write("session_summary", "decoded_video_frames",
                  result.decoded_video_frames);
  return result;
}

void RequestMediaPeerStop() {
  g_stop_requested.store(true);
}

}  // namespace webrtc::roq
