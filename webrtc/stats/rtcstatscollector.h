/*
 *  Copyright 2016 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_STATS_RTCSTATSCOLLECTOR_H_
#define WEBRTC_STATS_RTCSTATSCOLLECTOR_H_

#include <memory>

#include "webrtc/api/rtcstats_objects.h"
#include "webrtc/api/rtcstatsreport.h"
#include "webrtc/base/scoped_ref_ptr.h"
#include "webrtc/base/timeutils.h"

namespace webrtc {

class PeerConnection;

// All calls to the collector and gathering of stats is performed on the
// signaling thread. A stats report is cached for |cache_lifetime_| ms.
class RTCStatsCollector {
 public:
  explicit RTCStatsCollector(
      PeerConnection* pc,
      int64_t cache_lifetime_us = 50 * rtc::kNumMicrosecsPerMillisec);

  // Gets a recent stats report. If there is a report cached that is still fresh
  // it is returned, otherwise new stats are gathered and returned. A report is
  // considered fresh for |cache_lifetime_| ms. const RTCStatsReports are safe
  // to use across multiple threads and may be destructed on any thread.
  rtc::scoped_refptr<const RTCStatsReport> GetStatsReport();
  // Clears the cache's reference to the most recent stats report. Subsequently
  // calling |GetStatsReport| guarantees fresh stats.
  void ClearCachedStatsReport();

 private:
  bool IsOnSignalingThread() const;

  std::unique_ptr<RTCPeerConnectionStats> ProducePeerConnectionStats(
      int64_t timestamp_us) const;

  PeerConnection* const pc_;
  // A timestamp, in microseconds, that is based on a timer that is
  // monotonically increasing. That is, even if the system clock is modified the
  // difference between the timer and this timestamp is how fresh the cached
  // report is.
  int64_t cache_timestamp_us_;
  int64_t cache_lifetime_us_;
  rtc::scoped_refptr<const RTCStatsReport> cached_report_;
};

}  // namespace webrtc

#endif  // WEBRTC_STATS_RTCSTATSCOLLECTOR_H_
