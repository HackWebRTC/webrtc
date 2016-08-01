/*
 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/modules/congestion_controller/probe_bitrate_estimator.h"

#include <algorithm>

#include "webrtc/base/logging.h"

namespace {
// Max number of saved clusters.
constexpr size_t kMaxNumSavedClusters = 5;

// The minumum number of probes we need for a valid cluster.
constexpr int kMinNumProbesValidCluster = 4;

// The maximum (receive rate)/(send rate) ratio for a valid estimate.
constexpr float kValidRatio = 1.2f;
}

namespace webrtc {

ProbingResult::ProbingResult() : bps(kNoEstimate), timestamp(0) {}

ProbingResult::ProbingResult(int bps, int64_t timestamp)
    : bps(bps), timestamp(timestamp) {}

ProbeBitrateEstimator::ProbeBitrateEstimator() : last_valid_cluster_id_(0) {}

ProbingResult ProbeBitrateEstimator::PacketFeedback(
    const PacketInfo& packet_info) {
  // If this is not a probing packet or if this probing packet
  // belongs to an old cluster, do nothing.
  if (packet_info.probe_cluster_id == PacketInfo::kNotAProbe ||
      packet_info.probe_cluster_id < last_valid_cluster_id_) {
    return ProbingResult();
  }

  AggregatedCluster* cluster = &clusters_[packet_info.probe_cluster_id];
  cluster->first_send_ms =
      std::min(cluster->first_send_ms, packet_info.send_time_ms);
  cluster->last_send_ms =
      std::max(cluster->last_send_ms, packet_info.send_time_ms);
  cluster->first_receive_ms =
      std::min(cluster->first_receive_ms, packet_info.arrival_time_ms);
  cluster->last_receive_ms =
      std::max(cluster->last_receive_ms, packet_info.arrival_time_ms);
  cluster->size += packet_info.payload_size;
  cluster->num_probes += 1;

  // Clean up old clusters.
  while (clusters_.size() > kMaxNumSavedClusters)
    clusters_.erase(clusters_.begin());

  if (cluster->num_probes < kMinNumProbesValidCluster)
    return ProbingResult();

  int send_interval_ms = cluster->last_send_ms - cluster->first_send_ms;
  int receive_interval_ms =
      cluster->last_receive_ms - cluster->first_receive_ms;

  if (send_interval_ms == 0 || receive_interval_ms == 0) {
    LOG(LS_INFO) << "Probing unsuccessful, invalid send/receive interval"
                 << " [cluster id: " << packet_info.probe_cluster_id
                 << "] [send interval: " << send_interval_ms << " ms]"
                 << " [receive interval: " << receive_interval_ms << " ms]";

    return ProbingResult();
  }

  float send_bps = static_cast<float>(cluster->size) / send_interval_ms * 1000;
  float receive_bps =
      static_cast<float>(cluster->size) / receive_interval_ms * 1000;
  float ratio = receive_bps / send_bps;
  if (ratio > kValidRatio) {
    LOG(LS_INFO) << "Probing unsuccessful, receive/send ratio too high"
                 << " [cluster id: " << packet_info.probe_cluster_id
                 << "] [send: " << cluster->size << " bytes / "
                 << send_interval_ms << " ms = " << send_bps / 1000 << " kb/s]"
                 << " [receive: " << cluster->size << " bytes / "
                 << receive_interval_ms << " ms = " << receive_bps / 1000
                 << " kb/s]"
                 << " [ratio: " << receive_bps / 1000 << " / "
                 << send_bps / 1000 << " = " << ratio << " > kValidRatio ("
                 << kValidRatio << ")]";

    return ProbingResult();
  }
  // We have a valid estimate.
  int result_bps = std::min(send_bps, receive_bps);
  last_valid_cluster_id_ = packet_info.probe_cluster_id;
  LOG(LS_INFO) << "Probing successful"
               << " [cluster id: " << packet_info.probe_cluster_id
               << "] [send: " << cluster->size << " bytes / "
               << send_interval_ms << " ms = " << send_bps / 1000 << " kb/s]"
               << " [receive: " << cluster->size << " bytes / "
               << receive_interval_ms << " ms = " << receive_bps / 1000
               << " kb/s]";

  return ProbingResult(result_bps, packet_info.arrival_time_ms);
}
}  // namespace webrtc
