/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/audio_coding/neteq/delay_manager.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <algorithm>
#include <numeric>
#include <string>

#include "absl/memory/memory.h"
#include "modules/audio_coding/neteq/delay_peak_detector.h"
#include "modules/audio_coding/neteq/histogram.h"
#include "modules/audio_coding/neteq/statistics_calculator.h"
#include "modules/include/module_common_types_public.h"
#include "rtc_base/checks.h"
#include "rtc_base/logging.h"
#include "rtc_base/numerics/safe_conversions.h"
#include "rtc_base/numerics/safe_minmax.h"
#include "system_wrappers/include/field_trial.h"

namespace {

constexpr int kLimitProbability = 1020054733;           // 19/20 in Q30.
constexpr int kLimitProbabilityStreaming = 1073204953;  // 1999/2000 in Q30.
constexpr int kMaxStreamingPeakPeriodMs = 600000;       // 10 minutes in ms.
constexpr int kCumulativeSumDrift = 2;  // Drift term for cumulative sum
                                        // |iat_cumulative_sum_|.
constexpr int kMinBaseMinimumDelayMs = 0;
constexpr int kMaxBaseMinimumDelayMs = 10000;
constexpr int kIatFactor = 32745;  // 0.9993 in Q15.
constexpr int kMaxIat = 64;        // Max inter-arrival time to register.
constexpr int kMaxReorderedPackets =
    10;  // Max number of consecutive reordered packets.
constexpr int kMaxHistoryPackets =
    100;  // Max number of packets used to calculate relative packet arrival
          // delay.
constexpr int kDelayBuckets = 100;
constexpr int kBucketSizeMs = 20;

int PercentileToQuantile(double percentile) {
  return static_cast<int>((1 << 30) * percentile / 100.0 + 0.5);
}

absl::optional<int> GetForcedLimitProbability() {
  constexpr char kForceTargetDelayPercentileFieldTrial[] =
      "WebRTC-Audio-NetEqForceTargetDelayPercentile";
  const bool use_forced_target_delay_percentile =
      webrtc::field_trial::IsEnabled(kForceTargetDelayPercentileFieldTrial);
  if (use_forced_target_delay_percentile) {
    const std::string field_trial_string = webrtc::field_trial::FindFullName(
        kForceTargetDelayPercentileFieldTrial);
    double percentile = -1.0;
    if (sscanf(field_trial_string.c_str(), "Enabled-%lf", &percentile) == 1 &&
        percentile >= 0.0 && percentile <= 100.0) {
      return absl::make_optional<int>(
          PercentileToQuantile(percentile));  // in Q30.
    } else {
      RTC_LOG(LS_WARNING) << "Invalid parameter for "
                          << kForceTargetDelayPercentileFieldTrial
                          << ", ignored.";
    }
  }
  return absl::nullopt;
}

struct DelayHistogramConfig {
  int quantile = 1020054733;  // 0.95 in Q30.
  int forget_factor = 32745;  // 0.9993 in Q15.
};

absl::optional<DelayHistogramConfig> GetDelayHistogramConfig() {
  constexpr char kDelayHistogramFieldTrial[] =
      "WebRTC-Audio-NetEqDelayHistogram";
  const bool use_new_delay_manager =
      webrtc::field_trial::IsEnabled(kDelayHistogramFieldTrial);
  if (use_new_delay_manager) {
    const auto field_trial_string =
        webrtc::field_trial::FindFullName(kDelayHistogramFieldTrial);
    DelayHistogramConfig config;
    double percentile = -1.0;
    double forget_factor = -1.0;
    if (sscanf(field_trial_string.c_str(), "Enabled-%lf-%lf", &percentile,
               &forget_factor) == 2 &&
        percentile >= 0.0 && percentile <= 100.0 && forget_factor >= 0.0 &&
        forget_factor <= 1.0) {
      config.quantile = PercentileToQuantile(percentile);
      config.forget_factor = (1 << 15) * forget_factor;
    }
    RTC_LOG(LS_INFO) << "Delay histogram config:"
                     << " quantile=" << config.quantile
                     << " forget_factor=" << config.forget_factor;
    return absl::make_optional(config);
  }
  return absl::nullopt;
}

}  // namespace

