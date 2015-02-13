/*
 *  Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/modules/remote_bitrate_estimator/test/bwe_test_framework.h"

#include <stdio.h>

#include <sstream>

namespace webrtc {
namespace testing {
namespace bwe {

const int kMinBitrateKbps = 10;
const int kMaxBitrateKbps = 20000;

class DelayCapHelper {
 public:
  DelayCapHelper() : max_delay_us_(0), delay_stats_() {}

  void SetMaxDelay(int max_delay_ms) {
    BWE_TEST_LOGGING_ENABLE(false);
    BWE_TEST_LOGGING_LOG1("Max Delay", "%d ms", static_cast<int>(max_delay_ms));
    assert(max_delay_ms >= 0);
    max_delay_us_ = max_delay_ms * 1000;
  }

  bool ShouldSendPacket(int64_t send_time_us, int64_t arrival_time_us) {
    int64_t packet_delay_us = send_time_us - arrival_time_us;
    delay_stats_.Push(std::min(packet_delay_us, max_delay_us_) / 1000);
    return (max_delay_us_ == 0 || max_delay_us_ >= packet_delay_us);
  }

  const Stats<double>& delay_stats() const {
    return delay_stats_;
  }

 private:
  int64_t max_delay_us_;
  Stats<double> delay_stats_;

  DISALLOW_COPY_AND_ASSIGN(DelayCapHelper);
};

const FlowIds CreateFlowIds(const int *flow_ids_array, size_t num_flow_ids) {
  FlowIds flow_ids(&flow_ids_array[0], flow_ids_array + num_flow_ids);
  return flow_ids;
}

class RateCounter {
 public:
  RateCounter()
      : kWindowSizeUs(1000000),
        packets_per_second_(0),
        bytes_per_second_(0),
        last_accumulated_us_(0),
        window_() {}

  void UpdateRates(int64_t send_time_us, uint32_t payload_size) {
    packets_per_second_++;
    bytes_per_second_ += payload_size;
    last_accumulated_us_ = send_time_us;
    window_.push_back(std::make_pair(send_time_us, payload_size));
    while (!window_.empty()) {
      const TimeSizePair& packet = window_.front();
      if (packet.first > (last_accumulated_us_ - kWindowSizeUs)) {
        break;
      }
      assert(packets_per_second_ >= 1);
      assert(bytes_per_second_ >= packet.second);
      packets_per_second_--;
      bytes_per_second_ -= packet.second;
      window_.pop_front();
    }
  }

  uint32_t bits_per_second() const {
    return bytes_per_second_ * 8;
  }

  uint32_t packets_per_second() const { return packets_per_second_; }

 private:
  typedef std::pair<int64_t, uint32_t> TimeSizePair;

  const int64_t kWindowSizeUs;
  uint32_t packets_per_second_;
  uint32_t bytes_per_second_;
  int64_t last_accumulated_us_;
  std::list<TimeSizePair> window_;
};

Random::Random(uint32_t seed)
    : a_(0x531FDB97 ^ seed),
      b_(0x6420ECA8 + seed) {
}

float Random::Rand() {
  const float kScale = 1.0f / 0xffffffff;
  float result = kScale * b_;
  a_ ^= b_;
  b_ += a_;
  return result;
}

int Random::Gaussian(int mean, int standard_deviation) {
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
      sqrt(-2 * log(u1)) * cos(2 * kPi * u2));
}

Packet::Packet()
    : flow_id_(0), creation_time_us_(-1), send_time_us_(-1), payload_size_(0) {
}

Packet::Packet(int flow_id, int64_t send_time_us, size_t payload_size)
    : flow_id_(flow_id),
      creation_time_us_(send_time_us),
      send_time_us_(send_time_us),
      payload_size_(payload_size) {
}

Packet::~Packet() {
}

bool Packet::operator<(const Packet& rhs) const {
  return send_time_us_ < rhs.send_time_us_;
}

void Packet::set_send_time_us(int64_t send_time_us) {
  assert(send_time_us >= 0);
  send_time_us_ = send_time_us;
}

MediaPacket::MediaPacket() {
  memset(&header_, 0, sizeof(header_));
}

MediaPacket::MediaPacket(int flow_id,
                         int64_t send_time_us,
                         size_t payload_size,
                         const RTPHeader& header)
    : Packet(flow_id, send_time_us, payload_size), header_(header) {
}

MediaPacket::MediaPacket(int64_t send_time_us, uint32_t sequence_number)
    : Packet(0, send_time_us, 0) {
  memset(&header_, 0, sizeof(header_));
  header_.sequenceNumber = sequence_number;
}

void MediaPacket::SetAbsSendTimeMs(int64_t abs_send_time_ms) {
  header_.extension.hasAbsoluteSendTime = true;
  header_.extension.absoluteSendTime = ((static_cast<int64_t>(abs_send_time_ms *
    (1 << 18)) + 500) / 1000) & 0x00fffffful;
}

RembFeedback::RembFeedback(int flow_id,
                           int64_t send_time_us,
                           uint32_t estimated_bps,
                           RTCPReportBlock report_block)
    : FeedbackPacket(flow_id, send_time_us),
      estimated_bps_(estimated_bps),
      report_block_(report_block) {
}

SendSideBweFeedback::SendSideBweFeedback(
    int flow_id,
    int64_t send_time_us,
    const std::vector<PacketInfo>& packet_feedback_vector)
    : FeedbackPacket(flow_id, send_time_us),
      packet_feedback_vector_(packet_feedback_vector) {
}

bool IsTimeSorted(const Packets& packets) {
  PacketsConstIt last_it = packets.begin();
  for (PacketsConstIt it = last_it; it != packets.end(); ++it) {
    if (it != last_it && **it < **last_it) {
      return false;
    }
    last_it = it;
  }
  return true;
}

PacketProcessor::PacketProcessor(PacketProcessorListener* listener,
                                 int flow_id,
                                 ProcessorType type)
    : listener_(listener) {
  flow_ids_.insert(flow_id);
  if (listener_) {
    listener_->AddPacketProcessor(this, type);
  }
}

PacketProcessor::PacketProcessor(PacketProcessorListener* listener,
                                 const FlowIds& flow_ids,
                                 ProcessorType type)
    : listener_(listener), flow_ids_(flow_ids) {
  if (listener_) {
    listener_->AddPacketProcessor(this, type);
  }
}

PacketProcessor::~PacketProcessor() {
  if (listener_) {
    listener_->RemovePacketProcessor(this);
  }
}

RateCounterFilter::RateCounterFilter(PacketProcessorListener* listener,
                                     int flow_id,
                                     const char* name)
    : PacketProcessor(listener, flow_id, kRegular),
      rate_counter_(new RateCounter()),
      packets_per_second_stats_(),
      kbps_stats_(),
      name_(name) {
}

RateCounterFilter::RateCounterFilter(PacketProcessorListener* listener,
                                     const FlowIds& flow_ids,
                                     const char* name)
    : PacketProcessor(listener, flow_ids, kRegular),
      rate_counter_(new RateCounter()),
      packets_per_second_stats_(),
      kbps_stats_(),
      name_(name) {
  std::stringstream ss;
  ss << name_ << "_";
  for (int flow_id : flow_ids) {
    ss << flow_id << ",";
  }
  name_ = ss.str();
}

RateCounterFilter::~RateCounterFilter() {
  LogStats();
}

uint32_t RateCounterFilter::packets_per_second() const {
  return rate_counter_->packets_per_second();
}

uint32_t RateCounterFilter::bits_per_second() const {
  return rate_counter_->bits_per_second();
}

void RateCounterFilter::LogStats() {
  BWE_TEST_LOGGING_CONTEXT("RateCounterFilter");
  packets_per_second_stats_.Log("pps");
  kbps_stats_.Log("kbps");
}

Stats<double> RateCounterFilter::GetBitrateStats() const {
  return kbps_stats_;
}

void RateCounterFilter::Plot(int64_t timestamp_ms) {
  BWE_TEST_LOGGING_CONTEXT(name_.c_str());
  BWE_TEST_LOGGING_PLOT("Throughput_#1", timestamp_ms,
                        rate_counter_->bits_per_second() / 1000.0);
}

void RateCounterFilter::RunFor(int64_t /*time_ms*/, Packets* in_out) {
  assert(in_out);
  for (const Packet* packet : *in_out) {
    rate_counter_->UpdateRates(packet->send_time_us(), packet->payload_size());
  }
  packets_per_second_stats_.Push(rate_counter_->packets_per_second());
  kbps_stats_.Push(rate_counter_->bits_per_second() / 1000.0);
}

