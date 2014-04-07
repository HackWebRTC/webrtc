/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/modules/bitrate_controller/send_side_bandwidth_estimation.h"

#include <cmath>

#include "webrtc/system_wrappers/interface/logging.h"

namespace webrtc {
namespace {
enum { kBweIncreaseIntervalMs = 1000 };
enum { kBweDecreaseIntervalMs = 300 };
enum { kLimitNumPackets = 20 };
enum { kAvgPacketSizeBytes = 1000 };

// Calculate the rate that TCP-Friendly Rate Control (TFRC) would apply.
// The formula in RFC 3448, Section 3.1, is used.
uint32_t CalcTfrcBps(uint16_t rtt, uint8_t loss) {
  if (rtt == 0 || loss == 0) {
    // Input variables out of range.
    return 0;
  }
  double R = static_cast<double>(rtt) / 1000;  // RTT in seconds.
  int b = 1;  // Number of packets acknowledged by a single TCP acknowledgement:
              // recommended = 1.
  double t_RTO = 4.0 * R;  // TCP retransmission timeout value in seconds
                           // recommended = 4*R.
  double p = static_cast<double>(loss) / 255;  // Packet loss rate in [0, 1).
  double s = static_cast<double>(kAvgPacketSizeBytes);

  // Calculate send rate in bytes/second.
  double X =
      s / (R * std::sqrt(2 * b * p / 3) +
           (t_RTO * (3 * std::sqrt(3 * b * p / 8) * p * (1 + 32 * p * p))));

  // Convert to bits/second.
  return (static_cast<uint32_t>(X * 8));
}
}

SendSideBandwidthEstimation::SendSideBandwidthEstimation()
    : accumulate_lost_packets_Q8_(0),
      accumulate_expected_packets_(0),
      bitrate_(0),
      min_bitrate_configured_(0),
      max_bitrate_configured_(0),
      time_last_receiver_block_ms_(0),
      last_fraction_loss_(0),
      last_round_trip_time_ms_(0),
      bwe_incoming_(0),
      time_last_decrease_ms_(0) {}

SendSideBandwidthEstimation::~SendSideBandwidthEstimation() {}

void SendSideBandwidthEstimation::SetSendBitrate(uint32_t bitrate) {
  bitrate_ = bitrate;

  // Clear last sent bitrate history so the new value can be used directly
  // and not capped.
  min_bitrate_history_.clear();
}

void SendSideBandwidthEstimation::SetMinMaxBitrate(uint32_t min_bitrate,
                                                   uint32_t max_bitrate) {
  min_bitrate_configured_ = min_bitrate;
  max_bitrate_configured_ = max_bitrate;
}

void SendSideBandwidthEstimation::SetMinBitrate(uint32_t min_bitrate) {
  min_bitrate_configured_ = min_bitrate;
}

void SendSideBandwidthEstimation::CurrentEstimate(uint32_t* bitrate,
                                                  uint8_t* loss,
                                                  uint32_t* rtt) const {
  *bitrate = bitrate_;
  *loss = last_fraction_loss_;
  *rtt = last_round_trip_time_ms_;
}

void SendSideBandwidthEstimation::UpdateReceiverEstimate(uint32_t bandwidth) {
  bwe_incoming_ = bandwidth;
  CapBitrateToThresholds();
}

void SendSideBandwidthEstimation::UpdateReceiverBlock(uint8_t fraction_loss,
                                                      uint32_t rtt,
                                                      int number_of_packets,
                                                      uint32_t now_ms) {
  // Update RTT.
  last_round_trip_time_ms_ = rtt;

  // Check sequence number diff and weight loss report
  if (number_of_packets > 0) {
    // Calculate number of lost packets.
    const int num_lost_packets_Q8 = fraction_loss * number_of_packets;
    // Accumulate reports.
    accumulate_lost_packets_Q8_ += num_lost_packets_Q8;
    accumulate_expected_packets_ += number_of_packets;

    // Report loss if the total report is based on sufficiently many packets.
    if (accumulate_expected_packets_ >= kLimitNumPackets) {
      last_fraction_loss_ =
          accumulate_lost_packets_Q8_ / accumulate_expected_packets_;

      // Reset accumulators.
      accumulate_lost_packets_Q8_ = 0;
      accumulate_expected_packets_ = 0;
    } else {
      // Early return without updating estimate.
      return;
    }
  }
  time_last_receiver_block_ms_ = now_ms;
  UpdateEstimate(now_ms);
}

void SendSideBandwidthEstimation::UpdateEstimate(uint32_t now_ms) {
  UpdateMinHistory(now_ms);

  // Only start updating bitrate when receiving receiver blocks.
  if (time_last_receiver_block_ms_ != 0) {
    if (last_fraction_loss_ <= 5) {
      // Loss < 2%: Increase rate by 8% of the min bitrate in the last
      // kBweIncreaseIntervalMs.
      // Note that by remembering the bitrate over the last second one can
      // rampup up one second faster than if only allowed to start ramping
      // at 8% per second rate now. E.g.:
      //   If sending a constant 100kbps it can rampup immediatly to 108kbps
      //   whenever a receiver report is received with lower packet loss.
      //   If instead one would do: bitrate_ *= 1.08^(delta time), it would
      //   take over one second since the lower packet loss to achieve 108kbps.
      bitrate_ = static_cast<uint32_t>(
          min_bitrate_history_.front().second * 1.08 + 0.5);

      // Add 1 kbps extra, just to make sure that we do not get stuck
      // (gives a little extra increase at low rates, negligible at higher
      // rates).
      bitrate_ += 1000;

    } else if (last_fraction_loss_ <= 26) {
      // Loss between 2% - 10%: Do nothing.

    } else {
      // Loss > 10%: Limit the rate decreases to once a kBweDecreaseIntervalMs +
      // rtt.
      if ((now_ms - time_last_decrease_ms_) >=
          static_cast<uint32_t>(kBweDecreaseIntervalMs +
                                last_round_trip_time_ms_)) {
        time_last_decrease_ms_ = now_ms;

        // Reduce rate:
        //   newRate = rate * (1 - 0.5*lossRate);
        //   where packetLoss = 256*lossRate;
        bitrate_ = static_cast<uint32_t>(
            (bitrate_ * static_cast<double>(512 - last_fraction_loss_)) /
            512.0);

        // Calculate what rate TFRC would apply in this situation and to not
        // reduce further than it.
        bitrate_ = std::max(
            bitrate_,
            CalcTfrcBps(last_round_trip_time_ms_, last_fraction_loss_));
      }
    }
  }
  CapBitrateToThresholds();
}

void SendSideBandwidthEstimation::UpdateMinHistory(uint32_t now_ms) {
  // Remove old data points from history.
  // Since history precision is in ms, add one so it is able to increase
  // bitrate if it is off by as little as 0.5ms.
  while (!min_bitrate_history_.empty() &&
         now_ms - min_bitrate_history_.front().first + 1 >
             kBweIncreaseIntervalMs) {
    min_bitrate_history_.pop_front();
  }

  // Typical minimum sliding-window algorithm: Pop values higher than current
  // bitrate before pushing it.
  while (!min_bitrate_history_.empty() &&
         bitrate_ <= min_bitrate_history_.back().second) {
    min_bitrate_history_.pop_back();
  }

  min_bitrate_history_.push_back(std::make_pair(now_ms, bitrate_));
}

void SendSideBandwidthEstimation::CapBitrateToThresholds() {
  if (bwe_incoming_ > 0 && bitrate_ > bwe_incoming_) {
    bitrate_ = bwe_incoming_;
  }
  if (bitrate_ > max_bitrate_configured_) {
    bitrate_ = max_bitrate_configured_;
  }
  if (bitrate_ < min_bitrate_configured_) {
    LOG(LS_WARNING) << "Estimated available bandwidth " << bitrate_ / 1000
                    << " kbps is below configured min bitrate "
                    << min_bitrate_configured_ / 1000 << " kbps.";
    bitrate_ = min_bitrate_configured_;
  }
}

}  // namespace webrtc
