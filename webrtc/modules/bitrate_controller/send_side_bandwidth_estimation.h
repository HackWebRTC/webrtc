/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 *
 *  FEC and NACK added bitrate is handled outside class
 */

#ifndef WEBRTC_MODULES_BITRATE_CONTROLLER_SEND_SIDE_BANDWIDTH_ESTIMATION_H_
#define WEBRTC_MODULES_BITRATE_CONTROLLER_SEND_SIDE_BANDWIDTH_ESTIMATION_H_

#include "webrtc/modules/rtp_rtcp/interface/rtp_rtcp_defines.h"
#include "webrtc/system_wrappers/interface/critical_section_wrapper.h"

namespace webrtc {
class SendSideBandwidthEstimation {
 public:
  SendSideBandwidthEstimation();
  virtual ~SendSideBandwidthEstimation();

  void CurrentEstimate(uint32_t* bitrate, uint8_t* loss, uint32_t* rtt) const;

  // Call when we receive a RTCP message with TMMBR or REMB.
  void UpdateReceiverEstimate(uint32_t bandwidth);

  // Call when we receive a RTCP message with a ReceiveBlock.
  void UpdateReceiverBlock(uint8_t fraction_loss,
                           uint32_t rtt,
                           int number_of_packets,
                           uint32_t now_ms);

  void SetSendBitrate(uint32_t bitrate);
  void SetMinMaxBitrate(uint32_t min_bitrate, uint32_t max_bitrate);
  void SetMinBitrate(uint32_t min_bitrate);

 private:
  void UpdateEstimate(uint32_t now_ms);
  void CapBitrateToThresholds();

  // incoming filters
  int accumulate_lost_packets_Q8_;
  int accumulate_expected_packets_;

  uint32_t bitrate_;
  uint32_t min_bitrate_configured_;
  uint32_t max_bitrate_configured_;

  uint8_t last_fraction_loss_;
  uint16_t last_round_trip_time_;

  uint32_t bwe_incoming_;
  uint32_t time_last_increase_;
  uint32_t time_last_decrease_;
};
}  // namespace webrtc
#endif  // WEBRTC_MODULES_BITRATE_CONTROLLER_SEND_SIDE_BANDWIDTH_ESTIMATION_H_
