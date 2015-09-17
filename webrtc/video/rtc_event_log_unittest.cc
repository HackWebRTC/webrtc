/*
 *  Copyright (c) 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifdef ENABLE_RTC_EVENT_LOG

#include <stdio.h>
#include <string>
#include <vector>

#include "testing/gtest/include/gtest/gtest.h"
#include "webrtc/base/buffer.h"
#include "webrtc/base/checks.h"
#include "webrtc/base/scoped_ptr.h"
#include "webrtc/call.h"
#include "webrtc/modules/rtp_rtcp/source/rtp_sender.h"
#include "webrtc/system_wrappers/interface/clock.h"
#include "webrtc/test/test_suite.h"
#include "webrtc/test/testsupport/fileutils.h"
#include "webrtc/test/testsupport/gtest_disable.h"
#include "webrtc/video/rtc_event_log.h"

// Files generated at build-time by the protobuf compiler.
#ifdef WEBRTC_ANDROID_PLATFORM_BUILD
#include "external/webrtc/webrtc/video/rtc_event_log.pb.h"
#else
#include "webrtc/video/rtc_event_log.pb.h"
#endif

namespace webrtc {

namespace {

const RTPExtensionType kExtensionTypes[] = {
    RTPExtensionType::kRtpExtensionTransmissionTimeOffset,
    RTPExtensionType::kRtpExtensionAudioLevel,
    RTPExtensionType::kRtpExtensionAbsoluteSendTime,
    RTPExtensionType::kRtpExtensionVideoRotation,
    RTPExtensionType::kRtpExtensionTransportSequenceNumber};
const char* kExtensionNames[] = {RtpExtension::kTOffset,
                                 RtpExtension::kAudioLevel,
                                 RtpExtension::kAbsSendTime,
                                 RtpExtension::kVideoRotation,
                                 RtpExtension::kTransportSequenceNumber};
const size_t kNumExtensions = 5;

}  // namepsace

// TODO(terelius): Place this definition with other parsing functions?
MediaType GetRuntimeMediaType(rtclog::MediaType media_type) {
  switch (media_type) {
    case rtclog::MediaType::ANY:
      return MediaType::ANY;
    case rtclog::MediaType::AUDIO:
      return MediaType::AUDIO;
    case rtclog::MediaType::VIDEO:
      return MediaType::VIDEO;
    case rtclog::MediaType::DATA:
      return MediaType::DATA;
  }
  RTC_NOTREACHED();
  return MediaType::ANY;
}

// Checks that the event has a timestamp, a type and exactly the data field
// corresponding to the type.
::testing::AssertionResult IsValidBasicEvent(const rtclog::Event& event) {
  if (!event.has_timestamp_us())
    return ::testing::AssertionFailure() << "Event has no timestamp";
  if (!event.has_type())
    return ::testing::AssertionFailure() << "Event has no event type";
  rtclog::Event_EventType type = event.type();
  if ((type == rtclog::Event::RTP_EVENT) != event.has_rtp_packet())
    return ::testing::AssertionFailure()
           << "Event of type " << type << " has "
           << (event.has_rtp_packet() ? "" : "no ") << "RTP packet";
  if ((type == rtclog::Event::RTCP_EVENT) != event.has_rtcp_packet())
    return ::testing::AssertionFailure()
           << "Event of type " << type << " has "
           << (event.has_rtcp_packet() ? "" : "no ") << "RTCP packet";
  if ((type == rtclog::Event::DEBUG_EVENT) != event.has_debug_event())
    return ::testing::AssertionFailure()
           << "Event of type " << type << " has "
           << (event.has_debug_event() ? "" : "no ") << "debug event";
  if ((type == rtclog::Event::VIDEO_RECEIVER_CONFIG_EVENT) !=
      event.has_video_receiver_config())
    return ::testing::AssertionFailure()
           << "Event of type " << type << " has "
           << (event.has_video_receiver_config() ? "" : "no ")
           << "receiver config";
  if ((type == rtclog::Event::VIDEO_SENDER_CONFIG_EVENT) !=
      event.has_video_sender_config())
    return ::testing::AssertionFailure()
           << "Event of type " << type << " has "
           << (event.has_video_sender_config() ? "" : "no ") << "sender config";
  if ((type == rtclog::Event::AUDIO_RECEIVER_CONFIG_EVENT) !=
      event.has_audio_receiver_config()) {
    return ::testing::AssertionFailure()
           << "Event of type " << type << " has "
           << (event.has_audio_receiver_config() ? "" : "no ")
           << "audio receiver config";
  }
  if ((type == rtclog::Event::AUDIO_SENDER_CONFIG_EVENT) !=
      event.has_audio_sender_config()) {
    return ::testing::AssertionFailure()
           << "Event of type " << type << " has "
           << (event.has_audio_sender_config() ? "" : "no ")
           << "audio sender config";
  }
  return ::testing::AssertionSuccess();
}

void VerifyReceiveStreamConfig(const rtclog::Event& event,
                               const VideoReceiveStream::Config& config) {
  ASSERT_TRUE(IsValidBasicEvent(event));
  ASSERT_EQ(rtclog::Event::VIDEO_RECEIVER_CONFIG_EVENT, event.type());
  const rtclog::VideoReceiveConfig& receiver_config =
      event.video_receiver_config();
  // Check SSRCs.
  ASSERT_TRUE(receiver_config.has_remote_ssrc());
  EXPECT_EQ(config.rtp.remote_ssrc, receiver_config.remote_ssrc());
  ASSERT_TRUE(receiver_config.has_local_ssrc());
  EXPECT_EQ(config.rtp.local_ssrc, receiver_config.local_ssrc());
  // Check RTCP settings.
  ASSERT_TRUE(receiver_config.has_rtcp_mode());
  if (config.rtp.rtcp_mode == newapi::kRtcpCompound)
    EXPECT_EQ(rtclog::VideoReceiveConfig::RTCP_COMPOUND,
              receiver_config.rtcp_mode());
  else
    EXPECT_EQ(rtclog::VideoReceiveConfig::RTCP_REDUCEDSIZE,
              receiver_config.rtcp_mode());
  ASSERT_TRUE(receiver_config.has_receiver_reference_time_report());
  EXPECT_EQ(config.rtp.rtcp_xr.receiver_reference_time_report,
            receiver_config.receiver_reference_time_report());
  ASSERT_TRUE(receiver_config.has_remb());
  EXPECT_EQ(config.rtp.remb, receiver_config.remb());
  // Check RTX map.
  ASSERT_EQ(static_cast<int>(config.rtp.rtx.size()),
            receiver_config.rtx_map_size());
  for (const rtclog::RtxMap& rtx_map : receiver_config.rtx_map()) {
    ASSERT_TRUE(rtx_map.has_payload_type());
    ASSERT_TRUE(rtx_map.has_config());
    EXPECT_EQ(1u, config.rtp.rtx.count(rtx_map.payload_type()));
    const rtclog::RtxConfig& rtx_config = rtx_map.config();
    const VideoReceiveStream::Config::Rtp::Rtx& rtx =
        config.rtp.rtx.at(rtx_map.payload_type());
    ASSERT_TRUE(rtx_config.has_rtx_ssrc());
    ASSERT_TRUE(rtx_config.has_rtx_payload_type());
    EXPECT_EQ(rtx.ssrc, rtx_config.rtx_ssrc());
    EXPECT_EQ(rtx.payload_type, rtx_config.rtx_payload_type());
  }
  // Check header extensions.
  ASSERT_EQ(static_cast<int>(config.rtp.extensions.size()),
            receiver_config.header_extensions_size());
  for (int i = 0; i < receiver_config.header_extensions_size(); i++) {
    ASSERT_TRUE(receiver_config.header_extensions(i).has_name());
    ASSERT_TRUE(receiver_config.header_extensions(i).has_id());
    const std::string& name = receiver_config.header_extensions(i).name();
    int id = receiver_config.header_extensions(i).id();
    EXPECT_EQ(config.rtp.extensions[i].id, id);
    EXPECT_EQ(config.rtp.extensions[i].name, name);
  }
  // Check decoders.
  ASSERT_EQ(static_cast<int>(config.decoders.size()),
            receiver_config.decoders_size());
  for (int i = 0; i < receiver_config.decoders_size(); i++) {
    ASSERT_TRUE(receiver_config.decoders(i).has_name());
    ASSERT_TRUE(receiver_config.decoders(i).has_payload_type());
    const std::string& decoder_name = receiver_config.decoders(i).name();
    int decoder_type = receiver_config.decoders(i).payload_type();
    EXPECT_EQ(config.decoders[i].payload_name, decoder_name);
    EXPECT_EQ(config.decoders[i].payload_type, decoder_type);
  }
}

void VerifySendStreamConfig(const rtclog::Event& event,
                            const VideoSendStream::Config& config) {
  ASSERT_TRUE(IsValidBasicEvent(event));
  ASSERT_EQ(rtclog::Event::VIDEO_SENDER_CONFIG_EVENT, event.type());
  const rtclog::VideoSendConfig& sender_config = event.video_sender_config();
  // Check SSRCs.
  ASSERT_EQ(static_cast<int>(config.rtp.ssrcs.size()),
            sender_config.ssrcs_size());
  for (int i = 0; i < sender_config.ssrcs_size(); i++) {
    EXPECT_EQ(config.rtp.ssrcs[i], sender_config.ssrcs(i));
  }
  // Check header extensions.
  ASSERT_EQ(static_cast<int>(config.rtp.extensions.size()),
            sender_config.header_extensions_size());
  for (int i = 0; i < sender_config.header_extensions_size(); i++) {
    ASSERT_TRUE(sender_config.header_extensions(i).has_name());
    ASSERT_TRUE(sender_config.header_extensions(i).has_id());
    const std::string& name = sender_config.header_extensions(i).name();
    int id = sender_config.header_extensions(i).id();
    EXPECT_EQ(config.rtp.extensions[i].id, id);
    EXPECT_EQ(config.rtp.extensions[i].name, name);
  }
  // Check RTX settings.
  ASSERT_EQ(static_cast<int>(config.rtp.rtx.ssrcs.size()),
            sender_config.rtx_ssrcs_size());
  for (int i = 0; i < sender_config.rtx_ssrcs_size(); i++) {
    EXPECT_EQ(config.rtp.rtx.ssrcs[i], sender_config.rtx_ssrcs(i));
  }
  if (sender_config.rtx_ssrcs_size() > 0) {
    ASSERT_TRUE(sender_config.has_rtx_payload_type());
    EXPECT_EQ(config.rtp.rtx.payload_type, sender_config.rtx_payload_type());
  }
  // Check CNAME.
  ASSERT_TRUE(sender_config.has_c_name());
  EXPECT_EQ(config.rtp.c_name, sender_config.c_name());
  // Check encoder.
  ASSERT_TRUE(sender_config.has_encoder());
  ASSERT_TRUE(sender_config.encoder().has_name());
  ASSERT_TRUE(sender_config.encoder().has_payload_type());
  EXPECT_EQ(config.encoder_settings.payload_name,
            sender_config.encoder().name());
  EXPECT_EQ(config.encoder_settings.payload_type,
            sender_config.encoder().payload_type());
}

void VerifyRtpEvent(const rtclog::Event& event,
                    bool incoming,
                    MediaType media_type,
                    uint8_t* header,
                    size_t header_size,
                    size_t total_size) {
  ASSERT_TRUE(IsValidBasicEvent(event));
  ASSERT_EQ(rtclog::Event::RTP_EVENT, event.type());
  const rtclog::RtpPacket& rtp_packet = event.rtp_packet();
  ASSERT_TRUE(rtp_packet.has_incoming());
  EXPECT_EQ(incoming, rtp_packet.incoming());
  ASSERT_TRUE(rtp_packet.has_type());
  EXPECT_EQ(media_type, GetRuntimeMediaType(rtp_packet.type()));
  ASSERT_TRUE(rtp_packet.has_packet_length());
  EXPECT_EQ(total_size, rtp_packet.packet_length());
  ASSERT_TRUE(rtp_packet.has_header());
  ASSERT_EQ(header_size, rtp_packet.header().size());
  for (size_t i = 0; i < header_size; i++) {
    EXPECT_EQ(header[i], static_cast<uint8_t>(rtp_packet.header()[i]));
  }
}

void VerifyRtcpEvent(const rtclog::Event& event,
                     bool incoming,
                     MediaType media_type,
                     uint8_t* packet,
                     size_t total_size) {
  ASSERT_TRUE(IsValidBasicEvent(event));
  ASSERT_EQ(rtclog::Event::RTCP_EVENT, event.type());
  const rtclog::RtcpPacket& rtcp_packet = event.rtcp_packet();
  ASSERT_TRUE(rtcp_packet.has_incoming());
  EXPECT_EQ(incoming, rtcp_packet.incoming());
  ASSERT_TRUE(rtcp_packet.has_type());
  EXPECT_EQ(media_type, GetRuntimeMediaType(rtcp_packet.type()));
  ASSERT_TRUE(rtcp_packet.has_packet_data());
  ASSERT_EQ(total_size, rtcp_packet.packet_data().size());
  for (size_t i = 0; i < total_size; i++) {
    EXPECT_EQ(packet[i], static_cast<uint8_t>(rtcp_packet.packet_data()[i]));
  }
}

void VerifyPlayoutEvent(const rtclog::Event& event, uint32_t ssrc) {
  ASSERT_TRUE(IsValidBasicEvent(event));
  ASSERT_EQ(rtclog::Event::DEBUG_EVENT, event.type());
  const rtclog::DebugEvent& debug_event = event.debug_event();
  ASSERT_TRUE(debug_event.has_type());
  EXPECT_EQ(rtclog::DebugEvent::AUDIO_PLAYOUT, debug_event.type());
  ASSERT_TRUE(debug_event.has_local_ssrc());
  EXPECT_EQ(ssrc, debug_event.local_ssrc());
}

void VerifyLogStartEvent(const rtclog::Event& event) {
  ASSERT_TRUE(IsValidBasicEvent(event));
  ASSERT_EQ(rtclog::Event::DEBUG_EVENT, event.type());
  const rtclog::DebugEvent& debug_event = event.debug_event();
  ASSERT_TRUE(debug_event.has_type());
  EXPECT_EQ(rtclog::DebugEvent::LOG_START, debug_event.type());
}

/*
 * Bit number i of extension_bitvector is set to indicate the
 * presence of extension number i from kExtensionTypes / kExtensionNames.
 * The least significant bit extension_bitvector has number 0.
 */
