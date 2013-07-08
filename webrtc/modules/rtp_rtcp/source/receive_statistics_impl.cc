/*
 *  Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/modules/rtp_rtcp/source/receive_statistics_impl.h"

#include "webrtc/modules/rtp_rtcp/source/bitrate.h"
#include "webrtc/modules/rtp_rtcp/source/rtp_utility.h"
#include "webrtc/system_wrappers/interface/critical_section_wrapper.h"
#include "webrtc/system_wrappers/interface/scoped_ptr.h"

namespace webrtc {

enum { kRateUpdateIntervalMs = 1000 };

ReceiveStatistics* ReceiveStatistics::Create(Clock* clock) {
  return new ReceiveStatisticsImpl(clock);
}

ReceiveStatisticsImpl::ReceiveStatisticsImpl(Clock* clock)
    : crit_sect_(CriticalSectionWrapper::CreateCriticalSection()),
      clock_(clock),
      incoming_bitrate_(clock),
      ssrc_(0),
      jitter_q4_(0),
      jitter_max_q4_(0),
      cumulative_loss_(0),
      jitter_q4_transmission_time_offset_(0),
      local_time_last_received_timestamp_(0),
      last_received_timestamp_(0),
      last_received_transmission_time_offset_(0),

      received_seq_first_(0),
      received_seq_max_(0),
      received_seq_wraps_(0),

      received_packet_oh_(12),  // RTP header.
      received_byte_count_(0),
      received_retransmitted_packets_(0),
      received_inorder_packet_count_(0),

      last_report_inorder_packets_(0),
      last_report_old_packets_(0),
      last_report_seq_max_(0),
      last_reported_statistics_() {}

void ReceiveStatisticsImpl::ResetStatistics() {
  CriticalSectionScoped lock(crit_sect_.get());
  last_report_inorder_packets_ = 0;
  last_report_old_packets_ = 0;
  last_report_seq_max_ = 0;
  memset(&last_reported_statistics_, 0, sizeof(last_reported_statistics_));
  jitter_q4_ = 0;
  jitter_max_q4_ = 0;
  cumulative_loss_ = 0;
  jitter_q4_transmission_time_offset_ = 0;
  received_seq_wraps_ = 0;
  received_seq_max_ = 0;
  received_seq_first_ = 0;
  received_byte_count_ = 0;
  received_retransmitted_packets_ = 0;
  received_inorder_packet_count_ = 0;
}

void ReceiveStatisticsImpl::ResetDataCounters() {
  CriticalSectionScoped lock(crit_sect_.get());
  received_byte_count_ = 0;
  received_retransmitted_packets_ = 0;
  received_inorder_packet_count_ = 0;
  last_report_inorder_packets_ = 0;
}

void ReceiveStatisticsImpl::IncomingPacket(const RTPHeader& header,
                                           size_t bytes,
                                           bool retransmitted,
                                           bool in_order) {
  ssrc_ = header.ssrc;
  incoming_bitrate_.Update(bytes);

  received_byte_count_ += bytes;

  if (received_seq_max_ == 0 && received_seq_wraps_ == 0) {
    // This is the first received report.
    received_seq_first_ = header.sequenceNumber;
    received_seq_max_ = header.sequenceNumber;
    received_inorder_packet_count_ = 1;
    // Current time in samples.
    local_time_last_received_timestamp_ =
        ModuleRTPUtility::GetCurrentRTP(clock_, header.payload_type_frequency);
    return;
  }

  // Count only the new packets received. That is, if packets 1, 2, 3, 5, 4, 6
  // are received, 4 will be ignored.
  if (in_order) {
    // Current time in samples.
    const uint32_t RTPtime =
        ModuleRTPUtility::GetCurrentRTP(clock_, header.payload_type_frequency);
    received_inorder_packet_count_++;

    // Wrong if we use RetransmitOfOldPacket.
    int32_t seq_diff =
        header.sequenceNumber - received_seq_max_;
    if (seq_diff < 0) {
      // Wrap around detected.
      received_seq_wraps_++;
    }
    // New max.
    received_seq_max_ = header.sequenceNumber;

    if (header.timestamp != last_received_timestamp_ &&
        received_inorder_packet_count_ > 1) {
      int32_t time_diff_samples =
          (RTPtime - local_time_last_received_timestamp_) -
          (header.timestamp - last_received_timestamp_);

      time_diff_samples = abs(time_diff_samples);

      // lib_jingle sometimes deliver crazy jumps in TS for the same stream.
      // If this happens, don't update jitter value. Use 5 secs video frequency
      // as the threshold.
      if (time_diff_samples < 450000) {
        // Note we calculate in Q4 to avoid using float.
        int32_t jitter_diff_q4 = (time_diff_samples << 4) - jitter_q4_;
        jitter_q4_ += ((jitter_diff_q4 + 8) >> 4);
      }

      // Extended jitter report, RFC 5450.
      // Actual network jitter, excluding the source-introduced jitter.
      int32_t time_diff_samples_ext =
        (RTPtime - local_time_last_received_timestamp_) -
        ((header.timestamp +
          header.extension.transmissionTimeOffset) -
         (last_received_timestamp_ +
          last_received_transmission_time_offset_));

      time_diff_samples_ext = abs(time_diff_samples_ext);

      if (time_diff_samples_ext < 450000) {
        int32_t jitter_diffQ4TransmissionTimeOffset =
          (time_diff_samples_ext << 4) - jitter_q4_transmission_time_offset_;
        jitter_q4_transmission_time_offset_ +=
          ((jitter_diffQ4TransmissionTimeOffset + 8) >> 4);
      }
    }
    last_received_timestamp_ = header.timestamp;
    local_time_last_received_timestamp_ = RTPtime;
  } else {
    if (retransmitted) {
      received_retransmitted_packets_++;
    } else {
      received_inorder_packet_count_++;
    }
  }

  uint16_t packet_oh = header.headerLength + header.paddingLength;

  // Our measured overhead. Filter from RFC 5104 4.2.1.2:
  // avg_OH (new) = 15/16*avg_OH (old) + 1/16*pckt_OH,
  received_packet_oh_ = (15 * received_packet_oh_ + packet_oh) >> 4;
}

bool ReceiveStatisticsImpl::Statistics(RtpReceiveStatistics* statistics,
                                       bool reset) {
  int32_t missing;
  return Statistics(statistics, &missing, reset);
}

bool ReceiveStatisticsImpl::Statistics(RtpReceiveStatistics* statistics,
                                       int32_t*  missing, bool reset) {
  CriticalSectionScoped lock(crit_sect_.get());

  if (missing == NULL) {
    return false;
  }
  if (received_seq_first_ == 0 && received_byte_count_ == 0) {
    // We have not received anything.
    return false;
  }
  if (!reset) {
    if (last_report_inorder_packets_ == 0) {
      // No report.
      return false;
    }
    // Just get last report.
    *statistics = last_reported_statistics_;
    return true;
  }

  if (last_report_inorder_packets_ == 0) {
    // First time we send a report.
    last_report_seq_max_ = received_seq_first_ - 1;
  }
  // Calculate fraction lost.
  uint16_t exp_since_last = (received_seq_max_ - last_report_seq_max_);

  if (last_report_seq_max_ > received_seq_max_) {
    // Can we assume that the seq_num can't go decrease over a full RTCP period?
    exp_since_last = 0;
  }

  // Number of received RTP packets since last report, counts all packets but
  // not re-transmissions.
  uint32_t rec_since_last =
      received_inorder_packet_count_ - last_report_inorder_packets_;

  // With NACK we don't know the expected retransmissions during the last
  // second. We know how many "old" packets we have received. We just count
  // the number of old received to estimate the loss, but it still does not
  // guarantee an exact number since we run this based on time triggered by
  // sending of an RTP packet. This should have a minimum effect.

  // With NACK we don't count old packets as received since they are
  // re-transmitted. We use RTT to decide if a packet is re-ordered or
  // re-transmitted.
  uint32_t retransmitted_packets =
      received_retransmitted_packets_ - last_report_old_packets_;
  rec_since_last += retransmitted_packets;

  *missing = 0;
  if (exp_since_last > rec_since_last) {
    *missing = (exp_since_last - rec_since_last);
  }
  uint8_t local_fraction_lost = 0;
  if (exp_since_last) {
    // Scale 0 to 255, where 255 is 100% loss.
    local_fraction_lost = (uint8_t)((255 * (*missing)) / exp_since_last);
  }
  statistics->fraction_lost = local_fraction_lost;

  // We need a counter for cumulative loss too.
  cumulative_loss_ += *missing;

  if (jitter_q4_ > jitter_max_q4_) {
    jitter_max_q4_ = jitter_q4_;
  }
  statistics->cumulative_lost = cumulative_loss_;
  statistics->extended_max_sequence_number = (received_seq_wraps_ << 16) +
      received_seq_max_;
  // Note: internal jitter value is in Q4 and needs to be scaled by 1/16.
  statistics->jitter = jitter_q4_ >> 4;
  statistics->max_jitter = jitter_max_q4_ >> 4;
  if (reset) {
    // Store this report.
    last_reported_statistics_ = *statistics;

    // Only for report blocks in RTCP SR and RR.
    last_report_inorder_packets_ = received_inorder_packet_count_;
    last_report_old_packets_ = received_retransmitted_packets_;
    last_report_seq_max_ = received_seq_max_;
  }
  return true;
}

void ReceiveStatisticsImpl::GetDataCounters(
    uint32_t* bytes_received, uint32_t* packets_received) const {
  CriticalSectionScoped lock(crit_sect_.get());

  if (bytes_received) {
    *bytes_received = received_byte_count_;
  }
  if (packets_received) {
    *packets_received =
        received_retransmitted_packets_ + received_inorder_packet_count_;
  }
}

uint32_t ReceiveStatisticsImpl::BitrateReceived() {
  return incoming_bitrate_.BitrateNow();
}

int32_t ReceiveStatisticsImpl::TimeUntilNextProcess() {
  int time_since_last_update = clock_->TimeInMilliseconds() -
      incoming_bitrate_.time_last_rate_update();
  return std::max(kRateUpdateIntervalMs - time_since_last_update, 0);
}

int32_t ReceiveStatisticsImpl::Process() {
  incoming_bitrate_.Process();
  return 0;
}

}  // namespace webrtc
