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
#include "webrtc/modules/remote_bitrate_estimator/include/mock/mock_remote_bitrate_estimator.h"
#include "webrtc/modules/rtp_rtcp/source/byte_io.h"

namespace webrtc {

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
  int32_t rtp_header_length = kRtpHeaderSize;

  BuildAbsoluteSendTimeExtension(header + rtp_header_length, extension_id,
                                 abs_send_time);
  rtp_header_length += kAbsoluteSendTimeLength;
  return rtp_header_length;
}

TEST(AudioReceiveStreamTest, AudioPacketUpdatesBweWithTimestamp) {
  MockRemoteBitrateEstimator rbe;
  AudioReceiveStream::Config config;
  config.combined_audio_video_bwe = true;
  config.voe_channel_id = 1;
  const int kAbsSendTimeId = 3;
  config.rtp.extensions.push_back(
      RtpExtension(RtpExtension::kAbsSendTime, kAbsSendTimeId));
  internal::AudioReceiveStream recv_stream(&rbe, config);
  uint8_t rtp_packet[30];
  const int kAbsSendTimeValue = 1234;
  CreateRtpHeaderWithAbsSendTime(rtp_packet, kAbsSendTimeId, kAbsSendTimeValue);
  PacketTime packet_time(5678000, 0);
  const size_t kExpectedHeaderLength = 20;
  EXPECT_CALL(rbe, IncomingPacket(packet_time.timestamp / 1000,
                                  sizeof(rtp_packet) - kExpectedHeaderLength,
                                  testing::_, false))
      .Times(1);
  EXPECT_TRUE(
      recv_stream.DeliverRtp(rtp_packet, sizeof(rtp_packet), packet_time));
}
}  // namespace webrtc
