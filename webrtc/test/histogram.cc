/*
 *  Copyright (c) 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/test/histogram.h"

#include <map>

#include "webrtc/base/criticalsection.h"
#include "webrtc/base/thread_annotations.h"
#include "webrtc/system_wrappers/include/metrics.h"

// Test implementation of histogram methods in
// webrtc/system_wrappers/include/metrics.h.

namespace webrtc {
namespace {
struct SampleInfo {
  SampleInfo(int sample)
      : last(sample), total(1) {}
  int last;   // Last added sample.
  int total;  // Total number of added samples.
};

rtc::CriticalSection histogram_crit_;
// Map holding info about added samples to a histogram (mapped by the histogram
// name).
std::map<std::string, SampleInfo> histograms_ GUARDED_BY(histogram_crit_);
}  // namespace

namespace metrics {
Histogram* HistogramFactoryGetCounts(const std::string& name, int min, int max,
    int bucket_count) { return NULL; }

Histogram* HistogramFactoryGetEnumeration(const std::string& name,
    int boundary) { return NULL; }

void HistogramAdd(
    Histogram* histogram_pointer, const std::string& name, int sample) {
  rtc::CritScope cs(&histogram_crit_);
  auto it = histograms_.find(name);
  if (it == histograms_.end()) {
    histograms_.insert(std::make_pair(name, SampleInfo(sample)));
    return;
  }
  it->second.last = sample;
  ++it->second.total;
}
}  // namespace metrics

namespace test {
int LastHistogramSample(const std::string& name) {
  rtc::CritScope cs(&histogram_crit_);
  const auto it = histograms_.find(name);
  if (it == histograms_.end()) {
    return -1;
  }
  return it->second.last;
}

int NumHistogramSamples(const std::string& name) {
  rtc::CritScope cs(&histogram_crit_);
  const auto it = histograms_.find(name);
  if (it == histograms_.end()) {
    return 0;
  }
  return it->second.total;
}

void ClearHistograms() {
  rtc::CritScope cs(&histogram_crit_);
  histograms_.clear();
}
}  // namespace test
}  // namespace webrtc