size_t GenerateRtpPacket(uint32_t extensions_bitvector,
                         uint32_t csrcs_count,
                         uint8_t* packet,
                         size_t packet_size) {
  RTC_CHECK_GE(packet_size, 16 + 4 * csrcs_count + 4 * kNumExtensions);
  Clock* clock = Clock::GetRealTimeClock();

  RTPSender rtp_sender(false,     // bool audio
                       clock,     // Clock* clock
                       nullptr,   // Transport*
                       nullptr,   // RtpAudioFeedback*
                       nullptr,   // PacedSender*
                       nullptr,   // PacketRouter*
                       nullptr,   // SendTimeObserver*
                       nullptr,   // BitrateStatisticsObserver*
                       nullptr,   // FrameCountObserver*
                       nullptr);  // SendSideDelayObserver*

  std::vector<uint32_t> csrcs;
  for (unsigned i = 0; i < csrcs_count; i++) {
    csrcs.push_back(rand());
  }
  rtp_sender.SetCsrcs(csrcs);
  rtp_sender.SetSSRC(rand());
  rtp_sender.SetStartTimestamp(rand(), true);
  rtp_sender.SetSequenceNumber(rand());

  for (unsigned i = 0; i < kNumExtensions; i++) {
    if (extensions_bitvector & (1u << i)) {
      rtp_sender.RegisterRtpHeaderExtension(kExtensionTypes[i], i + 1);
    }
  }

  int8_t payload_type = rand() % 128;
  bool marker_bit = (rand() % 2 == 1);
  uint32_t capture_timestamp = rand();
  int64_t capture_time_ms = rand();
  bool timestamp_provided = (rand() % 2 == 1);
  bool inc_sequence_number = (rand() % 2 == 1);

  size_t header_size = rtp_sender.BuildRTPheader(
      packet, payload_type, marker_bit, capture_timestamp, capture_time_ms,
      timestamp_provided, inc_sequence_number);

  for (size_t i = header_size; i < packet_size; i++) {
    packet[i] = rand();
  }

  return header_size;
}

