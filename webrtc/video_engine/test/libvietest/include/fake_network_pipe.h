/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_VIDEO_ENGINE_TEST_LIBVIETEST_INCLUDE_FAKE_NETWORK_PIPE_H_
#define WEBRTC_VIDEO_ENGINE_TEST_LIBVIETEST_INCLUDE_FAKE_NETWORK_PIPE_H_

#include <queue>

#include "webrtc/system_wrappers/interface/constructor_magic.h"
#include "webrtc/system_wrappers/interface/scoped_ptr.h"
#include "webrtc/typedefs.h"

namespace webrtc {

class CriticalSectionWrapper;
class NetworkPacket;

class PacketReceiver {
 public:
  // Delivers a new packet to the receive side of the network pipe. The
  // implementor of PacketReceiver now owns the memory.
  virtual void IncomingPacket(uint8_t* packet, int length) = 0;
  virtual ~PacketReceiver() {}
};

// Class faking a network link. This is a simple and naive solution just faking
// capacity and adding an extra transport delay in addition to the capacity
// introduced delay.

// TODO(mflodman) Add random and bursty packet loss.
class FakeNetworkPipe {
 public:
  FakeNetworkPipe(PacketReceiver* packet_receiver,
                  size_t queue_length,
                  int queue_delay_ms,
                  int link_capacity_kbps,
                  int loss_percent);
  ~FakeNetworkPipe();

  // Sends a new packet to the link.
  void SendPacket(void* packet, int packet_length);

  // Processes the network queues and trigger PacketReceiver::IncomingPacket for
  // packets ready to be delivered.
  void NetworkProcess();

  // Get statistics.
  float PercentageLoss();
  int AverageDelay();
  int dropped_packets() { return dropped_packets_; }
  int sent_packets() { return sent_packets_; }

 private:
  PacketReceiver* packet_receiver_;
  scoped_ptr<CriticalSectionWrapper> link_cs_;
  std::queue<NetworkPacket*> capacity_link_;
  std::queue<NetworkPacket*> delay_link_;

  // Link configuration.
  const size_t queue_length_;
  const int queue_delay_ms_;
  const int link_capacity_bytes_ms_;  // In bytes per ms.

  const int loss_percent_;

  // Statistics.
  int dropped_packets_;
  int sent_packets_;
  int total_packet_delay_;

  DISALLOW_COPY_AND_ASSIGN(FakeNetworkPipe);
};

}  // namespace webrtc

#endif  // WEBRTC_VIDEO_ENGINE_TEST_LIBVIETEST_INCLUDE_FAKE_NETWORK_PIPE_H_
