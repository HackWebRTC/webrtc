/*
 *  Copyright (c) 2026 The WebRTC project authors. All Rights Reserved.
 */

#include "modules/roq/quic_bandwidth_policy.h"

#include <algorithm>

#include "api/units/time_delta.h"

namespace webrtc::roq {

QuicBandwidthPolicy::QuicBandwidthPolicy(DataRate minimum_rate,
                                         DataRate startup_rate,
                                         DataRate maximum_rate)
    : minimum_rate_(minimum_rate),
      startup_rate_(startup_rate),
      maximum_rate_(maximum_rate),
      target_rate_(startup_rate) {}

void QuicBandwidthPolicy::SetLimits(DataRate minimum_rate,
                                    DataRate startup_rate,
                                    DataRate maximum_rate) {
  minimum_rate_ = minimum_rate;
  startup_rate_ = startup_rate;
  maximum_rate_ = maximum_rate;
  target_rate_ =
      has_fresh_sample_ ? std::min(target_rate_, maximum_rate_) : startup_rate_;
}

QuicMediaBudget QuicBandwidthPolicy::Update(const QuicBandwidthSample& sample) {
  // ACK totals belong to one QUIC connection. A retained Call observes a
  // counter reset during reconnect, which must not wait for the old total.
  if (sample.acked_bytes < last_acked_bytes_) {
    last_acked_bytes_ = 0;
    last_ack_time_.reset();
    has_fresh_sample_ = false;
    target_rate_ = startup_rate_;
  }
  if (sample.acked_bytes > last_acked_bytes_) {
    last_acked_bytes_ = sample.acked_bytes;
    last_ack_time_ = sample.at_time;
  }
  const bool valid = sample.round_trip_time > TimeDelta::Zero() &&
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
    has_fresh_sample_ = true;
    int64_t filtered = 0;
    if (usable_media_rate <= target_rate_) {
      filtered = usable_media_rate.bps();
    } else {
      filtered = (target_rate_.bps() * 8 + usable_media_rate.bps() * 2) / 10;
    }
    target_rate_ =
        DataRate::BitsPerSec(std::min(filtered, maximum_rate_.bps()));
  } else {
    target_rate_ = has_fresh_sample_ ? DataRate::Zero() : startup_rate_;
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
