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

#include "webrtc/audio/audio_receive_stream.h"
#include "webrtc/audio/conversion.h"
#include "webrtc/modules/remote_bitrate_estimator/include/mock/mock_remote_bitrate_estimator.h"
#include "webrtc/modules/rtp_rtcp/source/byte_io.h"
#include "webrtc/test/mock_voice_engine.h"

namespace webrtc {
namespace test {
namespace {

AudioDecodingCallStats MakeAudioDecodeStatsForTest() {
  AudioDecodingCallStats audio_decode_stats;
  audio_decode_stats.calls_to_silence_generator = 234;
  audio_decode_stats.calls_to_neteq = 567;
  audio_decode_stats.decoded_normal = 890;
  audio_decode_stats.decoded_plc = 123;
  audio_decode_stats.decoded_cng = 456;
  audio_decode_stats.decoded_plc_cng = 789;
  return audio_decode_stats;
}

const int kChannelId = 2;
const uint32_t kRemoteSsrc = 1234;
const uint32_t kLocalSsrc = 5678;
const size_t kAbsoluteSendTimeLength = 4;
const int kAbsSendTimeId = 3;
const int kJitterBufferDelay = -7;
const int kPlayoutBufferDelay = 302;
const unsigned int kSpeechOutputLevel = 99;
const CallStatistics kCallStats = {
    345,  678,  901, 234, -12, 3456, 7890, 567, 890, 123};
const CodecInst kCodecInst = {
    123, "codec_name_recv", 96000, -187, -198, -103};
const NetworkStatistics kNetworkStats = {
    123, 456, false, 0, 0, 789, 12, 345, 678, 901, -1, -1, -1, -1, -1, 0};
const AudioDecodingCallStats kAudioDecodeStats = MakeAudioDecodeStatsForTest();

struct ConfigHelper {
  ConfigHelper() {
    EXPECT_CALL(voice_engine_,
        RegisterVoiceEngineObserver(testing::_)).WillOnce(testing::Return(0));
    EXPECT_CALL(voice_engine_,
        DeRegisterVoiceEngineObserver()).WillOnce(testing::Return(0));
    AudioState::Config config;
    config.voice_engine = &voice_engine_;
    audio_state_ = AudioState::Create(config);
    stream_config_.voe_channel_id = kChannelId;
    stream_config_.rtp.local_ssrc = kLocalSsrc;
    stream_config_.rtp.remote_ssrc = kRemoteSsrc;
  }

  MockRemoteBitrateEstimator* remote_bitrate_estimator() {
    return &remote_bitrate_estimator_;
  }
  AudioReceiveStream::Config& config() { return stream_config_; }
  rtc::scoped_refptr<AudioState> audio_state() { return audio_state_; }
  MockVoiceEngine& voice_engine() { return voice_engine_; }

  void SetupMockForGetStats() {
    using testing::_;
    using testing::DoAll;
    using testing::Return;
    using testing::SetArgPointee;
    using testing::SetArgReferee;
    EXPECT_CALL(voice_engine_, GetRemoteSSRC(kChannelId, _))
        .WillOnce(DoAll(SetArgReferee<1>(0), Return(0)));
    EXPECT_CALL(voice_engine_, GetRTCPStatistics(kChannelId, _))
        .WillOnce(DoAll(SetArgReferee<1>(kCallStats), Return(0)));
    EXPECT_CALL(voice_engine_, GetRecCodec(kChannelId, _))
        .WillOnce(DoAll(SetArgReferee<1>(kCodecInst), Return(0)));
    EXPECT_CALL(voice_engine_, GetDelayEstimate(kChannelId, _, _))
        .WillOnce(DoAll(SetArgPointee<1>(kJitterBufferDelay),
                        SetArgPointee<2>(kPlayoutBufferDelay), Return(0)));
    EXPECT_CALL(voice_engine_,
        GetSpeechOutputLevelFullRange(kChannelId, _)).WillOnce(
            DoAll(SetArgReferee<1>(kSpeechOutputLevel), Return(0)));
    EXPECT_CALL(voice_engine_, GetNetworkStatistics(kChannelId, _))
        .WillOnce(DoAll(SetArgReferee<1>(kNetworkStats), Return(0)));
    EXPECT_CALL(voice_engine_, GetDecodingCallStatistics(kChannelId, _))
        .WillOnce(DoAll(SetArgPointee<1>(kAudioDecodeStats), Return(0)));
  }

