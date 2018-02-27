/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_CONGESTION_CONTROLLER_RTP_INCLUDE_SEND_SIDE_CONGESTION_CONTROLLER_H_
#define MODULES_CONGESTION_CONTROLLER_RTP_INCLUDE_SEND_SIDE_CONGESTION_CONTROLLER_H_

#include <atomic>
#include <functional>
#include <map>
#include <memory>
#include <vector>

#include "common_types.h"  // NOLINT(build/include)
#include "modules/congestion_controller/rtp/network_control/include/network_control.h"
#include "modules/congestion_controller/rtp/network_control/include/network_types.h"
#include "modules/congestion_controller/rtp/pacer_controller.h"
#include "modules/congestion_controller/rtp/transport_feedback_adapter.h"
#include "modules/include/module.h"
#include "modules/include/module_common_types.h"
#include "modules/pacing/paced_sender.h"
#include "modules/rtp_rtcp/include/rtp_rtcp_defines.h"
#include "rtc_base/constructormagic.h"
#include "rtc_base/criticalsection.h"
#include "rtc_base/networkroute.h"
#include "rtc_base/race_checker.h"
#include "rtc_base/task_queue.h"

namespace rtc {
struct SentPacket;
}

namespace webrtc {

class Clock;
class RateLimiter;
class RtcEventLog;

namespace send_side_cc_internal {
// This is used to observe the network controller state and route calls to
// the proper handler. It also keeps cached values for safe asynchronous use.
// This makes sure that things running on the worker queue can't access state
// in SendSideCongestionController, which would risk causing data race on
// destruction unless members are properly ordered.
class ControlHandler;
}  // namespace send_side_cc_internal

class SendSideCongestionController : public CallStatsObserver,
                                     public Module,
                                     public TransportFeedbackObserver,
                                     public RtcpBandwidthObserver {
 public:
  // Observer class for bitrate changes announced due to change in bandwidth
  // estimate or due to that the send pacer is full. Fraction loss and rtt is
  // also part of this callback to allow the observer to optimize its settings
  // for different types of network environments. The bitrate does not include
  // packet headers and is measured in bits per second.
  class Observer {
   public:
    virtual void OnNetworkChanged(uint32_t bitrate_bps,
                                  uint8_t fraction_loss,  // 0 - 255.
                                  int64_t rtt_ms,
                                  int64_t probing_interval_ms) = 0;

   protected:
    virtual ~Observer() {}
  };
  SendSideCongestionController(const Clock* clock,
                               Observer* observer,
                               RtcEventLog* event_log,
                               PacedSender* pacer);
  ~SendSideCongestionController() override;

  void RegisterPacketFeedbackObserver(PacketFeedbackObserver* observer);
  void DeRegisterPacketFeedbackObserver(PacketFeedbackObserver* observer);

  // Currently, there can be at most one observer.
  // TODO(nisse): The RegisterNetworkObserver method is needed because we first
  // construct this object (as part of RtpTransportControllerSend), then pass a
  // reference to Call, which then registers itself as the observer. We should
  // try to break this circular chain of references, and make the observer a
  // construction time constant.
  void RegisterNetworkObserver(Observer* observer);
  void DeRegisterNetworkObserver(Observer* observer);

  virtual void SetBweBitrates(int min_bitrate_bps,
                              int start_bitrate_bps,
                              int max_bitrate_bps);
  // Resets the BWE state. Note the first argument is the bitrate_bps.
  virtual void OnNetworkRouteChanged(const rtc::NetworkRoute& network_route,
                                     int bitrate_bps,
                                     int min_bitrate_bps,
                                     int max_bitrate_bps);
  virtual void SignalNetworkState(NetworkState state);
  virtual void SetTransportOverhead(size_t transport_overhead_bytes_per_packet);

  virtual RtcpBandwidthObserver* GetBandwidthObserver();

  virtual bool AvailableBandwidth(uint32_t* bandwidth) const;
  virtual int64_t GetPacerQueuingDelayMs() const;
  virtual int64_t GetFirstPacketTimeMs() const;

  virtual TransportFeedbackObserver* GetTransportFeedbackObserver();

  RateLimiter* GetRetransmissionRateLimiter();
  void EnablePeriodicAlrProbing(bool enable);

  virtual void OnSentPacket(const rtc::SentPacket& sent_packet);

  // Implements RtcpBandwidthObserver
  void OnReceivedEstimatedBitrate(uint32_t bitrate) override;
  void OnReceivedRtcpReceiverReport(const ReportBlockList& report_blocks,
                                    int64_t rtt,
                                    int64_t now_ms) override;
  // Implements CallStatsObserver.
  void OnRttUpdate(int64_t avg_rtt_ms, int64_t max_rtt_ms) override;

  // Implements Module.
  int64_t TimeUntilNextProcess() override;
  void Process() override;

  // Implements TransportFeedbackObserver.
  void AddPacket(uint32_t ssrc,
                 uint16_t sequence_number,
                 size_t length,
                 const PacedPacketInfo& pacing_info) override;
  void OnTransportFeedback(const rtcp::TransportFeedback& feedback) override;
  std::vector<PacketFeedback> GetTransportFeedbackVector() const override;

  // Sets the minimum send bitrate and maximum padding bitrate requested by send
  // streams.
  // |min_send_bitrate_bps| might be higher that the estimated available network
  // bitrate and if so, the pacer will send with |min_send_bitrate_bps|.
  // |max_padding_bitrate_bps| might be higher than the estimate available
  // network bitrate and if so, the pacer will send padding packets to reach
  // the min of the estimated available bitrate and |max_padding_bitrate_bps|.
  void SetSendBitrateLimits(int64_t min_send_bitrate_bps,
                            int64_t max_padding_bitrate_bps);
  void SetPacingFactor(float pacing_factor);

 protected:
  // Waits long enough that any outstanding tasks should be finished.
  void WaitOnTasks();

 private:
  SendSideCongestionController(
      const Clock* clock,
      RtcEventLog* event_log,
      PacedSender* pacer,
      NetworkControllerFactoryInterface::uptr controller_factory);

  void UpdateStreamsConfig();
  void WaitOnTask(std::function<void()> closure);
  void MaybeUpdateOutstandingData();
  void OnReceivedRtcpReceiverReportBlocks(const ReportBlockList& report_blocks,
                                          int64_t now_ms);

  const Clock* const clock_;
  PacedSender* const pacer_;
  TransportFeedbackAdapter transport_feedback_adapter_;

  const std::unique_ptr<PacerController> pacer_controller_;
  const std::unique_ptr<send_side_cc_internal::ControlHandler> control_handler;
  const std::unique_ptr<NetworkControllerInterface> controller_;

  TimeDelta process_interval_;
  int64_t last_process_update_ms_ = 0;

  std::map<uint32_t, RTCPReportBlock> last_report_blocks_;
  Timestamp last_report_block_time_;

  StreamsConfig streams_config_;
  const bool send_side_bwe_with_overhead_;
  std::atomic<size_t> transport_overhead_bytes_per_packet_;
  std::atomic<bool> network_available_;

  rtc::RaceChecker worker_race_;

  // Note that moving ownership of the task queue makes it neccessary to make
  // sure that there is no outstanding tasks on it using destructed objects.
  // This is currently guranteed by using explicit reset in the destructor of
  // this class. It is declared last to indicate that it's lifetime is shorter
  // than all other members.
  std::unique_ptr<rtc::TaskQueue> task_queue_;

  RTC_DISALLOW_IMPLICIT_CONSTRUCTORS(SendSideCongestionController);
};
}  // namespace webrtc

#endif  // MODULES_CONGESTION_CONTROLLER_RTP_INCLUDE_SEND_SIDE_CONGESTION_CONTROLLER_H_
