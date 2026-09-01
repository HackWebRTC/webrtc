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
#include "rtc_tools/roq/media_source.h"
#include "rtc_tools/roq/network_lab.h"

namespace {

void PrintUsage(std::ostream& output) {
  output << "Usage: roq_test_peer <command> [options]\n"
            "Commands:\n"
            "  capability\n"
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
    webrtc::roq::PacketMetadata metadata = {.kind = std::string(*packet)};
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
              << "\",\"flow_id\":" << result.flow_id
              << ",\"generation\":" << result.generation
              << ",\"config_hash\":\"" << result.config_hash << "\"}\n";
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
    if (!profile || *profile != "m0" || !run_id || !event_log) {
      std::cerr << "udp-relay requires --profile m0 --run-id <id> "
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
          std::string(*run_id), std::string(*event_log));
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
              << ",\"forwarded\":" << result.forwarded
              << ",\"bandwidth_bps\":10000000,"
              << "\"base_one_way_delay_ms\":50,\"loss_percent\":1,"
              << "\"jitter_ms\":10,\"reorder_percent\":1,"
              << "\"queue_limit_ms\":200,\"average_rtt_ms\":"
              << result.average_rtt_ms
              << ",\"min_rtt_ms\":" << result.min_rtt_ms
              << ",\"max_rtt_ms\":" << result.max_rtt_ms << "}\n";
    return 0;
  }

  std::cerr << command << " is not implemented yet\n";
  return 2;
}
