/*
 *  Copyright (c) 2026 The WebRTC project authors. All Rights Reserved.
 */

#include "modules/roq/quic_bandwidth_policy.h"

#include <algorithm>

#include "api/units/time_delta.h"

namespace webrtc::roq {

QuicMediaBudget QuicBandwidthPolicy::Update(const QuicBandwidthSample& sample) {
  constexpr int64_t kStartupRateBps = 1'000'000;
  constexpr int64_t kMinimumRateBps = 256'000;
  constexpr int64_t kMaximumRateBps = 4'128'000;
  if (sample.acked_bytes > last_acked_bytes_) {
    last_acked_bytes_ = sample.acked_bytes;
    last_ack_time_ = sample.at_time;
  }
  const bool valid = sample.startup_complete &&
                     sample.round_trip_time > TimeDelta::Zero() &&
                     sample.max_bandwidth > DataRate::Zero() &&
                     sample.delivery_rate > DataRate::Zero();
  const bool fresh = valid && last_ack_time_.has_value() &&
                     sample.at_time - *last_ack_time_ <= TimeDelta::Seconds(1);
  const DataRate raw_capacity =
      valid ? std::min(sample.max_bandwidth, sample.delivery_rate)
            : DataRate::Zero();
  const DataRate usable_media_rate =
      valid ? DataRate::BitsPerSec(raw_capacity.bps() * 75 / 100)
            : DataRate::Zero();
  const DataRate overhead =
      valid ? DataRate::BitsPerSec(raw_capacity.bps() * 5 / 100)
            : DataRate::Zero();
  const DataRate recovery_reserve =
      valid ? DataRate::BitsPerSec(raw_capacity.bps() * 10 / 100)
            : DataRate::Zero();
  const DataRate safety_headroom =
      valid ? raw_capacity - usable_media_rate - overhead - recovery_reserve
            : DataRate::Zero();
  if (fresh) {
    int64_t filtered = 0;
    if (usable_media_rate <= target_rate_) {
      filtered = usable_media_rate.bps();
    } else {
      filtered = (target_rate_.bps() * 8 + usable_media_rate.bps() * 2) / 10;
    }
    target_rate_ = DataRate::BitsPerSec(
        std::clamp(filtered, kMinimumRateBps, kMaximumRateBps));
  } else {
    target_rate_ =
        std::min(target_rate_, DataRate::BitsPerSec(kStartupRateBps));
  }
  return {.fresh = fresh,
          .raw_capacity = raw_capacity,
          .overhead = overhead,
          .recovery_reserve = recovery_reserve,
          .safety_headroom = safety_headroom,
          .usable_media_rate = usable_media_rate,
          .target_rate = target_rate_,
          .round_trip_time = sample.round_trip_time};
}

}  // namespace webrtc::roq
