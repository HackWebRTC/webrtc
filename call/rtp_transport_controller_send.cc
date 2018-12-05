/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#include <utility>
#include <vector>

#include "absl/memory/memory.h"
#include "absl/types/optional.h"
#include "api/transport/goog_cc_factory.h"
#include "api/transport/network_types.h"
#include "api/units/data_rate.h"
#include "api/units/time_delta.h"
#include "api/units/timestamp.h"
#include "call/rtp_transport_controller_send.h"
#include "call/rtp_video_sender.h"
#include "rtc_base/checks.h"
#include "rtc_base/location.h"
#include "rtc_base/logging.h"
#include "rtc_base/rate_limiter.h"
#include "system_wrappers/include/field_trial.h"

namespace webrtc {
class RtpTransportControllerSend::PeriodicTask : public rtc::QueuedTask {
 public:
  virtual void Stop() = 0;
};

namespace {
static const int64_t kRetransmitWindowSizeMs = 500;
static const size_t kMaxOverheadBytes = 500;

const int64_t PacerQueueUpdateIntervalMs = 25;

TargetRateConstraints ConvertConstraints(int min_bitrate_bps,
                                         int max_bitrate_bps,
                                         int start_bitrate_bps,
                                         const Clock* clock) {
  TargetRateConstraints msg;
  msg.at_time = Timestamp::ms(clock->TimeInMilliseconds());
  msg.min_data_rate =
      min_bitrate_bps >= 0 ? DataRate::bps(min_bitrate_bps) : DataRate::Zero();
  msg.max_data_rate = max_bitrate_bps > 0 ? DataRate::bps(max_bitrate_bps)
                                          : DataRate::Infinity();
  if (start_bitrate_bps > 0)
    msg.starting_rate = DataRate::bps(start_bitrate_bps);
  return msg;
}

TargetRateConstraints ConvertConstraints(const BitrateConstraints& contraints,
                                         const Clock* clock) {
  return ConvertConstraints(contraints.min_bitrate_bps,
                            contraints.max_bitrate_bps,
                            contraints.start_bitrate_bps, clock);
}

// The template closure pattern is based on rtc::ClosureTask.
template <class Closure>
class PeriodicTaskImpl final : public RtpTransportControllerSend::PeriodicTask {
 public:
  PeriodicTaskImpl(rtc::TaskQueue* task_queue,
                   int64_t period_ms,
                   Closure&& closure)
      : task_queue_(task_queue),
        period_ms_(period_ms),
        closure_(std::forward<Closure>(closure)) {}
  bool Run() override {
    if (!running_)
      return true;
    closure_();
    // absl::WrapUnique lets us repost this task on the TaskQueue.
    task_queue_->PostDelayedTask(absl::WrapUnique(this), period_ms_);
    // Return false to tell TaskQueue to not destruct this object, since we have
    // taken ownership with absl::WrapUnique.
    return false;
  }
  void Stop() override {
    if (task_queue_->IsCurrent()) {
      RTC_DCHECK(running_);
      running_ = false;
    } else {
      task_queue_->PostTask([this] { Stop(); });
    }
  }

