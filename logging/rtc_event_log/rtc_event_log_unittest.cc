/*
 *  Copyright (c) 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <map>
#include <memory>
#include <ostream>
#include <string>
#include <utility>
#include <vector>

#include "call/call.h"
#include "logging/rtc_event_log/events/rtc_event_audio_network_adaptation.h"
#include "logging/rtc_event_log/events/rtc_event_audio_playout.h"
#include "logging/rtc_event_log/events/rtc_event_audio_receive_stream_config.h"
#include "logging/rtc_event_log/events/rtc_event_audio_send_stream_config.h"
#include "logging/rtc_event_log/events/rtc_event_bwe_update_delay_based.h"
#include "logging/rtc_event_log/events/rtc_event_bwe_update_loss_based.h"
#include "logging/rtc_event_log/events/rtc_event_logging_started.h"
#include "logging/rtc_event_log/events/rtc_event_logging_stopped.h"
#include "logging/rtc_event_log/events/rtc_event_probe_cluster_created.h"
#include "logging/rtc_event_log/events/rtc_event_probe_result_failure.h"
#include "logging/rtc_event_log/events/rtc_event_probe_result_success.h"
#include "logging/rtc_event_log/events/rtc_event_rtcp_packet_incoming.h"
#include "logging/rtc_event_log/events/rtc_event_rtcp_packet_outgoing.h"
#include "logging/rtc_event_log/events/rtc_event_rtp_packet_incoming.h"
#include "logging/rtc_event_log/events/rtc_event_rtp_packet_outgoing.h"
#include "logging/rtc_event_log/events/rtc_event_video_receive_stream_config.h"
#include "logging/rtc_event_log/events/rtc_event_video_send_stream_config.h"
#include "logging/rtc_event_log/output/rtc_event_log_output_file.h"
#include "logging/rtc_event_log/rtc_event_log.h"
#include "logging/rtc_event_log/rtc_event_log_parser.h"
#include "logging/rtc_event_log/rtc_event_log_unittest_helper.h"
#include "logging/rtc_event_log/rtc_stream_config.h"
#include "modules/audio_coding/audio_network_adaptor/include/audio_network_adaptor.h"
#include "modules/remote_bitrate_estimator/include/bwe_defines.h"
#include "modules/rtp_rtcp/include/rtp_header_extension_map.h"
#include "modules/rtp_rtcp/source/rtcp_packet.h"
#include "modules/rtp_rtcp/source/rtcp_packet/sender_report.h"
#include "modules/rtp_rtcp/source/rtp_header_extensions.h"
#include "modules/rtp_rtcp/source/rtp_packet_received.h"
#include "modules/rtp_rtcp/source/rtp_packet_to_send.h"
#include "rtc_base/buffer.h"
#include "rtc_base/checks.h"
#include "rtc_base/fakeclock.h"
#include "rtc_base/ptr_util.h"
#include "rtc_base/random.h"
#include "test/gtest.h"
#include "test/testsupport/fileutils.h"

namespace webrtc {

namespace {

const uint8_t kTransmissionTimeOffsetExtensionId = 1;
const uint8_t kAbsoluteSendTimeExtensionId = 14;
const uint8_t kTransportSequenceNumberExtensionId = 13;
const uint8_t kAudioLevelExtensionId = 9;
const uint8_t kVideoRotationExtensionId = 5;

const uint8_t kExtensionIds[] = {
    kTransmissionTimeOffsetExtensionId, kAbsoluteSendTimeExtensionId,
    kTransportSequenceNumberExtensionId, kAudioLevelExtensionId,
    kVideoRotationExtensionId};
const RTPExtensionType kExtensionTypes[] = {
    RTPExtensionType::kRtpExtensionTransmissionTimeOffset,
    RTPExtensionType::kRtpExtensionAbsoluteSendTime,
    RTPExtensionType::kRtpExtensionTransportSequenceNumber,
    RTPExtensionType::kRtpExtensionAudioLevel,
    RTPExtensionType::kRtpExtensionVideoRotation};
const char* kExtensionNames[] = {
    RtpExtension::kTimestampOffsetUri, RtpExtension::kAbsSendTimeUri,
    RtpExtension::kTransportSequenceNumberUri, RtpExtension::kAudioLevelUri,
    RtpExtension::kVideoRotationUri};

const size_t kNumExtensions = 5;

struct BweLossEvent {
  int32_t bitrate_bps;
  uint8_t fraction_loss;
  int32_t total_packets;
};

// TODO(terelius): Merge with event type in parser once updated?
enum class EventType {
  kIncomingRtp,
  kOutgoingRtp,
  kIncomingRtcp,
  kOutgoingRtcp,
  kAudioPlayout,
  kBweLossUpdate,
  kBweDelayUpdate,
  kVideoRecvConfig,
  kVideoSendConfig,
  kAudioRecvConfig,
  kAudioSendConfig,
  kAudioNetworkAdaptation,
  kBweProbeClusterCreated,
  kBweProbeResult,
};

const std::map<EventType, std::string> event_type_to_string(
    {{EventType::kIncomingRtp, "RTP(in)"},
     {EventType::kOutgoingRtp, "RTP(out)"},
     {EventType::kIncomingRtcp, "RTCP(in)"},
     {EventType::kOutgoingRtcp, "RTCP(out)"},
     {EventType::kAudioPlayout, "PLAYOUT"},
     {EventType::kBweLossUpdate, "BWE_LOSS"},
     {EventType::kBweDelayUpdate, "BWE_DELAY"},
     {EventType::kVideoRecvConfig, "VIDEO_RECV_CONFIG"},
     {EventType::kVideoSendConfig, "VIDEO_SEND_CONFIG"},
     {EventType::kAudioRecvConfig, "AUDIO_RECV_CONFIG"},
     {EventType::kAudioSendConfig, "AUDIO_SEND_CONFIG"},
     {EventType::kAudioNetworkAdaptation, "AUDIO_NETWORK_ADAPTATION"},
     {EventType::kBweProbeClusterCreated, "BWE_PROBE_CREATED"},
     {EventType::kBweProbeResult, "BWE_PROBE_RESULT"}});

const std::map<ParsedRtcEventLog::EventType, std::string>
    parsed_event_type_to_string(
        {{ParsedRtcEventLog::EventType::UNKNOWN_EVENT, "UNKNOWN_EVENT"},
         {ParsedRtcEventLog::EventType::LOG_START, "LOG_START"},
         {ParsedRtcEventLog::EventType::LOG_END, "LOG_END"},
         {ParsedRtcEventLog::EventType::RTP_EVENT, "RTP"},
         {ParsedRtcEventLog::EventType::RTCP_EVENT, "RTCP"},
         {ParsedRtcEventLog::EventType::AUDIO_PLAYOUT_EVENT, "AUDIO_PLAYOUT"},
         {ParsedRtcEventLog::EventType::LOSS_BASED_BWE_UPDATE,
          "LOSS_BASED_BWE_UPDATE"},
         {ParsedRtcEventLog::EventType::DELAY_BASED_BWE_UPDATE,
          "DELAY_BASED_BWE_UPDATE"},
         {ParsedRtcEventLog::EventType::VIDEO_RECEIVER_CONFIG_EVENT,
          "VIDEO_RECV_CONFIG"},
         {ParsedRtcEventLog::EventType::VIDEO_SENDER_CONFIG_EVENT,
          "VIDEO_SEND_CONFIG"},
         {ParsedRtcEventLog::EventType::AUDIO_RECEIVER_CONFIG_EVENT,
          "AUDIO_RECV_CONFIG"},
         {ParsedRtcEventLog::EventType::AUDIO_SENDER_CONFIG_EVENT,
          "AUDIO_SEND_CONFIG"},
         {ParsedRtcEventLog::EventType::AUDIO_NETWORK_ADAPTATION_EVENT,
          "AUDIO_NETWORK_ADAPTATION"},
         {ParsedRtcEventLog::EventType::BWE_PROBE_CLUSTER_CREATED_EVENT,
          "BWE_PROBE_CREATED"},
         {ParsedRtcEventLog::EventType::BWE_PROBE_RESULT_EVENT,
          "BWE_PROBE_RESULT"}});
}  // namespace

void PrintActualEvents(const ParsedRtcEventLog& parsed_log,
                       std::ostream& stream);

RtpPacketToSend GenerateOutgoingRtpPacket(
    const RtpHeaderExtensionMap* extensions,
    uint32_t csrcs_count,
    size_t packet_size,
    Random* prng) {
  RTC_CHECK_GE(packet_size, 16 + 4 * csrcs_count + 4 * kNumExtensions);

  std::vector<uint32_t> csrcs;
  for (unsigned i = 0; i < csrcs_count; i++) {
    csrcs.push_back(prng->Rand<uint32_t>());
  }

  RtpPacketToSend rtp_packet(extensions, packet_size);
  rtp_packet.SetPayloadType(prng->Rand(127));
  rtp_packet.SetMarker(prng->Rand<bool>());
  rtp_packet.SetSequenceNumber(prng->Rand<uint16_t>());
  rtp_packet.SetSsrc(prng->Rand<uint32_t>());
  rtp_packet.SetTimestamp(prng->Rand<uint32_t>());
  rtp_packet.SetCsrcs(csrcs);

  rtp_packet.SetExtension<TransmissionOffset>(prng->Rand(0x00ffffff));
  rtp_packet.SetExtension<AudioLevel>(prng->Rand<bool>(), prng->Rand(127));
  rtp_packet.SetExtension<AbsoluteSendTime>(prng->Rand(0x00ffffff));
  rtp_packet.SetExtension<VideoOrientation>(prng->Rand(2));
  rtp_packet.SetExtension<TransportSequenceNumber>(prng->Rand<uint16_t>());

  size_t payload_size = packet_size - rtp_packet.headers_size();
  uint8_t* payload = rtp_packet.AllocatePayload(payload_size);
  for (size_t i = 0; i < payload_size; i++) {
    payload[i] = prng->Rand<uint8_t>();
  }
  return rtp_packet;
}

RtpPacketReceived GenerateIncomingRtpPacket(
    const RtpHeaderExtensionMap* extensions,
    uint32_t csrcs_count,
    size_t packet_size,
    Random* prng) {
  RtpPacketToSend packet_out =
      GenerateOutgoingRtpPacket(extensions, csrcs_count, packet_size, prng);
  RtpPacketReceived packet_in(extensions);
  packet_in.Parse(packet_out.data(), packet_out.size());
  return packet_in;
}

rtc::Buffer GenerateRtcpPacket(Random* prng) {
  rtcp::ReportBlock report_block;
  report_block.SetMediaSsrc(prng->Rand<uint32_t>());  // Remote SSRC.
  report_block.SetFractionLost(prng->Rand(50));

  rtcp::SenderReport sender_report;
  sender_report.SetSenderSsrc(prng->Rand<uint32_t>());
  sender_report.SetNtp(NtpTime(prng->Rand<uint32_t>(), prng->Rand<uint32_t>()));
  sender_report.SetPacketCount(prng->Rand<uint32_t>());
  sender_report.AddReportBlock(report_block);

  return sender_report.Build();
}

void GenerateVideoReceiveConfig(const RtpHeaderExtensionMap& extensions,
                                rtclog::StreamConfig* config,
                                Random* prng) {
  // Add SSRCs for the stream.
  config->remote_ssrc = prng->Rand<uint32_t>();
  config->local_ssrc = prng->Rand<uint32_t>();
  // Add extensions and settings for RTCP.
  config->rtcp_mode =
      prng->Rand<bool>() ? RtcpMode::kCompound : RtcpMode::kReducedSize;
  config->remb = prng->Rand<bool>();
  config->rtx_ssrc = prng->Rand<uint32_t>();
  config->codecs.emplace_back(prng->Rand<bool>() ? "VP8" : "H264",
                              prng->Rand(1, 127), prng->Rand(1, 127));
  // Add header extensions.
  for (unsigned i = 0; i < kNumExtensions; i++) {
    uint8_t id = extensions.GetId(kExtensionTypes[i]);
    if (id != RtpHeaderExtensionMap::kInvalidId) {
      config->rtp_extensions.emplace_back(kExtensionNames[i], id);
    }
  }
}

void GenerateVideoSendConfig(const RtpHeaderExtensionMap& extensions,
                             rtclog::StreamConfig* config,
                             Random* prng) {
  config->codecs.emplace_back(prng->Rand<bool>() ? "VP8" : "H264",
                              prng->Rand(1, 127), prng->Rand(1, 127));
  config->local_ssrc = prng->Rand<uint32_t>();
  config->rtx_ssrc = prng->Rand<uint32_t>();
  // Add header extensions.
  for (unsigned i = 0; i < kNumExtensions; i++) {
    uint8_t id = extensions.GetId(kExtensionTypes[i]);
    if (id != RtpHeaderExtensionMap::kInvalidId) {
      config->rtp_extensions.emplace_back(kExtensionNames[i], id);
    }
  }
}

void GenerateAudioReceiveConfig(const RtpHeaderExtensionMap& extensions,
                                rtclog::StreamConfig* config,
                                Random* prng) {
  // Add SSRCs for the stream.
  config->remote_ssrc = prng->Rand<uint32_t>();
  config->local_ssrc = prng->Rand<uint32_t>();
  // Add header extensions.
  for (unsigned i = 0; i < kNumExtensions; i++) {
    uint8_t id = extensions.GetId(kExtensionTypes[i]);
    if (id != RtpHeaderExtensionMap::kInvalidId) {
      config->rtp_extensions.emplace_back(kExtensionNames[i], id);
    }
  }
}

void GenerateAudioSendConfig(const RtpHeaderExtensionMap& extensions,
                             rtclog::StreamConfig* config,
                             Random* prng) {
  // Add SSRC to the stream.
  config->local_ssrc = prng->Rand<uint32_t>();
  // Add header extensions.
  for (unsigned i = 0; i < kNumExtensions; i++) {
    uint8_t id = extensions.GetId(kExtensionTypes[i]);
    if (id != RtpHeaderExtensionMap::kInvalidId) {
      config->rtp_extensions.emplace_back(kExtensionNames[i], id);
    }
  }
}

BweLossEvent GenerateBweLossEvent(Random* prng) {
  BweLossEvent loss_event;
  loss_event.bitrate_bps = prng->Rand(6000, 10000000);
  loss_event.fraction_loss = prng->Rand<uint8_t>();
  loss_event.total_packets = prng->Rand(1, 1000);
  return loss_event;
}

void GenerateAudioNetworkAdaptation(const RtpHeaderExtensionMap& extensions,
                                    AudioEncoderRuntimeConfig* config,
                                    Random* prng) {
  config->bitrate_bps = rtc::Optional<int>(prng->Rand(0, 3000000));
  config->enable_fec = rtc::Optional<bool>(prng->Rand<bool>());
  config->enable_dtx = rtc::Optional<bool>(prng->Rand<bool>());
  config->frame_length_ms = rtc::Optional<int>(prng->Rand(10, 120));
  config->num_channels = rtc::Optional<size_t>(prng->Rand(1, 2));
  config->uplink_packet_loss_fraction =
      rtc::Optional<float>(prng->Rand<float>());
}

class RtcEventLogSessionDescription {
 public:
  explicit RtcEventLogSessionDescription(unsigned int random_seed)
      : prng(random_seed) {}
  void GenerateSessionDescription(size_t incoming_rtp_count,
                                  size_t outgoing_rtp_count,
                                  size_t incoming_rtcp_count,
                                  size_t outgoing_rtcp_count,
                                  size_t playout_count,
                                  size_t bwe_loss_count,
                                  size_t bwe_delay_count,
                                  const RtpHeaderExtensionMap& extensions,
                                  uint32_t csrcs_count);
  void WriteSession();
  void ReadAndVerifySession();
  void PrintExpectedEvents(std::ostream& stream);

 private:
  std::vector<RtpPacketReceived> incoming_rtp_packets;
  std::vector<RtpPacketToSend> outgoing_rtp_packets;
  std::vector<rtc::Buffer> incoming_rtcp_packets;
  std::vector<rtc::Buffer> outgoing_rtcp_packets;
  std::vector<uint32_t> playout_ssrcs;
  std::vector<BweLossEvent> bwe_loss_updates;
  std::vector<std::pair<int32_t, BandwidthUsage> > bwe_delay_updates;
  std::vector<rtclog::StreamConfig> receiver_configs;
  std::vector<rtclog::StreamConfig> sender_configs;
  std::vector<EventType> event_types;
  Random prng;
};

void RtcEventLogSessionDescription::GenerateSessionDescription(
    size_t incoming_rtp_count,
    size_t outgoing_rtp_count,
    size_t incoming_rtcp_count,
    size_t outgoing_rtcp_count,
    size_t playout_count,
    size_t bwe_loss_count,
    size_t bwe_delay_count,
    const RtpHeaderExtensionMap& extensions,
    uint32_t csrcs_count) {
  // Create configuration for the video receive stream.
  receiver_configs.push_back(rtclog::StreamConfig());
  GenerateVideoReceiveConfig(extensions, &receiver_configs.back(), &prng);
  event_types.push_back(EventType::kVideoRecvConfig);

  // Create configuration for the video send stream.
  sender_configs.push_back(rtclog::StreamConfig());
  GenerateVideoSendConfig(extensions, &sender_configs.back(), &prng);
  event_types.push_back(EventType::kVideoSendConfig);
  const size_t config_count = 2;

  // Create incoming and outgoing RTP packets containing random data.
  for (size_t i = 0; i < incoming_rtp_count; i++) {
    size_t packet_size = prng.Rand(1000, 1100);
    incoming_rtp_packets.push_back(GenerateIncomingRtpPacket(
        &extensions, csrcs_count, packet_size, &prng));
    event_types.push_back(EventType::kIncomingRtp);
  }
  for (size_t i = 0; i < outgoing_rtp_count; i++) {
    size_t packet_size = prng.Rand(1000, 1100);
    outgoing_rtp_packets.push_back(GenerateOutgoingRtpPacket(
        &extensions, csrcs_count, packet_size, &prng));
    event_types.push_back(EventType::kOutgoingRtp);
  }
  // Create incoming and outgoing RTCP packets containing random data.
  for (size_t i = 0; i < incoming_rtcp_count; i++) {
    incoming_rtcp_packets.push_back(GenerateRtcpPacket(&prng));
    event_types.push_back(EventType::kIncomingRtcp);
  }
  for (size_t i = 0; i < outgoing_rtcp_count; i++) {
    outgoing_rtcp_packets.push_back(GenerateRtcpPacket(&prng));
    event_types.push_back(EventType::kOutgoingRtcp);
  }
  // Create random SSRCs to use when logging AudioPlayout events.
  for (size_t i = 0; i < playout_count; i++) {
    playout_ssrcs.push_back(prng.Rand<uint32_t>());
    event_types.push_back(EventType::kAudioPlayout);
  }
  // Create random bitrate updates for LossBasedBwe.
  for (size_t i = 0; i < bwe_loss_count; i++) {
    bwe_loss_updates.push_back(GenerateBweLossEvent(&prng));
    event_types.push_back(EventType::kBweLossUpdate);
  }
  // Create random bitrate updates for DelayBasedBwe.
  for (size_t i = 0; i < bwe_delay_count; i++) {
    bwe_delay_updates.push_back(std::make_pair(
        prng.Rand(6000, 10000000), prng.Rand<bool>()
                                       ? BandwidthUsage::kBwOverusing
                                       : BandwidthUsage::kBwUnderusing));
    event_types.push_back(EventType::kBweDelayUpdate);
  }

  // Order the events randomly. The configurations are stored in a separate
  // buffer, so they might be written before any othe events. Hence, we can't
  // mix the config events with other events.
  for (size_t i = config_count; i < event_types.size(); i++) {
    size_t other = prng.Rand(static_cast<uint32_t>(i),
                             static_cast<uint32_t>(event_types.size() - 1));
    RTC_CHECK(i <= other && other < event_types.size());
    std::swap(event_types[i], event_types[other]);
  }
}

void RtcEventLogSessionDescription::WriteSession() {
  // Find the name of the current test, in order to use it as a temporary
  // filename.
  auto test_info = ::testing::UnitTest::GetInstance()->current_test_info();
  const std::string temp_filename =
      test::OutputPath() + test_info->test_case_name() + test_info->name();

  rtc::ScopedFakeClock fake_clock;
  fake_clock.SetTimeMicros(prng.Rand<uint32_t>());

  // When log_dumper goes out of scope, it causes the log file to be flushed
  // to disk.
  std::unique_ptr<RtcEventLog> log_dumper(
      RtcEventLog::Create(RtcEventLog::EncodingType::Legacy));

  size_t incoming_rtp_written = 0;
  size_t outgoing_rtp_written = 0;
  size_t incoming_rtcp_written = 0;
  size_t outgoing_rtcp_written = 0;
  size_t playouts_written = 0;
  size_t bwe_loss_written = 0;
  size_t bwe_delay_written = 0;
  size_t recv_configs_written = 0;
  size_t send_configs_written = 0;

  for (size_t i = 0; i < event_types.size(); i++) {
    fake_clock.AdvanceTimeMicros(prng.Rand(1, 1000));
    if (i == event_types.size() / 2)
      log_dumper->StartLogging(
          rtc::MakeUnique<RtcEventLogOutputFile>(temp_filename, 10000000));
    switch (event_types[i]) {
      case EventType::kIncomingRtp:
        RTC_CHECK(incoming_rtp_written < incoming_rtp_packets.size());
        log_dumper->Log(rtc::MakeUnique<RtcEventRtpPacketIncoming>(
            incoming_rtp_packets[incoming_rtp_written++]));
        break;
      case EventType::kOutgoingRtp: {
        RTC_CHECK(outgoing_rtp_written < outgoing_rtp_packets.size());
        constexpr int kNotAProbe = PacedPacketInfo::kNotAProbe;  // Compiler...
        log_dumper->Log(rtc::MakeUnique<RtcEventRtpPacketOutgoing>(
            outgoing_rtp_packets[outgoing_rtp_written++], kNotAProbe));
        break;
      }
      case EventType::kIncomingRtcp:
        RTC_CHECK(incoming_rtcp_written < incoming_rtcp_packets.size());
        log_dumper->Log(rtc::MakeUnique<RtcEventRtcpPacketIncoming>(
            incoming_rtcp_packets[incoming_rtcp_written++]));
        break;
      case EventType::kOutgoingRtcp:
        RTC_CHECK(outgoing_rtcp_written < outgoing_rtcp_packets.size());
        log_dumper->Log(rtc::MakeUnique<RtcEventRtcpPacketOutgoing>(
            outgoing_rtcp_packets[outgoing_rtcp_written++]));
        break;
      case EventType::kAudioPlayout:
        RTC_CHECK(playouts_written < playout_ssrcs.size());
        log_dumper->Log(rtc::MakeUnique<RtcEventAudioPlayout>(
            playout_ssrcs[playouts_written++]));
        break;
      case EventType::kBweLossUpdate:
        RTC_CHECK(bwe_loss_written < bwe_loss_updates.size());
        log_dumper->Log(rtc::MakeUnique<RtcEventBweUpdateLossBased>(
            bwe_loss_updates[bwe_loss_written].bitrate_bps,
            bwe_loss_updates[bwe_loss_written].fraction_loss,
            bwe_loss_updates[bwe_loss_written].total_packets));
        bwe_loss_written++;
        break;
      case EventType::kBweDelayUpdate:
        RTC_CHECK(bwe_delay_written < bwe_delay_updates.size());
        log_dumper->Log(rtc::MakeUnique<RtcEventBweUpdateDelayBased>(
            bwe_delay_updates[bwe_delay_written].first,
            bwe_delay_updates[bwe_delay_written].second));
        bwe_delay_written++;
        break;
      case EventType::kVideoRecvConfig:
        RTC_CHECK(recv_configs_written < receiver_configs.size());
        log_dumper->Log(rtc::MakeUnique<RtcEventVideoReceiveStreamConfig>(
            rtc::MakeUnique<rtclog::StreamConfig>(
                receiver_configs[recv_configs_written++])));
        break;
      case EventType::kVideoSendConfig:
        RTC_CHECK(send_configs_written < sender_configs.size());
        log_dumper->Log(rtc::MakeUnique<RtcEventVideoSendStreamConfig>(
            rtc::MakeUnique<rtclog::StreamConfig>(
                sender_configs[send_configs_written++])));
        break;
      case EventType::kAudioRecvConfig:
        // Not implemented
        RTC_NOTREACHED();
        break;
      case EventType::kAudioSendConfig:
        // Not implemented
        RTC_NOTREACHED();
        break;
      case EventType::kAudioNetworkAdaptation:
        // Not implemented
        RTC_NOTREACHED();
        break;
      case EventType::kBweProbeClusterCreated:
        // Not implemented
        RTC_NOTREACHED();
        break;
      case EventType::kBweProbeResult:
        // Not implemented
        RTC_NOTREACHED();
        break;
    }
  }

  log_dumper->StopLogging();
}

// Read the file and verify that what we read back from the event log is the
// same as what we wrote down.
void RtcEventLogSessionDescription::ReadAndVerifySession() {
  // Find the name of the current test, in order to use it as a temporary
  // filename.
  auto test_info = ::testing::UnitTest::GetInstance()->current_test_info();
  const std::string temp_filename =
      test::OutputPath() + test_info->test_case_name() + test_info->name();

  // Read the generated file from disk.
  ParsedRtcEventLog parsed_log;
  ASSERT_TRUE(parsed_log.ParseFile(temp_filename));
  EXPECT_GE(1000u, event_types.size() +
                       2);  // The events must fit in the message queue.
  EXPECT_EQ(event_types.size() + 2, parsed_log.GetNumberOfEvents());

  size_t incoming_rtp_read = 0;
  size_t outgoing_rtp_read = 0;
  size_t incoming_rtcp_read = 0;
  size_t outgoing_rtcp_read = 0;
  size_t playouts_read = 0;
  size_t bwe_loss_read = 0;
  size_t bwe_delay_read = 0;
  size_t recv_configs_read = 0;
  size_t send_configs_read = 0;

  RtcEventLogTestHelper::VerifyLogStartEvent(parsed_log, 0);

  for (size_t i = 0; i < event_types.size(); i++) {
    switch (event_types[i]) {
      case EventType::kIncomingRtp:
        RTC_CHECK(incoming_rtp_read < incoming_rtp_packets.size());
        RtcEventLogTestHelper::VerifyIncomingRtpEvent(
            parsed_log, i + 1, incoming_rtp_packets[incoming_rtp_read++]);
        break;
      case EventType::kOutgoingRtp:
        RTC_CHECK(outgoing_rtp_read < outgoing_rtp_packets.size());
        RtcEventLogTestHelper::VerifyOutgoingRtpEvent(
            parsed_log, i + 1, outgoing_rtp_packets[outgoing_rtp_read++]);
        break;
      case EventType::kIncomingRtcp:
        RTC_CHECK(incoming_rtcp_read < incoming_rtcp_packets.size());
        RtcEventLogTestHelper::VerifyRtcpEvent(
            parsed_log, i + 1, kIncomingPacket,
            incoming_rtcp_packets[incoming_rtcp_read].data(),
            incoming_rtcp_packets[incoming_rtcp_read].size());
        incoming_rtcp_read++;
        break;
      case EventType::kOutgoingRtcp:
        RTC_CHECK(outgoing_rtcp_read < outgoing_rtcp_packets.size());
        RtcEventLogTestHelper::VerifyRtcpEvent(
            parsed_log, i + 1, kOutgoingPacket,
            outgoing_rtcp_packets[outgoing_rtcp_read].data(),
            outgoing_rtcp_packets[outgoing_rtcp_read].size());
        outgoing_rtcp_read++;
        break;
      case EventType::kAudioPlayout:
        RTC_CHECK(playouts_read < playout_ssrcs.size());
        RtcEventLogTestHelper::VerifyPlayoutEvent(
            parsed_log, i + 1, playout_ssrcs[playouts_read++]);
        break;
      case EventType::kBweLossUpdate:
        RTC_CHECK(bwe_loss_read < bwe_loss_updates.size());
        RtcEventLogTestHelper::VerifyBweLossEvent(
            parsed_log, i + 1, bwe_loss_updates[bwe_loss_read].bitrate_bps,
            bwe_loss_updates[bwe_loss_read].fraction_loss,
            bwe_loss_updates[bwe_loss_read].total_packets);
        bwe_loss_read++;
        break;
      case EventType::kBweDelayUpdate:
        RTC_CHECK(bwe_delay_read < bwe_delay_updates.size());
        RtcEventLogTestHelper::VerifyBweDelayEvent(
            parsed_log, i + 1, bwe_delay_updates[bwe_delay_read].first,
            bwe_delay_updates[bwe_delay_read].second);
        bwe_delay_read++;
        break;
      case EventType::kVideoRecvConfig:
        RTC_CHECK(recv_configs_read < receiver_configs.size());
        RtcEventLogTestHelper::VerifyVideoReceiveStreamConfig(
            parsed_log, i + 1, receiver_configs[recv_configs_read++]);
        break;
      case EventType::kVideoSendConfig:
        RTC_CHECK(send_configs_read < sender_configs.size());
        RtcEventLogTestHelper::VerifyVideoSendStreamConfig(
            parsed_log, i + 1, sender_configs[send_configs_read++]);
        break;
      case EventType::kAudioRecvConfig:
        // Not implemented
        RTC_NOTREACHED();
        break;
      case EventType::kAudioSendConfig:
        // Not implemented
        RTC_NOTREACHED();
        break;
      case EventType::kAudioNetworkAdaptation:
        // Not implemented
        RTC_NOTREACHED();
        break;
      case EventType::kBweProbeClusterCreated:
        // Not implemented
        RTC_NOTREACHED();
        break;
      case EventType::kBweProbeResult:
        // Not implemented
        RTC_NOTREACHED();
        break;
    }
  }

  RtcEventLogTestHelper::VerifyLogEndEvent(parsed_log,
                                           parsed_log.GetNumberOfEvents() - 1);

  // Clean up temporary file - can be pretty slow.
  remove(temp_filename.c_str());
}

void RtcEventLogSessionDescription::PrintExpectedEvents(std::ostream& stream) {
  for (size_t i = 0; i < event_types.size(); i++) {
    auto it = event_type_to_string.find(event_types[i]);
    RTC_CHECK(it != event_type_to_string.end());
    stream << it->second << " ";
  }
  stream << std::endl;
}

void PrintActualEvents(const ParsedRtcEventLog& parsed_log,
                       std::ostream& stream) {
  for (size_t i = 0; i < parsed_log.GetNumberOfEvents(); i++) {
    auto it = parsed_event_type_to_string.find(parsed_log.GetEventType(i));
    RTC_CHECK(it != parsed_event_type_to_string.end());
    stream << it->second << " ";
  }
  stream << std::endl;
}

TEST(RtcEventLogTest, LogSessionAndReadBack) {
  RtpHeaderExtensionMap extensions;
  RtcEventLogSessionDescription session(321 /*Random seed*/);
  session.GenerateSessionDescription(3,  // Number of incoming RTP packets.
                                     2,  // Number of outgoing RTP packets.
                                     1,  // Number of incoming RTCP packets.
                                     1,  // Number of outgoing RTCP packets.
                                     0,  // Number of playout events.
                                     0,  // Number of BWE loss events.
                                     0,  // Number of BWE delay events.
                                     extensions,  // No extensions.
                                     0);  // Number of contributing sources.
  session.WriteSession();
  session.ReadAndVerifySession();
}

