/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <assert.h>
#include <math.h>
#include <string.h>

#include <algorithm>
#include <cmath>
#include <utility>

#include "call/call.h"
#include "call/fake_network_pipe.h"
#include "modules/rtp_rtcp/include/rtp_header_parser.h"
#include "rtc_base/logging.h"
#include "system_wrappers/include/clock.h"

namespace webrtc {

namespace {
constexpr int64_t kDefaultProcessIntervalMs = 5;
struct PacketArrivalTimeComparator {
  bool operator()(const NetworkPacket& p1, const NetworkPacket& p2) {
    return p1.arrival_time() < p2.arrival_time();
  }
};
}  // namespace

NetworkPacket::NetworkPacket(rtc::CopyOnWriteBuffer packet,
                             int64_t send_time,
                             int64_t arrival_time,
                             rtc::Optional<PacketOptions> packet_options,
                             bool is_rtcp,
                             MediaType media_type,
                             rtc::Optional<PacketTime> packet_time)
    : packet_(std::move(packet)),
      send_time_(send_time),
      arrival_time_(arrival_time),
      packet_options_(packet_options),
      is_rtcp_(is_rtcp),
      media_type_(media_type),
      packet_time_(packet_time) {}

NetworkPacket::NetworkPacket(NetworkPacket&& o)
    : packet_(std::move(o.packet_)),
      send_time_(o.send_time_),
      arrival_time_(o.arrival_time_),
      packet_options_(o.packet_options_),
      is_rtcp_(o.is_rtcp_),
      media_type_(o.media_type_),
      packet_time_(o.packet_time_) {}

NetworkPacket& NetworkPacket::operator=(NetworkPacket&& o) {
  packet_ = std::move(o.packet_);
  send_time_ = o.send_time_;
  arrival_time_ = o.arrival_time_;
  packet_options_ = o.packet_options_;
  is_rtcp_ = o.is_rtcp_;
  media_type_ = o.media_type_;
  packet_time_ = o.packet_time_;

  return *this;
}

DemuxerImpl::DemuxerImpl(const std::map<uint8_t, MediaType>& payload_type_map)
    : packet_receiver_(nullptr), payload_type_map_(payload_type_map) {}

void DemuxerImpl::SetReceiver(PacketReceiver* receiver) {
  packet_receiver_ = receiver;
}

void DemuxerImpl::DeliverPacket(const NetworkPacket* packet,
                                const PacketTime& packet_time) {
  // No packet receiver means that this demuxer will terminate the flow of
  // packets.
  if (!packet_receiver_)
    return;
  const uint8_t* const packet_data = packet->data();
  const size_t packet_length = packet->data_length();
  MediaType media_type = MediaType::ANY;
  if (!RtpHeaderParser::IsRtcp(packet_data, packet_length)) {
    RTC_CHECK_GE(packet_length, 2);
    const uint8_t payload_type = packet_data[1] & 0x7f;
    std::map<uint8_t, MediaType>::const_iterator it =
        payload_type_map_.find(payload_type);
    RTC_CHECK(it != payload_type_map_.end())
        << "payload type " << static_cast<int>(payload_type) << " unknown.";
    media_type = it->second;
  }
  packet_receiver_->DeliverPacket(
      media_type, rtc::CopyOnWriteBuffer(packet_data, packet_length),
      packet_time);
}

FakeNetworkPipe::FakeNetworkPipe(Clock* clock,
                                 const FakeNetworkPipe::Config& config)
    : FakeNetworkPipe(clock, config, nullptr, 1) {}

FakeNetworkPipe::FakeNetworkPipe(Clock* clock,
                                 const FakeNetworkPipe::Config& config,
                                 std::unique_ptr<Demuxer> demuxer)
    : FakeNetworkPipe(clock, config, std::move(demuxer), 1) {}

FakeNetworkPipe::FakeNetworkPipe(Clock* clock,
                                 const FakeNetworkPipe::Config& config,
                                 std::unique_ptr<Demuxer> demuxer,
                                 uint64_t seed)
    : clock_(clock),
      demuxer_(std::move(demuxer)),
      receiver_(nullptr),
      transport_(nullptr),
      random_(seed),
      config_(),
      dropped_packets_(0),
      sent_packets_(0),
      total_packet_delay_(0),
      bursting_(false),
      next_process_time_(clock_->TimeInMilliseconds()),
      last_log_time_(clock_->TimeInMilliseconds()) {
  SetConfig(config);
}

FakeNetworkPipe::FakeNetworkPipe(Clock* clock,
                                 const FakeNetworkPipe::Config& config,
                                 Transport* transport)
    : clock_(clock),
      receiver_(nullptr),
      transport_(transport),
      random_(1),
      config_(),
      dropped_packets_(0),
      sent_packets_(0),
      total_packet_delay_(0),
      bursting_(false),
      next_process_time_(clock_->TimeInMilliseconds()),
      last_log_time_(clock_->TimeInMilliseconds()) {
  SetConfig(config);
}

FakeNetworkPipe::~FakeNetworkPipe() = default;

void FakeNetworkPipe::SetReceiver(PacketReceiver* receiver) {
  rtc::CritScope crit(&config_lock_);
  if (demuxer_)
    demuxer_->SetReceiver(receiver);
  receiver_ = receiver;
}

bool FakeNetworkPipe::SendRtp(const uint8_t* packet,
                              size_t length,
                              const PacketOptions& options) {
  RTC_DCHECK(HasTransport());
  EnqueuePacket(rtc::CopyOnWriteBuffer(packet, length), options, false,
                MediaType::ANY, rtc::nullopt);
  return true;
}

bool FakeNetworkPipe::SendRtcp(const uint8_t* packet, size_t length) {
  RTC_DCHECK(HasTransport());
  EnqueuePacket(rtc::CopyOnWriteBuffer(packet, length), rtc::nullopt, true,
                MediaType::ANY, rtc::nullopt);
  return true;
}

PacketReceiver::DeliveryStatus FakeNetworkPipe::DeliverPacket(
    MediaType media_type,
    rtc::CopyOnWriteBuffer packet,
    const PacketTime& packet_time) {
  return EnqueuePacket(std::move(packet), rtc::nullopt, false, media_type,
                       packet_time)
             ? PacketReceiver::DELIVERY_OK
             : PacketReceiver::DELIVERY_PACKET_ERROR;
}

void FakeNetworkPipe::SetConfig(const FakeNetworkPipe::Config& config) {
  rtc::CritScope crit(&config_lock_);
  config_ = config;  // Shallow copy of the struct.
  double prob_loss = config.loss_percent / 100.0;
  if (config_.avg_burst_loss_length == -1) {
    // Uniform loss
    prob_loss_bursting_ = prob_loss;
    prob_start_bursting_ = prob_loss;
  } else {
    // Lose packets according to a gilbert-elliot model.
    int avg_burst_loss_length = config.avg_burst_loss_length;
    int min_avg_burst_loss_length = std::ceil(prob_loss / (1 - prob_loss));

    RTC_CHECK_GT(avg_burst_loss_length, min_avg_burst_loss_length)
        << "For a total packet loss of " << config.loss_percent << "%% then"
        << " avg_burst_loss_length must be " << min_avg_burst_loss_length + 1
        << " or higher.";

    prob_loss_bursting_ = (1.0 - 1.0 / avg_burst_loss_length);
    prob_start_bursting_ = prob_loss / (1 - prob_loss) / avg_burst_loss_length;
  }
}

void FakeNetworkPipe::SendPacket(const uint8_t* data, size_t data_length) {
  RTC_DCHECK(HasDemuxer());
  EnqueuePacket(rtc::CopyOnWriteBuffer(data, data_length), rtc::nullopt, false,
                MediaType::ANY, rtc::nullopt);
}

bool FakeNetworkPipe::EnqueuePacket(rtc::CopyOnWriteBuffer packet,
                                    rtc::Optional<PacketOptions> options,
                                    bool is_rtcp,
                                    MediaType media_type,
                                    rtc::Optional<PacketTime> packet_time) {
  Config config;
  {
    rtc::CritScope crit(&config_lock_);
    config = config_;
  }
  rtc::CritScope crit(&process_lock_);
  if (config.queue_length_packets > 0 &&
      capacity_link_.size() >= config.queue_length_packets) {
    // Too many packet on the link, drop this one.
    ++dropped_packets_;
    return false;
  }

  int64_t time_now = clock_->TimeInMilliseconds();

  // Delay introduced by the link capacity.
  int64_t capacity_delay_ms = 0;
  if (config.link_capacity_kbps > 0) {
    const int bytes_per_millisecond = config.link_capacity_kbps / 8;
    // To round to the closest millisecond we add half a milliseconds worth of
    // bytes to the delay calculation.
    capacity_delay_ms = (packet.size() + capacity_delay_error_bytes_ +
                         bytes_per_millisecond / 2) /
                        bytes_per_millisecond;
    capacity_delay_error_bytes_ +=
        packet.size() - capacity_delay_ms * bytes_per_millisecond;
  }
  int64_t network_start_time = time_now;

  // Check if there already are packets on the link and change network start
  // time forward if there is.
  if (!capacity_link_.empty() &&
      network_start_time < capacity_link_.back().arrival_time())
    network_start_time = capacity_link_.back().arrival_time();

  int64_t arrival_time = network_start_time + capacity_delay_ms;
  capacity_link_.emplace(std::move(packet), time_now, arrival_time, options,
                         is_rtcp, media_type, packet_time);
  return true;
}

float FakeNetworkPipe::PercentageLoss() {
  rtc::CritScope crit(&process_lock_);
  if (sent_packets_ == 0)
    return 0;

  return static_cast<float>(dropped_packets_) /
         (sent_packets_ + dropped_packets_);
}

int FakeNetworkPipe::AverageDelay() {
  rtc::CritScope crit(&process_lock_);
  if (sent_packets_ == 0)
    return 0;

  return static_cast<int>(total_packet_delay_ /
                          static_cast<int64_t>(sent_packets_));
}

size_t FakeNetworkPipe::DroppedPackets() {
  rtc::CritScope crit(&process_lock_);
  return dropped_packets_;
}

size_t FakeNetworkPipe::SentPackets() {
  rtc::CritScope crit(&process_lock_);
  return sent_packets_;
}

void FakeNetworkPipe::Process() {
  int64_t time_now = clock_->TimeInMilliseconds();
  std::queue<NetworkPacket> packets_to_deliver;
  Config config;
  double prob_loss_bursting;
  double prob_start_bursting;
  {
    rtc::CritScope crit(&config_lock_);
    config = config_;
    prob_loss_bursting = prob_loss_bursting_;
    prob_start_bursting = prob_start_bursting_;
  }
  {
    rtc::CritScope crit(&process_lock_);
    if (time_now - last_log_time_ > 5000) {
      int64_t queueing_delay_ms = 0;
      if (!capacity_link_.empty()) {
        queueing_delay_ms = time_now - capacity_link_.front().send_time();
      }
      RTC_LOG(LS_INFO) << "Network queue: " << queueing_delay_ms << " ms.";
      last_log_time_ = time_now;
    }

    // Check the capacity link first.
    if (!capacity_link_.empty()) {
      int64_t last_arrival_time =
          delay_link_.empty() ? -1 : delay_link_.back().arrival_time();
      bool needs_sort = false;
      while (!capacity_link_.empty() &&
             time_now >= capacity_link_.front().arrival_time()) {
        // Time to get this packet.
        NetworkPacket packet = std::move(capacity_link_.front());
        capacity_link_.pop();

        // Drop packets at an average rate of |config_.loss_percent| with
        // and average loss burst length of |config_.avg_burst_loss_length|.
        if ((bursting_ && random_.Rand<double>() < prob_loss_bursting) ||
            (!bursting_ && random_.Rand<double>() < prob_start_bursting)) {
          bursting_ = true;
          continue;
        } else {
          bursting_ = false;
        }

        int arrival_time_jitter = random_.Gaussian(
            config.queue_delay_ms, config.delay_standard_deviation_ms);

        // If reordering is not allowed then adjust arrival_time_jitter
        // to make sure all packets are sent in order.
        if (!config.allow_reordering && !delay_link_.empty() &&
            packet.arrival_time() + arrival_time_jitter < last_arrival_time) {
          arrival_time_jitter = last_arrival_time - packet.arrival_time();
        }
        packet.IncrementArrivalTime(arrival_time_jitter);
        if (packet.arrival_time() >= last_arrival_time) {
          last_arrival_time = packet.arrival_time();
        } else {
          needs_sort = true;
        }
        delay_link_.emplace_back(std::move(packet));
      }

      if (needs_sort) {
        // Packet(s) arrived out of order, make sure list is sorted.
        std::sort(delay_link_.begin(), delay_link_.end(),
                  PacketArrivalTimeComparator());
      }
    }

    // Check the extra delay queue.
    while (!delay_link_.empty() &&
           time_now >= delay_link_.front().arrival_time()) {
      // Deliver this packet.
      NetworkPacket packet(std::move(delay_link_.front()));
      delay_link_.pop_front();
      // |time_now| might be later than when the packet should have arrived, due
      // to NetworkProcess being called too late. For stats, use the time it
      // should have been on the link.
      total_packet_delay_ += packet.arrival_time() - packet.send_time();
      packets_to_deliver.push(std::move(packet));
    }
    sent_packets_ += packets_to_deliver.size();
  }

  rtc::CritScope crit(&config_lock_);
  while (!packets_to_deliver.empty()) {
    NetworkPacket packet = std::move(packets_to_deliver.front());
    packets_to_deliver.pop();
    DeliverPacket(&packet);
  }

  next_process_time_ = !delay_link_.empty()
                           ? delay_link_.begin()->arrival_time()
                           : time_now + kDefaultProcessIntervalMs;
}

void FakeNetworkPipe::DeliverPacket(NetworkPacket* packet) {
  if (demuxer_) {
    demuxer_->DeliverPacket(packet, PacketTime());
  } else if (transport_) {
    if (packet->is_rtcp()) {
      transport_->SendRtcp(packet->data(), packet->data_length());
    } else {
      transport_->SendRtp(packet->data(), packet->data_length(),
                          packet->packet_options());
    }
  } else if (receiver_) {
    PacketTime packet_time = packet->packet_time();
    if (packet_time.timestamp != -1) {
      int64_t queue_time = packet->arrival_time() - packet->send_time();
      RTC_CHECK(queue_time >= 0);
      packet_time.timestamp += (queue_time * 1000);
    }
    receiver_->DeliverPacket(packet->media_type(),
                             std::move(*packet->raw_packet()), packet_time);
  }
}

int64_t FakeNetworkPipe::TimeUntilNextProcess() {
  rtc::CritScope crit(&process_lock_);
  return std::max<int64_t>(next_process_time_ - clock_->TimeInMilliseconds(),
                           0);
}

bool FakeNetworkPipe::HasTransport() const {
  rtc::CritScope crit(&config_lock_);
  return transport_ != nullptr;
}

bool FakeNetworkPipe::HasDemuxer() const {
  rtc::CritScope crit(&config_lock_);
  return demuxer_ != nullptr;
}

void FakeNetworkPipe::DeliverPacketWithLock(NetworkPacket* packet) {
  rtc::CritScope crit(&config_lock_);
  DeliverPacket(packet);
}

void FakeNetworkPipe::ResetStats() {
  rtc::CritScope crit(&process_lock_);
  dropped_packets_ = 0;
  sent_packets_ = 0;
  total_packet_delay_ = 0;
}

int FakeNetworkPipe::GetConfigCapacityKbps() const {
  rtc::CritScope crit(&config_lock_);
  return config_.link_capacity_kbps;
}

void FakeNetworkPipe::AddToPacketDropCount() {
  rtc::CritScope crit(&process_lock_);
  ++dropped_packets_;
}

void FakeNetworkPipe::AddToPacketSentCount(int count) {
  rtc::CritScope crit(&process_lock_);
  sent_packets_ += count;
}

void FakeNetworkPipe::AddToTotalDelay(int delay_ms) {
  rtc::CritScope crit(&process_lock_);
  total_packet_delay_ += delay_ms;
}

int64_t FakeNetworkPipe::GetTimeInMilliseconds() const {
  return clock_->TimeInMilliseconds();
}

bool FakeNetworkPipe::IsRandomLoss(double prob_loss) {
  return random_.Rand<double>() < prob_loss;
}

bool FakeNetworkPipe::ShouldProcess(int64_t time_now) const {
  return time_now >= next_process_time_;
}

void FakeNetworkPipe::SetTimeToNextProcess(int64_t skip_ms) {
  next_process_time_ += skip_ms;
}

}  // namespace webrtc
