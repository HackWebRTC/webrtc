/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_VIDEO_CODING_PROTECTION_BITRATE_CALCULATOR_H_
#define MODULES_VIDEO_CODING_PROTECTION_BITRATE_CALCULATOR_H_

#include <list>
#include <memory>
#include <vector>

#include "modules/include/module_common_types.h"
#include "modules/video_coding/include/video_coding.h"
#include "modules/video_coding/media_opt_util.h"
#include "rtc_base/criticalsection.h"
#include "system_wrappers/include/clock.h"

namespace webrtc {

class ProtectionBitrateCalculator {
 public:
  virtual ~ProtectionBitrateCalculator() {}

  virtual void SetProtectionMethod(bool enable_fec, bool enable_nack) = 0;

  // Informs media optimization of initial encoding state.
  virtual void SetEncodingData(size_t width,
                               size_t height,
                               size_t num_temporal_layers,
                               size_t max_payload_size) = 0;

  // Returns target rate for the encoder given the channel parameters.
  // Inputs:  estimated_bitrate_bps - the estimated network bitrate in bits/s.
  //          actual_framerate - encoder frame rate.
  //          fraction_lost - packet loss rate in % in the network.
  //          round_trip_time_ms - round trip time in milliseconds.
  virtual uint32_t SetTargetRates(uint32_t estimated_bitrate_bps,
                                  int actual_framerate,
                                  uint8_t fraction_lost,
                                  int64_t round_trip_time_ms) = 0;

  virtual uint32_t SetTargetRates(uint32_t estimated_bitrate_bps,
                                  std::vector<uint8_t> loss_mask_vector,
                                  int64_t round_trip_time_ms) = 0;

  // Informs of encoded output.
  virtual void UpdateWithEncodedData(const EncodedImage& encoded_image) = 0;

  virtual void OnLossMaskVector(const std::vector<bool> loss_mask_vector) = 0;
};

}  // namespace webrtc

#endif  // MODULES_VIDEO_CODING_PROTECTION_BITRATE_CALCULATOR_H_
