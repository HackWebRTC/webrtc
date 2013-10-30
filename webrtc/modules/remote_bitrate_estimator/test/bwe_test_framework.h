/*
 *  Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MODULES_REMOTE_BITRATE_ESTIMATOR_TEST_BWE_TEST_FRAMEWORK_H_
#define WEBRTC_MODULES_REMOTE_BITRATE_ESTIMATOR_TEST_BWE_TEST_FRAMEWORK_H_

#include <algorithm>
#include <cassert>
#include <cmath>
#include <list>
#include <numeric>
#include <string>
#include <vector>

#include "webrtc/modules/interface/module_common_types.h"
#include "webrtc/modules/remote_bitrate_estimator/test/bwe_test_logging.h"

namespace webrtc {
namespace testing {
namespace bwe {

class Random {
 public:
  explicit Random(uint32_t seed)
      : a_(0x531FDB97 ^ seed),
        b_(0x6420ECA8 + seed) {
  }

  // Return semi-random number in the interval [0.0, 1.0].
  float Rand() {
    const float kScale = 1.0f / 0xffffffff;
    float result = kScale * b_;
    a_ ^= b_;
    b_ += a_;
    return result;
  }

  // Normal Distribution.
  int Gaussian(int mean, int standard_deviation) {
    // Creating a Normal distribution variable from two independent uniform
    // variables based on the Box-Muller transform, which is defined on the
    // interval (0, 1], hence the mask+add below.
    const double kPi = 3.14159265358979323846;
    const double kScale = 1.0 / 0x80000000ul;
    double u1 = kScale * ((a_ & 0x7ffffffful) + 1);
    double u2 = kScale * ((b_ & 0x7ffffffful) + 1);
    a_ ^= b_;
    b_ += a_;
    return static_cast<int>(mean + standard_deviation *
        std::sqrt(-2 * std::log(u1)) * std::cos(2 * kPi * u2));
  }

 private:
  uint32_t a_;
  uint32_t b_;

  DISALLOW_IMPLICIT_CONSTRUCTORS(Random);
};

template<typename T> class Stats {
 public:
  Stats()
      : data_(),
        last_mean_count_(0),
        last_variance_count_(0),
        last_minmax_count_(0),
        mean_(0),
        variance_(0),
        min_(0),
        max_(0) {
  }

  void Push(T data_point) {
    data_.push_back(data_point);
  }

  T GetMean() {
    if (last_mean_count_ != data_.size()) {
      last_mean_count_ = data_.size();
      mean_ = std::accumulate(data_.begin(), data_.end(), static_cast<T>(0));
      assert(last_mean_count_ != 0);
      mean_ /= static_cast<T>(last_mean_count_);
    }
    return mean_;
  }
  T GetVariance() {
    if (last_variance_count_ != data_.size()) {
      last_variance_count_ = data_.size();
      T mean = GetMean();
      variance_ = 0;
      for (typename std::vector<T>::const_iterator it = data_.begin();
          it != data_.end(); ++it) {
        T diff = (*it - mean);
        variance_ += diff * diff;
      }
      assert(last_variance_count_ != 0);
      variance_ /= static_cast<T>(last_variance_count_);
    }
    return variance_;
  }
  T GetStdDev() {
    return std::sqrt(static_cast<double>(GetVariance()));
  }
  T GetMin() {
    RefreshMinMax();
    return min_;
  }
  T GetMax() {
    RefreshMinMax();
    return max_;
  }

  void Log(const std::string& units) {
    BWE_TEST_LOGGING_LOG5("", "%f %s\t+/-%f\t[%f,%f]",
        GetMean(), units.c_str(), GetStdDev(), GetMin(), GetMax());
  }

 private:
  void RefreshMinMax() {
    if (last_minmax_count_ != data_.size()) {
      last_minmax_count_ = data_.size();
      min_ = max_ = 0;
      if (data_.empty()) {
        return;
      }
      typename std::vector<T>::const_iterator it = data_.begin();
      min_ = max_ = *it;
      while (++it != data_.end()) {
        min_ = std::min(min_, *it);
        max_ = std::max(max_, *it);
      }
    }
  }

  std::vector<T> data_;
  typename std::vector<T>::size_type last_mean_count_;
  typename std::vector<T>::size_type last_variance_count_;
  typename std::vector<T>::size_type last_minmax_count_;
  T mean_;
  T variance_;
  T min_;
  T max_;
};

class BwePacket {
 public:
  BwePacket()
      : send_time_us_(0),
        payload_size_(0) {
     memset(&header_, 0, sizeof(header_));
  }

  BwePacket(int64_t send_time_us, uint32_t payload_size,
            const RTPHeader& header)
    : send_time_us_(send_time_us),
      payload_size_(payload_size),
      header_(header) {
  }

  BwePacket(int64_t send_time_us, uint32_t sequence_number)
      : send_time_us_(send_time_us),
        payload_size_(0) {
     memset(&header_, 0, sizeof(header_));
     header_.sequenceNumber = sequence_number;
  }

  bool operator<(const BwePacket& rhs) const {
    return send_time_us_ < rhs.send_time_us_;
  }

  void set_send_time_us(int64_t send_time_us) {
    assert(send_time_us >= 0);
    send_time_us_ = send_time_us;
  }
  int64_t send_time_us() const { return send_time_us_; }
  uint32_t payload_size() const { return payload_size_; }
  const RTPHeader& header() const { return header_; }

 private:
  int64_t send_time_us_;   // Time the packet left last processor touching it.
  uint32_t payload_size_;  // Size of the (non-existent, simulated) payload.
  RTPHeader header_;       // Actual contents.
};

typedef std::list<BwePacket> Packets;
typedef std::list<BwePacket>::iterator PacketsIt;
typedef std::list<BwePacket>::const_iterator PacketsConstIt;

bool IsTimeSorted(const Packets& packets);

class PacketProcessorInterface {
 public:
  virtual ~PacketProcessorInterface() {}

  // Run simulation for |time_ms| micro seconds, consuming packets from, and
  // producing packets into in_out. The outgoing packet list must be sorted on
  // |send_time_us_|. The simulation time |time_ms| is optional to use.
  virtual void RunFor(int64_t time_ms, Packets* in_out) = 0;
};

class VideoSender : public PacketProcessorInterface {
 public:
  VideoSender(float fps, uint32_t kbps, uint32_t ssrc, float first_frame_offset)
      : kMaxPayloadSizeBytes(1000),
        kTimestampBase(0xff80ff00ul),
        frame_period_ms_(1000.0 / fps),
        next_frame_ms_(frame_period_ms_ * first_frame_offset),
        now_ms_(0.0),
        bytes_per_second_(1000 * kbps / 8),
        frame_size_bytes_(bytes_per_second_ / fps),
        prototype_header_() {
    assert(first_frame_offset >= 0.0f);
    assert(first_frame_offset < 1.0f);
    memset(&prototype_header_, 0, sizeof(prototype_header_));
    prototype_header_.ssrc = ssrc;
    prototype_header_.sequenceNumber = 0xf000u;
  }
  virtual ~VideoSender() {}

  uint32_t max_payload_size_bytes() const { return kMaxPayloadSizeBytes; }
  uint32_t bytes_per_second() const { return bytes_per_second_; }

  virtual void RunFor(int64_t time_ms, Packets* in_out) {
    assert(in_out);
    now_ms_ += time_ms;
    Packets newPackets;
    while (now_ms_ >= next_frame_ms_) {
      prototype_header_.sequenceNumber++;
      prototype_header_.timestamp = kTimestampBase +
          static_cast<uint32_t>(next_frame_ms_ * 90.0);
      prototype_header_.extension.absoluteSendTime = (kTimestampBase +
          ((static_cast<int64_t>(next_frame_ms_ * (1 << 18)) + 500) / 1000)) &
              0x00fffffful;
      prototype_header_.extension.transmissionTimeOffset = 0;

      // Generate new packets for this frame, all with the same timestamp,
      // but the payload size is capped, so if the whole frame doesn't fit in
      // one packet, we will see a number of equally sized packets followed by
      // one smaller at the tail.
      int64_t send_time_us = next_frame_ms_ * 1000.0;
      uint32_t payload_size = frame_size_bytes_;
      while (payload_size > 0) {
        uint32_t size = std::min(kMaxPayloadSizeBytes, payload_size);
        newPackets.push_back(BwePacket(send_time_us, size, prototype_header_));
        payload_size -= size;
      }

      next_frame_ms_ += frame_period_ms_;
    }
    in_out->merge(newPackets);
  }

 private:
  const uint32_t kMaxPayloadSizeBytes;
  const uint32_t kTimestampBase;
  double frame_period_ms_;
  double next_frame_ms_;
  double now_ms_;
  uint32_t bytes_per_second_;
  uint32_t frame_size_bytes_;
  RTPHeader prototype_header_;

  DISALLOW_IMPLICIT_CONSTRUCTORS(VideoSender);
};

class RateCounterFilter : public PacketProcessorInterface {
 public:
  RateCounterFilter()
      : kWindowSizeUs(1000000),
        packets_per_second_(0),
        bytes_per_second_(0),
        last_accumulated_us_(0),
        window_(),
        pps_stats_(),
        kbps_stats_() {
  }
  virtual ~RateCounterFilter() {
    LogStats();
  }

  uint32_t packets_per_second() const { return packets_per_second_; }
  uint32_t bits_per_second() const { return bytes_per_second_ * 8; }

  void LogStats() {
    BWE_TEST_LOGGING_CONTEXT("RateCounterFilter");
    pps_stats_.Log("pps");
    kbps_stats_.Log("kbps");
  }

  virtual void RunFor(int64_t /*time_ms*/, Packets* in_out) {
    assert(in_out);
    for (PacketsConstIt it = in_out->begin(); it != in_out->end(); ++it) {
      packets_per_second_++;
      bytes_per_second_ += it->payload_size();
      last_accumulated_us_ = it->send_time_us();
    }
    window_.insert(window_.end(), in_out->begin(), in_out->end());
    while (!window_.empty()) {
      const BwePacket& packet = window_.front();
      if (packet.send_time_us() > (last_accumulated_us_ - kWindowSizeUs)) {
        break;
      }
      assert(packets_per_second_ >= 1);
      assert(bytes_per_second_ >= packet.payload_size());
      packets_per_second_--;
      bytes_per_second_ -= packet.payload_size();
      window_.pop_front();
    }
    pps_stats_.Push(packets_per_second_);
    kbps_stats_.Push((bytes_per_second_ * 8) / 1000.0);
  }

 private:
  const int64_t kWindowSizeUs;
  uint32_t packets_per_second_;
  uint32_t bytes_per_second_;
  int64_t last_accumulated_us_;
  Packets window_;
  Stats<double> pps_stats_;
  Stats<double> kbps_stats_;

  DISALLOW_COPY_AND_ASSIGN(RateCounterFilter);
};

