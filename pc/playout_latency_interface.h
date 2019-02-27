/*
 *  Copyright 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef PC_PLAYOUT_LATENCY_INTERFACE_H_
#define PC_PLAYOUT_LATENCY_INTERFACE_H_

#include <stdint.h>

#include "media/base/delayable.h"
#include "rtc_base/ref_count.h"

namespace webrtc {

// PlayoutLatency delivers user's latency queries to the underlying media
// channel. It can describe either video or audio latency for receiving stream.
// "Interface" suffix in the interface name is required to be compatible with
// api/proxy.cc
class PlayoutLatencyInterface : public rtc::RefCountInterface {
 public:
  // OnStart allows to uniqely identify to which receiving stream playout
  // latency must correpond through |media_channel| and |ssrc| pair.
  virtual void OnStart(cricket::Delayable* media_channel, uint32_t ssrc) = 0;

  // Indicates that underlying receiving stream is stopped.
  virtual void OnStop() = 0;

  // Sets latency in seconds.
  virtual void SetLatency(double latency) = 0;

  // Returns latency in seconds.
  virtual double GetLatency() const = 0;
};

}  // namespace webrtc

#endif  // PC_PLAYOUT_LATENCY_INTERFACE_H_
