/*
 *  Copyright (c) 2026 The WebRTC project authors. All Rights Reserved.
 */

#ifndef API_ROQ_ROQ_MEDIA_SESSION_H_
#define API_ROQ_ROQ_MEDIA_SESSION_H_

#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
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

enum class RoqKeyFrameRequestMethod { kPli, kFir };

struct RoqMediaSessionConfig {
  bool send_audio = false;
  bool send_video = false;
  bool receive_audio = false;
  bool receive_video = false;
  uint32_t local_audio_ssrc = 0;
  uint32_t local_video_ssrc = 0;
  uint32_t local_video_rtx_ssrc = 0;
  uint32_t remote_audio_ssrc = 0;
  uint32_t remote_video_ssrc = 0;
  uint32_t remote_video_rtx_ssrc = 0;
  int audio_payload_type = 111;
  int video_payload_type = 96;
  int video_rtx_payload_type = 97;
  int audio_bitrate_bps = 0;
  int video_min_bitrate_bps = 0;
  int video_start_bitrate_bps = 0;
  int video_max_bitrate_bps = 0;
  size_t max_rtp_packet_size = 1200;
  RoqKeyFrameRequestMethod keyframe_request_method =
      RoqKeyFrameRequestMethod::kPli;
  std::string sync_group = "roq-media";
  Transport* audio_transport = nullptr;
  Transport* video_transport = nullptr;
  scoped_refptr<AudioDeviceModule> audio_device_module;
  scoped_refptr<AudioEncoderFactory> audio_encoder_factory;
  scoped_refptr<AudioDecoderFactory> audio_decoder_factory;
  VideoEncoderFactory* video_encoder_factory = nullptr;
  VideoDecoderFactory* video_decoder_factory = nullptr;
  AudioSinkInterface* audio_sink = nullptr;
  VideoSinkInterface<VideoFrame>* video_sink = nullptr;
  std::function<RoqSessionStats()> network_stats_provider;
  QuicNetworkControllerFactory::UpdateObserver network_control_observer;
};

struct RoqMediaSessionStats {
  int64_t sent_audio_packets = 0;
  int64_t sent_video_frames = 0;
  int audio_target_bitrate_bps = 0;
  int video_target_bitrate_bps = 0;
  int video_media_bitrate_bps = 0;
  int call_target_rate_bps = 0;
  int pacer_rate_bps = 0;
  int padding_rate_bps = 0;
  uint32_t video_frames_encoded = 0;
  uint32_t video_frames_dropped = 0;
  uint32_t decoded_video_frames = 0;
  uint32_t rendered_video_frames = 0;
  uint32_t key_frames_decoded = 0;
  uint32_t received_audio_packets = 0;
  uint64_t received_audio_samples = 0;
  uint64_t audio_fec_packets_received = 0;
  uint64_t audio_fec_packets_discarded = 0;
  float audio_secondary_decoded_rate = 0.0f;
  int audio_decoding_normal = 0;
  uint32_t pli_sent = 0;
  uint32_t fir_sent = 0;
  uint32_t nack_sent = 0;
  uint32_t nack_received = 0;
  uint64_t retransmitted_packets_sent = 0;
  uint64_t retransmitted_bytes_sent = 0;
  bool opus_inband_fec = false;
  uint32_t early_data_recovery_keyframes = 0;
};

class RoqMediaSession {
 public:
  static std::unique_ptr<RoqMediaSession> Create(RoqMediaSessionConfig config,
                                                 const Environment& env,
                                                 std::string* error);

  virtual ~RoqMediaSession() = default;
  virtual bool Start(std::string* error) = 0;
  virtual void Stop() = 0;
  virtual void SendVideoFrame(scoped_refptr<I420BufferInterface> buffer,
                              int64_t pts_us) = 0;
  virtual void SendAudioBlock(std::span<const int16_t> samples,
                              uint32_t rtp_timestamp) = 0;
  virtual void DeliverRtp(bool audio, std::span<const uint8_t> packet) = 0;
  virtual void DeliverRtcp(std::span<const uint8_t> packet) = 0;
  virtual void NotifyPacketAccepted(const PacketOptions& options,
                                    size_t packet_size) = 0;
  virtual void GenerateSenderKeyFrame() = 0;
  virtual void RequestReceiverKeyFrame() = 0;
  virtual void UpdateMaxRtpPacketSize(size_t max_rtp_packet_size) = 0;
  virtual RoqMediaSessionStats stats() = 0;
};

}  // namespace webrtc::roq

#endif  // API_ROQ_ROQ_MEDIA_SESSION_H_
