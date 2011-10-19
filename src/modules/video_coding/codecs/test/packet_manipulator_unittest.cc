/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#include "gtest/gtest.h"
#include "packet_manipulator.h"
#include "typedefs.h"
#include "unittest_utils.h"
#include "video_codec_interface.h"

namespace webrtc {
namespace test {

class PacketManipulatorTest: public PacketRelatedTest {
 protected:
  PacketReader packet_reader_;
  EncodedImage image_;
  const double kNeverDropProbability = 0.0;
  const double kAlwaysDropProbability = 1.0;
  const int kBurstLength = 1;
  NetworkingConfig drop_config_;
  NetworkingConfig no_drop_config_;

  PacketManipulatorTest() {
    // To avoid warnings when using ASSERT_DEATH
    ::testing::FLAGS_gtest_death_test_style = "threadsafe";

    image_._buffer = packet_data_;
    image_._length = kPacketDataLength;
    image_._size = kPacketDataLength;

    drop_config_.packet_size_in_bytes = kPacketSizeInBytes;
    drop_config_.packet_loss_probability = kAlwaysDropProbability;
    drop_config_.packet_loss_burst_length = kBurstLength;
    drop_config_.packet_loss_mode = kUniform;

    no_drop_config_.packet_size_in_bytes = kPacketSizeInBytes;
    no_drop_config_.packet_loss_probability = kNeverDropProbability;
    no_drop_config_.packet_loss_burst_length = kBurstLength;
    no_drop_config_.packet_loss_mode = kUniform;
  }

  virtual ~PacketManipulatorTest() {
  }

  void SetUp() {
    PacketRelatedTest::SetUp();
  }

  void TearDown() {
    PacketRelatedTest::TearDown();
  }

  void VerifyPacketLoss(int expected_nbr_packets_dropped,
                        int actual_nbr_packets_dropped,
                        int expected_packet_data_length,
                        WebRtc_UWord8* expected_packet_data,
                        EncodedImage& actual_image) {
    EXPECT_EQ(expected_nbr_packets_dropped, actual_nbr_packets_dropped);
    EXPECT_EQ(expected_packet_data_length, static_cast<int>(image_._length));
    EXPECT_EQ(0, memcmp(expected_packet_data, actual_image._buffer,
                        expected_packet_data_length));
  }
};

TEST_F(PacketManipulatorTest, Constructor) {
  PacketManipulatorImpl manipulator(&packet_reader_, no_drop_config_);
}

TEST_F(PacketManipulatorTest, ConstructorNullArgument) {
  ASSERT_DEATH(PacketManipulatorImpl manipulator(NULL, no_drop_config_), "");
}

TEST_F(PacketManipulatorTest, NullImageArgument) {
  PacketManipulatorImpl manipulator(&packet_reader_, no_drop_config_);
  ASSERT_DEATH(manipulator.ManipulatePackets(NULL), "");
}

TEST_F(PacketManipulatorTest, DropNone) {
  PacketManipulatorImpl manipulator(&packet_reader_,  no_drop_config_);
  int nbr_packets_dropped = manipulator.ManipulatePackets(&image_);
  VerifyPacketLoss(0, nbr_packets_dropped, kPacketDataLength,
                   packet_data_, image_);
}

TEST_F(PacketManipulatorTest, UniformDropNoneSmallFrame) {
  int data_length = 400;  // smaller than the packet size
  image_._length = data_length;
  PacketManipulatorImpl manipulator(&packet_reader_, no_drop_config_);
  int nbr_packets_dropped = manipulator.ManipulatePackets(&image_);

  VerifyPacketLoss(0, nbr_packets_dropped, data_length,
                     packet_data_, image_);
}

TEST_F(PacketManipulatorTest, UniformDropAll) {
  PacketManipulatorImpl manipulator(&packet_reader_, drop_config_);
  int nbr_packets_dropped = manipulator.ManipulatePackets(&image_);
  VerifyPacketLoss(kPacketDataNumberOfPackets, nbr_packets_dropped,
                   0, packet_data_, image_);
}

TEST_F(PacketManipulatorTest, UniformDropSinglePacket) {
  drop_config_.packet_loss_probability = 0.5;
  PacketManipulatorImpl manipulator(&packet_reader_, drop_config_);
  // Execute the test target method:
  int nbr_packets_dropped = manipulator.ManipulatePackets(&image_);

  // The deterministic behavior (since we've set srand) will
  // make the packet manipulator to throw away the second packet.
  // The third packet is lost because when we have lost one, the remains shall
  // also be discarded.
  VerifyPacketLoss(2, nbr_packets_dropped, kPacketSizeInBytes, packet1_,
                   image_);
}

TEST_F(PacketManipulatorTest, BurstDropNinePackets) {
  // Create a longer packet data structure
  const int kDataLength = kPacketSizeInBytes * 10;
  WebRtc_UWord8 data[kDataLength];
  WebRtc_UWord8* data_pointer = data;
  // Fill with 0s, 1s and so on to be able to easily verify which were dropped:
  for (int i = 0; i < 10; ++i) {
    memset(data_pointer + i * kPacketSizeInBytes, i, kPacketSizeInBytes);
  }
  // Overwrite the defaults from the test fixture:
  image_._buffer = data;
  image_._length = kDataLength;
  image_._size = kDataLength;

  drop_config_.packet_loss_probability = 0.4;
  drop_config_.packet_loss_burst_length = 5;
  drop_config_.packet_loss_mode = kBurst;
  PacketManipulatorImpl manipulator(&packet_reader_, drop_config_);
  // Execute the test target method:
  int nbr_packets_dropped = manipulator.ManipulatePackets(&image_);

  // Should discard every packet after the first one.
  VerifyPacketLoss(9, nbr_packets_dropped, kPacketSizeInBytes, data, image_);
}

}  // namespace test
}  // namespace webrtc
