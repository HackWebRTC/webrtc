/*
 *  Copyright (c) 2014 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 *
 *  Usage: this class will register multiple RtcpBitrateObserver's one at each
 *  RTCP module. It will aggregate the results and run one bandwidth estimation
 *  and push the result to the encoder via VideoEncoderCallback.
 */

#ifndef THIRD_PARTY_WEBRTC_MODULES_BITRATE_CONTROLLER_REMB_SUPPRESSOR_H_
#define THIRD_PARTY_WEBRTC_MODULES_BITRATE_CONTROLLER_REMB_SUPPRESSOR_H_

#include "webrtc/modules/rtp_rtcp/interface/rtp_rtcp_defines.h"
#include "webrtc/system_wrappers/interface/clock.h"

namespace webrtc {

class RembSuppressor {
 public:
  explicit RembSuppressor(Clock* clock);
  virtual ~RembSuppressor();

  // Check whether this new REMB value should be suppressed.
  bool SuppresNewRemb(uint32_t bitrate_bps);
  // Update the current bitrate actually being sent.
  void SetBitrateSent(uint32_t bitrate_bps);
  // Turn suppression on or off.
  void SetEnabled(bool enabled);

 protected:
  virtual bool Enabled();

 private:
  bool StartSuppressing(uint32_t bitrate_bps);
  bool ContinueSuppressing(uint32_t bitrate_bps);

  bool enabled_;
  Clock* const clock_;
  uint32_t last_remb_bps_;
  uint32_t bitrate_sent_bps_;
  uint32_t last_remb_ignored_bps_;
  uint32_t last_remb_ignore_time_ms_;
  int64_t remb_silence_start_;

  DISALLOW_COPY_AND_ASSIGN(RembSuppressor);
};

}  // namespace webrtc
#endif  // THIRD_PARTY_WEBRTC_MODULES_BITRATE_CONTROLLER_REMB_SUPPRESSOR_H_