LossFilter::LossFilter(PacketProcessorListener* listener, int flow_id)
    : PacketProcessor(listener, flow_id, kRegular),
      random_(0x12345678),
      loss_fraction_(0.0f) {
}

LossFilter::LossFilter(PacketProcessorListener* listener,
                       const FlowIds& flow_ids)
    : PacketProcessor(listener, flow_ids, kRegular),
      random_(0x12345678),
      loss_fraction_(0.0f) {
}

void LossFilter::SetLoss(float loss_percent) {
  BWE_TEST_LOGGING_ENABLE(false);
  BWE_TEST_LOGGING_LOG1("Loss", "%f%%", loss_percent);
  assert(loss_percent >= 0.0f);
  assert(loss_percent <= 100.0f);
  loss_fraction_ = loss_percent * 0.01f;
}

void LossFilter::RunFor(int64_t /*time_ms*/, Packets* in_out) {
  assert(in_out);
  for (PacketsIt it = in_out->begin(); it != in_out->end(); ) {
    if (random_.Rand() < loss_fraction_) {
      delete *it;
      it = in_out->erase(it);
    } else {
      ++it;
    }
  }
}

DelayFilter::DelayFilter(PacketProcessorListener* listener, int flow_id)
    : PacketProcessor(listener, flow_id, kRegular),
      delay_us_(0),
      last_send_time_us_(0) {
}