void GenerateRtcpPacket(uint8_t* packet, size_t packet_size) {
  for (size_t i = 0; i < packet_size; i++) {
    packet[i] = rand();
  }
}

void GenerateVideoReceiveConfig(uint32_t extensions_bitvector,
                                VideoReceiveStream::Config* config) {
  // Create a map from a payload type to an encoder name.
  VideoReceiveStream::Decoder decoder;
  decoder.payload_type = rand();
  decoder.payload_name = (rand() % 2 ? "VP8" : "H264");
  config->decoders.push_back(decoder);
  // Add SSRCs for the stream.
  config->rtp.remote_ssrc = rand();
  config->rtp.local_ssrc = rand();
  // Add extensions and settings for RTCP.
  config->rtp.rtcp_mode = rand() % 2 ? newapi::kRtcpCompound
                                     : newapi::kRtcpReducedSize;
  config->rtp.rtcp_xr.receiver_reference_time_report = (rand() % 2 == 1);
  config->rtp.remb = (rand() % 2 == 1);
  // Add a map from a payload type to a new ssrc and a new payload type for RTX.
  VideoReceiveStream::Config::Rtp::Rtx rtx_pair;
  rtx_pair.ssrc = rand();
  rtx_pair.payload_type = rand();
  config->rtp.rtx.insert(std::make_pair(rand(), rtx_pair));
  // Add header extensions.
  for (unsigned i = 0; i < kNumExtensions; i++) {
    if (extensions_bitvector & (1u << i)) {
      config->rtp.extensions.push_back(
          RtpExtension(kExtensionNames[i], rand()));
    }
  }
}

