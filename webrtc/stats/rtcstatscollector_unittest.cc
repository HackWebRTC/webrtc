/*
 *  Copyright 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/stats/rtcstatscollector.h"

#include <memory>
#include <string>
#include <vector>

#include "webrtc/api/jsepsessiondescription.h"
#include "webrtc/api/rtcstats_objects.h"
#include "webrtc/api/rtcstatsreport.h"
#include "webrtc/api/test/mock_datachannel.h"
#include "webrtc/api/test/mock_peerconnection.h"
#include "webrtc/api/test/mock_webrtcsession.h"
#include "webrtc/base/checks.h"
#include "webrtc/base/fakeclock.h"
#include "webrtc/base/gunit.h"
#include "webrtc/base/logging.h"
#include "webrtc/base/timedelta.h"
#include "webrtc/base/timeutils.h"
#include "webrtc/base/timing.h"
#include "webrtc/media/base/fakemediaengine.h"

using testing::Return;
using testing::ReturnRef;

namespace webrtc {

class RTCStatsCollectorTester : public SetSessionDescriptionObserver {
 public:
  RTCStatsCollectorTester()
      : worker_thread_(rtc::Thread::Current()),
        network_thread_(rtc::Thread::Current()),
        channel_manager_(new cricket::ChannelManager(
            new cricket::FakeMediaEngine(),
            worker_thread_,
            network_thread_)),
        media_controller_(
            MediaControllerInterface::Create(cricket::MediaConfig(),
                                             worker_thread_,
                                             channel_manager_.get())),
        session_(media_controller_.get()),
        pc_() {
    EXPECT_CALL(pc_, session()).WillRepeatedly(Return(&session_));
    EXPECT_CALL(pc_, sctp_data_channels()).WillRepeatedly(
        ReturnRef(data_channels_));
  }

  MockWebRtcSession& session() { return session_; }
  MockPeerConnection& pc() { return pc_; }
  std::vector<rtc::scoped_refptr<DataChannel>>& data_channels() {
    return data_channels_;
  }

  // SetSessionDescriptionObserver overrides.
  void OnSuccess() override {}
  void OnFailure(const std::string& error) override {
    RTC_NOTREACHED() << error;
  }

 private:
  rtc::Thread* const worker_thread_;
  rtc::Thread* const network_thread_;
  std::unique_ptr<cricket::ChannelManager> channel_manager_;
  std::unique_ptr<webrtc::MediaControllerInterface> media_controller_;
  MockWebRtcSession session_;
  MockPeerConnection pc_;

  std::vector<rtc::scoped_refptr<DataChannel>> data_channels_;
};

class RTCStatsCollectorTest : public testing::Test {
 public:
  RTCStatsCollectorTest()
    : test_(new rtc::RefCountedObject<RTCStatsCollectorTester>()),
      collector_(&test_->pc(), 50 * rtc::kNumMicrosecsPerMillisec) {
  }

 protected:
  rtc::scoped_refptr<RTCStatsCollectorTester> test_;
  RTCStatsCollector collector_;
};

TEST_F(RTCStatsCollectorTest, CachedStatsReport) {
  rtc::ScopedFakeClock fake_clock;
  // Caching should ensure |a| and |b| are the same report.
  rtc::scoped_refptr<const RTCStatsReport> a = collector_.GetStatsReport();
  rtc::scoped_refptr<const RTCStatsReport> b = collector_.GetStatsReport();
  EXPECT_TRUE(a);
  EXPECT_EQ(a.get(), b.get());
  // Invalidate cache by clearing it.
  collector_.ClearCachedStatsReport();
  rtc::scoped_refptr<const RTCStatsReport> c = collector_.GetStatsReport();
  EXPECT_TRUE(c);
  EXPECT_NE(b.get(), c.get());
  // Invalidate cache by advancing time.
  fake_clock.AdvanceTime(rtc::TimeDelta::FromMilliseconds(51));
  rtc::scoped_refptr<const RTCStatsReport> d = collector_.GetStatsReport();
  EXPECT_TRUE(d);
  EXPECT_NE(c.get(), d.get());
}

TEST_F(RTCStatsCollectorTest, CollectRTCPeerConnectionStats) {
  int64_t before = static_cast<int64_t>(
      rtc::Timing::WallTimeNow() * rtc::kNumMicrosecsPerSec);
  rtc::scoped_refptr<const RTCStatsReport> report = collector_.GetStatsReport();
  int64_t after = static_cast<int64_t>(
      rtc::Timing::WallTimeNow() * rtc::kNumMicrosecsPerSec);
  EXPECT_EQ(report->GetStatsOfType<RTCPeerConnectionStats>().size(),
            static_cast<size_t>(1)) << "Expecting 1 RTCPeerConnectionStats.";
  const RTCStats* stats = report->Get("RTCPeerConnection");
  EXPECT_TRUE(stats);
  EXPECT_LE(before, stats->timestamp_us());
  EXPECT_LE(stats->timestamp_us(), after);
  {
    // Expected stats with no data channels
    const RTCPeerConnectionStats& pcstats =
        stats->cast_to<RTCPeerConnectionStats>();
    EXPECT_EQ(*pcstats.data_channels_opened, static_cast<uint32_t>(0));
    EXPECT_EQ(*pcstats.data_channels_closed, static_cast<uint32_t>(0));
  }

  test_->data_channels().push_back(
      new MockDataChannel(DataChannelInterface::kConnecting));
  test_->data_channels().push_back(
      new MockDataChannel(DataChannelInterface::kOpen));
  test_->data_channels().push_back(
      new MockDataChannel(DataChannelInterface::kClosing));
  test_->data_channels().push_back(
      new MockDataChannel(DataChannelInterface::kClosed));

  collector_.ClearCachedStatsReport();
  report = collector_.GetStatsReport();
  EXPECT_EQ(report->GetStatsOfType<RTCPeerConnectionStats>().size(),
            static_cast<size_t>(1)) << "Expecting 1 RTCPeerConnectionStats.";
  stats = report->Get("RTCPeerConnection");
  EXPECT_TRUE(stats);
  {
    // Expected stats with the above four data channels
    // TODO(hbos): When the |RTCPeerConnectionStats| is the number of data
    // channels that have been opened and closed, not the numbers currently
    // open/closed, we would expect opened >= closed and (opened - closed) to be
    // the number currently open. crbug.com/636818.
    const RTCPeerConnectionStats& pcstats =
        stats->cast_to<RTCPeerConnectionStats>();
    EXPECT_EQ(*pcstats.data_channels_opened, static_cast<uint32_t>(1));
    EXPECT_EQ(*pcstats.data_channels_closed, static_cast<uint32_t>(3));
  }
}

}  // namespace webrtc