DelayFilter::DelayFilter(PacketProcessorListener* listener,
                         const FlowIds& flow_ids)
    : PacketProcessor(listener, flow_ids, kRegular),
      delay_us_(0),
      last_send_time_us_(0) {
}

void DelayFilter::SetDelay(int64_t delay_ms) {
  BWE_TEST_LOGGING_ENABLE(false);
  BWE_TEST_LOGGING_LOG1("Delay", "%d ms", static_cast<int>(delay_ms));
  assert(delay_ms >= 0);
  delay_us_ = delay_ms * 1000;
}

void DelayFilter::RunFor(int64_t /*time_ms*/, Packets* in_out) {
  assert(in_out);
  for (Packet* packet : *in_out) {
    int64_t new_send_time_us = packet->send_time_us() + delay_us_;
    last_send_time_us_ = std::max(last_send_time_us_, new_send_time_us);
    packet->set_send_time_us(last_send_time_us_);
  }
}

JitterFilter::JitterFilter(PacketProcessorListener* listener, int flow_id)
    : PacketProcessor(listener, flow_id, kRegular),
      random_(0x89674523),
      stddev_jitter_us_(0),
      last_send_time_us_(0) {
}

JitterFilter::JitterFilter(PacketProcessorListener* listener,
                           const FlowIds& flow_ids)
    : PacketProcessor(listener, flow_ids, kRegular),
      random_(0x89674523),
      stddev_jitter_us_(0),
      last_send_time_us_(0) {
}

void JitterFilter::SetJitter(int64_t stddev_jitter_ms) {
  BWE_TEST_LOGGING_ENABLE(false);
  BWE_TEST_LOGGING_LOG1("Jitter", "%d ms",
                        static_cast<int>(stddev_jitter_ms));
  assert(stddev_jitter_ms >= 0);
  stddev_jitter_us_ = stddev_jitter_ms * 1000;
}

void JitterFilter::RunFor(int64_t /*time_ms*/, Packets* in_out) {
  assert(in_out);
  for (Packet* packet : *in_out) {
    int64_t new_send_time_us = packet->send_time_us();
    new_send_time_us += random_.Gaussian(0, stddev_jitter_us_);
    last_send_time_us_ = std::max(last_send_time_us_, new_send_time_us);
    packet->set_send_time_us(last_send_time_us_);
  }
}

ReorderFilter::ReorderFilter(PacketProcessorListener* listener, int flow_id)
    : PacketProcessor(listener, flow_id, kRegular),
      random_(0x27452389),
      reorder_fraction_(0.0f) {
}

ReorderFilter::ReorderFilter(PacketProcessorListener* listener,
                             const FlowIds& flow_ids)
    : PacketProcessor(listener, flow_ids, kRegular),
      random_(0x27452389),
      reorder_fraction_(0.0f) {
}

void ReorderFilter::SetReorder(float reorder_percent) {
  BWE_TEST_LOGGING_ENABLE(false);
  BWE_TEST_LOGGING_LOG1("Reordering", "%f%%", reorder_percent);
  assert(reorder_percent >= 0.0f);
  assert(reorder_percent <= 100.0f);
  reorder_fraction_ = reorder_percent * 0.01f;
}

void ReorderFilter::RunFor(int64_t /*time_ms*/, Packets* in_out) {
  assert(in_out);
  if (in_out->size() >= 2) {
    PacketsIt last_it = in_out->begin();
    PacketsIt it = last_it;
    while (++it != in_out->end()) {
      if (random_.Rand() < reorder_fraction_) {
        int64_t t1 = (*last_it)->send_time_us();
        int64_t t2 = (*it)->send_time_us();
        std::swap(*last_it, *it);
        (*last_it)->set_send_time_us(t1);
        (*it)->set_send_time_us(t2);
      }
      last_it = it;
    }
  }
}

ChokeFilter::ChokeFilter(PacketProcessorListener* listener, int flow_id)
    : PacketProcessor(listener, flow_id, kRegular),
      kbps_(1200),
      last_send_time_us_(0),
      delay_cap_helper_(new DelayCapHelper()) {
}

ChokeFilter::ChokeFilter(PacketProcessorListener* listener,
                         const FlowIds& flow_ids)
    : PacketProcessor(listener, flow_ids, kRegular),
      kbps_(1200),
      last_send_time_us_(0),
      delay_cap_helper_(new DelayCapHelper()) {
}

ChokeFilter::~ChokeFilter() {}

void ChokeFilter::SetCapacity(uint32_t kbps) {
  BWE_TEST_LOGGING_ENABLE(false);
  BWE_TEST_LOGGING_LOG1("BitrateChoke", "%d kbps", kbps);
  kbps_ = kbps;
}