 private:
  rtc::TaskQueue* const task_queue_;
  const int64_t period_ms_;
  typename std::remove_const<
      typename std::remove_reference<Closure>::type>::type closure_;
  bool running_ = true;
};

template <class Closure>
static RtpTransportControllerSend::PeriodicTask* StartPeriodicTask(
    rtc::TaskQueue* task_queue,
    int64_t period_ms,
    Closure&& closure) {
  auto periodic_task = absl::make_unique<PeriodicTaskImpl<Closure>>(
      task_queue, period_ms, std::forward<Closure>(closure));
  RtpTransportControllerSend::PeriodicTask* periodic_task_ptr =
      periodic_task.get();
  task_queue->PostDelayedTask(std::move(periodic_task), period_ms);
  return periodic_task_ptr;
}
}  // namespace

RtpTransportControllerSend::RtpTransportControllerSend(
    Clock* clock,
    webrtc::RtcEventLog* event_log,
    NetworkControllerFactoryInterface* controller_factory,
    const BitrateConstraints& bitrate_config)
    : clock_(clock),
      pacer_(clock, &packet_router_, event_log),
      bitrate_configurator_(bitrate_config),
      process_thread_(ProcessThread::Create("SendControllerThread")),
      observer_(nullptr),
      transport_feedback_adapter_(clock_),
      controller_factory_override_(controller_factory),
      controller_factory_fallback_(
          absl::make_unique<GoogCcNetworkControllerFactory>(event_log)),
      process_interval_(controller_factory_fallback_->GetProcessInterval()),
      last_report_block_time_(Timestamp::ms(clock_->TimeInMilliseconds())),
      reset_feedback_on_route_change_(
          !field_trial::IsEnabled("WebRTC-Bwe-NoFeedbackReset")),
      send_side_bwe_with_overhead_(
          webrtc::field_trial::IsEnabled("WebRTC-SendSideBwe-WithOverhead")),
      transport_overhead_bytes_per_packet_(0),
      network_available_(false),
      periodic_tasks_enabled_(true),
      packet_feedback_available_(false),
      pacer_queue_update_task_(nullptr),
      controller_task_(nullptr),
      retransmission_rate_limiter_(clock, kRetransmitWindowSizeMs),
      task_queue_("rtp_send_controller") {
  initial_config_.constraints = ConvertConstraints(bitrate_config, clock_);
  RTC_DCHECK(bitrate_config.start_bitrate_bps > 0);

  pacer_.SetEstimatedBitrate(bitrate_config.start_bitrate_bps);

  process_thread_->RegisterModule(&pacer_, RTC_FROM_HERE);
  process_thread_->Start();
}

RtpTransportControllerSend::~RtpTransportControllerSend() {
  process_thread_->Stop();
  process_thread_->DeRegisterModule(&pacer_);
}

RtpVideoSenderInterface* RtpTransportControllerSend::CreateRtpVideoSender(
    const std::vector<uint32_t>& ssrcs,
    std::map<uint32_t, RtpState> suspended_ssrcs,
    const std::map<uint32_t, RtpPayloadState>& states,
    const RtpConfig& rtp_config,
    int rtcp_report_interval_ms,
    Transport* send_transport,
    const RtpSenderObservers& observers,
    RtcEventLog* event_log,
    std::unique_ptr<FecController> fec_controller,
    const RtpSenderFrameEncryptionConfig& frame_encryption_config) {
  video_rtp_senders_.push_back(absl::make_unique<RtpVideoSender>(
      ssrcs, suspended_ssrcs, states, rtp_config, rtcp_report_interval_ms,
      send_transport, observers,
      // TODO(holmer): Remove this circular dependency by injecting
      // the parts of RtpTransportControllerSendInterface that are really used.
      this, event_log, &retransmission_rate_limiter_, std::move(fec_controller),
      frame_encryption_config.frame_encryptor,
      frame_encryption_config.crypto_options));
  return video_rtp_senders_.back().get();
}

void RtpTransportControllerSend::DestroyRtpVideoSender(
    RtpVideoSenderInterface* rtp_video_sender) {
  std::vector<std::unique_ptr<RtpVideoSenderInterface>>::iterator it =
      video_rtp_senders_.end();
  for (it = video_rtp_senders_.begin(); it != video_rtp_senders_.end(); ++it) {
    if (it->get() == rtp_video_sender) {
      break;
    }
  }
  RTC_DCHECK(it != video_rtp_senders_.end());
  video_rtp_senders_.erase(it);
}

void RtpTransportControllerSend::OnNetworkChanged(uint32_t bitrate_bps,
                                                  uint8_t fraction_loss,
                                                  int64_t rtt_ms,
                                                  int64_t probing_interval_ms) {
  TargetTransferRate msg;
  msg.at_time = Timestamp::ms(clock_->TimeInMilliseconds());
  msg.target_rate = DataRate::bps(bitrate_bps);
  // TODO(srte): Remove this interface and push information about bandwidth
  // estimation to users of this class, thereby reducing synchronous calls.
  RTC_DCHECK_RUN_ON(&task_queue_);
  RTC_DCHECK(control_handler_->last_transfer_rate().has_value());
  msg.network_estimate =
      control_handler_->last_transfer_rate()->network_estimate;

  retransmission_rate_limiter_.SetMaxRate(msg.network_estimate.bandwidth.bps());

  // We won't register as observer until we have an observers.
  RTC_DCHECK(observer_ != nullptr);
  observer_->OnTargetTransferRate(msg);
}

rtc::TaskQueue* RtpTransportControllerSend::GetWorkerQueue() {
  return &task_queue_;
}

PacketRouter* RtpTransportControllerSend::packet_router() {
  return &packet_router_;
}

TransportFeedbackObserver*
RtpTransportControllerSend::transport_feedback_observer() {
  return this;
}

RtpPacketSender* RtpTransportControllerSend::packet_sender() {
  return &pacer_;
}

const RtpKeepAliveConfig& RtpTransportControllerSend::keepalive_config() const {
  return keepalive_;
}

void RtpTransportControllerSend::SetAllocatedSendBitrateLimits(
    int min_send_bitrate_bps,
    int max_padding_bitrate_bps,
    int max_total_bitrate_bps) {
  RTC_DCHECK_RUN_ON(&task_queue_);
  streams_config_.min_pacing_rate = DataRate::bps(min_send_bitrate_bps);
  streams_config_.max_padding_rate = DataRate::bps(max_padding_bitrate_bps);
  streams_config_.max_total_allocated_bitrate =
      DataRate::bps(max_total_bitrate_bps);
  UpdateStreamsConfig();
}

void RtpTransportControllerSend::SetKeepAliveConfig(
    const RtpKeepAliveConfig& config) {
  keepalive_ = config;
}
void RtpTransportControllerSend::SetPacingFactor(float pacing_factor) {
  RTC_DCHECK_RUN_ON(&task_queue_);
  streams_config_.pacing_factor = pacing_factor;
  UpdateStreamsConfig();
}
void RtpTransportControllerSend::SetQueueTimeLimit(int limit_ms) {
  pacer_.SetQueueTimeLimit(limit_ms);
}
CallStatsObserver* RtpTransportControllerSend::GetCallStatsObserver() {
  return this;
}
void RtpTransportControllerSend::RegisterPacketFeedbackObserver(
    PacketFeedbackObserver* observer) {
  transport_feedback_adapter_.RegisterPacketFeedbackObserver(observer);
}
void RtpTransportControllerSend::DeRegisterPacketFeedbackObserver(
    PacketFeedbackObserver* observer) {
  transport_feedback_adapter_.DeRegisterPacketFeedbackObserver(observer);
}

void RtpTransportControllerSend::RegisterTargetTransferRateObserver(
    TargetTransferRateObserver* observer) {
  task_queue_.PostTask([this, observer] {
    RTC_DCHECK_RUN_ON(&task_queue_);
    RTC_DCHECK(observer_ == nullptr);
    observer_ = observer;
    MaybeCreateControllers();
  });
}
void RtpTransportControllerSend::OnNetworkRouteChanged(
    const std::string& transport_name,
    const rtc::NetworkRoute& network_route) {
  // Check if the network route is connected.
  if (!network_route.connected) {
    RTC_LOG(LS_INFO) << "Transport " << transport_name << " is disconnected";
    // TODO(honghaiz): Perhaps handle this in SignalChannelNetworkState and
    // consider merging these two methods.
    return;
  }

  // Check whether the network route has changed on each transport.
  auto result =
      network_routes_.insert(std::make_pair(transport_name, network_route));
  auto kv = result.first;
  bool inserted = result.second;
  if (inserted) {
    // No need to reset BWE if this is the first time the network connects.
    return;
  }
  if (kv->second.connected != network_route.connected ||
      kv->second.local_network_id != network_route.local_network_id ||
      kv->second.remote_network_id != network_route.remote_network_id) {
    kv->second = network_route;
    BitrateConstraints bitrate_config = bitrate_configurator_.GetConfig();
    RTC_LOG(LS_INFO) << "Network route changed on transport " << transport_name
                     << ": new local network id "
                     << network_route.local_network_id
                     << " new remote network id "
                     << network_route.remote_network_id
                     << " Reset bitrates to min: "
                     << bitrate_config.min_bitrate_bps
                     << " bps, start: " << bitrate_config.start_bitrate_bps
                     << " bps,  max: " << bitrate_config.max_bitrate_bps
                     << " bps.";
    RTC_DCHECK_GT(bitrate_config.start_bitrate_bps, 0);

    if (reset_feedback_on_route_change_)
      transport_feedback_adapter_.SetNetworkIds(
          network_route.local_network_id, network_route.remote_network_id);
    transport_overhead_bytes_per_packet_ = network_route.packet_overhead;

    NetworkRouteChange msg;
    msg.at_time = Timestamp::ms(clock_->TimeInMilliseconds());
    msg.constraints = ConvertConstraints(bitrate_config, clock_);
    task_queue_.PostTask([this, msg] {
      RTC_DCHECK_RUN_ON(&task_queue_);
      if (controller_) {
        control_handler_->PostUpdates(controller_->OnNetworkRouteChange(msg));
      } else {
        UpdateInitialConstraints(msg.constraints);
      }
      pacer_.UpdateOutstandingData(0);
    });
  }
}
void RtpTransportControllerSend::OnNetworkAvailability(bool network_available) {
  RTC_LOG(LS_INFO) << "SignalNetworkState "
                   << (network_available ? "Up" : "Down");
  NetworkAvailability msg;
  msg.at_time = Timestamp::ms(clock_->TimeInMilliseconds());
  msg.network_available = network_available;
  task_queue_.PostTask([this, msg]() {
    RTC_DCHECK_RUN_ON(&task_queue_);
    network_available_ = msg.network_available;
    if (controller_) {
      control_handler_->PostUpdates(controller_->OnNetworkAvailability(msg));
      control_handler_->OnNetworkAvailability(msg);
    } else {
      MaybeCreateControllers();
    }
  });

  for (auto& rtp_sender : video_rtp_senders_) {
    rtp_sender->OnNetworkAvailability(network_available);
  }
}
RtcpBandwidthObserver* RtpTransportControllerSend::GetBandwidthObserver() {
  return this;
}
int64_t RtpTransportControllerSend::GetPacerQueuingDelayMs() const {
  return pacer_.QueueInMs();
}
int64_t RtpTransportControllerSend::GetFirstPacketTimeMs() const {
  return pacer_.FirstSentPacketTimeMs();
}
void RtpTransportControllerSend::SetPerPacketFeedbackAvailable(bool available) {
  RTC_DCHECK_RUN_ON(&task_queue_);
  packet_feedback_available_ = available;
  if (!controller_)
    MaybeCreateControllers();
}
void RtpTransportControllerSend::EnablePeriodicAlrProbing(bool enable) {
  task_queue_.PostTask([this, enable]() {
    RTC_DCHECK_RUN_ON(&task_queue_);
    streams_config_.requests_alr_probing = enable;
    UpdateStreamsConfig();
  });
}
void RtpTransportControllerSend::OnSentPacket(
    const rtc::SentPacket& sent_packet) {
  absl::optional<SentPacket> packet_msg =
      transport_feedback_adapter_.ProcessSentPacket(sent_packet);
  if (packet_msg) {
    task_queue_.PostTask([this, packet_msg]() {
      RTC_DCHECK_RUN_ON(&task_queue_);
      if (controller_)
        control_handler_->PostUpdates(controller_->OnSentPacket(*packet_msg));
    });
  }
  MaybeUpdateOutstandingData();
}

void RtpTransportControllerSend::SetSdpBitrateParameters(
    const BitrateConstraints& constraints) {
  absl::optional<BitrateConstraints> updated =
      bitrate_configurator_.UpdateWithSdpParameters(constraints);
  if (updated.has_value()) {
    TargetRateConstraints msg = ConvertConstraints(*updated, clock_);
    task_queue_.PostTask([this, msg]() {
      RTC_DCHECK_RUN_ON(&task_queue_);
      if (controller_) {
        control_handler_->PostUpdates(
            controller_->OnTargetRateConstraints(msg));
      } else {
        UpdateInitialConstraints(msg);
      }
    });
  } else {
    RTC_LOG(LS_VERBOSE)
        << "WebRTC.RtpTransportControllerSend.SetSdpBitrateParameters: "
        << "nothing to update";
  }
}

void RtpTransportControllerSend::SetClientBitratePreferences(
    const BitrateSettings& preferences) {
  absl::optional<BitrateConstraints> updated =
      bitrate_configurator_.UpdateWithClientPreferences(preferences);
  if (updated.has_value()) {
    TargetRateConstraints msg = ConvertConstraints(*updated, clock_);
    task_queue_.PostTask([this, msg]() {
      RTC_DCHECK_RUN_ON(&task_queue_);
      if (controller_) {
        control_handler_->PostUpdates(
            controller_->OnTargetRateConstraints(msg));
      } else {
        UpdateInitialConstraints(msg);
      }
    });
  } else {
    RTC_LOG(LS_VERBOSE)
        << "WebRTC.RtpTransportControllerSend.SetClientBitratePreferences: "
        << "nothing to update";
  }
}

void RtpTransportControllerSend::SetAllocatedBitrateWithoutFeedback(
    uint32_t bitrate_bps) {
  // Audio transport feedback will not be reported in this mode, instead update
  // acknowledged bitrate estimator with the bitrate allocated for audio.
  if (field_trial::IsEnabled("WebRTC-Audio-ABWENoTWCC")) {
    // TODO(srte): Make sure it's safe to always report this and remove the
    // field trial check.
    task_queue_.PostTask([this, bitrate_bps]() {
      RTC_DCHECK_RUN_ON(&task_queue_);
      streams_config_.unacknowledged_rate_allocation =
          DataRate::bps(bitrate_bps);
      UpdateStreamsConfig();
    });
  }
}

void RtpTransportControllerSend::OnTransportOverheadChanged(
    size_t transport_overhead_bytes_per_packet) {
  if (transport_overhead_bytes_per_packet >= kMaxOverheadBytes) {
    RTC_LOG(LS_ERROR) << "Transport overhead exceeds " << kMaxOverheadBytes;
    return;
  }

  // TODO(holmer): Call AudioRtpSenders when they have been moved to
  // RtpTransportControllerSend.
  for (auto& rtp_video_sender : video_rtp_senders_) {
    rtp_video_sender->OnTransportOverheadChanged(
        transport_overhead_bytes_per_packet);
  }
}

void RtpTransportControllerSend::OnReceivedEstimatedBitrate(uint32_t bitrate) {
  RemoteBitrateReport msg;
  msg.receive_time = Timestamp::ms(clock_->TimeInMilliseconds());
  msg.bandwidth = DataRate::bps(bitrate);
  task_queue_.PostTask([this, msg]() {
    RTC_DCHECK_RUN_ON(&task_queue_);
    if (controller_)
      control_handler_->PostUpdates(controller_->OnRemoteBitrateReport(msg));
  });
}

void RtpTransportControllerSend::OnReceivedRtcpReceiverReport(
    const ReportBlockList& report_blocks,
    int64_t rtt_ms,
    int64_t now_ms) {
  task_queue_.PostTask([this, report_blocks, now_ms]() {
    RTC_DCHECK_RUN_ON(&task_queue_);
    OnReceivedRtcpReceiverReportBlocks(report_blocks, now_ms);
  });

  task_queue_.PostTask([this, now_ms, rtt_ms]() {
    RTC_DCHECK_RUN_ON(&task_queue_);
    RoundTripTimeUpdate report;
    report.receive_time = Timestamp::ms(now_ms);
    report.round_trip_time = TimeDelta::ms(rtt_ms);
    report.smoothed = false;
    if (controller_)
      control_handler_->PostUpdates(controller_->OnRoundTripTimeUpdate(report));
  });
}

void RtpTransportControllerSend::AddPacket(uint32_t ssrc,
                                           uint16_t sequence_number,
                                           size_t length,
                                           const PacedPacketInfo& pacing_info) {
  if (send_side_bwe_with_overhead_) {
    length += transport_overhead_bytes_per_packet_;
  }
  transport_feedback_adapter_.AddPacket(ssrc, sequence_number, length,
                                        pacing_info);
}

void RtpTransportControllerSend::OnTransportFeedback(
    const rtcp::TransportFeedback& feedback) {
  RTC_DCHECK_RUNS_SERIALIZED(&worker_race_);

  absl::optional<TransportPacketsFeedback> feedback_msg =
      transport_feedback_adapter_.ProcessTransportFeedback(feedback);
  if (feedback_msg) {
    task_queue_.PostTask([this, feedback_msg]() {
      RTC_DCHECK_RUN_ON(&task_queue_);
      if (controller_)
        control_handler_->PostUpdates(
            controller_->OnTransportPacketsFeedback(*feedback_msg));
    });
  }
  MaybeUpdateOutstandingData();
}

void RtpTransportControllerSend::OnRttUpdate(int64_t avg_rtt_ms,
                                             int64_t max_rtt_ms) {
  int64_t now_ms = clock_->TimeInMilliseconds();
  RoundTripTimeUpdate report;
  report.receive_time = Timestamp::ms(now_ms);
  report.round_trip_time = TimeDelta::ms(avg_rtt_ms);
  report.smoothed = true;
  task_queue_.PostTask([this, report]() {
    RTC_DCHECK_RUN_ON(&task_queue_);
    if (controller_)
      control_handler_->PostUpdates(controller_->OnRoundTripTimeUpdate(report));
  });
}

void RtpTransportControllerSend::MaybeCreateControllers() {
  RTC_DCHECK(!controller_);
  RTC_DCHECK(!control_handler_);

  if (!network_available_ || !observer_)
    return;
  control_handler_ = absl::make_unique<CongestionControlHandler>(this, &pacer_);

  initial_config_.constraints.at_time =
      Timestamp::ms(clock_->TimeInMilliseconds());
  initial_config_.stream_based_config = streams_config_;

  // TODO(srte): Use fallback controller if no feedback is available.
  if (controller_factory_override_) {
    RTC_LOG(LS_INFO) << "Creating overridden congestion controller";
    controller_ = controller_factory_override_->Create(initial_config_);
    process_interval_ = controller_factory_override_->GetProcessInterval();
  } else {
    RTC_LOG(LS_INFO) << "Creating fallback congestion controller";
    controller_ = controller_factory_fallback_->Create(initial_config_);
    process_interval_ = controller_factory_fallback_->GetProcessInterval();
  }
  UpdateControllerWithTimeInterval();
  StartProcessPeriodicTasks();
}

void RtpTransportControllerSend::UpdateInitialConstraints(
    TargetRateConstraints new_contraints) {
  if (!new_contraints.starting_rate)
    new_contraints.starting_rate = initial_config_.constraints.starting_rate;
  RTC_DCHECK(new_contraints.starting_rate);
  initial_config_.constraints = new_contraints;
}

void RtpTransportControllerSend::StartProcessPeriodicTasks() {
  if (!periodic_tasks_enabled_)
    return;
  if (!pacer_queue_update_task_) {
    pacer_queue_update_task_ =
        StartPeriodicTask(&task_queue_, PacerQueueUpdateIntervalMs, [this]() {
          RTC_DCHECK_RUN_ON(&task_queue_);
          UpdatePacerQueue();
        });
  }
  if (controller_task_) {
    // Stop is not synchronous, but is guaranteed to occur before the first
    // invocation of the new controller task started below.
    controller_task_->Stop();
    controller_task_ = nullptr;
  }
  if (process_interval_.IsFinite()) {
    // The controller task is owned by the task queue and lives until the task
    // queue is destroyed or some time after Stop() is called, whichever comes
    // first.
    controller_task_ =
        StartPeriodicTask(&task_queue_, process_interval_.ms(), [this]() {
          RTC_DCHECK_RUN_ON(&task_queue_);
          UpdateControllerWithTimeInterval();
        });
  }
}

void RtpTransportControllerSend::UpdateControllerWithTimeInterval() {
  if (controller_) {
    ProcessInterval msg;
    msg.at_time = Timestamp::ms(clock_->TimeInMilliseconds());
    control_handler_->PostUpdates(controller_->OnProcessInterval(msg));
  }
}

void RtpTransportControllerSend::UpdatePacerQueue() {
  if (control_handler_) {
    TimeDelta expected_queue_time = TimeDelta::ms(pacer_.ExpectedQueueTimeMs());
    control_handler_->OnPacerQueueUpdate(expected_queue_time);
  }
}

void RtpTransportControllerSend::MaybeUpdateOutstandingData() {
  DataSize in_flight_data = transport_feedback_adapter_.GetOutstandingData();
  task_queue_.PostTask([this, in_flight_data]() {
    RTC_DCHECK_RUN_ON(&task_queue_);
    if (control_handler_)
      control_handler_->OnOutstandingData(in_flight_data);
  });
}

void RtpTransportControllerSend::UpdateStreamsConfig() {
  streams_config_.at_time = Timestamp::ms(clock_->TimeInMilliseconds());
  if (controller_)
    control_handler_->PostUpdates(
        controller_->OnStreamsConfig(streams_config_));
}

void RtpTransportControllerSend::OnReceivedRtcpReceiverReportBlocks(
    const ReportBlockList& report_blocks,
    int64_t now_ms) {
  if (report_blocks.empty())
    return;

  int total_packets_lost_delta = 0;
  int total_packets_delta = 0;

  // Compute the packet loss from all report blocks.
  for (const RTCPReportBlock& report_block : report_blocks) {
    auto it = last_report_blocks_.find(report_block.source_ssrc);
    if (it != last_report_blocks_.end()) {
      auto number_of_packets = report_block.extended_highest_sequence_number -
                               it->second.extended_highest_sequence_number;
      total_packets_delta += number_of_packets;
      auto lost_delta = report_block.packets_lost - it->second.packets_lost;
      total_packets_lost_delta += lost_delta;
    }
    last_report_blocks_[report_block.source_ssrc] = report_block;
  }
  // Can only compute delta if there has been previous blocks to compare to. If
  // not, total_packets_delta will be unchanged and there's nothing more to do.
  if (!total_packets_delta)
    return;
  int packets_received_delta = total_packets_delta - total_packets_lost_delta;
  // To detect lost packets, at least one packet has to be received. This check
  // is needed to avoid bandwith detection update in
  // VideoSendStreamTest.SuspendBelowMinBitrate

  if (packets_received_delta < 1)
    return;
  Timestamp now = Timestamp::ms(now_ms);
  TransportLossReport msg;
  msg.packets_lost_delta = total_packets_lost_delta;
  msg.packets_received_delta = packets_received_delta;
  msg.receive_time = now;
  msg.start_time = last_report_block_time_;
  msg.end_time = now;
  if (controller_)
    control_handler_->PostUpdates(controller_->OnTransportLossReport(msg));
  last_report_block_time_ = now;
}

}  // namespace webrtc
