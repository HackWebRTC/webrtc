/*
 *  Copyright 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/video/receive_statistics_proxy.h"

#include <memory>

#include "webrtc/system_wrappers/include/metrics_default.h"
#include "webrtc/test/gtest.h"

namespace webrtc {
namespace {
const int64_t kFreqOffsetProcessIntervalInMs = 40000;
}  // namespace

// TODO(sakal): ReceiveStatisticsProxy is lacking unittesting.
class ReceiveStatisticsProxyTest : public ::testing::Test {
 public:
  ReceiveStatisticsProxyTest() : fake_clock_(1234), config_(GetTestConfig()) {}
  virtual ~ReceiveStatisticsProxyTest() {}

 protected:
  virtual void SetUp() {
    metrics::Reset();
    statistics_proxy_.reset(new ReceiveStatisticsProxy(&config_, &fake_clock_));
  }

  VideoReceiveStream::Config GetTestConfig() {
    VideoReceiveStream::Config config(nullptr);
    return config;
  }

  SimulatedClock fake_clock_;
  const VideoReceiveStream::Config config_;
  std::unique_ptr<ReceiveStatisticsProxy> statistics_proxy_;
};

TEST_F(ReceiveStatisticsProxyTest, OnDecodedFrameIncreasesFramesDecoded) {
  EXPECT_EQ(0u, statistics_proxy_->GetStats().frames_decoded);
  for (uint32_t i = 1; i <= 3; ++i) {
    statistics_proxy_->OnDecodedFrame();
    EXPECT_EQ(i, statistics_proxy_->GetStats().frames_decoded);
  }
}

TEST_F(ReceiveStatisticsProxyTest, RtpToNtpFrequencyOffsetHistogramIsUpdated) {
  const int64_t kSyncOffsetMs = 22;
  const double kFreqKhz = 90.0;
  statistics_proxy_->OnSyncOffsetUpdated(kSyncOffsetMs, kFreqKhz);
  statistics_proxy_->OnSyncOffsetUpdated(kSyncOffsetMs, kFreqKhz + 2.2);
  fake_clock_.AdvanceTimeMilliseconds(kFreqOffsetProcessIntervalInMs);
  // Process interval passed, max diff: 2.
  statistics_proxy_->OnSyncOffsetUpdated(kSyncOffsetMs, kFreqKhz + 1.1);
  statistics_proxy_->OnSyncOffsetUpdated(kSyncOffsetMs, kFreqKhz - 4.2);
  statistics_proxy_->OnSyncOffsetUpdated(kSyncOffsetMs, kFreqKhz - 0.9);
  fake_clock_.AdvanceTimeMilliseconds(kFreqOffsetProcessIntervalInMs);
  // Process interval passed, max diff: 4.
  statistics_proxy_->OnSyncOffsetUpdated(kSyncOffsetMs, kFreqKhz);
  statistics_proxy_.reset();
  // Average reported: (2 + 4) / 2 = 3.
  EXPECT_EQ(1, metrics::NumSamples("WebRTC.Video.RtpToNtpFreqOffsetInKhz"));
  EXPECT_EQ(1, metrics::NumEvents("WebRTC.Video.RtpToNtpFreqOffsetInKhz", 3));
}

}  // namespace webrtc
