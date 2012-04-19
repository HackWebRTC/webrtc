/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <gtest/gtest.h>

#include <algorithm>
#include <vector>

#include "modules/bitrate_controller/include/bitrate_controller.h"
#include "modules/rtp_rtcp/interface/rtp_rtcp_defines.h"

using webrtc::RtcpBandwidthObserver;
using webrtc::BitrateObserver;
using webrtc::BitrateController;

class TestBitrateObserver: public BitrateObserver {
 public:
  TestBitrateObserver()
      : last_bitrate(0),
        last_fraction_loss(0),
        last_rtt(0) {
  }

  virtual void OnNetworkChanged(const uint32_t bitrate,
                                const uint8_t fraction_loss,
                                const uint32_t rtt) {
    last_bitrate = bitrate;
    last_fraction_loss = fraction_loss;
    last_rtt = rtt;
  }
  uint32_t last_bitrate;
  uint8_t last_fraction_loss;
  uint32_t last_rtt;
};

class BitrateControllerTest : public ::testing::Test {
 protected:
  BitrateControllerTest() {
  }
  ~BitrateControllerTest() {}

  virtual void SetUp() {
    controller_ = BitrateController::CreateBitrateController();
    bandwidth_observer_ = controller_->CreateRtcpBandwidthObserver();
  }

  virtual void TearDown() {
    delete bandwidth_observer_;
    delete controller_;
  }
  BitrateController* controller_;
  RtcpBandwidthObserver* bandwidth_observer_;
};

TEST_F(BitrateControllerTest, Basic) {
  TestBitrateObserver bitrate_observer;
  controller_->SetBitrateObserver(&bitrate_observer, 200000, 100000, 300000);
  controller_->RemoveBitrateObserver(&bitrate_observer);
}

TEST_F(BitrateControllerTest, OneBitrateObserverOneRtcpObserver) {
  TestBitrateObserver bitrate_observer;
  controller_->SetBitrateObserver(&bitrate_observer, 200000, 100000, 300000);

  // Receive a high remb, test bitrate inc.
  bandwidth_observer_->OnReceivedEstimatedBitrate(400000);

  // Test start bitrate.
  bandwidth_observer_->OnReceivedRtcpReceiverReport(1, 0, 50, 1, 1);
  EXPECT_EQ(bitrate_observer.last_bitrate, 200000u);
  EXPECT_EQ(bitrate_observer.last_fraction_loss, 0);
  EXPECT_EQ(bitrate_observer.last_rtt, 50u);

  // Test bitrate increase 8% per second.
  bandwidth_observer_->OnReceivedRtcpReceiverReport(1, 0, 50, 21, 1001);
  EXPECT_EQ(bitrate_observer.last_bitrate, 217000u);

  bandwidth_observer_->OnReceivedRtcpReceiverReport(1, 0, 50, 41, 2001);
  EXPECT_EQ(bitrate_observer.last_bitrate, 235360u);

  bandwidth_observer_->OnReceivedRtcpReceiverReport(1, 0, 50, 61, 3001);
  EXPECT_EQ(bitrate_observer.last_bitrate, 255189u);

  bandwidth_observer_->OnReceivedRtcpReceiverReport(1, 0, 50, 81, 4001);
  EXPECT_EQ(bitrate_observer.last_bitrate, 276604u);

  bandwidth_observer_->OnReceivedRtcpReceiverReport(1, 0, 50, 101, 5001);
  EXPECT_EQ(bitrate_observer.last_bitrate, 299732u);

  bandwidth_observer_->OnReceivedRtcpReceiverReport(1, 0, 50, 121, 6001);
  EXPECT_EQ(bitrate_observer.last_bitrate, 300000u);  // Max cap.

  bandwidth_observer_->OnReceivedRtcpReceiverReport(1, 0, 50, 141, 7001);
  EXPECT_EQ(bitrate_observer.last_bitrate, 300000u);  // Max cap.

  // Test that a low REMB trigger immediately.
  bandwidth_observer_->OnReceivedEstimatedBitrate(250000);
  EXPECT_EQ(bitrate_observer.last_bitrate, 250000u);
  EXPECT_EQ(bitrate_observer.last_fraction_loss, 0);
  EXPECT_EQ(bitrate_observer.last_rtt, 50u);

  bandwidth_observer_->OnReceivedEstimatedBitrate(1000);
  EXPECT_EQ(bitrate_observer.last_bitrate, 100000u);  // Min cap.
  controller_->RemoveBitrateObserver(&bitrate_observer);
}