class LossFilter : public PacketProcessorInterface {
 public:
  LossFilter() : random_(0x12345678), loss_fraction_(0.0f) {}
  virtual ~LossFilter() {}

  void SetLoss(float loss_percent) {
    BWE_TEST_LOGGING_ENABLE(false);
    BWE_TEST_LOGGING_LOG1("Loss", "%f%%", loss_percent);
    assert(loss_percent >= 0.0f);
    assert(loss_percent <= 100.0f);
    loss_fraction_ = loss_percent * 0.01f;
  }

  virtual void RunFor(int64_t /*time_ms*/, Packets* in_out) {
    assert(in_out);
    for (PacketsIt it = in_out->begin(); it != in_out->end(); ) {
      if (random_.Rand() < loss_fraction_) {
        it = in_out->erase(it);
      } else {
        ++it;
      }
    }
  }

 private:
  Random random_;
  float loss_fraction_;

  DISALLOW_COPY_AND_ASSIGN(LossFilter);
};

class DelayFilter : public PacketProcessorInterface {
 public:
  DelayFilter() : delay_us_(0), last_send_time_us_(0) {}
  virtual ~DelayFilter() {}

  void SetDelay(int64_t delay_ms) {
    BWE_TEST_LOGGING_ENABLE(false);
    BWE_TEST_LOGGING_LOG1("Delay", "%d ms", static_cast<int>(delay_ms));
    assert(delay_ms >= 0);
    delay_us_ = delay_ms * 1000;
  }

