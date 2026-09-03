/*
 *  Copyright (c) 2026 The WebRTC project authors. All Rights Reserved.
 */

#ifndef RTC_TOOLS_ROQ_MEDIA_PEER_H_
#define RTC_TOOLS_ROQ_MEDIA_PEER_H_

#include <optional>
#include <string>

#include "rtc_tools/roq/roq_peer.h"

namespace webrtc::roq {

struct MediaPeerOptions {
  PeerRole role = PeerRole::kClient;
  std::string scenario = "media-loopback";
  std::string address;
  std::string manifest_path;
  std::string executable_path;
  std::string input_path;
  std::string event_log_path;
  std::string run_id;
  std::string cache_directory;
  uint8_t context_byte = 1;
  bool replay_first_provisional = false;
  bool drop_all_provisional = false;
  std::optional<double> duration_seconds;
  int audio_bitrate_bps = 128000;
  int video_bitrate_bps = 4000000;
  bool no_render = false;
  bool drop_first_idr = false;
  std::string keyframe_request = "pli";
  int request_keyframe_after_ms = 0;
  int request_keyframe_interval_ms = 0;
  int shrink_datagram_after_ms = 0;
  size_t shrink_datagram_to = 0;
  bool probe_datagram_boundary = false;
  size_t drop_video_packet_after = 0;
  size_t drop_audio_every = 0;
  uint64_t capacity_load_bps = 0;
};

struct MediaPeerResult {
  size_t sent_audio_packets = 0;
  size_t sent_video_frames = 0;
  size_t sent_video_rtp_packets = 0;
  size_t sent_video_rtp_bytes = 0;
  size_t sent_audio_rtp_bytes = 0;
  int audio_target_bitrate_bps = 0;
  int video_target_bitrate_bps = 0;
  int video_media_bitrate_bps = 0;
  size_t video_frames_encoded = 0;
  size_t video_frames_dropped = 0;
  size_t received_audio_packets = 0;
  size_t decoded_video_frames = 0;
  size_t decoded_audio_samples_per_channel = 0;
  uint32_t decoded_video_timestamp_span = 0;
  uint32_t decoded_audio_timestamp_span = 0;
  int audio_decoding_normal = 0;
  size_t reflected_audio_packets = 0;
  size_t reflected_video_packets = 0;
  size_t received_rtcp_packets = 0;
  size_t queue_full_drops = 0;
  size_t oversize_drops = 0;
  size_t pmtu_purge_drops = 0;
  size_t expired_drops = 0;
  size_t invalid_drops = 0;
  size_t quiche_slot_expired_drops = 0;
  size_t peak_queue_size = 0;
  int64_t max_queue_delay_us = 0;
  size_t dropped_periodic_rtcp = 0;
  size_t dropped_video_delta = 0;
  size_t dropped_video_key = 0;
  size_t dropped_audio = 0;
  size_t dropped_critical_rtcp = 0;
  size_t max_datagram_payload = 0;
  size_t loops_started = 0;
  size_t idr_access_units = 0;
  size_t dropped_idr_access_units = 0;
  size_t recovery_idr_access_units = 0;
  size_t pli_packets = 0;
  size_t fir_packets = 0;
  size_t sender_reports = 0;
  size_t receiver_reports = 0;
  int64_t idr_recovery_ms = -1;
  bool boundary_packet_accepted = false;
  bool oversize_packet_rejected = false;
  bool early_boundary_packet_accepted = false;
  bool early_oversize_packet_rejected = false;
  bool clean_shutdown = false;
  uint64_t rtt_ns = 0;
  uint64_t max_rtt_ns = 0;
  uint64_t delivery_rate_bytes_per_second = 0;
  uint64_t max_bandwidth_bytes_per_second = 0;
  size_t pmtu_bytes = 0;
  size_t dgram_lost = 0;
  size_t sent_udp_packets = 0;
  size_t received_udp_packets = 0;
  uint64_t max_pacing_delay_ns = 0;
  size_t sent_rtx_packets = 0;
  size_t received_rtx_packets = 0;
  size_t nack_packets = 0;
  uint64_t retransmitted_packets_sent = 0;
  uint64_t retransmitted_bytes_sent = 0;
  bool opus_inband_fec = false;
  size_t injected_video_packet_drops = 0;
  size_t injected_audio_packet_drops = 0;
  uint64_t audio_fec_packets_received = 0;
  uint64_t audio_fec_packets_discarded = 0;
  float audio_secondary_decoded_rate = 0.0f;
  bool resumption_cache_hit = false;
  bool session_resumed = false;
  bool early_data_attempted = false;
  bool early_data_accepted = false;
  bool early_data_fallback = false;
  uint32_t early_data_reason = 0;
  size_t zero_rtt_packets_received = 0;
  size_t provisional_datagrams_received = 0;
  size_t provisional_datagrams_committed = 0;
  size_t provisional_datagrams_dropped = 0;
  size_t provisional_datagrams_deduplicated = 0;
  size_t early_datagrams_received = 0;
  size_t early_datagrams_sent = 0;
  size_t early_reliable_messages_received = 0;
  bool peer_address_validated = false;
  size_t unknown_flow_buffered = 0;
  size_t unknown_flow_released = 0;
  size_t unknown_flow_dropped = 0;
  size_t early_rejection_queue_drops = 0;
  uint32_t early_data_recovery_keyframes = 0;
  int64_t first_decoded_frame_monotonic_us = -1;
  std::string error;
};

MediaPeerResult RunMediaPeer(const MediaPeerOptions& options);
MediaPeerResult RunMediaPeerWithPlatformEventLoop(
    const MediaPeerOptions& options);
void RequestMediaPeerStop();

}  // namespace webrtc::roq

#endif  // RTC_TOOLS_ROQ_MEDIA_PEER_H_
