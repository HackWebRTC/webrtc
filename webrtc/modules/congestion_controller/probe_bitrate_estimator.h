/*
 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MODULES_CONGESTION_CONTROLLER_PROBE_BITRATE_ESTIMATOR_H_
#define WEBRTC_MODULES_CONGESTION_CONTROLLER_PROBE_BITRATE_ESTIMATOR_H_

#include <map>
#include <limits>

#include "webrtc/modules/rtp_rtcp/include/rtp_rtcp_defines.h"

namespace webrtc {

struct ProbingResult {
  static constexpr int kNoEstimate = -1;

  ProbingResult();
  ProbingResult(int bps, int64_t timestamp);

  int bps;
  int64_t timestamp;
};

class ProbeBitrateEstimator {
 public:
  ProbeBitrateEstimator();

  // Should be called for every packet we receive feedback about. If the
  // packet was used for probing it will validate/calculate the resulting
  // bitrate and return the result.
  ProbingResult PacketFeedback(const PacketInfo& packet_info);

 private:
  struct AggregatedCluster {
    int num_probes = 0;
    int64_t first_send_ms = std::numeric_limits<int64_t>::max();
    int64_t last_send_ms = 0;
    int64_t first_receive_ms = std::numeric_limits<int64_t>::max();
    int64_t last_receive_ms = 0;
    size_t size = 0;
  };

  std::map<int, AggregatedCluster> clusters_;
  int last_valid_cluster_id_;
};

}  // namespace webrtc

#endif  // WEBRTC_MODULES_CONGESTION_CONTROLLER_PROBE_BITRATE_ESTIMATOR_H_