  virtual void RunFor(int64_t /*time_ms*/, Packets* in_out) {
    assert(in_out);
    for (PacketsIt it = in_out->begin(); it != in_out->end(); ++it) {
      int64_t new_send_time_us = it->send_time_us() + delay_us_;
      last_send_time_us_ = std::max(last_send_time_us_, new_send_time_us);
      it->set_send_time_us(last_send_time_us_);
    }
  }

 private:
  int64_t delay_us_;
  int64_t last_send_time_us_;

  DISALLOW_COPY_AND_ASSIGN(DelayFilter);
};

class JitterFilter : public PacketProcessorInterface {
 public:
  JitterFilter()
      : random_(0x89674523),
        stddev_jitter_us_(0),
        last_send_time_us_(0) {
  }
  virtual ~JitterFilter() {}

  void SetJitter(int64_t stddev_jitter_ms) {
    BWE_TEST_LOGGING_ENABLE(false);
    BWE_TEST_LOGGING_LOG1("Jitter", "%d ms",
                          static_cast<int>(stddev_jitter_ms));
    assert(stddev_jitter_ms >= 0);
    stddev_jitter_us_ = stddev_jitter_ms * 1000;
  }

  virtual void RunFor(int64_t /*time_ms*/, Packets* in_out) {
    assert(in_out);
    for (PacketsIt it = in_out->begin(); it != in_out->end(); ++it) {
      int64_t new_send_time_us = it->send_time_us();
      new_send_time_us += random_.Gaussian(0, stddev_jitter_us_);
      last_send_time_us_ = std::max(last_send_time_us_, new_send_time_us);
      it->set_send_time_us(last_send_time_us_);
    }
  }

