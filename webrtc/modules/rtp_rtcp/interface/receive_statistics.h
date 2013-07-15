/*
 *  Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MODULES_RTP_RTCP_INTERFACE_RECEIVE_STATISTICS_H_
#define WEBRTC_MODULES_RTP_RTCP_INTERFACE_RECEIVE_STATISTICS_H_

#include "webrtc/modules/interface/module.h"
#include "webrtc/modules/interface/module_common_types.h"
#include "webrtc/typedefs.h"

namespace webrtc {

class Clock;

class ReceiveStatistics : public Module {
 public:
  struct RtpReceiveStatistics {
    uint8_t fraction_lost;
    uint32_t cumulative_lost;
    uint32_t extended_max_sequence_number;
    uint32_t jitter;
    uint32_t max_jitter;
  };

  virtual ~ReceiveStatistics() {}

  static ReceiveStatistics* Create(Clock* clock);

  virtual void IncomingPacket(const RTPHeader& rtp_header, size_t bytes,
                              bool retransmitted, bool in_order) = 0;

  virtual bool Statistics(RtpReceiveStatistics* statistics, bool reset) = 0;

  virtual bool Statistics(RtpReceiveStatistics* statistics, int32_t* missing,
                          bool reset) = 0;

  virtual void GetDataCounters(uint32_t* bytes_received,
                               uint32_t* packets_received) const = 0;

  virtual uint32_t BitrateReceived() = 0;

  virtual void ResetStatistics() = 0;

  virtual void ResetDataCounters() = 0;
};
}  // namespace webrtc
#endif  // WEBRTC_MODULES_RTP_RTCP_INTERFACE_RECEIVE_STATISTICS_H_
