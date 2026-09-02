/*
 *  Copyright (c) 2026 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS. All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <cerrno>
#include <cstdlib>
#include <iostream>
#include <optional>
#include <span>
#include <string>
#include <string_view>

#include "rtc_tools/roq/capability.h"
#include "rtc_tools/roq/manifest.h"
#include "rtc_tools/roq/media_peer.h"
#include "rtc_tools/roq/media_source.h"
#include "rtc_tools/roq/network_lab.h"
#include "rtc_tools/roq/quiche_abi.h"
#include "rtc_tools/roq/roq_peer.h"

namespace {

void PrintUsage(std::ostream& output) {
  output << "Usage: roq_test_peer <command> [options]\n"
            "Commands:\n"
            "  capability\n"
            "  quiche-abi\n"
            "  client\n"
            "  server\n"
            "  media-source\n"
            "  manifest\n"
            "  udp-echo\n"
            "  udp-relay\n";
}

bool HasOption(std::span<char*> args, std::string_view option) {
  for (size_t index = 2; index < args.size(); ++index) {
    if (args[index] == option) {
      return true;
    }
  }
  return false;
}

std::optional<std::string_view> OptionValue(std::span<char*> args,
                                            std::string_view option) {
  for (size_t index = 2; index + 1 < args.size(); ++index) {
    if (args[index] == option) {
      return args[index + 1];
    }
  }
  return std::nullopt;
}

std::optional<uint32_t> UIntOption(std::span<char*> args,
                                   std::string_view option) {
  const std::optional<std::string_view> value = OptionValue(args, option);
  if (!value) {
    return std::nullopt;
  }
  const std::string text(*value);
  char* end = nullptr;
  errno = 0;
  const unsigned long parsed = std::strtoul(text.c_str(), &end, 10);
  if (errno != 0 || end == text.c_str() || *end != '\0' ||
      parsed > UINT32_MAX) {
    return std::nullopt;
  }
  return static_cast<uint32_t>(parsed);
}

std::optional<double> DoubleOption(std::span<char*> args,
                                   std::string_view option) {
  const std::optional<std::string_view> value = OptionValue(args, option);
  if (!value) {
    return std::nullopt;
  }
  const std::string text(*value);
  char* end = nullptr;
  errno = 0;
  const double parsed = std::strtod(text.c_str(), &end);
  if (errno != 0 || end == text.c_str() || *end != '\0') {
    return std::nullopt;
  }
  return parsed;
}

}  // namespace

int main(int argc, char* argv[]) {
  std::span<char*> args(argv, static_cast<size_t>(argc));
  if (args.size() == 1 || std::string_view(args[1]) == "--help" ||
      std::string_view(args[1]) == "-h") {
    PrintUsage(std::cout);
    return argc == 1 ? 2 : 0;
  }

  const std::string_view command = args[1];
  if (command == "capability") {
    const webrtc::roq::CapabilityResult result =
        webrtc::roq::RunCapabilityChecks(args[0]);
    if (!result.error.empty()) {
      std::cerr << result.error << '\n';
      return 1;
    }
    std::cout << "{\"schema_version\":1,\"event\":\"capability\","
              << "\"arch\":\"arm64\",\"quic_tls\":\"ok\","
              << "\"alpn\":\"" << result.alpn
              << "\",\"congestion_control\":\"bbr2_gcongestion\","
              << "\"ffmpeg_mov_demuxer\":\"ok\","
              << "\"ffmpeg_h264_decoder\":\"ok\","
              << "\"ffmpeg_aac_decoder\":\"ok\","
              << "\"openh264_encode\":\"ok\",\"encoded_bytes\":"
              << result.encoded_bytes << "}\n";
    return 0;
  }
  if (command == "quiche-abi") {
    const webrtc::roq::QuicheAbiResult result =
        webrtc::roq::RunQuicheAbiChecks(args[0]);
    if (!result.error.empty()) {
      std::cerr << result.error << '\n';
      return 1;
    }
    std::cout << "{\"schema_version\":1,\"event\":\"quiche_abi\","
              << "\"max_pacing_delay_ns\":" << result.max_pacing_delay_ns << ','
              << "\"rtt_ns\":" << result.rtt_ns << ','
              << "\"min_rtt_ns\":" << result.min_rtt_ns << ','
              << "\"max_rtt_ns\":" << result.max_rtt_ns << ','
              << "\"delivery_rate_bytes_per_second\":"
              << result.delivery_rate_bytes_per_second << ','
              << "\"max_bandwidth_bytes_per_second\":"
              << result.max_bandwidth_bytes_per_second << ','
              << "\"pmtu_bytes\":" << result.pmtu_bytes << ','
              << "\"dgram_lost\":" << result.dgram_lost << ','
              << "\"early_data_accepted_reason\":"
              << result.early_data_accepted_reason << ','
              << "\"early_data_rejected_reason\":"
              << result.early_data_rejected_reason << ','
              << "\"rejection_fell_back_to_1rtt\":"
              << (result.rejection_fell_back_to_1rtt ? "true" : "false")
              << "}\n";
    return 0;
  }
  if (command == "client" || command == "server") {
    const std::optional<std::string_view> scenario =
        OptionValue(args, "--scenario");
    const std::optional<std::string_view> address =
        OptionValue(args, command == "client" ? "--connect" : "--listen");
    const std::optional<std::string_view> manifest =
        OptionValue(args, "--manifest");
    const std::optional<double> duration = DoubleOption(args, "--duration");
    const std::optional<std::string_view> event_log =
        OptionValue(args, "--event-log");
    const std::optional<std::string_view> run_id =
        OptionValue(args, "--run-id");
    const std::optional<std::string_view> inject_error =
        OptionValue(args, "--inject-error");
    if (scenario &&
        (*scenario == "media-loopback" || *scenario == "media-send" ||
         *scenario == "media-receive")) {
      const std::optional<std::string_view> input =
          OptionValue(args, "--input");
      const std::optional<std::string_view> keyframe_request =
          OptionValue(args, "--keyframe-request");
      const std::optional<uint32_t> request_keyframe_after_ms =
          UIntOption(args, "--request-keyframe-after-ms");
      const std::optional<uint32_t> request_keyframe_interval_ms =
          UIntOption(args, "--request-keyframe-interval-ms");
      const std::optional<uint32_t> shrink_datagram_after_ms =
          UIntOption(args, "--shrink-datagram-after-ms");
      const std::optional<uint32_t> shrink_datagram_to =
          UIntOption(args, "--shrink-datagram-to");
      const std::optional<uint32_t> drop_video_packet_after =
          UIntOption(args, "--drop-video-packet-after");
      const std::optional<uint32_t> drop_audio_every =
          UIntOption(args, "--drop-audio-every");
      if (!address || !manifest || !event_log || !run_id ||
          (command == "client" && !input) ||
          (command == "client" && *scenario == "media-receive") ||
          (command == "server" && *scenario == "media-send") ||
          (duration && *duration <= 0)) {
        std::cerr << command << " media-loopback requires --"
                  << (command == "client" ? "connect" : "listen")
                  << " HOST:PORT --manifest <path> --event-log <path> "
                     "--run-id <id>"
                  << (command == "client" ? " --input <path>" : "")
                  << " [--duration <seconds>] [--no-render]\n";
        return 2;
      }
      const webrtc::roq::MediaPeerOptions media_options = {
          .role = command == "client" ? webrtc::roq::PeerRole::kClient
                                      : webrtc::roq::PeerRole::kServer,
          .scenario = std::string(*scenario),
          .address = std::string(*address),
          .manifest_path = std::string(*manifest),
          .executable_path = args[0],
          .input_path = input ? std::string(*input) : "",
          .event_log_path = std::string(*event_log),
          .run_id = std::string(*run_id),
          .duration_seconds = duration,
          .no_render = HasOption(args, "--no-render"),
          .drop_first_idr = HasOption(args, "--drop-first-idr"),
          .keyframe_request =
              keyframe_request ? std::string(*keyframe_request) : "pli",
          .request_keyframe_after_ms =
              static_cast<int>(request_keyframe_after_ms.value_or(0)),
          .request_keyframe_interval_ms =
              static_cast<int>(request_keyframe_interval_ms.value_or(0)),
          .shrink_datagram_after_ms =
              static_cast<int>(shrink_datagram_after_ms.value_or(0)),
          .shrink_datagram_to = shrink_datagram_to.value_or(0),
          .probe_datagram_boundary =
              HasOption(args, "--probe-datagram-boundary"),
          .drop_video_packet_after = drop_video_packet_after.value_or(0),
          .drop_audio_every = drop_audio_every.value_or(0),
          .capacity_load_bps = static_cast<uint64_t>(
              UIntOption(args, "--capacity-load-bps").value_or(0)),
      };
      const webrtc::roq::MediaPeerResult result =
          command == "client" && *scenario == "media-loopback" &&
                  !media_options.no_render
              ? webrtc::roq::RunMediaPeerWithPlatformEventLoop(media_options)
              : webrtc::roq::RunMediaPeer(media_options);
      if (!result.error.empty()) {
        std::cerr << result.error << '\n';
        return 1;
      }
      std::cout
          << "{\"schema_version\":1,"
          << "\"event\":\"media_peer_summary\","
          << "\"scenario\":\"" << *scenario << "\","
          << "\"sent_audio_packets\":" << result.sent_audio_packets << ','
          << "\"sent_video_frames\":" << result.sent_video_frames << ','
          << "\"sent_video_rtp_packets\":" << result.sent_video_rtp_packets
          << ',' << "\"sent_video_rtp_bytes\":" << result.sent_video_rtp_bytes
          << ',' << "\"sent_audio_rtp_bytes\":" << result.sent_audio_rtp_bytes
          << ','
          << "\"audio_target_bitrate_bps\":" << result.audio_target_bitrate_bps
          << ','
          << "\"video_target_bitrate_bps\":" << result.video_target_bitrate_bps
          << ','
          << "\"video_media_bitrate_bps\":" << result.video_media_bitrate_bps
          << ',' << "\"video_frames_encoded\":" << result.video_frames_encoded
          << ',' << "\"video_frames_dropped\":" << result.video_frames_dropped
          << ','
          << "\"received_audio_packets\":" << result.received_audio_packets
          << ',' << "\"decoded_video_frames\":" << result.decoded_video_frames
          << ',' << "\"decoded_audio_samples_per_channel\":"
          << result.decoded_audio_samples_per_channel << ','
          << "\"decoded_video_timestamp_span\":"
          << result.decoded_video_timestamp_span << ','
          << "\"decoded_audio_timestamp_span\":"
          << result.decoded_audio_timestamp_span << ','
          << "\"audio_decoding_normal\":" << result.audio_decoding_normal << ','
          << "\"reflected_audio_packets\":" << result.reflected_audio_packets
          << ','
          << "\"reflected_video_packets\":" << result.reflected_video_packets
          << ',' << "\"received_rtcp_packets\":" << result.received_rtcp_packets
          << ',' << "\"queue_full_drops\":" << result.queue_full_drops << ','
          << "\"oversize_drops\":" << result.oversize_drops << ','
          << "\"pmtu_purge_drops\":" << result.pmtu_purge_drops << ','
          << "\"expired_drops\":" << result.expired_drops << ','
          << "\"invalid_drops\":" << result.invalid_drops << ','
          << "\"quiche_slot_expired_drops\":"
          << result.quiche_slot_expired_drops << ','
          << "\"peak_queue_size\":" << result.peak_queue_size << ','
          << "\"max_queue_delay_us\":" << result.max_queue_delay_us << ','
          << "\"dropped_background\":" << result.dropped_background << ','
          << "\"dropped_periodic_rtcp\":" << result.dropped_periodic_rtcp << ','
          << "\"dropped_video_delta\":" << result.dropped_video_delta << ','
          << "\"dropped_video_key\":" << result.dropped_video_key << ','
          << "\"dropped_audio\":" << result.dropped_audio << ','
          << "\"dropped_critical_rtcp\":" << result.dropped_critical_rtcp << ','
          << "\"max_datagram_payload\":" << result.max_datagram_payload << ','
          << "\"loops_started\":" << result.loops_started << ','
          << "\"idr_access_units\":" << result.idr_access_units << ','
          << "\"dropped_idr_access_units\":" << result.dropped_idr_access_units
          << ',' << "\"recovery_idr_access_units\":"
          << result.recovery_idr_access_units << ','
          << "\"pli_packets\":" << result.pli_packets << ','
          << "\"fir_packets\":" << result.fir_packets << ','
          << "\"sender_reports\":" << result.sender_reports << ','
          << "\"receiver_reports\":" << result.receiver_reports << ','
          << "\"idr_recovery_ms\":" << result.idr_recovery_ms << ','
          << "\"boundary_packet_accepted\":"
          << (result.boundary_packet_accepted ? "true" : "false") << ','
          << "\"oversize_packet_rejected\":"
          << (result.oversize_packet_rejected ? "true" : "false") << ','
          << "\"clean_shutdown\":" << (result.clean_shutdown ? "true" : "false")
          << ',' << "\"rtt_ns\":" << result.rtt_ns << ','
          << "\"max_rtt_ns\":" << result.max_rtt_ns << ','
          << "\"delivery_rate_bytes_per_second\":"
          << result.delivery_rate_bytes_per_second << ','
          << "\"max_bandwidth_bytes_per_second\":"
          << result.max_bandwidth_bytes_per_second << ','
          << "\"pmtu_bytes\":" << result.pmtu_bytes << ','
          << "\"dgram_lost\":" << result.dgram_lost << ','
          << "\"sent_rtx_packets\":" << result.sent_rtx_packets << ','
          << "\"received_rtx_packets\":" << result.received_rtx_packets << ','
          << "\"nack_packets\":" << result.nack_packets << ','
          << "\"retransmitted_packets_sent\":"
          << result.retransmitted_packets_sent << ','
          << "\"retransmitted_bytes_sent\":" << result.retransmitted_bytes_sent
          << ',' << "\"opus_inband_fec\":"
          << (result.opus_inband_fec ? "true" : "false") << ','
          << "\"injected_video_packet_drops\":"
          << result.injected_video_packet_drops << ','
          << "\"injected_audio_packet_drops\":"
          << result.injected_audio_packet_drops << ','
          << "\"audio_fec_packets_received\":"
          << result.audio_fec_packets_received << ','
          << "\"audio_fec_packets_discarded\":"
          << result.audio_fec_packets_discarded << ','
          << "\"audio_secondary_decoded_rate\":"
          << result.audio_secondary_decoded_rate << "}\n";
      return 0;
    }
    if (!scenario || *scenario != "synthetic-roq" || !address || !manifest ||
        !duration || *duration <= 0 || !event_log || !run_id) {
      std::cerr << command << " requires --scenario synthetic-roq --"
                << (command == "client" ? "connect" : "listen")
                << " HOST:PORT --manifest <path> --duration <seconds> "
                   "--event-log <path> --run-id <id>\n";
      return 2;
    }
    webrtc::roq::SyntheticRoqOptions options = {
        .role = command == "client" ? webrtc::roq::PeerRole::kClient
                                    : webrtc::roq::PeerRole::kServer,
        .address = std::string(*address),
        .manifest_path = std::string(*manifest),
        .executable_path = args[0],
        .event_log_path = std::string(*event_log),
        .run_id = std::string(*run_id),
        .inject_error = inject_error ? std::string(*inject_error) : "",
        .duration_seconds = *duration,
        .disable_datagram = HasOption(args, "--disable-datagram")};
    const webrtc::roq::SyntheticRoqResult result =
        webrtc::roq::RunSyntheticRoq(options);
    if (!result.error.empty()) {
      std::cerr << result.error << '\n';
      return 1;
    }
    std::cout << "{\"schema_version\":1,"
              << "\"event\":\"synthetic_roq_summary\","
              << "\"alpn\":\"" << result.alpn << "\","
              << "\"datagram_negotiated\":"
              << (result.datagram_negotiated ? "true" : "false") << ','
              << "\"peer_verification\":"
              << (result.peer_verification ? "true" : "false") << ','
              << "\"generated_audio_rtp\":" << result.generated_audio_rtp << ','
              << "\"generated_video_rtp\":" << result.generated_video_rtp << ','
              << "\"generated_rtcp\":" << result.generated_rtcp << ','
              << "\"received_audio_rtp\":" << result.received_audio_rtp << ','
              << "\"received_video_rtp\":" << result.received_video_rtp << ','
              << "\"received_rtcp\":" << result.received_rtcp << ','
              << "\"queue_full_drop\":" << result.queue_full_drop << ','
              << "\"malformed\":" << result.malformed << ','
              << "\"unknown_flow\":" << result.unknown_flow << ','
              << "\"duplicate\":" << result.duplicate << ','
              << "\"reordered\":" << result.reordered << ','
              << "\"audio_received_in_final_second\":"
              << (result.audio_received_in_final_second ? "true" : "false")
              << ',' << "\"video_received_in_final_second\":"
              << (result.video_received_in_final_second ? "true" : "false")
              << ',' << "\"close_code\":" << result.close_code << ','
              << "\"close_is_application\":"
              << (result.close_is_application ? "true" : "false") << ','
              << "\"close_observed\":"
              << (result.close_observed ? "true" : "false") << ','
              << "\"max_pacing_delay_ns\":" << result.max_pacing_delay_ns << ','
              << "\"rtt_ns\":" << result.rtt_ns << ','
              << "\"max_rtt_ns\":" << result.max_rtt_ns << ','
              << "\"delivery_rate_bytes_per_second\":"
              << result.delivery_rate_bytes_per_second << ','
              << "\"max_bandwidth_bytes_per_second\":"
              << result.max_bandwidth_bytes_per_second << ','
              << "\"pmtu_bytes\":" << result.pmtu_bytes << ','
              << "\"dgram_lost\":" << result.dgram_lost << "}\n";
    return 0;
  }
  if (command == "media-source" && !HasOption(args, "--input")) {
    std::cerr << "--input is required for media-source\n";
    return 2;
  }
  if (command == "media-source") {
    const std::optional<std::string_view> input = OptionValue(args, "--input");
    const std::optional<std::string_view> duration =
        OptionValue(args, "--duration");
    const std::optional<std::string_view> clock = OptionValue(args, "--clock");
    const std::optional<std::string_view> run_id =
        OptionValue(args, "--run-id");
    const std::optional<std::string_view> event_log =
        OptionValue(args, "--event-log");
    if (!input || !duration || !clock ||
        (*clock != "virtual" && *clock != "realtime")) {
      std::cerr << "media-source requires --input <path> --duration <seconds> "
                   "--clock <virtual|realtime>\n";
      return 2;
    }
    const std::optional<double> duration_seconds =
        DoubleOption(args, "--duration");
    if (!duration_seconds) {
      std::cerr << "invalid --duration\n";
      return 2;
    }
    const webrtc::roq::MediaSourceResult result = webrtc::roq::RunMediaSource(
        std::string(*input), *duration_seconds,
        *clock == "virtual" ? webrtc::roq::MediaClockMode::kVirtual
                            : webrtc::roq::MediaClockMode::kRealtime,
        run_id ? std::string(*run_id) : "",
        event_log ? std::string(*event_log) : "");
    if (!result.error.empty()) {
      std::cerr << result.error << '\n';
      return 1;
    }
    std::cout << "{\"schema_version\":1,"
              << "\"event\":\"media_source_summary\","
              << "\"video_frames\":" << result.video_frames << ','
              << "\"audio_samples_per_channel\":"
              << result.audio_samples_per_channel << ','
              << "\"audio_blocks\":" << result.audio_blocks << ','
              << "\"audio_sample_rate_hz\":" << result.audio_sample_rate_hz
              << ',' << "\"audio_channels\":" << result.audio_channels << ','
              << "\"video_width\":" << result.video_width << ','
              << "\"video_height\":" << result.video_height << ','
              << "\"video_hash\":\"" << webrtc::roq::HexHash(result.video_hash)
              << "\","
              << "\"audio_hash\":\"" << webrtc::roq::HexHash(result.audio_hash)
              << "\","
              << "\"video_pts_hash\":\""
              << webrtc::roq::HexHash(result.video_pts_hash) << "\","
              << "\"audio_pts_hash\":\""
              << webrtc::roq::HexHash(result.audio_pts_hash) << "\","
              << "\"max_pacing_error_us\":" << result.max_pacing_error_us
              << "}\n";
    return 0;
  }
  if (command == "manifest") {
    const std::optional<std::string_view> manifest =
        OptionValue(args, "--manifest");
    const std::optional<std::string_view> packet =
        OptionValue(args, "--packet");
    if (!manifest || !packet) {
      std::cerr << "manifest requires --manifest <path> --packet <kind>\n";
      return 2;
    }
    const std::optional<std::string_view> role = OptionValue(args, "--role");
    webrtc::roq::PacketMetadata metadata = {
        .kind = std::string(*packet), .role = role ? std::string(*role) : ""};
    if (*packet != "rtcp") {
      metadata.payload_type = UIntOption(args, "--pt");
      metadata.ssrc = UIntOption(args, "--ssrc");
      if (!metadata.payload_type || !metadata.ssrc) {
        std::cerr << "RTP mapping requires numeric --pt and --ssrc\n";
        return 2;
      }
    }
    const webrtc::roq::ManifestMappingResult result =
        webrtc::roq::LoadAndMapManifest(std::string(*manifest), metadata);
    if (!result.error.empty()) {
      std::cerr << result.error << '\n';
      return 1;
    }
    std::cout << "{\"schema_version\":1,\"event\":\"manifest_mapping\","
              << "\"packet\":\"" << *packet
              << "\",\"flow_id\":" << result.flow_id << ','
              << "\"sender_ssrc\":" << result.sender_ssrc << ','
              << "\"generation\":" << result.generation << ",\"config_hash\":\""
              << result.config_hash << "\"}\n";
    return 0;
  }
  if (command == "udp-echo") {
    const std::optional<uint32_t> count = UIntOption(args, "--count");
    if (!HasOption(args, "--self-test") || !count || *count == 0) {
      std::cerr << "udp-echo requires --self-test --count <positive>\n";
      return 2;
    }
    const webrtc::roq::UdpSelfTestResult result =
        webrtc::roq::RunUdpEchoSelfTest(*count);
    if (!result.error.empty()) {
      std::cerr << result.error << '\n';
      return 1;
    }
    std::cout << "{\"schema_version\":1,\"event\":\"udp_echo_summary\","
              << "\"sent\":" << result.sent
              << ",\"received\":" << result.received << "}\n";
    return 0;
  }
  if (command == "udp-relay") {
    const std::optional<std::string_view> listen =
        OptionValue(args, "--listen");
    const std::optional<std::string_view> forward =
        OptionValue(args, "--forward");
    const std::optional<uint32_t> count = UIntOption(args, "--count");
    const std::optional<std::string_view> profile =
        OptionValue(args, "--profile");
    const std::optional<std::string_view> run_id =
        OptionValue(args, "--run-id");
    const std::optional<std::string_view> event_log =
        OptionValue(args, "--event-log");
    if (!profile || (*profile != "m0" && *profile != "m3") || !run_id ||
        !event_log) {
      std::cerr << "udp-relay requires --profile <m0|m3> --run-id <id> "
                   "--event-log <path>\n";
      return 2;
    }
    webrtc::roq::UdpSelfTestResult result;
    if (HasOption(args, "--self-test")) {
      if (!count || *count == 0) {
        std::cerr << "udp-relay self-test requires --count <positive>\n";
        return 2;
      }
      result = webrtc::roq::RunUdpRelaySelfTest(*count, std::string(*run_id),
                                                std::string(*event_log));
    } else {
      const std::optional<double> duration = DoubleOption(args, "--duration");
      if (!listen || !forward || !duration || *duration <= 0) {
        std::cerr << "udp-relay requires --listen HOST:PORT --forward "
                     "HOST:PORT --duration <seconds>\n";
        return 2;
      }
      result = webrtc::roq::RunUdpRelay(
          std::string(*listen), std::string(*forward), *duration,
          std::string(*profile), std::string(*run_id), std::string(*event_log));
    }
    if (!result.error.empty()) {
      std::cerr << result.error << '\n';
      return 1;
    }
    std::cout << "{\"schema_version\":1,\"event\":\"udp_relay_summary\","
              << "\"sent\":" << result.sent
              << ",\"received\":" << result.received
              << ",\"injected_loss\":" << result.injected_loss
              << ",\"injected_reorder\":" << result.injected_reorder
              << ",\"forwarded\":" << result.forwarded << ",\"profile\":\""
              << *profile
              << "\",\"bandwidth_bps\":" << (*profile == "m3" ? 0 : 10'000'000)
              << ",\"bandwidth_steps_bps\":"
              << (*profile == "m3" ? "[10000000,800000,6000000]" : "[]") << ','
              << "\"base_one_way_delay_ms\":50,\"loss_percent\":"
              << (*profile == "m3" ? 0 : 1) << ','
              << "\"jitter_ms\":" << (*profile == "m3" ? 0 : 10)
              << ",\"reorder_percent\":" << (*profile == "m3" ? 0 : 1) << ','
              << "\"queue_limit_ms\":200,\"average_rtt_ms\":"
              << result.average_rtt_ms
              << ",\"min_rtt_ms\":" << result.min_rtt_ms
              << ",\"max_rtt_ms\":" << result.max_rtt_ms << "}\n";
    return 0;
  }

  std::cerr << command << " is not implemented yet\n";
  return 2;
}