void ChokeFilter::RunFor(int64_t /*time_ms*/, Packets* in_out) {
  assert(in_out);
  for (PacketsIt it = in_out->begin(); it != in_out->end(); ) {
    int64_t earliest_send_time_us =
        last_send_time_us_ +
        ((*it)->payload_size() * 8 * 1000 + kbps_ / 2) / kbps_;
    int64_t new_send_time_us =
        std::max((*it)->send_time_us(), earliest_send_time_us);
    if (delay_cap_helper_->ShouldSendPacket(new_send_time_us,
                                            (*it)->send_time_us())) {
      (*it)->set_send_time_us(new_send_time_us);
      last_send_time_us_ = new_send_time_us;
      ++it;
    } else {
      delete *it;
      it = in_out->erase(it);
    }
  }
}

void ChokeFilter::SetMaxDelay(int max_delay_ms) {
  delay_cap_helper_->SetMaxDelay(max_delay_ms);
}

Stats<double> ChokeFilter::GetDelayStats() const {
  return delay_cap_helper_->delay_stats();
}

TraceBasedDeliveryFilter::TraceBasedDeliveryFilter(
    PacketProcessorListener* listener,
    int flow_id)
    : PacketProcessor(listener, flow_id, kRegular),
      current_offset_us_(0),
      delivery_times_us_(),
      next_delivery_it_(),
      local_time_us_(-1),
      rate_counter_(new RateCounter),
      name_(""),
      delay_cap_helper_(new DelayCapHelper()),
      packets_per_second_stats_(),
      kbps_stats_() {
}

TraceBasedDeliveryFilter::TraceBasedDeliveryFilter(
    PacketProcessorListener* listener,
    const FlowIds& flow_ids)
    : PacketProcessor(listener, flow_ids, kRegular),
      current_offset_us_(0),
      delivery_times_us_(),
      next_delivery_it_(),
      local_time_us_(-1),
      rate_counter_(new RateCounter),
      name_(""),
      delay_cap_helper_(new DelayCapHelper()),
      packets_per_second_stats_(),
      kbps_stats_() {
}

TraceBasedDeliveryFilter::TraceBasedDeliveryFilter(
    PacketProcessorListener* listener,
    int flow_id,
    const char* name)
    : PacketProcessor(listener, flow_id, kRegular),
      current_offset_us_(0),
      delivery_times_us_(),
      next_delivery_it_(),
      local_time_us_(-1),
      rate_counter_(new RateCounter),
      name_(name),
      delay_cap_helper_(new DelayCapHelper()),
      packets_per_second_stats_(),
      kbps_stats_() {
}

TraceBasedDeliveryFilter::~TraceBasedDeliveryFilter() {
}

bool TraceBasedDeliveryFilter::Init(const std::string& filename) {
  FILE* trace_file = fopen(filename.c_str(), "r");
  if (!trace_file) {
    return false;
  }
  int64_t first_timestamp = -1;
  while(!feof(trace_file)) {
    const size_t kMaxLineLength = 100;
    char line[kMaxLineLength];
    if (fgets(line, kMaxLineLength, trace_file)) {
      std::string line_string(line);
      std::istringstream buffer(line_string);
      int64_t timestamp;
      buffer >> timestamp;
      timestamp /= 1000;  // Convert to microseconds.
      if (first_timestamp == -1)
        first_timestamp = timestamp;
      assert(delivery_times_us_.empty() ||
             timestamp - first_timestamp - delivery_times_us_.back() >= 0);
      delivery_times_us_.push_back(timestamp - first_timestamp);
    }
  }
  assert(!delivery_times_us_.empty());
  next_delivery_it_ = delivery_times_us_.begin();
  fclose(trace_file);
  return true;
}

void TraceBasedDeliveryFilter::Plot(int64_t timestamp_ms) {
  BWE_TEST_LOGGING_CONTEXT(name_.c_str());
  // This plots the max possible throughput of the trace-based delivery filter,
  // which will be reached if a packet sent on every packet slot of the trace.
  BWE_TEST_LOGGING_PLOT("MaxThroughput_#1", timestamp_ms,
                        rate_counter_->bits_per_second() / 1000.0);
}

void TraceBasedDeliveryFilter::RunFor(int64_t time_ms, Packets* in_out) {
  assert(in_out);
  for (PacketsIt it = in_out->begin(); it != in_out->end();) {
    while (local_time_us_ < (*it)->send_time_us()) {
      ProceedToNextSlot();
    }
    // Drop any packets that have been queued for too long.
    while (!delay_cap_helper_->ShouldSendPacket(local_time_us_,
                                                (*it)->send_time_us())) {
      delete *it;
      it = in_out->erase(it);
      if (it == in_out->end()) {
        return;
      }
    }
    if (local_time_us_ >= (*it)->send_time_us()) {
      (*it)->set_send_time_us(local_time_us_);
      ProceedToNextSlot();
    }
    ++it;
  }
  packets_per_second_stats_.Push(rate_counter_->packets_per_second());
  kbps_stats_.Push(rate_counter_->bits_per_second() / 1000.0);
}

