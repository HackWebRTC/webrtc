/*
 *  Copyright 2016 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_API_RTCSTATSCOLLECTOR_H_
#define WEBRTC_API_RTCSTATSCOLLECTOR_H_

#include <memory>
#include <vector>

#include "webrtc/api/stats/rtcstats_objects.h"
#include "webrtc/api/stats/rtcstatsreport.h"
#include "webrtc/base/asyncinvoker.h"
#include "webrtc/base/refcount.h"
#include "webrtc/base/scoped_ref_ptr.h"
#include "webrtc/base/timeutils.h"

namespace webrtc {

class PeerConnection;

class RTCStatsCollectorCallback : public virtual rtc::RefCountInterface {
 public:
  virtual ~RTCStatsCollectorCallback() {}

  virtual void OnStatsDelivered(
      const rtc::scoped_refptr<const RTCStatsReport>& report) = 0;
};

// All public methods of the collector are to be called on the signaling thread.
// Stats are gathered on the signaling, worker and network threads
// asynchronously. The callback is invoked on the signaling thread. Resulting
// reports are cached for |cache_lifetime_| ms.
class RTCStatsCollector : public virtual rtc::RefCountInterface {
 public:
  static rtc::scoped_refptr<RTCStatsCollector> Create(
      PeerConnection* pc,
      int64_t cache_lifetime_us = 50 * rtc::kNumMicrosecsPerMillisec);

  // Gets a recent stats report. If there is a report cached that is still fresh
  // it is returned, otherwise new stats are gathered and returned. A report is
  // considered fresh for |cache_lifetime_| ms. const RTCStatsReports are safe
  // to use across multiple threads and may be destructed on any thread.
  void GetStatsReport(rtc::scoped_refptr<RTCStatsCollectorCallback> callback);
  // Clears the cache's reference to the most recent stats report. Subsequently
  // calling |GetStatsReport| guarantees fresh stats.
  void ClearCachedStatsReport();

 protected:
  RTCStatsCollector(PeerConnection* pc, int64_t cache_lifetime_us);

  // Stats gathering on a particular thread. Calls |AddPartialResults| before
  // returning. Virtual for the sake of testing.
  virtual void ProducePartialResultsOnSignalingThread(int64_t timestamp_us);
  virtual void ProducePartialResultsOnWorkerThread(int64_t timestamp_us);
  virtual void ProducePartialResultsOnNetworkThread(int64_t timestamp_us);

  // Can be called on any thread.
  void AddPartialResults(
      const rtc::scoped_refptr<RTCStatsReport>& partial_report);

 private:
  void AddPartialResults_s(rtc::scoped_refptr<RTCStatsReport> partial_report);
  void DeliverCachedReport();

  std::unique_ptr<RTCPeerConnectionStats> ProducePeerConnectionStats_s(
      int64_t timestamp_us) const;

  PeerConnection* const pc_;
  rtc::Thread* const signaling_thread_;
  rtc::Thread* const worker_thread_;
  rtc::Thread* const network_thread_;
  rtc::AsyncInvoker invoker_;

  int num_pending_partial_reports_;
  int64_t partial_report_timestamp_us_;
  rtc::scoped_refptr<RTCStatsReport> partial_report_;
  std::vector<rtc::scoped_refptr<RTCStatsCollectorCallback>> callbacks_;

  // A timestamp, in microseconds, that is based on a timer that is
  // monotonically increasing. That is, even if the system clock is modified the
  // difference between the timer and this timestamp is how fresh the cached
  // report is.
  int64_t cache_timestamp_us_;
  int64_t cache_lifetime_us_;
  rtc::scoped_refptr<const RTCStatsReport> cached_report_;
};

}  // namespace webrtc

#endif  // WEBRTC_API_RTCSTATSCOLLECTOR_H_
