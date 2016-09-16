/*
 *  Copyright 2016 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/api/rtcstatscollector.h"

#include <memory>
#include <utility>
#include <vector>

#include "webrtc/api/peerconnection.h"
#include "webrtc/base/checks.h"

namespace webrtc {

rtc::scoped_refptr<RTCStatsCollector> RTCStatsCollector::Create(
    PeerConnection* pc, int64_t cache_lifetime_us) {
  return rtc::scoped_refptr<RTCStatsCollector>(
      new rtc::RefCountedObject<RTCStatsCollector>(pc, cache_lifetime_us));
}

RTCStatsCollector::RTCStatsCollector(PeerConnection* pc,
                                     int64_t cache_lifetime_us)
    : pc_(pc),
      signaling_thread_(pc->session()->signaling_thread()),
      worker_thread_(pc->session()->worker_thread()),
      network_thread_(pc->session()->network_thread()),
      num_pending_partial_reports_(0),
      partial_report_timestamp_us_(0),
      cache_timestamp_us_(0),
      cache_lifetime_us_(cache_lifetime_us) {
  RTC_DCHECK(pc_);
  RTC_DCHECK(signaling_thread_);
  RTC_DCHECK(worker_thread_);
  RTC_DCHECK(network_thread_);
  RTC_DCHECK_GE(cache_lifetime_us_, 0);
}

void RTCStatsCollector::GetStatsReport(
    rtc::scoped_refptr<RTCStatsCollectorCallback> callback) {
  RTC_DCHECK(signaling_thread_->IsCurrent());
  RTC_DCHECK(callback);
  callbacks_.push_back(callback);

  // "Now" using a monotonically increasing timer.
  int64_t cache_now_us = rtc::TimeMicros();
  if (cached_report_ &&
      cache_now_us - cache_timestamp_us_ <= cache_lifetime_us_) {
    // We have a fresh cached report to deliver.
    DeliverCachedReport();
  } else if (!num_pending_partial_reports_) {
    // Only start gathering stats if we're not already gathering stats. In the
    // case of already gathering stats, |callback_| will be invoked when there
    // are no more pending partial reports.

    // "Now" using a system clock, relative to the UNIX epoch (Jan 1, 1970,
    // UTC), in microseconds. The system clock could be modified and is not
    // necessarily monotonically increasing.
    int64_t timestamp_us = rtc::TimeUTCMicros();

    num_pending_partial_reports_ = 3;
    partial_report_timestamp_us_ = cache_now_us;
    invoker_.AsyncInvoke<void>(RTC_FROM_HERE, signaling_thread_,
        rtc::Bind(&RTCStatsCollector::ProducePartialResultsOnSignalingThread,
            rtc::scoped_refptr<RTCStatsCollector>(this), timestamp_us));
    invoker_.AsyncInvoke<void>(RTC_FROM_HERE, worker_thread_,
        rtc::Bind(&RTCStatsCollector::ProducePartialResultsOnWorkerThread,
            rtc::scoped_refptr<RTCStatsCollector>(this), timestamp_us));
    invoker_.AsyncInvoke<void>(RTC_FROM_HERE, network_thread_,
        rtc::Bind(&RTCStatsCollector::ProducePartialResultsOnNetworkThread,
            rtc::scoped_refptr<RTCStatsCollector>(this), timestamp_us));
  }
}

void RTCStatsCollector::ClearCachedStatsReport() {
  RTC_DCHECK(signaling_thread_->IsCurrent());
  cached_report_ = nullptr;
}

void RTCStatsCollector::ProducePartialResultsOnSignalingThread(
    int64_t timestamp_us) {
  RTC_DCHECK(signaling_thread_->IsCurrent());
  rtc::scoped_refptr<RTCStatsReport> report = RTCStatsReport::Create();

  report->AddStats(ProducePeerConnectionStats_s(timestamp_us));

  AddPartialResults(report);
}

void RTCStatsCollector::ProducePartialResultsOnWorkerThread(
    int64_t timestamp_us) {
  RTC_DCHECK(worker_thread_->IsCurrent());
  rtc::scoped_refptr<RTCStatsReport> report = RTCStatsReport::Create();

  // TODO(hbos): Gather stats on worker thread.

  AddPartialResults(report);
}

void RTCStatsCollector::ProducePartialResultsOnNetworkThread(
    int64_t timestamp_us) {
  RTC_DCHECK(network_thread_->IsCurrent());
  rtc::scoped_refptr<RTCStatsReport> report = RTCStatsReport::Create();

  // TODO(hbos): Gather stats on network thread.

  AddPartialResults(report);
}

void RTCStatsCollector::AddPartialResults(
    const rtc::scoped_refptr<RTCStatsReport>& partial_report) {
  if (!signaling_thread_->IsCurrent()) {
    invoker_.AsyncInvoke<void>(RTC_FROM_HERE, signaling_thread_,
        rtc::Bind(&RTCStatsCollector::AddPartialResults_s,
                  rtc::scoped_refptr<RTCStatsCollector>(this),
                  partial_report));
    return;
  }
  AddPartialResults_s(partial_report);
}

void RTCStatsCollector::AddPartialResults_s(
    rtc::scoped_refptr<RTCStatsReport> partial_report) {
  RTC_DCHECK(signaling_thread_->IsCurrent());
  RTC_DCHECK_GT(num_pending_partial_reports_, 0);
  if (!partial_report_)
    partial_report_ = partial_report;
  else
    partial_report_->TakeMembersFrom(partial_report);
  --num_pending_partial_reports_;
  if (!num_pending_partial_reports_) {
    cache_timestamp_us_ = partial_report_timestamp_us_;
    cached_report_ = partial_report_;
    partial_report_ = nullptr;
    DeliverCachedReport();
  }
}

void RTCStatsCollector::DeliverCachedReport() {
  RTC_DCHECK(signaling_thread_->IsCurrent());
  RTC_DCHECK(!callbacks_.empty());
  RTC_DCHECK(cached_report_);
  for (const rtc::scoped_refptr<RTCStatsCollectorCallback>& callback :
       callbacks_) {
    callback->OnStatsDelivered(cached_report_);
  }
  callbacks_.clear();
}

std::unique_ptr<RTCPeerConnectionStats>
RTCStatsCollector::ProducePeerConnectionStats_s(int64_t timestamp_us) const {
  RTC_DCHECK(signaling_thread_->IsCurrent());
  // TODO(hbos): If data channels are removed from the peer connection this will
  // yield incorrect counts. Address before closing crbug.com/636818. See
  // https://w3c.github.io/webrtc-stats/webrtc-stats.html#pcstats-dict*.
  uint32_t data_channels_opened = 0;
  const std::vector<rtc::scoped_refptr<DataChannel>>& data_channels =
      pc_->sctp_data_channels();
  for (const rtc::scoped_refptr<DataChannel>& data_channel : data_channels) {
    if (data_channel->state() == DataChannelInterface::kOpen)
      ++data_channels_opened;
  }
  // There is always just one |RTCPeerConnectionStats| so its |id| can be a
  // constant.
  std::unique_ptr<RTCPeerConnectionStats> stats(
    new RTCPeerConnectionStats("RTCPeerConnection", timestamp_us));
  stats->data_channels_opened = data_channels_opened;
  stats->data_channels_closed = static_cast<uint32_t>(data_channels.size()) -
                                data_channels_opened;
  return stats;
}

}  // namespace webrtc
