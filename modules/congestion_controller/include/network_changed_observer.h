/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_CONGESTION_CONTROLLER_INCLUDE_NETWORK_CHANGED_OBSERVER_H_
#define MODULES_CONGESTION_CONTROLLER_INCLUDE_NETWORK_CHANGED_OBSERVER_H_

#include <stdint.h>

namespace webrtc {

// Observer class for bitrate changes announced due to change in bandwidth
// estimate or due to that the send pacer is full. Fraction loss and rtt is
// also part of this callback to allow the observer to optimize its settings
// for different types of network environments. The bitrate does not include
// packet headers and is measured in bits per second.
class NetworkChangedObserver {
 public:
  virtual void OnNetworkChanged(uint32_t bitrate_bps,
                                uint8_t fraction_loss,  // 0 - 255.
                                int64_t rtt_ms,
                                int64_t probing_interval_ms) = 0;

 protected:
  virtual ~NetworkChangedObserver() {}
};
}  // namespace webrtc
#endif  // MODULES_CONGESTION_CONTROLLER_INCLUDE_NETWORK_CHANGED_OBSERVER_H_
