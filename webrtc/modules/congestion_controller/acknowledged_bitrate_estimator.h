/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MODULES_CONGESTION_CONTROLLER_ACKNOWLEDGED_BITRATE_ESTIMATOR_H_
#define WEBRTC_MODULES_CONGESTION_CONTROLLER_ACKNOWLEDGED_BITRATE_ESTIMATOR_H_

#include <memory>
#include <vector>

#include "webrtc/base/optional.h"
#include "webrtc/modules/congestion_controller/bitrate_estimator.h"

namespace webrtc {

struct PacketFeedback;

class BitrateEstimatorCreator {
 public:
  virtual ~BitrateEstimatorCreator() = default;
  virtual std::unique_ptr<BitrateEstimator> Create();
};

class AcknowledgedBitrateEstimator {
 public:
  explicit AcknowledgedBitrateEstimator(
      std::unique_ptr<BitrateEstimatorCreator> bitrate_estimator_creator);

  AcknowledgedBitrateEstimator();

  void IncomingPacketFeedbackVector(
      const std::vector<PacketFeedback>& packet_feedback_vector,
      bool currently_in_alr);
  rtc::Optional<uint32_t> bitrate_bps() const;

 private:
  bool SentBeforeAlrEnded(const PacketFeedback& packet);
  bool AlrEnded(bool currently_in_alr) const;
  void MaybeResetBitrateEstimator(bool currently_in_alr);

  bool was_in_alr_;
  rtc::Optional<int64_t> alr_ended_time_ms_;
  const std::unique_ptr<BitrateEstimatorCreator> bitrate_estimator_creator_;
  std::unique_ptr<BitrateEstimator> bitrate_estimator_;
};

}  // namespace webrtc

#endif  // WEBRTC_MODULES_CONGESTION_CONTROLLER_ACKNOWLEDGED_BITRATE_ESTIMATOR_H_