TEST_F(BitrateControllerTest, OneBitrateObserverTwoRtcpObservers) {
  TestBitrateObserver bitrate_observer;
  controller_->SetBitrateObserver(&bitrate_observer, 200000, 100000, 300000);

  RtcpBandwidthObserver* second_bandwidth_observer =
      controller_->CreateRtcpBandwidthObserver();

  // Receive a high remb, test bitrate inc.
  bandwidth_observer_->OnReceivedEstimatedBitrate(400000);

  // Test start bitrate.
  bandwidth_observer_->OnReceivedRtcpReceiverReport(1, 0, 50, 1, 1);
  second_bandwidth_observer->OnReceivedRtcpReceiverReport(1, 0, 100, 1, 1);
  EXPECT_EQ(bitrate_observer.last_bitrate, 200000u);
  EXPECT_EQ(bitrate_observer.last_fraction_loss, 0);
  EXPECT_EQ(bitrate_observer.last_rtt, 100u);

  // Test bitrate increase 8% per second.
  bandwidth_observer_->OnReceivedRtcpReceiverReport(1, 0, 50, 21, 1001);
  second_bandwidth_observer->OnReceivedRtcpReceiverReport(1, 0, 100, 21, 1001);
  EXPECT_EQ(bitrate_observer.last_bitrate, 217000u);

  // Extra report should not change estimate.
  second_bandwidth_observer->OnReceivedRtcpReceiverReport(1, 0, 100, 31, 1501);
  EXPECT_EQ(bitrate_observer.last_bitrate, 217000u);

  bandwidth_observer_->OnReceivedRtcpReceiverReport(1, 0, 50, 41, 2001);
  EXPECT_EQ(bitrate_observer.last_bitrate, 235360u);
  // Second report should not change estimate.
  second_bandwidth_observer->OnReceivedRtcpReceiverReport(1, 0, 100, 41, 2001);
  EXPECT_EQ(bitrate_observer.last_bitrate, 235360u);

  // Reports from only one bandwidth observer is ok.
  second_bandwidth_observer->OnReceivedRtcpReceiverReport(1, 0, 50, 61, 3001);
  EXPECT_EQ(bitrate_observer.last_bitrate, 255189u);

  second_bandwidth_observer->OnReceivedRtcpReceiverReport(1, 0, 50, 81, 4001);
  EXPECT_EQ(bitrate_observer.last_bitrate, 276604u);

  second_bandwidth_observer->OnReceivedRtcpReceiverReport(1, 0, 50, 101, 5001);
  EXPECT_EQ(bitrate_observer.last_bitrate, 299732u);

  second_bandwidth_observer->OnReceivedRtcpReceiverReport(1, 0, 50, 121, 6001);
  EXPECT_EQ(bitrate_observer.last_bitrate, 300000u);  // Max cap.

  second_bandwidth_observer->OnReceivedRtcpReceiverReport(1, 0, 50, 141, 7001);
  EXPECT_EQ(bitrate_observer.last_bitrate, 300000u);  // Max cap.

  // Test that a low REMB trigger immediately.
  // We don't care which bandwidth observer that delivers the REMB.
  second_bandwidth_observer->OnReceivedEstimatedBitrate(250000);
  EXPECT_EQ(bitrate_observer.last_bitrate, 250000u);
  EXPECT_EQ(bitrate_observer.last_fraction_loss, 0);
  EXPECT_EQ(bitrate_observer.last_rtt, 50u);

  bandwidth_observer_->OnReceivedEstimatedBitrate(1000);
  EXPECT_EQ(bitrate_observer.last_bitrate, 100000u);  // Min cap.
  controller_->RemoveBitrateObserver(&bitrate_observer);
  delete second_bandwidth_observer;
}

