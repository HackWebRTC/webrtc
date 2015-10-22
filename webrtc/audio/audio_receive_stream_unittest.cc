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
#include "webrtc/test/fake_voice_engine.h"

namespace {

using webrtc::ByteWriter;

const size_t kAbsoluteSendTimeLength = 4;

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

namespace webrtc {
namespace test {

TEST(AudioReceiveStreamTest, AudioPacketUpdatesBweWithTimestamp) {
  MockRemoteBitrateEstimator remote_bitrate_estimator;
  FakeVoiceEngine voice_engine;
  AudioReceiveStream::Config config;
  config.combined_audio_video_bwe = true;
  config.voe_channel_id = voice_engine.kReceiveChannelId;
  const int kAbsSendTimeId = 3;
  config.rtp.extensions.push_back(
      RtpExtension(RtpExtension::kAbsSendTime, kAbsSendTimeId));
  internal::AudioReceiveStream recv_stream(&remote_bitrate_estimator, config,
                                           &voice_engine);
  uint8_t rtp_packet[30];
  const int kAbsSendTimeValue = 1234;
  CreateRtpHeaderWithAbsSendTime(rtp_packet, kAbsSendTimeId, kAbsSendTimeValue);
  PacketTime packet_time(5678000, 0);
  const size_t kExpectedHeaderLength = 20;
  EXPECT_CALL(remote_bitrate_estimator,
      IncomingPacket(packet_time.timestamp / 1000,
          sizeof(rtp_packet) - kExpectedHeaderLength, testing::_, false))
      .Times(1);
  EXPECT_TRUE(
      recv_stream.DeliverRtp(rtp_packet, sizeof(rtp_packet), packet_time));
}

TEST(AudioReceiveStreamTest, GetStats) {
  const uint32_t kSsrc1 = 667;

  MockRemoteBitrateEstimator remote_bitrate_estimator;
  FakeVoiceEngine voice_engine;
  AudioReceiveStream::Config config;
  config.rtp.remote_ssrc = kSsrc1;
  config.voe_channel_id = voice_engine.kReceiveChannelId;
  internal::AudioReceiveStream recv_stream(&remote_bitrate_estimator, config,
                                           &voice_engine);

  AudioReceiveStream::Stats stats = recv_stream.GetStats();
  const CallStatistics& call_stats = voice_engine.GetRecvCallStats();
  const CodecInst& codec_inst = voice_engine.GetRecvRecCodecInst();
  const NetworkStatistics& net_stats = voice_engine.GetRecvNetworkStats();
  const AudioDecodingCallStats& decode_stats =
      voice_engine.GetRecvAudioDecodingCallStats();
  EXPECT_EQ(kSsrc1, stats.remote_ssrc);
  EXPECT_EQ(static_cast<int64_t>(call_stats.bytesReceived), stats.bytes_rcvd);
  EXPECT_EQ(static_cast<uint32_t>(call_stats.packetsReceived),
            stats.packets_rcvd);
  EXPECT_EQ(call_stats.cumulativeLost, stats.packets_lost);
  EXPECT_EQ(static_cast<float>(call_stats.fractionLost) / 256,
            stats.fraction_lost);
  EXPECT_EQ(std::string(codec_inst.plname), stats.codec_name);
  EXPECT_EQ(call_stats.extendedMax, stats.ext_seqnum);
  EXPECT_EQ(call_stats.jitterSamples / (codec_inst.plfreq / 1000),
            stats.jitter_ms);
  EXPECT_EQ(net_stats.currentBufferSize, stats.jitter_buffer_ms);
  EXPECT_EQ(net_stats.preferredBufferSize, stats.jitter_buffer_preferred_ms);
  EXPECT_EQ(static_cast<uint32_t>(voice_engine.kRecvJitterBufferDelay +
      voice_engine.kRecvPlayoutBufferDelay), stats.delay_estimate_ms);
  EXPECT_EQ(static_cast<int32_t>(voice_engine.kRecvSpeechOutputLevel),
            stats.audio_level);
  EXPECT_EQ(Q14ToFloat(net_stats.currentExpandRate), stats.expand_rate);
  EXPECT_EQ(Q14ToFloat(net_stats.currentSpeechExpandRate),
            stats.speech_expand_rate);
  EXPECT_EQ(Q14ToFloat(net_stats.currentSecondaryDecodedRate),
            stats.secondary_decoded_rate);
  EXPECT_EQ(Q14ToFloat(net_stats.currentAccelerateRate), stats.accelerate_rate);
  EXPECT_EQ(Q14ToFloat(net_stats.currentPreemptiveRate),
            stats.preemptive_expand_rate);
  EXPECT_EQ(decode_stats.calls_to_silence_generator,
            stats.decoding_calls_to_silence_generator);
  EXPECT_EQ(decode_stats.calls_to_neteq, stats.decoding_calls_to_neteq);
  EXPECT_EQ(decode_stats.decoded_normal, stats.decoding_normal);
  EXPECT_EQ(decode_stats.decoded_plc, stats.decoding_plc);
  EXPECT_EQ(decode_stats.decoded_cng, stats.decoding_cng);
  EXPECT_EQ(decode_stats.decoded_plc_cng, stats.decoding_plc_cng);
  EXPECT_EQ(call_stats.capture_start_ntp_time_ms_,
            stats.capture_start_ntp_time_ms);
}
}  // namespace test
}  // namespace webrtc
