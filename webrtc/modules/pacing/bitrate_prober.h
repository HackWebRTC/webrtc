/*
 *  Copyright (c) 2014 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MODULES_PACING_BITRATE_PROBER_H_
#define WEBRTC_MODULES_PACING_BITRATE_PROBER_H_

#include <queue>

#include "webrtc/base/basictypes.h"
#include "webrtc/typedefs.h"

namespace webrtc {

// Note that this class isn't thread-safe by itself and therefore relies
// on being protected by the caller.
class BitrateProber {
 public:
  BitrateProber();

  void SetEnabled(bool enable);

  // Returns true if the prober is in a probing session, i.e., it currently
  // wants packets to be sent out according to the time returned by
  // TimeUntilNextProbe().
  bool IsProbing() const;

  // Initializes a new probing session if the prober is allowed to probe. Does
  // not initialize the prober unless the packet size is large enough to probe
  // with.
  void OnIncomingPacket(size_t packet_size);

  // Create a cluster used to probe for |bitrate_bps| with |num_packets| number
  // of packets.
  void CreateProbeCluster(int bitrate_bps, int num_packets);

  // Returns the number of milliseconds until the next packet should be sent to
  // get accurate probing.
  int TimeUntilNextProbe(int64_t now_ms);

  // Which cluster that is currently being used for probing.
  int CurrentClusterId() const;

  // Returns the number of bytes that the prober recommends for the next probe
  // packet.
  size_t RecommendedPacketSize() const;

  // Called to report to the prober that a packet has been sent, which helps the
  // prober know when to move to the next packet in a probe.
  void PacketSent(int64_t now_ms, size_t packet_size);

 private:
  enum class ProbingState {
    // Probing will not be triggered in this state at all times.
    kDisabled,
    // Probing is enabled and ready to trigger on the first packet arrival.
    kInactive,
    // Probe cluster is filled with the set of data rates to be probed and
    // probes are being sent.
    kActive,
    // Probing is enabled, but currently suspended until an explicit trigger
    // to start probing again.
    kSuspended,
  };

  struct ProbeCluster {
    int max_probe_packets = 0;
    int sent_probe_packets = 0;
    int probe_bitrate_bps = 0;
    int id = -1;
  };

  // Resets the state of the prober and clears any cluster/timing data tracked.
  void ResetState();

  ProbingState probing_state_;
  // Probe bitrate per packet. These are used to compute the delta relative to
  // the previous probe packet based on the size and time when that packet was
  // sent.
  std::queue<ProbeCluster> clusters_;
  size_t packet_size_last_sent_;
  // The last time a probe was sent.
  int64_t time_last_probe_sent_ms_;
  int next_cluster_id_;
};
}  // namespace webrtc
#endif  // WEBRTC_MODULES_PACING_BITRATE_PROBER_H_