void TraceBasedDeliveryFilter::SetMaxDelay(int max_delay_ms) {
  delay_cap_helper_->SetMaxDelay(max_delay_ms);
}

Stats<double> TraceBasedDeliveryFilter::GetDelayStats() const {
  return delay_cap_helper_->delay_stats();
}

Stats<double> TraceBasedDeliveryFilter::GetBitrateStats() const {
  return kbps_stats_;
}

void TraceBasedDeliveryFilter::ProceedToNextSlot() {
  if (*next_delivery_it_ <= local_time_us_) {
    ++next_delivery_it_;
    if (next_delivery_it_ == delivery_times_us_.end()) {
      // When the trace wraps we allow two packets to be sent back-to-back.
      for (int64_t& delivery_time_us : delivery_times_us_) {
        delivery_time_us += local_time_us_ - current_offset_us_;
      }
      current_offset_us_ += local_time_us_ - current_offset_us_;
      next_delivery_it_ = delivery_times_us_.begin();
    }
  }
  local_time_us_ = *next_delivery_it_;
  const int kPayloadSize = 1200;
  rate_counter_->UpdateRates(local_time_us_, kPayloadSize);
}

VideoSource::VideoSource(int flow_id,
                         float fps,
                         uint32_t kbps,
                         uint32_t ssrc,
                         int64_t first_frame_offset_ms)
    : kMaxPayloadSizeBytes(1200),
      kTimestampBase(0xff80ff00ul),
      frame_period_ms_(1000.0 / fps),
      bits_per_second_(1000 * kbps),
      frame_size_bytes_(bits_per_second_ / 8 / fps),
      flow_id_(flow_id),
      next_frame_ms_(first_frame_offset_ms),
      now_ms_(0),
      prototype_header_() {
  memset(&prototype_header_, 0, sizeof(prototype_header_));
  prototype_header_.ssrc = ssrc;
  prototype_header_.sequenceNumber = 0xf000u;
}

uint32_t VideoSource::NextFrameSize() {
  return frame_size_bytes_;
}

uint32_t VideoSource::NextPacketSize(uint32_t frame_size,
                                     uint32_t remaining_payload) {
  return std::min(kMaxPayloadSizeBytes, remaining_payload);
}

void VideoSource::RunFor(int64_t time_ms, Packets* in_out) {
  assert(in_out);
  now_ms_ += time_ms;
  Packets new_packets;
  while (now_ms_ >= next_frame_ms_) {
    prototype_header_.timestamp = kTimestampBase +
        static_cast<uint32_t>(next_frame_ms_ * 90.0);
    prototype_header_.extension.transmissionTimeOffset = 0;

    // Generate new packets for this frame, all with the same timestamp,
    // but the payload size is capped, so if the whole frame doesn't fit in
    // one packet, we will see a number of equally sized packets followed by
    // one smaller at the tail.
    int64_t send_time_us = next_frame_ms_ * 1000.0;
    uint32_t frame_size = NextFrameSize();
    uint32_t payload_size = frame_size;

    while (payload_size > 0) {
      ++prototype_header_.sequenceNumber;
      uint32_t size = NextPacketSize(frame_size, payload_size);
      MediaPacket* new_packet =
          new MediaPacket(flow_id_, send_time_us, size, prototype_header_);
      new_packets.push_back(new_packet);
      new_packet->SetAbsSendTimeMs(next_frame_ms_);
      payload_size -= size;
    }

    next_frame_ms_ += frame_period_ms_;
  }
  in_out->merge(new_packets, DereferencingComparator<Packet>);
}

AdaptiveVideoSource::AdaptiveVideoSource(int flow_id,
                                         float fps,
                                         uint32_t kbps,
                                         uint32_t ssrc,
                                         int64_t first_frame_offset_ms)
    : VideoSource(flow_id, fps, kbps, ssrc, first_frame_offset_ms) {
}

void AdaptiveVideoSource::SetBitrateBps(int bitrate_bps) {
  bits_per_second_ = std::min(bitrate_bps, 2500000);
  frame_size_bytes_ = (bits_per_second_ / 8 * frame_period_ms_ + 500) / 1000;
}

PeriodicKeyFrameSource::PeriodicKeyFrameSource(int flow_id,
                                               float fps,
                                               uint32_t kbps,
                                               uint32_t ssrc,
                                               int64_t first_frame_offset_ms,
                                               int key_frame_interval)
    : AdaptiveVideoSource(flow_id, fps, kbps, ssrc, first_frame_offset_ms),
      key_frame_interval_(key_frame_interval),
      frame_counter_(0),
      compensation_bytes_(0),
      compensation_per_frame_(0) {
}

