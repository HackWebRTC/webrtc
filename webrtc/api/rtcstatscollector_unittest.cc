/*
 *  Copyright 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/api/rtcstatscollector.h"

#include <memory>
#include <string>
#include <vector>

#include "webrtc/api/jsepsessiondescription.h"
#include "webrtc/api/stats/rtcstats_objects.h"
#include "webrtc/api/stats/rtcstatsreport.h"
#include "webrtc/api/test/mock_datachannel.h"
#include "webrtc/api/test/mock_peerconnection.h"
#include "webrtc/api/test/mock_webrtcsession.h"
#include "webrtc/base/checks.h"
#include "webrtc/base/fakeclock.h"
#include "webrtc/base/gunit.h"
#include "webrtc/base/logging.h"
#include "webrtc/base/thread_checker.h"
#include "webrtc/base/timedelta.h"
#include "webrtc/base/timeutils.h"
#include "webrtc/media/base/fakemediaengine.h"

using testing::Return;
using testing::ReturnRef;

namespace webrtc {

namespace {

const int64_t kGetStatsReportTimeoutMs = 1000;

class RTCStatsCollectorTestHelper : public SetSessionDescriptionObserver {
 public:
  RTCStatsCollectorTestHelper()
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

  rtc::ScopedFakeClock& fake_clock() { return fake_clock_; }
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
  rtc::ScopedFakeClock fake_clock_;
  rtc::Thread* const worker_thread_;
  rtc::Thread* const network_thread_;
  std::unique_ptr<cricket::ChannelManager> channel_manager_;
  std::unique_ptr<webrtc::MediaControllerInterface> media_controller_;
  MockWebRtcSession session_;
  MockPeerConnection pc_;

  std::vector<rtc::scoped_refptr<DataChannel>> data_channels_;
};

class RTCTestStats : public RTCStats {
 public:
  RTCTestStats(const std::string& id, int64_t timestamp_us)
      : RTCStats(id, timestamp_us),
        dummy_stat("dummyStat") {}

  WEBRTC_RTCSTATS_IMPL(RTCStats, RTCTestStats,
      &dummy_stat);

  RTCStatsMember<int32_t> dummy_stat;
};

const char RTCTestStats::kType[] = "test-stats";

// Overrides the stats collection to verify thread usage and that the resulting
// partial reports are merged.
class FakeRTCStatsCollector : public RTCStatsCollector,
                              public RTCStatsCollectorCallback {
 public:
  static rtc::scoped_refptr<FakeRTCStatsCollector> Create(
      PeerConnection* pc,
      int64_t cache_lifetime_us) {
    return rtc::scoped_refptr<FakeRTCStatsCollector>(
        new rtc::RefCountedObject<FakeRTCStatsCollector>(
            pc, cache_lifetime_us));
  }

  // RTCStatsCollectorCallback implementation.
  void OnStatsDelivered(
      const rtc::scoped_refptr<const RTCStatsReport>& report) override {
    EXPECT_TRUE(signaling_thread_->IsCurrent());
    rtc::CritScope cs(&lock_);
    delivered_report_ = report;
  }

  void VerifyThreadUsageAndResultsMerging() {
    GetStatsReport(rtc::scoped_refptr<RTCStatsCollectorCallback>(this));
    EXPECT_TRUE_WAIT(HasVerifiedResults(), kGetStatsReportTimeoutMs);
  }

  bool HasVerifiedResults() {
    EXPECT_TRUE(signaling_thread_->IsCurrent());
    rtc::CritScope cs(&lock_);
    if (!delivered_report_)
      return false;
    EXPECT_EQ(produced_on_signaling_thread_, 1);
    EXPECT_EQ(produced_on_worker_thread_, 1);
    EXPECT_EQ(produced_on_network_thread_, 1);

    EXPECT_TRUE(delivered_report_->Get("SignalingThreadStats"));
    EXPECT_TRUE(delivered_report_->Get("WorkerThreadStats"));
    EXPECT_TRUE(delivered_report_->Get("NetworkThreadStats"));

    produced_on_signaling_thread_ = 0;
    produced_on_worker_thread_ = 0;
    produced_on_network_thread_ = 0;
    delivered_report_ = nullptr;
    return true;
  }

 protected:
  FakeRTCStatsCollector(
      PeerConnection* pc,
      int64_t cache_lifetime)
      : RTCStatsCollector(pc, cache_lifetime),
        signaling_thread_(pc->session()->signaling_thread()),
        worker_thread_(pc->session()->worker_thread()),
        network_thread_(pc->session()->network_thread()) {
  }

  void ProducePartialResultsOnSignalingThread(int64_t timestamp_us) override {
    EXPECT_TRUE(signaling_thread_->IsCurrent());
    {
      rtc::CritScope cs(&lock_);
      EXPECT_FALSE(delivered_report_);
      ++produced_on_signaling_thread_;
    }

    rtc::scoped_refptr<RTCStatsReport> signaling_report =
        RTCStatsReport::Create();
    signaling_report->AddStats(std::unique_ptr<const RTCStats>(
        new RTCTestStats("SignalingThreadStats", timestamp_us)));
    AddPartialResults(signaling_report);
  }
  void ProducePartialResultsOnWorkerThread(int64_t timestamp_us) override {
    EXPECT_TRUE(worker_thread_->IsCurrent());
    {
      rtc::CritScope cs(&lock_);
      EXPECT_FALSE(delivered_report_);
      ++produced_on_worker_thread_;
    }

    rtc::scoped_refptr<RTCStatsReport> worker_report = RTCStatsReport::Create();
    worker_report->AddStats(std::unique_ptr<const RTCStats>(
        new RTCTestStats("WorkerThreadStats", timestamp_us)));
    AddPartialResults(worker_report);
  }
  void ProducePartialResultsOnNetworkThread(int64_t timestamp_us) override {
    EXPECT_TRUE(network_thread_->IsCurrent());
    {
      rtc::CritScope cs(&lock_);
      EXPECT_FALSE(delivered_report_);
      ++produced_on_network_thread_;
    }

    rtc::scoped_refptr<RTCStatsReport> network_report =
        RTCStatsReport::Create();
    network_report->AddStats(std::unique_ptr<const RTCStats>(
        new RTCTestStats("NetworkThreadStats", timestamp_us)));
    AddPartialResults(network_report);
  }

 private:
  rtc::Thread* const signaling_thread_;
  rtc::Thread* const worker_thread_;
  rtc::Thread* const network_thread_;

  rtc::CriticalSection lock_;
  rtc::scoped_refptr<const RTCStatsReport> delivered_report_;
  int produced_on_signaling_thread_ = 0;
  int produced_on_worker_thread_ = 0;
  int produced_on_network_thread_ = 0;
};

class StatsCallback : public RTCStatsCollectorCallback {
 public:
  static rtc::scoped_refptr<StatsCallback> Create(
      rtc::scoped_refptr<const RTCStatsReport>* report_ptr = nullptr) {
    return rtc::scoped_refptr<StatsCallback>(
        new rtc::RefCountedObject<StatsCallback>(report_ptr));
  }

  void OnStatsDelivered(
      const rtc::scoped_refptr<const RTCStatsReport>& report) override {
    EXPECT_TRUE(thread_checker_.CalledOnValidThread());
    report_ = report;
    if (report_ptr_)
      *report_ptr_ = report_;
  }

  rtc::scoped_refptr<const RTCStatsReport> report() const {
    EXPECT_TRUE(thread_checker_.CalledOnValidThread());
    return report_;
  }

 protected:
  explicit StatsCallback(rtc::scoped_refptr<const RTCStatsReport>* report_ptr)
      : report_ptr_(report_ptr) {}

 private:
  rtc::ThreadChecker thread_checker_;
  rtc::scoped_refptr<const RTCStatsReport> report_;
  rtc::scoped_refptr<const RTCStatsReport>* report_ptr_;
};

class RTCStatsCollectorTest : public testing::Test {
 public:
  RTCStatsCollectorTest()
    : test_(new rtc::RefCountedObject<RTCStatsCollectorTestHelper>()),
      collector_(RTCStatsCollector::Create(
          &test_->pc(), 50 * rtc::kNumMicrosecsPerMillisec)) {
  }

  rtc::scoped_refptr<const RTCStatsReport> GetStatsReport() {
    rtc::scoped_refptr<StatsCallback> callback = StatsCallback::Create();
    collector_->GetStatsReport(callback);
    EXPECT_TRUE_WAIT(callback->report(), kGetStatsReportTimeoutMs);
    return callback->report();
  }

 protected:
  rtc::scoped_refptr<RTCStatsCollectorTestHelper> test_;
  rtc::scoped_refptr<RTCStatsCollector> collector_;
};

TEST_F(RTCStatsCollectorTest, SingleCallback) {
  rtc::scoped_refptr<const RTCStatsReport> result;
  collector_->GetStatsReport(StatsCallback::Create(&result));
  EXPECT_TRUE_WAIT(result, kGetStatsReportTimeoutMs);
}

TEST_F(RTCStatsCollectorTest, MultipleCallbacks) {
  rtc::scoped_refptr<const RTCStatsReport> a;
  rtc::scoped_refptr<const RTCStatsReport> b;
  rtc::scoped_refptr<const RTCStatsReport> c;
  collector_->GetStatsReport(StatsCallback::Create(&a));
  collector_->GetStatsReport(StatsCallback::Create(&b));
  collector_->GetStatsReport(StatsCallback::Create(&c));
  EXPECT_TRUE_WAIT(a, kGetStatsReportTimeoutMs);
  EXPECT_TRUE_WAIT(b, kGetStatsReportTimeoutMs);
  EXPECT_TRUE_WAIT(c, kGetStatsReportTimeoutMs);
  EXPECT_EQ(a.get(), b.get());
  EXPECT_EQ(b.get(), c.get());
}

TEST_F(RTCStatsCollectorTest, CachedStatsReports) {
  // Caching should ensure |a| and |b| are the same report.
  rtc::scoped_refptr<const RTCStatsReport> a = GetStatsReport();
  rtc::scoped_refptr<const RTCStatsReport> b = GetStatsReport();
  EXPECT_EQ(a.get(), b.get());
  // Invalidate cache by clearing it.
  collector_->ClearCachedStatsReport();
  rtc::scoped_refptr<const RTCStatsReport> c = GetStatsReport();
  EXPECT_NE(b.get(), c.get());
  // Invalidate cache by advancing time.
  test_->fake_clock().AdvanceTime(rtc::TimeDelta::FromMilliseconds(51));
  rtc::scoped_refptr<const RTCStatsReport> d = GetStatsReport();
  EXPECT_TRUE(d);
  EXPECT_NE(c.get(), d.get());
}

TEST_F(RTCStatsCollectorTest, MultipleCallbacksWithInvalidatedCacheInBetween) {
  rtc::scoped_refptr<const RTCStatsReport> a;
  rtc::scoped_refptr<const RTCStatsReport> b;
  rtc::scoped_refptr<const RTCStatsReport> c;
  collector_->GetStatsReport(StatsCallback::Create(&a));
  collector_->GetStatsReport(StatsCallback::Create(&b));
  // Cache is invalidated after 50 ms.
  test_->fake_clock().AdvanceTime(rtc::TimeDelta::FromMilliseconds(51));
  collector_->GetStatsReport(StatsCallback::Create(&c));
  EXPECT_TRUE_WAIT(a, kGetStatsReportTimeoutMs);
  EXPECT_TRUE_WAIT(b, kGetStatsReportTimeoutMs);
  EXPECT_TRUE_WAIT(c, kGetStatsReportTimeoutMs);
  EXPECT_EQ(a.get(), b.get());
  // The act of doing |AdvanceTime| processes all messages. If this was not the
  // case we might not require |c| to be fresher than |b|.
  EXPECT_NE(c.get(), b.get());
}

TEST_F(RTCStatsCollectorTest, CollectRTCPeerConnectionStats) {
  int64_t before = rtc::TimeUTCMicros();
  rtc::scoped_refptr<const RTCStatsReport> report = GetStatsReport();
  int64_t after = rtc::TimeUTCMicros();
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

  collector_->ClearCachedStatsReport();
  report = GetStatsReport();
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

class RTCStatsCollectorTestWithFakeCollector : public testing::Test {
 public:
  RTCStatsCollectorTestWithFakeCollector()
    : test_(new rtc::RefCountedObject<RTCStatsCollectorTestHelper>()),
      collector_(FakeRTCStatsCollector::Create(
          &test_->pc(), 50 * rtc::kNumMicrosecsPerMillisec)) {
  }

 protected:
  rtc::scoped_refptr<RTCStatsCollectorTestHelper> test_;
  rtc::scoped_refptr<FakeRTCStatsCollector> collector_;
};

TEST_F(RTCStatsCollectorTestWithFakeCollector, ThreadUsageAndResultsMerging) {
  collector_->VerifyThreadUsageAndResultsMerging();
}

}  // namespace

}  // namespace webrtc