TEST_F(BitrateControllerTest, TwoBitrateObserversOneRtcpObserver) {
  TestBitrateObserver bitrate_observer_1;
  TestBitrateObserver bitrate_observer_2;
  controller_->SetBitrateObserver(&bitrate_observer_1, 200000, 100000, 300000);
  controller_->SetBitrateObserver(&bitrate_observer_2, 200000, 200000, 300000);

  // Receive a high remb, test bitrate inc.
  bandwidth_observer_->OnReceivedEstimatedBitrate(400000);

  // Test too low start bitrate, hence lower than sum of min.
  bandwidth_observer_->OnReceivedRtcpReceiverReport(1, 0, 50, 1, 1);
  EXPECT_EQ(bitrate_observer_1.last_bitrate, 100000u);
  EXPECT_EQ(bitrate_observer_1.last_fraction_loss, 0);
  EXPECT_EQ(bitrate_observer_1.last_rtt, 50u);

  EXPECT_EQ(bitrate_observer_2.last_bitrate, 200000u);
  EXPECT_EQ(bitrate_observer_2.last_fraction_loss, 0);
  EXPECT_EQ(bitrate_observer_2.last_rtt, 50u);

  // Test bitrate increase 8% per second, distributed equaly.
  bandwidth_observer_->OnReceivedRtcpReceiverReport(1, 0, 50, 21, 1001);
  EXPECT_EQ(bitrate_observer_1.last_bitrate, 112500u);
  EXPECT_EQ(bitrate_observer_2.last_bitrate, 212500u);

  bandwidth_observer_->OnReceivedRtcpReceiverReport(1, 0, 50, 41, 2001);
  EXPECT_EQ(bitrate_observer_1.last_bitrate, 126000u);
  EXPECT_EQ(bitrate_observer_2.last_bitrate, 226000u);

  bandwidth_observer_->OnReceivedRtcpReceiverReport(1, 0, 50, 61, 3001);
  EXPECT_EQ(bitrate_observer_1.last_bitrate, 140580u);
  EXPECT_EQ(bitrate_observer_2.last_bitrate, 240580u);

  // Check that the bitrate sum honor our REMB.
  bandwidth_observer_->OnReceivedRtcpReceiverReport(1, 0, 50, 81, 4001);
  EXPECT_EQ(bitrate_observer_1.last_bitrate, 150000u);
  EXPECT_EQ(bitrate_observer_2.last_bitrate, 250000u);

  // Remove REMB cap, higher than sum of max.
  bandwidth_observer_->OnReceivedEstimatedBitrate(700000);

  bandwidth_observer_->OnReceivedRtcpReceiverReport(1, 0, 50, 101, 5001);
  EXPECT_EQ(bitrate_observer_1.last_bitrate, 166500u);
  EXPECT_EQ(bitrate_observer_2.last_bitrate, 266500u);

  bandwidth_observer_->OnReceivedRtcpReceiverReport(1, 0, 50, 121, 6001);
  EXPECT_EQ(bitrate_observer_1.last_bitrate, 184320u);
  EXPECT_EQ(bitrate_observer_2.last_bitrate, 284320u);

  bandwidth_observer_->OnReceivedRtcpReceiverReport(1, 0, 50, 141, 7001);
  EXPECT_EQ(bitrate_observer_1.last_bitrate, 207130u);
  EXPECT_EQ(bitrate_observer_2.last_bitrate, 300000u);  // Max cap.

  bandwidth_observer_->OnReceivedRtcpReceiverReport(1, 0, 50, 161, 8001);
  EXPECT_EQ(bitrate_observer_1.last_bitrate, 248700u);
  EXPECT_EQ(bitrate_observer_2.last_bitrate, 300000u);

  bandwidth_observer_->OnReceivedRtcpReceiverReport(1, 0, 50, 181, 9001);
  EXPECT_EQ(bitrate_observer_1.last_bitrate, 293596u);
  EXPECT_EQ(bitrate_observer_2.last_bitrate, 300000u);

  bandwidth_observer_->OnReceivedRtcpReceiverReport(1, 0, 50, 201, 10001);
  EXPECT_EQ(bitrate_observer_1.last_bitrate, 300000u);  // Max cap.
  EXPECT_EQ(bitrate_observer_2.last_bitrate, 300000u);

  // Test that a low REMB trigger immediately.
  bandwidth_observer_->OnReceivedEstimatedBitrate(350000);
  EXPECT_EQ(bitrate_observer_1.last_bitrate, 125000u);
  EXPECT_EQ(bitrate_observer_1.last_fraction_loss, 0);
  EXPECT_EQ(bitrate_observer_1.last_rtt, 50u);
  EXPECT_EQ(bitrate_observer_2.last_bitrate, 225000u);
  EXPECT_EQ(bitrate_observer_2.last_fraction_loss, 0);
  EXPECT_EQ(bitrate_observer_2.last_rtt, 50u);

  bandwidth_observer_->OnReceivedEstimatedBitrate(1000);
  EXPECT_EQ(bitrate_observer_1.last_bitrate, 100000u);  // Min cap.
  EXPECT_EQ(bitrate_observer_2.last_bitrate, 200000u);  // Min cap.
  controller_->RemoveBitrateObserver(&bitrate_observer_1);
  controller_->RemoveBitrateObserver(&bitrate_observer_2);
}


