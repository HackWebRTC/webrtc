/*
 *  Copyright (c) 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "testing/gtest/include/gtest/gtest.h"

#include "webrtc/audio/audio_send_stream.h"
#include "webrtc/audio/conversion.h"
#include "webrtc/test/fake_voice_engine.h"

namespace webrtc {
namespace test {

TEST(AudioSendStreamTest, ConfigToString) {
  const int kAbsSendTimeId = 3;
  AudioSendStream::Config config(nullptr);
  config.rtp.ssrc = 1234;
  config.rtp.extensions.push_back(
      RtpExtension(RtpExtension::kAbsSendTime, kAbsSendTimeId));
  config.voe_channel_id = 1;
  config.cng_payload_type = 42;
  config.red_payload_type = 17;
  EXPECT_EQ("{rtp: {ssrc: 1234, extensions: [{name: "
      "http://www.webrtc.org/experiments/rtp-hdrext/abs-send-time, id: 3}]}, "
      "voe_channel_id: 1, cng_payload_type: 42, red_payload_type: 17}",
      config.ToString());
}

TEST(AudioSendStreamTest, ConstructDestruct) {
  FakeVoiceEngine voice_engine;
  AudioSendStream::Config config(nullptr);
  config.voe_channel_id = 1;
  internal::AudioSendStream send_stream(config, &voice_engine);
}

TEST(AudioSendStreamTest, GetStats) {
  FakeVoiceEngine voice_engine;
  AudioSendStream::Config config(nullptr);
  config.rtp.ssrc = FakeVoiceEngine::kSendSsrc;
  config.voe_channel_id = FakeVoiceEngine::kSendChannelId;
  internal::AudioSendStream send_stream(config, &voice_engine);

  AudioSendStream::Stats stats = send_stream.GetStats();
  const CallStatistics& call_stats = FakeVoiceEngine::kSendCallStats;
  const CodecInst& codec_inst = FakeVoiceEngine::kSendCodecInst;
  const ReportBlock& report_block = FakeVoiceEngine::kSendReportBlock;
  EXPECT_EQ(FakeVoiceEngine::kSendSsrc, stats.local_ssrc);
  EXPECT_EQ(static_cast<int64_t>(call_stats.bytesSent), stats.bytes_sent);
  EXPECT_EQ(call_stats.packetsSent, stats.packets_sent);
  EXPECT_EQ(static_cast<int32_t>(report_block.cumulative_num_packets_lost),
            stats.packets_lost);
  EXPECT_EQ(Q8ToFloat(report_block.fraction_lost), stats.fraction_lost);
  EXPECT_EQ(std::string(codec_inst.plname), stats.codec_name);
  EXPECT_EQ(static_cast<int32_t>(report_block.extended_highest_sequence_number),
            stats.ext_seqnum);
  EXPECT_EQ(static_cast<int32_t>(report_block.interarrival_jitter /
                (codec_inst.plfreq / 1000)), stats.jitter_ms);
  EXPECT_EQ(call_stats.rttMs, stats.rtt_ms);
  EXPECT_EQ(static_cast<int32_t>(FakeVoiceEngine::kSendSpeechInputLevel),
            stats.audio_level);
  EXPECT_EQ(-1, stats.aec_quality_min);
  EXPECT_EQ(FakeVoiceEngine::kSendEchoDelayMedian, stats.echo_delay_median_ms);
  EXPECT_EQ(FakeVoiceEngine::kSendEchoDelayStdDev, stats.echo_delay_std_ms);
  EXPECT_EQ(FakeVoiceEngine::kSendEchoReturnLoss, stats.echo_return_loss);
  EXPECT_EQ(FakeVoiceEngine::kSendEchoReturnLossEnhancement,
            stats.echo_return_loss_enhancement);
  EXPECT_FALSE(stats.typing_noise_detected);
}
}  // namespace test
}  // namespace webrtc
