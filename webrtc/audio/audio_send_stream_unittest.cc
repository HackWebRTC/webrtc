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
#include "webrtc/test/mock_voice_engine.h"

namespace webrtc {
namespace test {
namespace {

const int kChannelId = 1;
const uint32_t kSsrc = 1234;
}  // namespace

TEST(AudioSendStreamTest, ConfigToString) {
  const int kAbsSendTimeId = 3;
  AudioSendStream::Config config(nullptr);
  config.rtp.ssrc = kSsrc;
  config.rtp.extensions.push_back(
      RtpExtension(RtpExtension::kAbsSendTime, kAbsSendTimeId));
  config.voe_channel_id = kChannelId;
  config.cng_payload_type = 42;
  config.red_payload_type = 17;
  EXPECT_EQ(
      "{rtp: {ssrc: 1234, extensions: [{name: "
      "http://www.webrtc.org/experiments/rtp-hdrext/abs-send-time, id: 3}]}, "
      "voe_channel_id: 1, cng_payload_type: 42, red_payload_type: 17}",
      config.ToString());
}

TEST(AudioSendStreamTest, ConstructDestruct) {
  MockVoiceEngine voice_engine;
  AudioSendStream::Config config(nullptr);
  config.voe_channel_id = kChannelId;
  internal::AudioSendStream send_stream(config, &voice_engine);
}

TEST(AudioSendStreamTest, GetStats) {
  const int kEchoDelayMedian = 254;
  const int kEchoDelayStdDev = -3;
  const int kEchoReturnLoss = -65;
  const int kEchoReturnLossEnhancement = 101;
  const unsigned int kSpeechInputLevel = 96;

  const CallStatistics kCallStats = {1345,  1678,  1901, 1234,  112,
                                     13456, 17890, 1567, -1890, -1123};

  const CodecInst kCodecInst = {-121, "codec_name_send", 48000, -231, -451,
                                -671};

  const ReportBlock kReportBlock = {456, 780, 123, 567, 890, 132, 143, 13354};

  std::vector<ReportBlock> report_blocks;
  {
    webrtc::ReportBlock block = kReportBlock;
    report_blocks.push_back(block);  // Has wrong SSRC.
    block.source_SSRC = kSsrc;
    report_blocks.push_back(block);  // Correct block.
    block.fraction_lost = 0;
    report_blocks.push_back(block);  // Duplicate SSRC, bad fraction_lost.
  }

  MockVoiceEngine voice_engine;
  AudioSendStream::Config config(nullptr);
  config.rtp.ssrc = kSsrc;
  config.voe_channel_id = kChannelId;
  internal::AudioSendStream send_stream(config, &voice_engine);

  using testing::_;
  using testing::DoAll;
  using testing::Return;
  using testing::SetArgPointee;
  using testing::SetArgReferee;
  EXPECT_CALL(voice_engine, GetLocalSSRC(kChannelId, _))
      .WillOnce(DoAll(SetArgReferee<1>(0), Return(0)));
  EXPECT_CALL(voice_engine, GetRTCPStatistics(kChannelId, _))
      .WillOnce(DoAll(SetArgReferee<1>(kCallStats), Return(0)));
  EXPECT_CALL(voice_engine, GetSendCodec(kChannelId, _))
      .WillOnce(DoAll(SetArgReferee<1>(kCodecInst), Return(0)));
  EXPECT_CALL(voice_engine, GetRemoteRTCPReportBlocks(kChannelId, _))
      .WillOnce(DoAll(SetArgPointee<1>(report_blocks), Return(0)));
  EXPECT_CALL(voice_engine, GetSpeechInputLevelFullRange(_))
      .WillOnce(DoAll(SetArgReferee<0>(kSpeechInputLevel), Return(0)));
  EXPECT_CALL(voice_engine, GetEcMetricsStatus(_))
      .WillOnce(DoAll(SetArgReferee<0>(true), Return(0)));
  EXPECT_CALL(voice_engine, GetEchoMetrics(_, _, _, _))
      .WillOnce(DoAll(SetArgReferee<0>(kEchoReturnLoss),
                      SetArgReferee<1>(kEchoReturnLossEnhancement), Return(0)));
  EXPECT_CALL(voice_engine, GetEcDelayMetrics(_, _, _))
      .WillOnce(DoAll(SetArgReferee<0>(kEchoDelayMedian),
                      SetArgReferee<1>(kEchoDelayStdDev), Return(0)));

  AudioSendStream::Stats stats = send_stream.GetStats();
  EXPECT_EQ(kSsrc, stats.local_ssrc);
  EXPECT_EQ(static_cast<int64_t>(kCallStats.bytesSent), stats.bytes_sent);
  EXPECT_EQ(kCallStats.packetsSent, stats.packets_sent);
  EXPECT_EQ(static_cast<int32_t>(kReportBlock.cumulative_num_packets_lost),
            stats.packets_lost);
  EXPECT_EQ(Q8ToFloat(kReportBlock.fraction_lost), stats.fraction_lost);
  EXPECT_EQ(std::string(kCodecInst.plname), stats.codec_name);
  EXPECT_EQ(static_cast<int32_t>(kReportBlock.extended_highest_sequence_number),
            stats.ext_seqnum);
  EXPECT_EQ(static_cast<int32_t>(kReportBlock.interarrival_jitter /
                                 (kCodecInst.plfreq / 1000)),
            stats.jitter_ms);
  EXPECT_EQ(kCallStats.rttMs, stats.rtt_ms);
  EXPECT_EQ(static_cast<int32_t>(kSpeechInputLevel), stats.audio_level);
  EXPECT_EQ(-1, stats.aec_quality_min);
  EXPECT_EQ(kEchoDelayMedian, stats.echo_delay_median_ms);
  EXPECT_EQ(kEchoDelayStdDev, stats.echo_delay_std_ms);
  EXPECT_EQ(kEchoReturnLoss, stats.echo_return_loss);
  EXPECT_EQ(kEchoReturnLossEnhancement, stats.echo_return_loss_enhancement);
  EXPECT_FALSE(stats.typing_noise_detected);
}
}  // namespace test
}  // namespace webrtc
