/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 *
 */

#include "webrtc/modules/remote_bitrate_estimator/test/estimators/congestion_window.h"

#include <algorithm>

#include "webrtc/modules/remote_bitrate_estimator/test/estimators/bbr.h"

namespace webrtc {
namespace testing {
namespace bwe {
namespace {
// kStartingCongestionWindow is used to set congestion window when bandwidth
// delay product is equal to zero, so that we don't set window to zero as well.
// Chosen randomly by me, because this value shouldn't make any significant
// difference, as bandwidth delay product is more than zero almost every time.
const int kStartingCongestionWindow = 6000;
// Size of congestion window while in PROBE_RTT mode, suggested by BBR's source
// code of QUIC's implementation.
const int kMinimumCongestionWindow = 5840;
}  // namespace

CongestionWindow::CongestionWindow() : data_inflight_bytes_(0) {}

CongestionWindow::~CongestionWindow() {}

int CongestionWindow::GetCongestionWindow(
    BbrBweSender::Mode mode,
    int64_t bandwidth_estimate_bytes_per_ms,
    int64_t min_rtt_ms,
    float gain) {
  if (mode == BbrBweSender::PROBE_RTT)
    return kMinimumCongestionWindow;
  return GetTargetCongestionWindow(bandwidth_estimate_bytes_per_ms, min_rtt_ms,
                                   gain);
}

void CongestionWindow::PacketSent(size_t sent_packet_size_bytes) {
  data_inflight_bytes_ += sent_packet_size_bytes;
}

void CongestionWindow::AckReceived(size_t received_packet_size_bytes) {
  data_inflight_bytes_ -= received_packet_size_bytes;
}

int CongestionWindow::GetTargetCongestionWindow(
    int64_t bandwidth_estimate_bytes_per_ms,
    int64_t min_rtt_ms,
    float gain) {
  int bdp = min_rtt_ms * bandwidth_estimate_bytes_per_ms;
  int congestion_window = bdp * gain;
  // Congestion window could be zero in rare cases, when either no bandwidth
  // estimate is available, or path's min_rtt value is zero.
  if (!congestion_window)
    congestion_window = gain * kStartingCongestionWindow;
  return std::max(congestion_window, kMinimumCongestionWindow);
}
}  // namespace bwe
}  // namespace testing
}  // namespace webrtc