TEST(RtcEventLogTest, LogSessionAndReadBackWith2Extensions) {
  RtpHeaderExtensionMap extensions;
  extensions.Register(kRtpExtensionAbsoluteSendTime,
                      kAbsoluteSendTimeExtensionId);
  extensions.Register(kRtpExtensionTransportSequenceNumber,
                      kTransportSequenceNumberExtensionId);
  RtcEventLogSessionDescription session(3141592653u /*Random seed*/);
  session.GenerateSessionDescription(4, 4, 1, 1, 0, 0, 0, extensions, 0);
  session.WriteSession();
  session.ReadAndVerifySession();
}

TEST(RtcEventLogTest, LogSessionAndReadBackWithAllExtensions) {
  RtpHeaderExtensionMap extensions;
  for (uint32_t i = 0; i < kNumExtensions; i++) {
    extensions.Register(kExtensionTypes[i], kExtensionIds[i]);
  }
  RtcEventLogSessionDescription session(2718281828u /*Random seed*/);
  session.GenerateSessionDescription(5, 4, 1, 1, 3, 2, 2, extensions, 2);
  session.WriteSession();
  session.ReadAndVerifySession();
}

TEST(RtcEventLogTest, LogSessionAndReadBackAllCombinations) {
  // Try all combinations of header extensions and up to 2 CSRCS.
  for (uint32_t extension_selection = 0;
       extension_selection < (1u << kNumExtensions); extension_selection++) {
    RtpHeaderExtensionMap extensions;
    for (uint32_t i = 0; i < kNumExtensions; i++) {
      if (extension_selection & (1u << i)) {
        extensions.Register(kExtensionTypes[i], kExtensionIds[i]);
      }
    }
    for (uint32_t csrcs_count = 0; csrcs_count < 3; csrcs_count++) {
      RtcEventLogSessionDescription session(extension_selection * 3 +
                                            csrcs_count + 1 /*Random seed*/);
      session.GenerateSessionDescription(
          2 + extension_selection,  // Number of incoming RTP packets.
          2 + extension_selection,  // Number of outgoing RTP packets.
          1 + csrcs_count,          // Number of incoming RTCP packets.
          1 + csrcs_count,          // Number of outgoing RTCP packets.
          3 + csrcs_count,          // Number of playout events.
          1 + csrcs_count,          // Number of BWE loss events.
          2 + csrcs_count,          // Number of BWE delay events.
          extensions,               // Bit vector choosing extensions.
          csrcs_count);             // Number of contributing sources.
      session.WriteSession();
      session.ReadAndVerifySession();
    }
  }
}

}  // namespace webrtc
