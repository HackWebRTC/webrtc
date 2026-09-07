/*
 *  Copyright (c) 2026 The WebRTC project authors. All Rights Reserved.
 */

#ifndef API_ROQ_ROQ_MEDIA_ENGINE_H_
#define API_ROQ_ROQ_MEDIA_ENGINE_H_

#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <span>
#include <string>

#include "api/audio/audio_device.h"
#include "api/audio_codecs/audio_decoder_factory.h"
#include "api/audio_codecs/audio_encoder_factory.h"
#include "api/call/audio_sink.h"
#include "api/call/transport.h"
#include "api/environment/environment.h"
#include "api/roq/quic_network_controller.h"
#include "api/roq/roq_session.h"
#include "api/scoped_refptr.h"
#include "api/video/video_frame.h"
#include "api/video/video_frame_buffer.h"
#include "api/video/video_sink_interface.h"
#include "api/video_codecs/video_decoder_factory.h"
#include "api/video_codecs/video_encoder_factory.h"

namespace webrtc::roq {

enum class RoqMediaKind { kAudio, kVideo };
enum class RoqAudioInputMode { kExternalPcm, kDevice };
enum class RoqKeyFrameRequestMethod { kPli, kFir };

struct RoqMediaEngineConfig {
  scoped_refptr<AudioDeviceModule> audio_device_module;
  scoped_refptr<AudioEncoderFactory> audio_encoder_factory;
  scoped_refptr<AudioDecoderFactory> audio_decoder_factory;
  VideoEncoderFactory* video_encoder_factory = nullptr;
  VideoDecoderFactory* video_decoder_factory = nullptr;
  std::function<RoqSessionStats()> network_stats_provider;
  QuicNetworkControllerFactory::UpdateObserver network_control_observer;
  bool use_audio_device_playout = false;
  bool use_audio_device_recording = false;
  size_t max_rtp_packet_size = 1200;
  int send_min_bitrate_bps = 1;
  int send_start_bitrate_bps = 1;
  int send_max_bitrate_bps = -1;
};

struct RoqMediaBindingConfig {
  std::string binding_id;
  std::string sync_group;
  RoqMediaKind kind = RoqMediaKind::kAudio;
  uint32_t primary_ssrc = 0;
  uint32_t rtx_ssrc = 0;
  int payload_type = 0;
  int rtx_payload_type = 0;
  Transport* transport = nullptr;
  AudioSinkInterface* audio_sink = nullptr;
  VideoSinkInterface<VideoFrame>* video_sink = nullptr;
  RoqAudioInputMode audio_input_mode = RoqAudioInputMode::kExternalPcm;
  RoqKeyFrameRequestMethod keyframe_request_method =
      RoqKeyFrameRequestMethod::kPli;
  bool enable_nack = true;
  int audio_bitrate_bps = 0;
  int video_min_bitrate_bps = 0;
  int video_start_bitrate_bps = 0;
  int video_max_bitrate_bps = 0;
};

struct RoqMediaEngineStats {
  int64_t sent_audio_packets = 0;
  int64_t sent_video_frames = 0;
  int audio_target_bitrate_bps = 0;
  int video_target_bitrate_bps = 0;
  int video_media_bitrate_bps = 0;
  uint32_t video_frames_encoded = 0;
  uint32_t video_frames_dropped = 0;
  uint32_t received_audio_packets = 0;
  uint64_t received_audio_samples = 0;
  uint64_t audio_fec_packets_received = 0;
  uint64_t audio_fec_packets_discarded = 0;
  float audio_secondary_decoded_rate = 0.0f;
  int audio_decoding_normal = 0;
  uint32_t decoded_video_frames = 0;
  uint32_t pli_sent = 0;
  uint32_t fir_sent = 0;
  uint32_t nack_sent = 0;
  uint32_t nack_received = 0;
  uint64_t retransmitted_packets_sent = 0;
  uint64_t retransmitted_bytes_sent = 0;
  uint64_t network_control_updates = 0;
  int64_t call_target_rate_bps = 0;
  int64_t controller_target_rate_bps = 0;
  int64_t controller_pacer_rate_bps = 0;
  int64_t controller_padding_rate_bps = 0;
  size_t controller_probe_cluster_count = 0;
  bool controller_has_congestion_window = false;
  bool audio_processing_active = false;
  bool echo_cancellation_enabled = false;
  bool noise_suppression_enabled = false;
  bool automatic_gain_control_enabled = false;
  bool audio_send_nack_enabled = false;
  bool audio_receive_nack_enabled = false;
  bool opus_inband_fec = false;
};

struct RoqMediaBindingStats {
  int64_t sent_audio_packets = 0;
  int64_t sent_video_frames = 0;
  uint32_t received_audio_packets = 0;
  uint32_t decoded_video_frames = 0;
  uint32_t pli_sent = 0;
  uint32_t fir_sent = 0;
  uint32_t nack_sent = 0;
  uint32_t nack_received = 0;
  uint32_t nack_requests_received = 0;
  uint64_t retransmitted_packets_sent = 0;
};

class RoqMediaEngine {
 public:
  static std::unique_ptr<RoqMediaEngine> Create(RoqMediaEngineConfig config,
                                                const Environment& env,
                                                std::string* error);

  virtual ~RoqMediaEngine() = default;
  virtual bool Start(std::string* error) = 0;
  virtual void Stop() = 0;
  virtual bool AddReceiveBinding(RoqMediaBindingConfig config,
                                 std::string* error) = 0;
  virtual bool AddSendBinding(RoqMediaBindingConfig config,
                              std::string* error) = 0;
  virtual bool RemoveBinding(const std::string& binding_id) = 0;
  virtual void UpdateSendBitrateConstraints(int min_bitrate_bps,
                                            int start_bitrate_bps,
                                            int max_bitrate_bps) = 0;
  virtual void UpdateMaxRtpPacketSize(size_t max_rtp_packet_size) = 0;
  virtual void SendVideoFrame(const std::string& binding_id,
                              scoped_refptr<I420BufferInterface> buffer,
                              int64_t pts_us) = 0;
  virtual void SendAudioBlock(const std::string& binding_id,
                              std::span<const int16_t> samples,
                              uint32_t rtp_timestamp) = 0;
  virtual void DeliverRtp(const std::string& binding_id,
                          std::span<const uint8_t> packet) = 0;
  virtual void DeliverRtcp(const std::string& binding_id,
                           std::span<const uint8_t> packet) = 0;
  virtual void GenerateSenderKeyFrame(const std::string& binding_id) = 0;
  virtual void RequestReceiverKeyFrame(const std::string& binding_id) = 0;
  virtual void PullAudio(const std::string& binding_id) = 0;
  virtual void NotifyPacketAccepted(const PacketOptions& options,
                                    size_t packet_size) = 0;
  virtual RoqMediaEngineStats stats() = 0;
  virtual std::optional<RoqMediaBindingStats> binding_stats(
      const std::string& binding_id) = 0;
  virtual bool HasBinding(const std::string& binding_id) const = 0;
  virtual size_t binding_count() const = 0;
};

}  // namespace webrtc::roq

#endif  // API_ROQ_ROQ_MEDIA_ENGINE_H_