uint32_t PeriodicKeyFrameSource::NextFrameSize() {
  uint32_t payload_size = frame_size_bytes_;
  if (frame_counter_ == 0) {
    payload_size = kMaxPayloadSizeBytes * 12;
    compensation_bytes_ = 4 * frame_size_bytes_;
    compensation_per_frame_ = compensation_bytes_ / 30;
  } else if (key_frame_interval_ > 0 &&
             (frame_counter_ % key_frame_interval_ == 0)) {
    payload_size *= 5;
    compensation_bytes_ = payload_size - frame_size_bytes_;
    compensation_per_frame_ = compensation_bytes_ / 30;
  } else if (compensation_bytes_ > 0) {
    if (compensation_per_frame_ > static_cast<int>(payload_size)) {
      // Skip this frame.
      compensation_bytes_ -= payload_size;
      payload_size = 0;
    } else {
      payload_size -= compensation_per_frame_;
      compensation_bytes_ -= compensation_per_frame_;
    }
  }
  if (compensation_bytes_ < 0)
    compensation_bytes_ = 0;
  ++frame_counter_;
  return payload_size;
}

uint32_t PeriodicKeyFrameSource::NextPacketSize(uint32_t frame_size,
                                                uint32_t remaining_payload) {
  uint32_t fragments =
      (frame_size + (kMaxPayloadSizeBytes - 1)) / kMaxPayloadSizeBytes;
  uint32_t avg_size = (frame_size + fragments - 1) / fragments;
  return std::min(avg_size, remaining_payload);
}

RembSendSideBwe::RembSendSideBwe(int kbps,
                                 BitrateObserver* observer,
                                 Clock* clock)
    : bitrate_controller_(
          BitrateController::CreateBitrateController(clock, false)),
      feedback_observer_(bitrate_controller_->CreateRtcpBandwidthObserver()),
      clock_(clock) {
  assert(kbps >= kMinBitrateKbps);
  assert(kbps <= kMaxBitrateKbps);
  bitrate_controller_->SetBitrateObserver(
      observer, 1000 * kbps, 1000 * kMinBitrateKbps, 1000 * kMaxBitrateKbps);
}

RembSendSideBwe::~RembSendSideBwe() {
}

void RembSendSideBwe::GiveFeedback(const FeedbackPacket& feedback) {
  const RembFeedback& remb_feedback =
      static_cast<const RembFeedback&>(feedback);
  feedback_observer_->OnReceivedEstimatedBitrate(remb_feedback.estimated_bps());
  ReportBlockList report_blocks;
  report_blocks.push_back(remb_feedback.report_block());
  feedback_observer_->OnReceivedRtcpReceiverReport(
      report_blocks, 0, clock_->TimeInMilliseconds());
  bitrate_controller_->Process();
}

int64_t RembSendSideBwe::TimeUntilNextProcess() {
  return bitrate_controller_->TimeUntilNextProcess();
}

int RembSendSideBwe::Process() {
  return bitrate_controller_->Process();
}

FullSendSideBwe::FullSendSideBwe(int kbps,
                                 BitrateObserver* observer,
                                 Clock* clock)
    : bitrate_controller_(
          BitrateController::CreateBitrateController(clock, false)),
      rbe_(AbsoluteSendTimeRemoteBitrateEstimatorFactory()
               .Create(this, clock, kAimdControl, 1000 * kMinBitrateKbps)),
      feedback_observer_(bitrate_controller_->CreateRtcpBandwidthObserver()),
      clock_(clock) {
  assert(kbps >= kMinBitrateKbps);
  assert(kbps <= kMaxBitrateKbps);
  bitrate_controller_->SetBitrateObserver(
      observer, 1000 * kbps, 1000 * kMinBitrateKbps, 1000 * kMaxBitrateKbps);
}

FullSendSideBwe::~FullSendSideBwe() {
}

void FullSendSideBwe::GiveFeedback(const FeedbackPacket& feedback) {
  const SendSideBweFeedback& fb =
      static_cast<const SendSideBweFeedback&>(feedback);
  if (fb.packet_feedback_vector().empty())
    return;
  rbe_->IncomingPacketFeedbackVector(fb.packet_feedback_vector());
  // TODO(holmer): Handle losses in between feedback packets.
  int expected_packets = fb.packet_feedback_vector().back().sequence_number -
                         fb.packet_feedback_vector().front().sequence_number +
                         1;
  // Assuming no reordering for now.
  if (expected_packets <= 0)
    return;
  int lost_packets = expected_packets - fb.packet_feedback_vector().size();
  report_block_.fractionLost = (lost_packets << 8) / expected_packets;
  report_block_.cumulativeLost += lost_packets;
  ReportBlockList report_blocks;
  report_blocks.push_back(report_block_);
  feedback_observer_->OnReceivedRtcpReceiverReport(
      report_blocks, 0, clock_->TimeInMilliseconds());
  bitrate_controller_->Process();
}

void FullSendSideBwe::OnReceiveBitrateChanged(
    const std::vector<unsigned int>& ssrcs,
    unsigned int bitrate) {
  feedback_observer_->OnReceivedEstimatedBitrate(bitrate);
}

int64_t FullSendSideBwe::TimeUntilNextProcess() {
  return bitrate_controller_->TimeUntilNextProcess();
}

int FullSendSideBwe::Process() {
  rbe_->Process();
  return bitrate_controller_->Process();
}