 private:
  Random random_;
  int64_t stddev_jitter_us_;
  int64_t last_send_time_us_;

  DISALLOW_COPY_AND_ASSIGN(JitterFilter);
};

class ReorderFilter : public PacketProcessorInterface {
 public:
  ReorderFilter() : random_(0x27452389), reorder_fraction_(0.0f) {}
  virtual ~ReorderFilter() {}

  void SetReorder(float reorder_percent) {
    BWE_TEST_LOGGING_ENABLE(false);
    BWE_TEST_LOGGING_LOG1("Reordering", "%f%%", reorder_percent);
    assert(reorder_percent >= 0.0f);
    assert(reorder_percent <= 100.0f);
    reorder_fraction_ = reorder_percent * 0.01f;
  }

  virtual void RunFor(int64_t /*time_ms*/, Packets* in_out) {
    assert(in_out);
    if (in_out->size() >= 2) {
      PacketsIt last_it = in_out->begin();
      PacketsIt it = last_it;
      while (++it != in_out->end()) {
        if (random_.Rand() < reorder_fraction_) {
          int64_t t1 = last_it->send_time_us();
          int64_t t2 = it->send_time_us();
          std::swap(*last_it, *it);
          last_it->set_send_time_us(t1);
          it->set_send_time_us(t2);
        }
        last_it = it;
      }
    }
  }

 private:
  Random random_;
  float reorder_fraction_;

  DISALLOW_COPY_AND_ASSIGN(ReorderFilter);
};

// Apply a bitrate choke with an infinite queue on the packet stream.
class ChokeFilter : public PacketProcessorInterface {
 public:
  ChokeFilter() : kbps_(1200), last_send_time_us_(0) {}
  virtual ~ChokeFilter() {}

  void SetCapacity(uint32_t kbps) {
    BWE_TEST_LOGGING_ENABLE(false);
    BWE_TEST_LOGGING_LOG1("BitrateChoke", "%d kbps", kbps);
    kbps_ = kbps;
  }

  virtual void RunFor(int64_t /*time_ms*/, Packets* in_out) {
    assert(in_out);
    for (PacketsIt it = in_out->begin(); it != in_out->end(); ++it) {
      int64_t earliest_send_time_us = last_send_time_us_ +
          (it->payload_size() * 8 * 1000 + kbps_ / 2) / kbps_;
      last_send_time_us_ = std::max(it->send_time_us(), earliest_send_time_us);
      it->set_send_time_us(last_send_time_us_);
    }
  }

 private:
  uint32_t kbps_;
  int64_t last_send_time_us_;

  DISALLOW_COPY_AND_ASSIGN(ChokeFilter);
};
}  // namespace bwe
}  // namespace testing
}  // namespace webrtc

#endif  // WEBRTC_MODULES_REMOTE_BITRATE_ESTIMATOR_TEST_BWE_TEST_FRAMEWORK_H_
