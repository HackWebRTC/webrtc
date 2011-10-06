// Copyright 2011 Google Inc. All Rights Reserved.
// Author: kjellander@google.com (Henrik Kjellander)


#ifndef SRC_MODULES_VIDEO_CODING_CODECS_TEST_UNITTEST_UTILS_H_
#define SRC_MODULES_VIDEO_CODING_CODECS_TEST_UNITTEST_UTILS_H_

namespace webrtc {
namespace test {

const int kPacketSizeInBytes = 1500;
const int kPacketDataLength = kPacketSizeInBytes * 2 + 1;
const int kPacketDataNumberOfPackets = 3;

// A base test fixture for packet related tests. Contains
// two full prepared packets with 1s, 2s in their data and a third packet with
// a single 3 in it (size=1).
// A packet data structure is also available, that contains these three packets
// in order.
class PacketRelatedTest: public testing::Test {
 protected:
  // Tree packet byte arrays with data used for verification:
  WebRtc_UWord8 packet1_[kPacketSizeInBytes];
  WebRtc_UWord8 packet2_[kPacketSizeInBytes];
  WebRtc_UWord8 packet3_[1];
  // Construct a data structure containing these packets
  WebRtc_UWord8 packet_data_[kPacketDataLength];
  WebRtc_UWord8* packet_data_pointer_;

  PacketRelatedTest() {
    packet_data_pointer_ = packet_data_;

    memset(packet1_, 1, kPacketSizeInBytes);
    memset(packet2_, 2, kPacketSizeInBytes);
    memset(packet3_, 3, 1);
    // Fill the packet_data:
    memcpy(packet_data_pointer_, packet1_, kPacketSizeInBytes);
    memcpy(packet_data_pointer_ + kPacketSizeInBytes, packet2_,
           kPacketSizeInBytes);
    memcpy(packet_data_pointer_ + kPacketSizeInBytes * 2, packet3_, 1);
  }

  virtual ~PacketRelatedTest() {
  }

  void SetUp() {
    // Initialize the random generator with 0 to get determenistic behaviour
    srand(0);
  }

  void TearDown() {
  }
};

}  // namespace test
}  // namespace webrtc

#endif  // SRC_MODULES_VIDEO_CODING_CODECS_TEST_UNITTEST_UTILS_H_
