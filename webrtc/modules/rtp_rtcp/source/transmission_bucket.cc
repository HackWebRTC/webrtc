/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "transmission_bucket.h"

#include <assert.h>
#include "critical_section_wrapper.h"
#include "rtp_utility.h"

namespace webrtc {
namespace {
// Factor that is applied to the target bitrate to calculate the number of
// bytes that can be transmitted per interval.
// Increasing this factor will result in lower delays in cases of bitrate
// overshoots.
const float kBytesPerIntervalMargin = 1.5f;

// Time limit in ms between packets within a frame.
// A packet will be transmitted if the elapsed time since the last transmitted
// packet (for packets within same frames) has exceed this time limit.
const int kPacketLimitMs = 5;

// Time limit factor between frames.
// A packet in a new frame will be transmitted if the elapsed time since the
// last transmitted packet in the previous frame has exceeded the time
// difference for when the packets were stored, multiplied by this factor.
const float kFrameLimitFactor = 1.2f;
}  // namespace

TransmissionBucket::TransmissionBucket(RtpRtcpClock* clock)
  : clock_(clock),
    critsect_(CriticalSectionWrapper::CreateCriticalSection()),
    accumulator_(0),
    bytes_rem_interval_(0),
    packets_(),
    last_transmitted_packet_(0, 0, 0, 0) {
}

TransmissionBucket::~TransmissionBucket() {
  packets_.clear();
  delete critsect_;
}

void TransmissionBucket::Reset() {
  CriticalSectionScoped cs(critsect_);
  accumulator_ = 0;
  bytes_rem_interval_ = 0;
  packets_.clear();
}

void TransmissionBucket::Fill(uint16_t seq_num,
                              uint32_t timestamp,
                              uint16_t num_bytes) {
  CriticalSectionScoped cs(critsect_);
  accumulator_ += num_bytes;

  Packet p(seq_num, timestamp, num_bytes, clock_->GetTimeInMS());
  packets_.push_back(p);
}

bool TransmissionBucket::Empty() {
  CriticalSectionScoped cs(critsect_);
  return packets_.empty();
}

void TransmissionBucket::UpdateBytesPerInterval(
    uint32_t delta_time_ms,
    uint16_t target_bitrate_kbps) {
  CriticalSectionScoped cs(critsect_);

  uint32_t bytes_per_interval = 
      kBytesPerIntervalMargin * (target_bitrate_kbps * delta_time_ms / 8);

  if (bytes_rem_interval_ < 0) {
    bytes_rem_interval_ += bytes_per_interval;
  } else {
    bytes_rem_interval_ = bytes_per_interval;
  }
}

int32_t TransmissionBucket::GetNextPacket() {
  CriticalSectionScoped cs(critsect_);

  if (accumulator_ == 0) {
    // Empty.
    return -1;
  }

  std::vector<Packet>::const_iterator it_begin = packets_.begin();
  const uint16_t num_bytes = (*it_begin).length;
  const uint16_t seq_num = (*it_begin).sequence_number;

  if (bytes_rem_interval_ <= 0 &&
      !SameFrameAndPacketIntervalTimeElapsed(*it_begin) &&
      !NewFrameAndFrameIntervalTimeElapsed(*it_begin)) {
    // All bytes consumed for this interval.
    return -1;
  }

  // Ok to transmit packet.
  bytes_rem_interval_ -= num_bytes;

  assert(accumulator_ >= num_bytes);
  accumulator_ -= num_bytes;

  last_transmitted_packet_ = packets_[0];
  last_transmitted_packet_.transmitted_ms = clock_->GetTimeInMS();
  packets_.erase(packets_.begin());
  return seq_num;
}

bool TransmissionBucket::SameFrameAndPacketIntervalTimeElapsed(
    const Packet& current_packet) {
  if (last_transmitted_packet_.length == 0) {
    // Not stored.
    return false;
  }
  if (current_packet.timestamp != last_transmitted_packet_.timestamp) {
    // Not same frame.
    return false;
  }
  if ((clock_->GetTimeInMS() - last_transmitted_packet_.transmitted_ms) <
      kPacketLimitMs) {
    // Time has not elapsed.
    return false;
  }
  return true;
}

bool TransmissionBucket::NewFrameAndFrameIntervalTimeElapsed(
    const Packet& current_packet) {
  if (last_transmitted_packet_.length == 0) {
    // Not stored.
    return false;
  }
  if (current_packet.timestamp == last_transmitted_packet_.timestamp) {
    // Not a new frame.
    return false;
  }
  if ((clock_->GetTimeInMS() - last_transmitted_packet_.transmitted_ms)  <
      kFrameLimitFactor *
      (current_packet.stored_ms - last_transmitted_packet_.stored_ms)) {
    // Time has not elapsed.
    return false;
  }
  return true;
}
} // namespace webrtc