SendSideBwe* CreateEstimator(BandwidthEstimatorType estimator,
                             int kbps,
                             BitrateObserver* observer,
                             Clock* clock) {
  switch (estimator) {
    case kRembEstimator:
      return new RembSendSideBwe(kbps, observer, clock);
    case kFullSendSideEstimator:
      return new FullSendSideBwe(kbps, observer, clock);
    case kNullEstimator:
      return new NullSendSideBwe();
  }
  assert(false);
  return NULL;
}

PacketSender::PacketSender(PacketProcessorListener* listener,
                           VideoSource* source,
                           BandwidthEstimatorType estimator)
    : PacketProcessor(listener, source->flow_id(), kSender),
      // For Packet::send_time_us() to be comparable with timestamps from
      // clock_, the clock of the PacketSender and the Source must be aligned.
      // We assume that both start at time 0.
      clock_(0),
      source_(source),
      bwe_(CreateEstimator(estimator,
                           source_->bits_per_second() / 1000,
                           this,
                           &clock_)) {
  modules_.push_back(bwe_.get());
}

PacketSender::~PacketSender() {
}

void PacketSender::RunFor(int64_t time_ms, Packets* in_out) {
  int64_t now_ms = clock_.TimeInMilliseconds();
  std::list<FeedbackPacket*> feedbacks =
      GetFeedbackPackets(in_out, now_ms + time_ms);
  ProcessFeedbackAndGeneratePackets(time_ms, &feedbacks, in_out);
}

void PacketSender::ProcessFeedbackAndGeneratePackets(
    int64_t time_ms,
    std::list<FeedbackPacket*>* feedbacks,
    Packets* generated) {
  do {
    // Make sure to at least run Process() below every 100 ms.
    int64_t time_to_run_ms = std::min<int64_t>(time_ms, 100);
    if (!feedbacks->empty()) {
      int64_t time_until_feedback_ms =
          feedbacks->front()->send_time_us() / 1000 -
          clock_.TimeInMilliseconds();
      time_to_run_ms =
          std::max<int64_t>(std::min(time_ms, time_until_feedback_ms), 0);
    }
    source_->RunFor(time_to_run_ms, generated);
    clock_.AdvanceTimeMilliseconds(time_to_run_ms);
    if (!feedbacks->empty()) {
      bwe_->GiveFeedback(*feedbacks->front());
      delete feedbacks->front();
      feedbacks->pop_front();
    }
    bwe_->Process();
    time_ms -= time_to_run_ms;
  } while (time_ms > 0);
  assert(feedbacks->empty());
}

int PacketSender::GetFeedbackIntervalMs() const {
  return bwe_->GetFeedbackIntervalMs();
}

std::list<FeedbackPacket*> PacketSender::GetFeedbackPackets(
    Packets* in_out,
    int64_t end_time_ms) {
  std::list<FeedbackPacket*> fb_packets;
  for (auto it = in_out->begin(); it != in_out->end();) {
    if ((*it)->send_time_us() > 1000 * end_time_ms)
      break;
    if ((*it)->GetPacketType() == Packet::kFeedback &&
        source()->flow_id() == (*it)->flow_id()) {
      fb_packets.push_back(static_cast<FeedbackPacket*>(*it));
      it = in_out->erase(it);
    } else {
      ++it;
    }
  }
  return fb_packets;
}

void PacketSender::OnNetworkChanged(uint32_t target_bitrate_bps,
                                    uint8_t fraction_lost,
                                    int64_t rtt) {
  source_->SetBitrateBps(target_bitrate_bps);
  std::stringstream ss;
  ss << "SendEstimate_" << source_->flow_id() << "#1";
  BWE_TEST_LOGGING_PLOT(ss.str(), clock_.TimeInMilliseconds(),
                        target_bitrate_bps / 1000);
}

PacedVideoSender::PacedVideoSender(PacketProcessorListener* listener,
                                   VideoSource* source,
                                   BandwidthEstimatorType estimator)
    : PacketSender(listener, source, estimator),
      pacer_(&clock_,
             this,
             source->bits_per_second() / 1000,
             PacedSender::kDefaultPaceMultiplier * source->bits_per_second() /
                 1000,
             0) {
  modules_.push_back(&pacer_);
}

PacedVideoSender::~PacedVideoSender() {
  for (Packet* packet : pacer_queue_)
    delete packet;
  for (Packet* packet : queue_)
    delete packet;
}

