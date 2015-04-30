/*
 *  Copyright (c) 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/voice_engine/include/voe_network.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "webrtc/voice_engine/include/voe_errors.h"
#include "webrtc/voice_engine/voice_engine_fixture.h"

namespace webrtc {

enum {
  kSizeTooSmallForRtcp = 2,  // Minimum size of a valid RTCP packet is 4.
  kSizeTooSmallForRtp = 10,  // Minimum size of a valid RTP packet is 12.
  kSizeGood = 12,            // Acceptable size for both RTP and RTCP packets.
  kSizeTooLarge = 1300       // Maximum size of a valid RTP packet is 1292.
};

// A packet with a valid header for both RTP and RTCP.
// Methods that are tested here are checking only packet header.
static const uint8_t kPacket[kSizeGood] = {0x80};
static const uint8_t kPacketJunk[kSizeGood] = {};

static const int kNonExistingChannel = 1234;

class VoENetworkFixture : public VoiceEngineFixture {
 protected:
  int CreateChannelAndRegisterExternalTransport() {
    EXPECT_EQ(0, base_->Init(&adm_, nullptr));
    int channelID = base_->CreateChannel();
    EXPECT_NE(channelID, -1);
    EXPECT_EQ(0, network_->RegisterExternalTransport(channelID, transport_));
    return channelID;
  }
};

TEST_F(VoENetworkFixture, RegisterExternalTransport) {
  int channelID = CreateChannelAndRegisterExternalTransport();
  EXPECT_EQ(0, network_->DeRegisterExternalTransport(channelID));
}

TEST_F(VoENetworkFixture, RegisterExternalTransportBeforeInitShouldFail) {
  EXPECT_NE(
      0, network_->RegisterExternalTransport(kNonExistingChannel, transport_));
}

TEST_F(VoENetworkFixture, DeRegisterExternalTransportBeforeInitShouldFail) {
  EXPECT_NE(0, network_->DeRegisterExternalTransport(kNonExistingChannel));
}

TEST_F(VoENetworkFixture,
       RegisterExternalTransportOnNonExistingChannelShouldFail) {
  EXPECT_EQ(0, base_->Init(&adm_, nullptr));
  EXPECT_NE(
      0, network_->RegisterExternalTransport(kNonExistingChannel, transport_));
}

TEST_F(VoENetworkFixture,
       DeRegisterExternalTransportOnNonExistingChannelShouldFail) {
  EXPECT_EQ(0, base_->Init(&adm_, nullptr));
  EXPECT_NE(0, network_->DeRegisterExternalTransport(kNonExistingChannel));
}

TEST_F(VoENetworkFixture, DeRegisterExternalTransportBeforeRegister) {
  EXPECT_EQ(0, base_->Init(&adm_, nullptr));
  int channelID = base_->CreateChannel();
  EXPECT_NE(channelID, -1);
  EXPECT_EQ(0, network_->DeRegisterExternalTransport(channelID));
}

TEST_F(VoENetworkFixture, ReceivedRTPPacketWithJunkDataShouldFail) {
  int channelID = CreateChannelAndRegisterExternalTransport();
  EXPECT_EQ(-1, network_->ReceivedRTPPacket(channelID, kPacketJunk,
                                            sizeof(kPacketJunk)));
}

TEST_F(VoENetworkFixture, ReceivedRTPPacketBeforeInitShouldFail) {
  EXPECT_EQ(-1, network_->ReceivedRTPPacket(0, kPacket, sizeof(kPacket)));
}

TEST_F(VoENetworkFixture, ReceivedRTPPacketOnNonExistingChannelShouldFail) {
  EXPECT_EQ(0, base_->Init(&adm_, nullptr));
  EXPECT_EQ(-1, network_->ReceivedRTPPacket(kNonExistingChannel, kPacket,
                                            sizeof(kPacket)));
}

TEST_F(VoENetworkFixture,
       ReceivedRTPPacketOnChannelWithoutTransportShouldFail) {
  EXPECT_EQ(0, base_->Init(&adm_, nullptr));
  int channelID = base_->CreateChannel();
  EXPECT_NE(channelID, -1);
  EXPECT_EQ(-1,
            network_->ReceivedRTPPacket(channelID, kPacket, sizeof(kPacket)));
}

TEST_F(VoENetworkFixture, ReceivedTooSmallRTPPacketShouldFail) {
  int channelID = CreateChannelAndRegisterExternalTransport();
  EXPECT_EQ(
      -1, network_->ReceivedRTPPacket(channelID, kPacket, kSizeTooSmallForRtp));
}

TEST_F(VoENetworkFixture, ReceivedTooLargeRTPPacketShouldFail) {
  int channelID = CreateChannelAndRegisterExternalTransport();
  EXPECT_EQ(-1, network_->ReceivedRTPPacket(channelID, kPacket, kSizeTooLarge));
}

TEST_F(VoENetworkFixture, ReceivedRTPPacketWithNullDataShouldFail) {
  int channelID = CreateChannelAndRegisterExternalTransport();
  EXPECT_EQ(-1, network_->ReceivedRTPPacket(channelID, nullptr, 0));
}

TEST_F(VoENetworkFixture, ReceivedRTCPPacketWithJunkDataShouldFail) {
  int channelID = CreateChannelAndRegisterExternalTransport();
  EXPECT_EQ(0, network_->ReceivedRTCPPacket(channelID, kPacketJunk,
                                            sizeof(kPacketJunk)));
  EXPECT_EQ(VE_SOCKET_TRANSPORT_MODULE_ERROR, base_->LastError());
}

TEST_F(VoENetworkFixture, ReceivedRTCPPacketBeforeInitShouldFail) {
  EXPECT_EQ(-1, network_->ReceivedRTCPPacket(kNonExistingChannel, kPacket,
                                             sizeof(kPacket)));
}

TEST_F(VoENetworkFixture, ReceivedRTCPPacketOnNonExistingChannelShouldFail) {
  EXPECT_EQ(0, base_->Init(&adm_, nullptr));
  EXPECT_EQ(-1, network_->ReceivedRTCPPacket(kNonExistingChannel, kPacket,
                                             sizeof(kPacket)));
}

TEST_F(VoENetworkFixture,
       ReceivedRTCPPacketOnChannelWithoutTransportShouldFail) {
  EXPECT_EQ(0, base_->Init(&adm_, nullptr));
  int channelID = base_->CreateChannel();
  EXPECT_NE(channelID, -1);
  EXPECT_EQ(-1,
            network_->ReceivedRTCPPacket(channelID, kPacket, sizeof(kPacket)));
}

TEST_F(VoENetworkFixture, ReceivedTooSmallRTCPPacket4ShouldFail) {
  int channelID = CreateChannelAndRegisterExternalTransport();
  EXPECT_EQ(-1, network_->ReceivedRTCPPacket(channelID, kPacket,
                                             kSizeTooSmallForRtcp));
}

TEST_F(VoENetworkFixture, ReceivedRTCPPacketWithNullDataShouldFail) {
  int channelID = CreateChannelAndRegisterExternalTransport();
  EXPECT_EQ(-1, network_->ReceivedRTCPPacket(channelID, nullptr, 0));
}

}  // namespace webrtc