void GenerateVideoSendConfig(uint32_t extensions_bitvector,
                             VideoSendStream::Config* config) {
  // Create a map from a payload type to an encoder name.
  config->encoder_settings.payload_type = rand();
  config->encoder_settings.payload_name = (rand() % 2 ? "VP8" : "H264");
  // Add SSRCs for the stream.
  config->rtp.ssrcs.push_back(rand());
  // Add a map from a payload type to new ssrcs and a new payload type for RTX.
  config->rtp.rtx.ssrcs.push_back(rand());
  config->rtp.rtx.payload_type = rand();
  // Add a CNAME.
  config->rtp.c_name = "some.user@some.host";
  // Add header extensions.
  for (unsigned i = 0; i < kNumExtensions; i++) {
    if (extensions_bitvector & (1u << i)) {
      config->rtp.extensions.push_back(
          RtpExtension(kExtensionNames[i], rand()));
    }
  }
}

// Test for the RtcEventLog class. Dumps some RTP packets to disk, then reads
// them back to see if they match.
void LogSessionAndReadBack(size_t rtp_count,
                           size_t rtcp_count,
                           size_t debug_count,
                           uint32_t extensions_bitvector,
                           uint32_t csrcs_count,
                           unsigned random_seed) {
  ASSERT_LE(rtcp_count, rtp_count);
  ASSERT_LE(debug_count, rtp_count);
  std::vector<rtc::Buffer> rtp_packets;
  std::vector<rtc::Buffer> rtcp_packets;
  std::vector<size_t> rtp_header_sizes;
  std::vector<uint32_t> playout_ssrcs;

  VideoReceiveStream::Config receiver_config(nullptr);
  VideoSendStream::Config sender_config(nullptr);

  srand(random_seed);

  // Create rtp_count RTP packets containing random data.
  for (size_t i = 0; i < rtp_count; i++) {
    size_t packet_size = 1000 + rand() % 64;
    rtp_packets.push_back(rtc::Buffer(packet_size));
    size_t header_size = GenerateRtpPacket(extensions_bitvector, csrcs_count,
                                           rtp_packets[i].data(), packet_size);
    rtp_header_sizes.push_back(header_size);
  }
  // Create rtcp_count RTCP packets containing random data.
  for (size_t i = 0; i < rtcp_count; i++) {
    size_t packet_size = 1000 + rand() % 64;
    rtcp_packets.push_back(rtc::Buffer(packet_size));
    GenerateRtcpPacket(rtcp_packets[i].data(), packet_size);
  }
  // Create debug_count random SSRCs to use when logging AudioPlayout events.
  for (size_t i = 0; i < debug_count; i++) {
    playout_ssrcs.push_back(static_cast<uint32_t>(rand()));
  }
  // Create configurations for the video streams.
  GenerateVideoReceiveConfig(extensions_bitvector, &receiver_config);
  GenerateVideoSendConfig(extensions_bitvector, &sender_config);
  const int config_count = 2;

  // Find the name of the current test, in order to use it as a temporary
  // filename.
  auto test_info = ::testing::UnitTest::GetInstance()->current_test_info();
  const std::string temp_filename =
      test::OutputPath() + test_info->test_case_name() + test_info->name();

  // When log_dumper goes out of scope, it causes the log file to be flushed
  // to disk.
  {
    rtc::scoped_ptr<RtcEventLog> log_dumper(RtcEventLog::Create());
    log_dumper->LogVideoReceiveStreamConfig(receiver_config);
    log_dumper->LogVideoSendStreamConfig(sender_config);
    size_t rtcp_index = 1, debug_index = 1;
    for (size_t i = 1; i <= rtp_count; i++) {
      log_dumper->LogRtpHeader(
          (i % 2 == 0),  // Every second packet is incoming.
          (i % 3 == 0) ? MediaType::AUDIO : MediaType::VIDEO,
          rtp_packets[i - 1].data(), rtp_packets[i - 1].size());
      if (i * rtcp_count >= rtcp_index * rtp_count) {
        log_dumper->LogRtcpPacket(
            rtcp_index % 2 == 0,  // Every second packet is incoming
            rtcp_index % 3 == 0 ? MediaType::AUDIO : MediaType::VIDEO,
            rtcp_packets[rtcp_index - 1].data(),
            rtcp_packets[rtcp_index - 1].size());
        rtcp_index++;
      }
      if (i * debug_count >= debug_index * rtp_count) {
        log_dumper->LogAudioPlayout(playout_ssrcs[debug_index - 1]);
        debug_index++;
      }
      if (i == rtp_count / 2) {
        log_dumper->StartLogging(temp_filename, 10000000);
      }
    }
  }

  // Read the generated file from disk.
  rtclog::EventStream parsed_stream;

  ASSERT_TRUE(RtcEventLog::ParseRtcEventLog(temp_filename, &parsed_stream));

  // Verify the result.
  const int event_count =
      config_count + debug_count + rtcp_count + rtp_count + 1;
  EXPECT_EQ(event_count, parsed_stream.stream_size());
  VerifyReceiveStreamConfig(parsed_stream.stream(0), receiver_config);
  VerifySendStreamConfig(parsed_stream.stream(1), sender_config);
  size_t event_index = config_count, rtcp_index = 1, debug_index = 1;
  for (size_t i = 1; i <= rtp_count; i++) {
    VerifyRtpEvent(parsed_stream.stream(event_index),
                   (i % 2 == 0),  // Every second packet is incoming.
                   (i % 3 == 0) ? MediaType::AUDIO : MediaType::VIDEO,
                   rtp_packets[i - 1].data(), rtp_header_sizes[i - 1],
                   rtp_packets[i - 1].size());
    event_index++;
    if (i * rtcp_count >= rtcp_index * rtp_count) {
      VerifyRtcpEvent(parsed_stream.stream(event_index),
                      rtcp_index % 2 == 0,  // Every second packet is incoming.
                      rtcp_index % 3 == 0 ? MediaType::AUDIO : MediaType::VIDEO,
                      rtcp_packets[rtcp_index - 1].data(),
                      rtcp_packets[rtcp_index - 1].size());
      event_index++;
      rtcp_index++;
    }
    if (i * debug_count >= debug_index * rtp_count) {
      VerifyPlayoutEvent(parsed_stream.stream(event_index),
                         playout_ssrcs[debug_index - 1]);
      event_index++;
      debug_index++;
    }
    if (i == rtp_count / 2) {
      VerifyLogStartEvent(parsed_stream.stream(event_index));
      event_index++;
    }
  }

  // Clean up temporary file - can be pretty slow.
  remove(temp_filename.c_str());
}

