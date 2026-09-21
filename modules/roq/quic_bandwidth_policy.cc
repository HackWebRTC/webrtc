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
      capacity_known_ ? std::min(target_rate_, maximum_rate_) : startup_rate_;
}

QuicMediaBudget QuicBandwidthPolicy::Update(const QuicBandwidthSample& sample) {
  // A retained Call can move to another QUIC connection. Only its explicit
  // identity change grants startup again, never an ACK counter anomaly or a
  // publication/subscription update. Empty IDs mean no live transport yet.
  if (!sample.connection_id.empty()) {
    if (!connection_id_.empty() && connection_id_ != sample.connection_id)
      *this = QuicBandwidthPolicy(minimum_rate_, startup_rate_, maximum_rate_);
    connection_id_ = sample.connection_id;
  }
  const bool ack_advanced = sample.acked_bytes > last_acked_bytes_;
  last_acked_bytes_ = sample.acked_bytes;
  if (ack_advanced)
    last_ack_time_ = sample.at_time;
  const bool has_demand = sample.maximum_media_rate > DataRate::Zero();
  if (!has_demand)
    demand_started_at_.reset();
  else if (!demand_started_at_)
    demand_started_at_ = sample.at_time;
  const bool valid = sample.round_trip_time > TimeDelta::Zero() &&
                     sample.max_bandwidth > DataRate::Zero() &&
                     sample.delivery_rate > DataRate::Zero();
  const bool fresh = valid && last_ack_time_.has_value() &&
                     sample.at_time - *last_ack_time_ <= TimeDelta::Seconds(1);
  const DataRate raw_capacity =
      valid ? std::min(sample.max_bandwidth, sample.delivery_rate)
            : DataRate::Zero();
  // Learn packetization and RED cost from produced bytes, not just survivors
  // of a congested queue. Keep the last ratio while video is suspended.
  if (!cost_sample_ ||
      sample.audio_wire_bytes < cost_sample_->audio_wire_bytes ||
      sample.audio_payload_bytes < cost_sample_->audio_payload_bytes ||
      sample.audio_packets < cost_sample_->audio_packets ||
      sample.audio_redundant_payload_bytes <
          cost_sample_->audio_redundant_payload_bytes ||
      sample.audio_redundant_blocks < cost_sample_->audio_redundant_blocks ||
      sample.rtcp_wire_bytes < cost_sample_->rtcp_wire_bytes ||
      sample.video_payload_bytes < cost_sample_->video_payload_bytes ||
      sample.video_wire_bytes < cost_sample_->video_wire_bytes) {
    cost_sample_ = sample;
    audio_redundancy_multiplier_ = 2.0;
    audio_header_bytes_per_packet_ = 118;
    audio_packet_rate_ = sample.audio_packet_rate;
    video_wire_ratio_ = 1.08;
  } else if (sample.at_time - cost_sample_->at_time >= TimeDelta::Millis(500)) {
    auto ratio = [](uint64_t payload, uint64_t wire, double previous) {
      if (payload == 0)
        return previous;
      const double observed = std::max(1.0, double(wire) / payload);
      // Fast overhead increase; slow decrease avoids overshoot after a burst.
      return std::max(observed, previous * 0.8 + observed * 0.2);
    };
    const uint64_t packets = sample.audio_packets - cost_sample_->audio_packets;
    const uint64_t primary =
        sample.audio_payload_bytes - cost_sample_->audio_payload_bytes;
    const uint64_t redundant = sample.audio_redundant_payload_bytes -
                               cost_sample_->audio_redundant_payload_bytes;
    const uint64_t wire =
        sample.audio_wire_bytes - cost_sample_->audio_wire_bytes;
    // A short burst is not a representative packetization window. Count RED
    // blocks, not redundant/primary byte ratios (speech -> silence can make
    // the latter arbitrarily large). Empty windows cannot inflate costs.
    if (packets >= 10 && primary > 0 && wire >= primary + redundant) {
      auto smooth = [](double old, double observed) {
        return std::max(observed, old * 0.8 + observed * 0.2);
      };
      audio_redundancy_multiplier_ =
          smooth(audio_redundancy_multiplier_,
                 1.0 + double(sample.audio_redundant_blocks -
                              cost_sample_->audio_redundant_blocks) /
                           packets);
      audio_header_bytes_per_packet_ =
          smooth(audio_header_bytes_per_packet_,
                 double(wire - primary - redundant) / packets);
      audio_packet_rate_ = smooth(
          audio_packet_rate_,
          packets / (sample.at_time - cost_sample_->at_time).seconds<double>());
    }
    video_wire_ratio_ =
        ratio(sample.video_payload_bytes - cost_sample_->video_payload_bytes,
              sample.video_wire_bytes - cost_sample_->video_wire_bytes,
              video_wire_ratio_);
    rtcp_rate_ = DataRate::BitsPerSec(std::max<int64_t>(
        4000, (sample.rtcp_wire_bytes - cost_sample_->rtcp_wire_bytes) * 8 /
                  (sample.at_time - cost_sample_->at_time).seconds<double>()));
    cost_sample_ = sample;
  }
  // This reserve bounds RTX queue service, not burst admission or eligibility.
  const DataRate recovery_reserve = raw_capacity * 0.10;
  const DataRate control = sample.maximum_media_rate > DataRate::Zero()
                               ? rtcp_rate_
                               : DataRate::Zero();
  const DataRate primary_wire = DataRate::BitsPerSec(std::max<int64_t>(
      0, raw_capacity.bps() - recovery_reserve.bps() - control.bps()));
  // Headers are a packet-rate cost, independent of configured codec bytes.
  // A configured cadence floor prevents DTX from underpricing resumed speech.
  const DataRate audio_headers =
      sample.audio_payload_rate.IsZero()
          ? DataRate::Zero()
          : DataRate::BitsPerSec(
                audio_header_bytes_per_packet_ * 8 *
                std::max<double>(sample.audio_packet_rate, audio_packet_rate_));
  const double after_audio_headers =
      std::max<int64_t>(0, primary_wire.bps() - audio_headers.bps());
  const double audio_bps =
      std::min<double>(sample.audio_payload_rate.bps(),
                       after_audio_headers / audio_redundancy_multiplier_);
  const double video_bps =
      std::max(0.0,
               after_audio_headers - audio_bps * audio_redundancy_multiplier_) /
      video_wire_ratio_;
  const DataRate usable_media_rate =
      DataRate::BitsPerSec(audio_bps + video_bps);
  const DataRate overhead = primary_wire - usable_media_rate + control;
  const DataRate safety_headroom = DataRate::Zero();
  // Demand, not the constrained BBR estimate, bounds probe supply. quiche
  // authorizes phases, pacing, cwnd and debits media against this SAME cap.
  const double max_audio_bps = sample.audio_payload_rate.bps();
  const double max_video_bps = std::max<int64_t>(
      0, sample.maximum_media_rate.bps() - sample.audio_payload_rate.bps());
  const DataRate probe_max_rate =
      sample.maximum_media_rate.IsZero()
          ? DataRate::Zero()
          : DataRate::BitsPerSec((max_audio_bps * audio_redundancy_multiplier_ +
                                  audio_headers.bps() +
                                  max_video_bps * video_wire_ratio_ +
                                  control.bps()) /
                                 0.90);
  // Maintain the configured demand, not an unconstrained historical max_bw.
  // A resume-minimum-only ceiling leaves sampled delivery too close to the
  // native toggle threshold under random loss. The configured maximum is a
  // supply ceiling, never permission to bypass BBR pacing/cwnd. Real media
  // debits the same credit; only its unused portion can become padding.
  const DataRate resume_demand = std::min(sample.resume_media_rate,
                                         sample.maximum_media_rate);
  const double resume_video_bps = std::max<int64_t>(
      0, resume_demand.bps() - sample.audio_payload_rate.bps());
  const DataRate maintenance_rate =
      fresh && resume_video_bps > 0 ? probe_max_rate : DataRate::Zero();
  const DataRate resume_wire_rate =
      DataRate::BitsPerSec((max_audio_bps * audio_redundancy_multiplier_ +
          audio_headers.bps() + resume_video_bps * video_wire_ratio_ +
          control.bps()) / 0.90);
  if (resume_demand != last_resume_demand_ ||
      sample.evidence_round < last_evidence_round_) {
    resume_after_round_ = sample.evidence_round;
    last_resume_demand_ = resume_demand;
  }
  last_evidence_round_ = sample.evidence_round;
  if (!fresh || usable_media_rate < resume_demand)
    resume_after_round_ = sample.evidence_round;
  bool resume_fast_path = false;
  if (fresh) {
    int64_t filtered = 0;
    if (usable_media_rate <= target_rate_) {
      filtered = usable_media_rate.bps();
    } else {
      filtered = (target_rate_.bps() * 8 + usable_media_rate.bps() * 2) / 10;
    }
    target_rate_ =
        DataRate::BitsPerSec(std::min(filtered, maximum_rate_.bps()));
  }
  if (!capacity_known_) {
    const bool covered =
        fresh && has_demand && usable_media_rate >= sample.maximum_media_rate;
    if (!covered)
      demand_satisfied_at_.reset();
    else if (!demand_satisfied_at_)
      demand_satisfied_at_ = sample.at_time;
    const Timestamp ack_origin =
        std::max(last_ack_time_.value_or(Timestamp::MinusInfinity()),
                 demand_started_at_.value_or(sample.at_time));
    if (sample.oldest_video_queue_age >= TimeDelta::Millis(500) ||
        sample.expired_video_frames > 0) {
      capacity_state_ = "queue_overload";
      capacity_known_ = true;
    } else if (has_demand &&
               sample.at_time - ack_origin > TimeDelta::Seconds(1)) {
      capacity_state_ = "no_ack_progress";
      capacity_known_ = true;
    } else if (valid && sample.startup_complete) {
      capacity_state_ = "bbr_startup_complete";
      capacity_known_ = true;
    } else if (covered && ack_advanced &&
               sample.at_time - *demand_satisfied_at_ >=
                   std::max(sample.round_trip_time * 2,
                            TimeDelta::Millis(100))) {
      capacity_state_ = "demand_satisfied";
      capacity_known_ = true;
      // Do not pause demand we just proved fits using an EWMA-low estimate.
      target_rate_ = std::min(usable_media_rate, maximum_rate_);
    }
  }
  // Two fully new delivery-sampled rounds since the last insufficient budget
  // are required. REFILL clearing bw_lo, repeated polling, ACK-only progress,
  // and stale high max_bw cannot satisfy this evidence contract.
  if (capacity_known_ && fresh && resume_video_bps > 0 &&
      target_rate_ < resume_demand && usable_media_rate >= resume_demand &&
      resume_after_round_ && sample.evidence_round >= *resume_after_round_ &&
      sample.evidence_round - *resume_after_round_ >= 2 &&
      sample.evidence_age <= std::max(sample.round_trip_time * 2,
                                      TimeDelta::Millis(100)) &&
      sample.evidence_age <= TimeDelta::Seconds(1) &&
      sample.evidence_rate >= resume_wire_rate && ack_advanced) {
    target_rate_ = std::min(resume_demand, maximum_rate_);
    resume_fast_path = true;
  }
  if (capacity_known_ && !fresh)
    target_rate_ = DataRate::Zero();
  // ACK timeout must not clear the evidence barrier using the pre-timeout
  // target, then zero the target afterwards and lose the fast-resume arm.
  if (target_rate_ >= resume_demand)
    resume_after_round_.reset();
  const DataRate applied =
      capacity_known_ ? target_rate_ : std::min(startup_rate_, maximum_rate_);
  return {.fresh = fresh,
          .capacity_known = capacity_known_,
          .capacity_state = capacity_state_,
          .audio_redundancy_multiplier = audio_redundancy_multiplier_,
          .audio_header_rate = audio_headers,
          .raw_capacity = raw_capacity,
          .overhead = overhead,
          .recovery_reserve = recovery_reserve,
          .safety_headroom = safety_headroom,
          .probe_max_rate = probe_max_rate,
          .maintenance_rate = maintenance_rate,
          .resume_fast_path = resume_fast_path,
          .usable_media_rate = usable_media_rate,
          .target_rate = applied,
          .round_trip_time = sample.round_trip_time};
}

}  // namespace webrtc::roq
