/*
 *  Copyright (c) 2014 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/modules/pacing/bitrate_prober.h"

#include <algorithm>

#include "webrtc/base/checks.h"
#include "webrtc/base/logging.h"
#include "webrtc/modules/pacing/paced_sender.h"

namespace webrtc {

namespace {

// Inactivity threshold above which probing is restarted.
constexpr int kInactivityThresholdMs = 5000;

int ComputeDeltaFromBitrate(size_t packet_size, uint32_t bitrate_bps) {
  RTC_CHECK_GT(bitrate_bps, 0u);
  // Compute the time delta needed to send packet_size bytes at bitrate_bps
  // bps. Result is in milliseconds.
  return static_cast<int>((1000ll * packet_size * 8) / bitrate_bps);
}
}  // namespace

BitrateProber::BitrateProber()
    : probing_state_(ProbingState::kDisabled),
      packet_size_last_sent_(0),
      time_last_probe_sent_ms_(-1),
      next_cluster_id_(0) {
  SetEnabled(true);
}

void BitrateProber::SetEnabled(bool enable) {
  if (enable) {
    if (probing_state_ == ProbingState::kDisabled) {
      probing_state_ = ProbingState::kInactive;
      LOG(LS_INFO) << "Bandwidth probing enabled, set to inactive";
    }
  } else {
    probing_state_ = ProbingState::kDisabled;
    LOG(LS_INFO) << "Bandwidth probing disabled";
  }
}

bool BitrateProber::IsProbing() const {
  return probing_state_ == ProbingState::kActive;
}

void BitrateProber::OnIncomingPacket(size_t packet_size) {
  // Don't initialize probing unless we have something large enough to start
  // probing.
  if (probing_state_ == ProbingState::kInactive &&
      !clusters_.empty() &&
      packet_size >= PacedSender::kMinProbePacketSize) {
    probing_state_ = ProbingState::kActive;
  }
}

void BitrateProber::CreateProbeCluster(int bitrate_bps, int num_packets) {
  RTC_DCHECK(probing_state_ != ProbingState::kDisabled);
  ProbeCluster cluster;
  cluster.max_probe_packets = num_packets;
  cluster.probe_bitrate_bps = bitrate_bps;
  cluster.id = next_cluster_id_++;
  clusters_.push(cluster);
  LOG(LS_INFO) << "Probe cluster (bitrate:packets): ("
               << cluster.probe_bitrate_bps << ":" << cluster.max_probe_packets
               << ") ";
  if (probing_state_ != ProbingState::kActive)
    probing_state_ = ProbingState::kInactive;
}

void BitrateProber::ResetState() {
  time_last_probe_sent_ms_ = -1;
  packet_size_last_sent_ = 0;

  // Recreate all probing clusters.
  std::queue<ProbeCluster> clusters;
  clusters.swap(clusters_);
  while (!clusters.empty()) {
    CreateProbeCluster(clusters.front().probe_bitrate_bps,
                       clusters.front().max_probe_packets);
    clusters.pop();
  }
  // If its enabled, reset to inactive.
  if (probing_state_ != ProbingState::kDisabled)
    probing_state_ = ProbingState::kInactive;
}

int BitrateProber::TimeUntilNextProbe(int64_t now_ms) {
  // Probing is not active or probing is already complete.
  if (probing_state_ != ProbingState::kActive || clusters_.empty())
    return -1;
  // time_last_probe_sent_ms_ of -1 indicates no probes have yet been sent.
  int64_t elapsed_time_ms;
  if (time_last_probe_sent_ms_ == -1) {
    elapsed_time_ms = 0;
  } else {
    elapsed_time_ms = now_ms - time_last_probe_sent_ms_;
  }
  // If no probes have been sent for a while, abort current probing and
  // reset.
  if (elapsed_time_ms > kInactivityThresholdMs) {
    ResetState();
    return -1;
  }
  // We will send the first probe packet immediately if no packet has been
  // sent before.
  int time_until_probe_ms = 0;
  if (packet_size_last_sent_ != 0 && probing_state_ == ProbingState::kActive) {
    int next_delta_ms = ComputeDeltaFromBitrate(
        packet_size_last_sent_, clusters_.front().probe_bitrate_bps);
    time_until_probe_ms = next_delta_ms - elapsed_time_ms;
    // There is no point in trying to probe with less than 1 ms between packets
    // as it essentially means trying to probe at infinite bandwidth.
    const int kMinProbeDeltaMs = 1;
    // If we have waited more than 3 ms for a new packet to probe with we will
    // consider this probing session over.
    const int kMaxProbeDelayMs = 3;
    if (next_delta_ms < kMinProbeDeltaMs ||
        time_until_probe_ms < -kMaxProbeDelayMs) {
      probing_state_ = ProbingState::kSuspended;
      LOG(LS_INFO) << "Delta too small or missed probing accurately, suspend";
      time_until_probe_ms = 0;
    }
  }
  return std::max(time_until_probe_ms, 0);
}

int BitrateProber::CurrentClusterId() const {
  RTC_DCHECK(!clusters_.empty());
  RTC_DCHECK(ProbingState::kActive == probing_state_);
  return clusters_.front().id;
}

size_t BitrateProber::RecommendedPacketSize() const {
  return packet_size_last_sent_;
}

void BitrateProber::PacketSent(int64_t now_ms, size_t packet_size) {
  assert(packet_size > 0);
  if (packet_size < PacedSender::kMinProbePacketSize)
    return;
  packet_size_last_sent_ = packet_size;
  if (probing_state_ != ProbingState::kActive)
    return;
  time_last_probe_sent_ms_ = now_ms;
  if (!clusters_.empty()) {
    ProbeCluster* cluster = &clusters_.front();
    ++cluster->sent_probe_packets;
    if (cluster->sent_probe_packets == cluster->max_probe_packets)
      clusters_.pop();
    if (clusters_.empty())
      probing_state_ = ProbingState::kSuspended;
  }
}
}  // namespace webrtc