 private:
  MockRemoteBitrateEstimator remote_bitrate_estimator_;
  MockVoiceEngine voice_engine_;
  rtc::scoped_refptr<AudioState> audio_state_;
  AudioReceiveStream::Config stream_config_;
};

void BuildAbsoluteSendTimeExtension(uint8_t* buffer,
                                    int id,
                                    uint32_t abs_send_time) {
  const size_t kRtpOneByteHeaderLength = 4;
  const uint16_t kRtpOneByteHeaderExtensionId = 0xBEDE;
  ByteWriter<uint16_t>::WriteBigEndian(buffer, kRtpOneByteHeaderExtensionId);

  const uint32_t kPosLength = 2;
  ByteWriter<uint16_t>::WriteBigEndian(buffer + kPosLength,
                                       kAbsoluteSendTimeLength / 4);

  const uint8_t kLengthOfData = 3;
  buffer[kRtpOneByteHeaderLength] = (id << 4) + (kLengthOfData - 1);
  ByteWriter<uint32_t, kLengthOfData>::WriteBigEndian(
      buffer + kRtpOneByteHeaderLength + 1, abs_send_time);
}

size_t CreateRtpHeaderWithAbsSendTime(uint8_t* header,
                                      int extension_id,
                                      uint32_t abs_send_time) {
  header[0] = 0x80;   // Version 2.
  header[0] |= 0x10;  // Set extension bit.
  header[1] = 100;    // Payload type.
  header[1] |= 0x80;  // Marker bit is set.
  ByteWriter<uint16_t>::WriteBigEndian(header + 2, 0x1234);  // Sequence number.
  ByteWriter<uint32_t>::WriteBigEndian(header + 4, 0x5678);  // Timestamp.
  ByteWriter<uint32_t>::WriteBigEndian(header + 8, 0x4321);  // SSRC.
  int32_t rtp_header_length = webrtc::kRtpHeaderSize;

  BuildAbsoluteSendTimeExtension(header + rtp_header_length, extension_id,
                                 abs_send_time);
  rtp_header_length += kAbsoluteSendTimeLength;
  return rtp_header_length;
}
}  // namespace

TEST(AudioReceiveStreamTest, ConfigToString) {
  AudioReceiveStream::Config config;
  config.rtp.remote_ssrc = kRemoteSsrc;
  config.rtp.local_ssrc = kLocalSsrc;
  config.rtp.extensions.push_back(
      RtpExtension(RtpExtension::kAbsSendTime, kAbsSendTimeId));
  config.voe_channel_id = kChannelId;
  config.combined_audio_video_bwe = true;
  EXPECT_EQ(
      "{rtp: {remote_ssrc: 1234, local_ssrc: 5678, extensions: [{name: "
      "http://www.webrtc.org/experiments/rtp-hdrext/abs-send-time, id: 3}]}, "
      "receive_transport: nullptr, rtcp_send_transport: nullptr, "
      "voe_channel_id: 2, combined_audio_video_bwe: true}",
      config.ToString());
}

TEST(AudioReceiveStreamTest, ConstructDestruct) {
  ConfigHelper helper;
  internal::AudioReceiveStream recv_stream(
      helper.remote_bitrate_estimator(), helper.config(), helper.audio_state());
}

TEST(AudioReceiveStreamTest, AudioPacketUpdatesBweWithTimestamp) {
  ConfigHelper helper;
  helper.config().combined_audio_video_bwe = true;
  helper.config().rtp.extensions.push_back(
      RtpExtension(RtpExtension::kAbsSendTime, kAbsSendTimeId));
  internal::AudioReceiveStream recv_stream(
      helper.remote_bitrate_estimator(), helper.config(), helper.audio_state());
  uint8_t rtp_packet[30];
  const int kAbsSendTimeValue = 1234;
  CreateRtpHeaderWithAbsSendTime(rtp_packet, kAbsSendTimeId, kAbsSendTimeValue);
  PacketTime packet_time(5678000, 0);
  const size_t kExpectedHeaderLength = 20;
  EXPECT_CALL(*helper.remote_bitrate_estimator(),
              IncomingPacket(packet_time.timestamp / 1000,
                             sizeof(rtp_packet) - kExpectedHeaderLength,
                             testing::_, false))
      .Times(1);
  EXPECT_TRUE(
      recv_stream.DeliverRtp(rtp_packet, sizeof(rtp_packet), packet_time));
}

TEST(AudioReceiveStreamTest, GetStats) {
  ConfigHelper helper;
  internal::AudioReceiveStream recv_stream(
      helper.remote_bitrate_estimator(), helper.config(), helper.audio_state());
  helper.SetupMockForGetStats();
  AudioReceiveStream::Stats stats = recv_stream.GetStats();
  EXPECT_EQ(kRemoteSsrc, stats.remote_ssrc);
  EXPECT_EQ(static_cast<int64_t>(kCallStats.bytesReceived), stats.bytes_rcvd);
  EXPECT_EQ(static_cast<uint32_t>(kCallStats.packetsReceived),
            stats.packets_rcvd);
  EXPECT_EQ(kCallStats.cumulativeLost, stats.packets_lost);
  EXPECT_EQ(Q8ToFloat(kCallStats.fractionLost), stats.fraction_lost);
  EXPECT_EQ(std::string(kCodecInst.plname), stats.codec_name);
  EXPECT_EQ(kCallStats.extendedMax, stats.ext_seqnum);
  EXPECT_EQ(kCallStats.jitterSamples / (kCodecInst.plfreq / 1000),
            stats.jitter_ms);
  EXPECT_EQ(kNetworkStats.currentBufferSize, stats.jitter_buffer_ms);
  EXPECT_EQ(kNetworkStats.preferredBufferSize,
            stats.jitter_buffer_preferred_ms);
  EXPECT_EQ(static_cast<uint32_t>(kJitterBufferDelay + kPlayoutBufferDelay),
            stats.delay_estimate_ms);
  EXPECT_EQ(static_cast<int32_t>(kSpeechOutputLevel), stats.audio_level);
  EXPECT_EQ(Q14ToFloat(kNetworkStats.currentExpandRate), stats.expand_rate);
  EXPECT_EQ(Q14ToFloat(kNetworkStats.currentSpeechExpandRate),
            stats.speech_expand_rate);
  EXPECT_EQ(Q14ToFloat(kNetworkStats.currentSecondaryDecodedRate),
            stats.secondary_decoded_rate);
  EXPECT_EQ(Q14ToFloat(kNetworkStats.currentAccelerateRate),
            stats.accelerate_rate);
  EXPECT_EQ(Q14ToFloat(kNetworkStats.currentPreemptiveRate),
            stats.preemptive_expand_rate);
  EXPECT_EQ(kAudioDecodeStats.calls_to_silence_generator,
            stats.decoding_calls_to_silence_generator);
  EXPECT_EQ(kAudioDecodeStats.calls_to_neteq, stats.decoding_calls_to_neteq);
  EXPECT_EQ(kAudioDecodeStats.decoded_normal, stats.decoding_normal);
  EXPECT_EQ(kAudioDecodeStats.decoded_plc, stats.decoding_plc);
  EXPECT_EQ(kAudioDecodeStats.decoded_cng, stats.decoding_cng);
  EXPECT_EQ(kAudioDecodeStats.decoded_plc_cng, stats.decoding_plc_cng);
  EXPECT_EQ(kCallStats.capture_start_ntp_time_ms_,
            stats.capture_start_ntp_time_ms);
}
}  // namespace test
}  // namespace webrtc