namespace webrtc {

DelayManager::DelayManager(size_t max_packets_in_buffer,
                           int base_minimum_delay_ms,
                           int histogram_quantile,
                           HistogramMode histogram_mode,
                           bool enable_rtx_handling,
                           DelayPeakDetector* peak_detector,
                           const TickTimer* tick_timer,
                           StatisticsCalculator* statistics,
                           std::unique_ptr<Histogram> histogram)
    : first_packet_received_(false),
      max_packets_in_buffer_(max_packets_in_buffer),
      histogram_(std::move(histogram)),
      histogram_quantile_(histogram_quantile),
      histogram_mode_(histogram_mode),
      tick_timer_(tick_timer),
      statistics_(statistics),
      base_minimum_delay_ms_(base_minimum_delay_ms),
      effective_minimum_delay_ms_(base_minimum_delay_ms),
      base_target_level_(4),                   // In Q0 domain.
      target_level_(base_target_level_ << 8),  // In Q8 domain.
      packet_len_ms_(0),
      streaming_mode_(false),
      last_seq_no_(0),
      last_timestamp_(0),
      minimum_delay_ms_(0),
      maximum_delay_ms_(0),
      iat_cumulative_sum_(0),
      max_iat_cumulative_sum_(0),
      peak_detector_(*peak_detector),
      last_pack_cng_or_dtmf_(1),
      frame_length_change_experiment_(
          field_trial::IsEnabled("WebRTC-Audio-NetEqFramelengthExperiment")),
      enable_rtx_handling_(enable_rtx_handling) {
  assert(peak_detector);  // Should never be NULL.
  RTC_CHECK(histogram_);
  RTC_DCHECK_GE(base_minimum_delay_ms_, 0);

  Reset();
}

std::unique_ptr<DelayManager> DelayManager::Create(
    size_t max_packets_in_buffer,
    int base_minimum_delay_ms,
    bool enable_rtx_handling,
    DelayPeakDetector* peak_detector,
    const TickTimer* tick_timer,
    StatisticsCalculator* statistics) {
  int quantile;
  std::unique_ptr<Histogram> histogram;
  HistogramMode mode;
  auto delay_histogram_config = GetDelayHistogramConfig();
  if (delay_histogram_config) {
    DelayHistogramConfig config = delay_histogram_config.value();
    quantile = config.quantile;
    histogram =
        absl::make_unique<Histogram>(kDelayBuckets, config.forget_factor);
    mode = RELATIVE_ARRIVAL_DELAY;
  } else {
    quantile = GetForcedLimitProbability().value_or(kLimitProbability);
    histogram = absl::make_unique<Histogram>(kMaxIat + 1, kIatFactor);
    mode = INTER_ARRIVAL_TIME;
  }
  return absl::make_unique<DelayManager>(
      max_packets_in_buffer, base_minimum_delay_ms, quantile, mode,
      enable_rtx_handling, peak_detector, tick_timer, statistics,
      std::move(histogram));
}

DelayManager::~DelayManager() {}

int DelayManager::Update(uint16_t sequence_number,
                         uint32_t timestamp,
                         int sample_rate_hz) {
  if (sample_rate_hz <= 0) {
    return -1;
  }

  if (!first_packet_received_) {
    // Prepare for next packet arrival.
    packet_iat_stopwatch_ = tick_timer_->GetNewStopwatch();
    last_seq_no_ = sequence_number;
    last_timestamp_ = timestamp;
    first_packet_received_ = true;
    return 0;
  }

  // Try calculating packet length from current and previous timestamps.
  int packet_len_ms;
  if (!IsNewerTimestamp(timestamp, last_timestamp_) ||
      !IsNewerSequenceNumber(sequence_number, last_seq_no_)) {
    // Wrong timestamp or sequence order; use stored value.
    packet_len_ms = packet_len_ms_;
  } else {
    // Calculate timestamps per packet and derive packet length in ms.
    int64_t packet_len_samp =
        static_cast<uint32_t>(timestamp - last_timestamp_) /
        static_cast<uint16_t>(sequence_number - last_seq_no_);
    packet_len_ms =
        rtc::saturated_cast<int>(1000 * packet_len_samp / sample_rate_hz);
  }

  bool reordered = false;
  if (packet_len_ms > 0) {
    // Cannot update statistics unless |packet_len_ms| is valid.
    if (streaming_mode_) {
      UpdateCumulativeSums(packet_len_ms, sequence_number);
    }

    // Inter-arrival time (IAT) in integer "packet times" (rounding down). This
    // is the value added to the inter-arrival time histogram.
    int iat_ms = packet_iat_stopwatch_->ElapsedMs();
    int iat_packets = iat_ms / packet_len_ms;
    // Check for discontinuous packet sequence and re-ordering.
    if (IsNewerSequenceNumber(sequence_number, last_seq_no_ + 1)) {
      // Compensate for gap in the sequence numbers. Reduce IAT with the
      // expected extra time due to lost packets.
      int packet_offset =
          static_cast<uint16_t>(sequence_number - last_seq_no_ - 1);
      iat_packets -= packet_offset;
      iat_ms -= packet_offset * packet_len_ms;
    } else if (!IsNewerSequenceNumber(sequence_number, last_seq_no_)) {
      int packet_offset =
          static_cast<uint16_t>(last_seq_no_ + 1 - sequence_number);
      iat_packets += packet_offset;
      iat_ms += packet_offset * packet_len_ms;
      reordered = true;
    }

    int iat_delay = iat_ms - packet_len_ms;
    int relative_delay;
    if (reordered) {
      relative_delay = std::max(iat_delay, 0);
    } else {
      UpdateDelayHistory(iat_delay);
      relative_delay = CalculateRelativePacketArrivalDelay();
    }
    statistics_->RelativePacketArrivalDelay(relative_delay);

    switch (histogram_mode_) {
      case RELATIVE_ARRIVAL_DELAY: {
        const int index = relative_delay / kBucketSizeMs;
        if (index < histogram_->NumBuckets()) {
          // Maximum delay to register is 2000 ms.
          histogram_->Add(index);
        }
        break;
      }
      case INTER_ARRIVAL_TIME: {
        // Saturate IAT between 0 and maximum value.
        iat_packets =
            std::max(std::min(iat_packets, histogram_->NumBuckets() - 1), 0);
        histogram_->Add(iat_packets);
        break;
      }
    }
    // Calculate new |target_level_| based on updated statistics.
    target_level_ = CalculateTargetLevel(iat_packets, reordered);
    if (streaming_mode_) {
      target_level_ = std::max(target_level_, max_iat_cumulative_sum_);
    }

    LimitTargetLevel();
  }  // End if (packet_len_ms > 0).

  if (enable_rtx_handling_ && reordered &&
      num_reordered_packets_ < kMaxReorderedPackets) {
    ++num_reordered_packets_;
    return 0;
  }
  num_reordered_packets_ = 0;
  // Prepare for next packet arrival.
  packet_iat_stopwatch_ = tick_timer_->GetNewStopwatch();
  last_seq_no_ = sequence_number;
  last_timestamp_ = timestamp;
  return 0;
}

void DelayManager::UpdateDelayHistory(int iat_delay) {
  delay_history_.push_back(iat_delay);
  if (delay_history_.size() > kMaxHistoryPackets) {
    delay_history_.pop_front();
  }
}

int DelayManager::CalculateRelativePacketArrivalDelay() const {
  // This effectively calculates arrival delay of a packet relative to the
  // packet preceding the history window. If the arrival delay ever becomes
  // smaller than zero, it means the reference packet is invalid, and we
  // move the reference.
  int relative_delay = 0;
  for (int delay : delay_history_) {
    relative_delay += delay;
    relative_delay = std::max(relative_delay, 0);
  }
  return relative_delay;
}

void DelayManager::UpdateCumulativeSums(int packet_len_ms,
                                        uint16_t sequence_number) {
  // Calculate IAT in Q8, including fractions of a packet (i.e., more
  // accurate than |iat_packets|.
  int iat_packets_q8 =
      (packet_iat_stopwatch_->ElapsedMs() << 8) / packet_len_ms;
  // Calculate cumulative sum IAT with sequence number compensation. The sum
  // is zero if there is no clock-drift.
  iat_cumulative_sum_ +=
      (iat_packets_q8 -
       (static_cast<int>(sequence_number - last_seq_no_) << 8));
  // Subtract drift term.
  iat_cumulative_sum_ -= kCumulativeSumDrift;
  // Ensure not negative.
  iat_cumulative_sum_ = std::max(iat_cumulative_sum_, 0);
  if (iat_cumulative_sum_ > max_iat_cumulative_sum_) {
    // Found a new maximum.
    max_iat_cumulative_sum_ = iat_cumulative_sum_;
    max_iat_stopwatch_ = tick_timer_->GetNewStopwatch();
  }
  if (max_iat_stopwatch_->ElapsedMs() > kMaxStreamingPeakPeriodMs) {
    // Too long since the last maximum was observed; decrease max value.
    max_iat_cumulative_sum_ -= kCumulativeSumDrift;
  }
}

// Enforces upper and lower limits for |target_level_|. The upper limit is
// chosen to be minimum of i) 75% of |max_packets_in_buffer_|, to leave some
// headroom for natural fluctuations around the target, and ii) equivalent of
// |maximum_delay_ms_| in packets. Note that in practice, if no
// |maximum_delay_ms_| is specified, this does not have any impact, since the
// target level is far below the buffer capacity in all reasonable cases.
// The lower limit is equivalent of |effective_minimum_delay_ms_| in packets.
// We update |least_required_level_| while the above limits are applied.
// TODO(hlundin): Move this check to the buffer logistics class.
void DelayManager::LimitTargetLevel() {
  if (packet_len_ms_ > 0 && effective_minimum_delay_ms_ > 0) {
    int minimum_delay_packet_q8 =
        (effective_minimum_delay_ms_ << 8) / packet_len_ms_;
    target_level_ = std::max(target_level_, minimum_delay_packet_q8);
  }

  if (maximum_delay_ms_ > 0 && packet_len_ms_ > 0) {
    int maximum_delay_packet_q8 = (maximum_delay_ms_ << 8) / packet_len_ms_;
    target_level_ = std::min(target_level_, maximum_delay_packet_q8);
  }

  // Shift to Q8, then 75%.;
  int max_buffer_packets_q8 =
      static_cast<int>((3 * (max_packets_in_buffer_ << 8)) / 4);
  target_level_ = std::min(target_level_, max_buffer_packets_q8);

  // Sanity check, at least 1 packet (in Q8).
  target_level_ = std::max(target_level_, 1 << 8);
}

int DelayManager::CalculateTargetLevel(int iat_packets, bool reordered) {
  int limit_probability = histogram_quantile_;
  if (streaming_mode_) {
    limit_probability = kLimitProbabilityStreaming;
  }

  int bucket_index = histogram_->Quantile(limit_probability);
  int target_level;
  switch (histogram_mode_) {
    case RELATIVE_ARRIVAL_DELAY: {
      target_level = 1 + bucket_index * kBucketSizeMs / packet_len_ms_;
      base_target_level_ = target_level;
      break;
    }
    case INTER_ARRIVAL_TIME: {
      target_level = bucket_index;
      base_target_level_ = target_level;
      // Update detector for delay peaks.
      bool delay_peak_found =
          peak_detector_.Update(iat_packets, reordered, target_level);
      if (delay_peak_found) {
        target_level = std::max(target_level, peak_detector_.MaxPeakHeight());
      }
      break;
    }
  }

  // Sanity check. |target_level| must be strictly positive.
  target_level = std::max(target_level, 1);
  // Scale to Q8 and assign to member variable.
  target_level_ = target_level << 8;
  return target_level_;
}

int DelayManager::SetPacketAudioLength(int length_ms) {
  if (length_ms <= 0) {
    RTC_LOG_F(LS_ERROR) << "length_ms = " << length_ms;
    return -1;
  }
  if (histogram_mode_ == INTER_ARRIVAL_TIME &&
      frame_length_change_experiment_ && packet_len_ms_ != length_ms &&
      packet_len_ms_ > 0) {
    histogram_->Scale(packet_len_ms_, length_ms);
  }

  packet_len_ms_ = length_ms;
  peak_detector_.SetPacketAudioLength(packet_len_ms_);
  packet_iat_stopwatch_ = tick_timer_->GetNewStopwatch();
  last_pack_cng_or_dtmf_ = 1;  // TODO(hlundin): Legacy. Remove?
  return 0;
}

void DelayManager::Reset() {
  packet_len_ms_ = 0;  // Packet size unknown.
  streaming_mode_ = false;
  peak_detector_.Reset();
  histogram_->Reset();
  base_target_level_ = 4;
  target_level_ = base_target_level_ << 8;
  packet_iat_stopwatch_ = tick_timer_->GetNewStopwatch();
  max_iat_stopwatch_ = tick_timer_->GetNewStopwatch();
  iat_cumulative_sum_ = 0;
  max_iat_cumulative_sum_ = 0;
  last_pack_cng_or_dtmf_ = 1;
}

double DelayManager::EstimatedClockDriftPpm() const {
  double sum = 0.0;
  // Calculate the expected value based on the probabilities in
  // |histogram_|.
  auto buckets = histogram_->buckets();
  for (size_t i = 0; i < buckets.size(); ++i) {
    sum += static_cast<double>(buckets[i]) * i;
  }
  // The probabilities in |histogram_| are in Q30. Divide by 1 << 30 to
  // convert to Q0; subtract the nominal inter-arrival time (1) to make a zero
  // clockdrift represent as 0; mulitply by 1000000 to produce parts-per-million
  // (ppm).
  return (sum / (1 << 30) - 1) * 1e6;
}

bool DelayManager::PeakFound() const {
  return peak_detector_.peak_found();
}

void DelayManager::ResetPacketIatCount() {
  packet_iat_stopwatch_ = tick_timer_->GetNewStopwatch();
}

// Note that |low_limit| and |higher_limit| are not assigned to
// |minimum_delay_ms_| and |maximum_delay_ms_| defined by the client of this
// class. They are computed from |target_level_| and used for decision making.
void DelayManager::BufferLimits(int* lower_limit, int* higher_limit) const {
  if (!lower_limit || !higher_limit) {
    RTC_LOG_F(LS_ERROR) << "NULL pointers supplied as input";
    assert(false);
    return;
  }

  int window_20ms = 0x7FFF;  // Default large value for legacy bit-exactness.
  if (packet_len_ms_ > 0) {
    window_20ms = (20 << 8) / packet_len_ms_;
  }

  // |target_level_| is in Q8 already.
  *lower_limit = (target_level_ * 3) / 4;
  // |higher_limit| is equal to |target_level_|, but should at
  // least be 20 ms higher than |lower_limit_|.
  *higher_limit = std::max(target_level_, *lower_limit + window_20ms);
}

int DelayManager::TargetLevel() const {
  return target_level_;
}

void DelayManager::LastDecodedWasCngOrDtmf(bool it_was) {
  if (it_was) {
    last_pack_cng_or_dtmf_ = 1;
  } else if (last_pack_cng_or_dtmf_ != 0) {
    last_pack_cng_or_dtmf_ = -1;
  }
}

void DelayManager::RegisterEmptyPacket() {
  ++last_seq_no_;
}

bool DelayManager::IsValidMinimumDelay(int delay_ms) const {
  return 0 <= delay_ms && delay_ms <= MinimumDelayUpperBound();
}

bool DelayManager::IsValidBaseMinimumDelay(int delay_ms) const {
  return kMinBaseMinimumDelayMs <= delay_ms &&
         delay_ms <= kMaxBaseMinimumDelayMs;
}

bool DelayManager::SetMinimumDelay(int delay_ms) {
  if (!IsValidMinimumDelay(delay_ms)) {
    return false;
  }

  minimum_delay_ms_ = delay_ms;
  UpdateEffectiveMinimumDelay();
  return true;
}

bool DelayManager::SetMaximumDelay(int delay_ms) {
  // If |delay_ms| is zero then it unsets the maximum delay and target level is
  // unconstrained by maximum delay.
  if (delay_ms != 0 &&
      (delay_ms < minimum_delay_ms_ || delay_ms < packet_len_ms_)) {
    // Maximum delay shouldn't be less than minimum delay or less than a packet.
    return false;
  }

  maximum_delay_ms_ = delay_ms;
  UpdateEffectiveMinimumDelay();
  return true;
}

bool DelayManager::SetBaseMinimumDelay(int delay_ms) {
  if (!IsValidBaseMinimumDelay(delay_ms)) {
    return false;
  }

  base_minimum_delay_ms_ = delay_ms;
  UpdateEffectiveMinimumDelay();
  return true;
}

int DelayManager::GetBaseMinimumDelay() const {
  return base_minimum_delay_ms_;
}

int DelayManager::base_target_level() const {
  return base_target_level_;
}
void DelayManager::set_streaming_mode(bool value) {
  streaming_mode_ = value;
}
int DelayManager::last_pack_cng_or_dtmf() const {
  return last_pack_cng_or_dtmf_;
}

void DelayManager::set_last_pack_cng_or_dtmf(int value) {
  last_pack_cng_or_dtmf_ = value;
}

void DelayManager::UpdateEffectiveMinimumDelay() {
  // Clamp |base_minimum_delay_ms_| into the range which can be effectively
  // used.
  const int base_minimum_delay_ms =
      rtc::SafeClamp(base_minimum_delay_ms_, 0, MinimumDelayUpperBound());
  effective_minimum_delay_ms_ =
      std::max(minimum_delay_ms_, base_minimum_delay_ms);
}

int DelayManager::MinimumDelayUpperBound() const {
  // Choose the lowest possible bound discarding 0 cases which mean the value
  // is not set and unconstrained.
  int q75 = MaxBufferTimeQ75();
  q75 = q75 > 0 ? q75 : kMaxBaseMinimumDelayMs;
  const int maximum_delay_ms =
      maximum_delay_ms_ > 0 ? maximum_delay_ms_ : kMaxBaseMinimumDelayMs;
  return std::min(maximum_delay_ms, q75);
}

int DelayManager::MaxBufferTimeQ75() const {
  const int max_buffer_time = max_packets_in_buffer_ * packet_len_ms_;
  return rtc::dchecked_cast<int>(3 * max_buffer_time / 4);
}
}  // namespace webrtc