void PacedVideoSender::RunFor(int64_t time_ms, Packets* in_out) {
  int64_t end_time_ms = clock_.TimeInMilliseconds() + time_ms;
  // Run process periodically to allow the packets to be paced out.
  std::list<FeedbackPacket*> feedbacks =
      GetFeedbackPackets(in_out, end_time_ms);
  int64_t last_run_time_ms = -1;
  do {
    int64_t time_until_process_ms = TimeUntilNextProcess(modules_);
    int64_t time_until_feedback_ms = time_ms;
    if (!feedbacks.empty())
      time_until_feedback_ms = feedbacks.front()->send_time_us() / 1000 -
                               clock_.TimeInMilliseconds();

    int64_t time_until_next_event_ms =
        std::min(time_until_feedback_ms, time_until_process_ms);

    time_until_next_event_ms =
        std::min(source_->GetTimeUntilNextFrameMs(), time_until_next_event_ms);

    // Never run for longer than we have been asked for.
    if (clock_.TimeInMilliseconds() + time_until_next_event_ms > end_time_ms)
      time_until_next_event_ms = end_time_ms - clock_.TimeInMilliseconds();

    // Make sure we don't get stuck if an event doesn't trigger. This typically
    // happens if the prober wants to probe, but there's no packet to send.
    if (time_until_next_event_ms == 0 && last_run_time_ms == 0)
      time_until_next_event_ms = 1;
    last_run_time_ms = time_until_next_event_ms;

    Packets generated_packets;
    source_->RunFor(time_until_next_event_ms, &generated_packets);
    if (!generated_packets.empty()) {
      for (Packet* packet : generated_packets) {
        MediaPacket* media_packet = static_cast<MediaPacket*>(packet);
        pacer_.SendPacket(PacedSender::kNormalPriority,
                          media_packet->header().ssrc,
                          media_packet->header().sequenceNumber,
                          (media_packet->send_time_us() + 500) / 1000,
                          media_packet->payload_size(), false);
        pacer_queue_.push_back(packet);
        assert(pacer_queue_.size() < 10000);
      }
    }

    clock_.AdvanceTimeMilliseconds(time_until_next_event_ms);

    if (time_until_next_event_ms == time_until_feedback_ms) {
      if (!feedbacks.empty()) {
        bwe_->GiveFeedback(*feedbacks.front());
        delete feedbacks.front();
        feedbacks.pop_front();
      }
      bwe_->Process();
    }

    if (time_until_next_event_ms == time_until_process_ms) {
      CallProcess(modules_);
    }
  } while (clock_.TimeInMilliseconds() < end_time_ms);
  QueuePackets(in_out, end_time_ms * 1000);
}

int64_t PacedVideoSender::TimeUntilNextProcess(
    const std::list<Module*>& modules) {
  int64_t time_until_next_process_ms = 10;
  for (Module* module : modules) {
    int64_t next_process_ms = module->TimeUntilNextProcess();
    if (next_process_ms < time_until_next_process_ms)
      time_until_next_process_ms = next_process_ms;
  }
  if (time_until_next_process_ms < 0)
    time_until_next_process_ms = 0;
  return time_until_next_process_ms;
}

void PacedVideoSender::CallProcess(const std::list<Module*>& modules) {
  for (Module* module : modules) {
    if (module->TimeUntilNextProcess() <= 0) {
      module->Process();
    }
  }
}

void PacedVideoSender::QueuePackets(Packets* batch,
                                    int64_t end_of_batch_time_us) {
  queue_.merge(*batch, DereferencingComparator<Packet>);
  if (queue_.empty()) {
    return;
  }
  Packets::iterator it = queue_.begin();
  for (; it != queue_.end(); ++it) {
    if ((*it)->send_time_us() > end_of_batch_time_us) {
      break;
    }
  }
  Packets to_transfer;
  to_transfer.splice(to_transfer.begin(), queue_, queue_.begin(), it);
  batch->merge(to_transfer, DereferencingComparator<Packet>);
}

bool PacedVideoSender::TimeToSendPacket(uint32_t ssrc,
                                        uint16_t sequence_number,
                                        int64_t capture_time_ms,
                                        bool retransmission) {
  for (Packets::iterator it = pacer_queue_.begin(); it != pacer_queue_.end();
       ++it) {
    MediaPacket* media_packet = static_cast<MediaPacket*>(*it);
    if (media_packet->header().sequenceNumber == sequence_number) {
      int64_t pace_out_time_ms = clock_.TimeInMilliseconds();
      // Make sure a packet is never paced out earlier than when it was put into
      // the pacer.
      assert(pace_out_time_ms >= (media_packet->send_time_us() + 500) / 1000);
      media_packet->SetAbsSendTimeMs(pace_out_time_ms);
      media_packet->set_send_time_us(1000 * pace_out_time_ms);
      queue_.push_back(media_packet);
      pacer_queue_.erase(it);
      return true;
    }
  }
  return false;
}

size_t PacedVideoSender::TimeToSendPadding(size_t bytes) {
  return 0;
}

void PacedVideoSender::OnNetworkChanged(uint32_t target_bitrate_bps,
                                        uint8_t fraction_lost,
                                        int64_t rtt) {
  PacketSender::OnNetworkChanged(target_bitrate_bps, fraction_lost, rtt);
  pacer_.UpdateBitrate(
      target_bitrate_bps / 1000,
      PacedSender::kDefaultPaceMultiplier * target_bitrate_bps / 1000, 0);
}
}  // namespace bwe
}  // namespace testing
}  // namespace webrtc