TEST(RtcEventLogTest, LogSessionAndReadBack) {
  // Log 5 RTP, 2 RTCP, and 0 playout events with no header extensions or CSRCS.
  LogSessionAndReadBack(5, 2, 0, 0, 0, 321);

  // Enable AbsSendTime and TransportSequenceNumbers
  uint32_t extensions = 0;
  for (uint32_t i = 0; i < kNumExtensions; i++) {
    if (kExtensionTypes[i] == RTPExtensionType::kRtpExtensionAbsoluteSendTime ||
        kExtensionTypes[i] ==
            RTPExtensionType::kRtpExtensionTransportSequenceNumber) {
      extensions |= 1u << i;
    }
  }
  LogSessionAndReadBack(8, 2, 0, extensions, 0, 3141592653u);

  extensions = (1u << kNumExtensions) - 1;  // Enable all header extensions
  LogSessionAndReadBack(9, 2, 3, extensions, 2, 2718281828u);

  // Try all combinations of header extensions and up to 2 CSRCS.
  for (extensions = 0; extensions < (1u << kNumExtensions); extensions++) {
    for (uint32_t csrcs_count = 0; csrcs_count < 3; csrcs_count++) {
      LogSessionAndReadBack(5 + extensions,   // Number of RTP packets.
                            2 + csrcs_count,  // Number of RTCP packets.
                            3 + csrcs_count,  // Number of playout events
                            extensions,       // Bit vector choosing extensions
                            csrcs_count,      // Number of contributing sources
                            rand());
    }
  }
}

}  // namespace webrtc

#endif  // ENABLE_RTC_EVENT_LOG
