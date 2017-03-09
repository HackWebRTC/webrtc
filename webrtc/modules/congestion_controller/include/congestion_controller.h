/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MODULES_CONGESTION_CONTROLLER_INCLUDE_CONGESTION_CONTROLLER_H_
#define WEBRTC_MODULES_CONGESTION_CONTROLLER_INCLUDE_CONGESTION_CONTROLLER_H_

#include <memory>
#include <vector>

#include "webrtc/base/constructormagic.h"
#include "webrtc/base/criticalsection.h"
#include "webrtc/base/thread_checker.h"
#include "webrtc/common_types.h"
#include "webrtc/modules/congestion_controller/delay_based_bwe.h"
#include "webrtc/modules/congestion_controller/transport_feedback_adapter.h"
#include "webrtc/modules/include/module.h"
#include "webrtc/modules/include/module_common_types.h"
#include "webrtc/modules/pacing/paced_sender.h"
#include "webrtc/modules/pacing/packet_router.h"
#include "webrtc/modules/remote_bitrate_estimator/remote_estimator_proxy.h"

namespace rtc {
struct SentPacket;
}

namespace webrtc {

class BitrateController;
class Clock;
class ProbeController;
class RateLimiter;
class RemoteBitrateEstimator;
class RemoteBitrateObserver;
class RtcEventLog;

class CongestionController : public CallStatsObserver,
                             public Module,
                             public TransportFeedbackObserver {
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
  CongestionController(const Clock* clock,
                       Observer* observer,
                       RemoteBitrateObserver* remote_bitrate_observer,
                       RtcEventLog* event_log,
                       PacketRouter* packet_router);
  CongestionController(const Clock* clock,
                       Observer* observer,
                       RemoteBitrateObserver* remote_bitrate_observer,
                       RtcEventLog* event_log,
                       PacketRouter* packet_router,
                       std::unique_ptr<PacedSender> pacer);
  virtual ~CongestionController();

  virtual void OnReceivedPacket(int64_t arrival_time_ms,
                                size_t payload_size,
                                const RTPHeader& header);

  virtual void SetBweBitrates(int min_bitrate_bps,
                              int start_bitrate_bps,
                              int max_bitrate_bps);
  // Resets both the BWE state and the bitrate estimator. Note the first
  // argument is the bitrate_bps.
  virtual void ResetBweAndBitrates(int bitrate_bps,
                                   int min_bitrate_bps,
                                   int max_bitrate_bps);
  virtual void SignalNetworkState(NetworkState state);
  virtual void SetTransportOverhead(size_t transport_overhead_bytes_per_packet);

  virtual BitrateController* GetBitrateController() const;
  virtual RemoteBitrateEstimator* GetRemoteBitrateEstimator(
      bool send_side_bwe);
  virtual int64_t GetPacerQueuingDelayMs() const;
  // TODO(nisse): Delete this accessor function. The pacer should be
  // internal to the congestion controller.
  virtual PacedSender* pacer() { return pacer_.get(); }
  virtual TransportFeedbackObserver* GetTransportFeedbackObserver() {
    return this;
  }
  RateLimiter* GetRetransmissionRateLimiter();
  void EnablePeriodicAlrProbing(bool enable);

  // SetAllocatedSendBitrateLimits sets bitrates limits imposed by send codec
  // settings.
  // |min_send_bitrate_bps| is the total minimum send bitrate required by all
  // sending streams.  This is the minimum bitrate the PacedSender will use.
  // Note that CongestionController::OnNetworkChanged can still be called with
  // a lower bitrate estimate.
  // |max_padding_bitrate_bps| is the max bitrate the send streams request for
  // padding. This can be higher than the current network estimate and tells
  // the PacedSender how much it should max pad unless there is real packets to
  // send.
  void SetAllocatedSendBitrateLimits(int min_send_bitrate_bps,
                                     int max_padding_bitrate_bps);

  virtual void OnSentPacket(const rtc::SentPacket& sent_packet);

  // Implements CallStatsObserver.
  void OnRttUpdate(int64_t avg_rtt_ms, int64_t max_rtt_ms) override;

  // Implements Module.
  int64_t TimeUntilNextProcess() override;
  void Process() override;

  // Implements TransportFeedbackObserver.
  void AddPacket(uint16_t sequence_number,
                 size_t length,
                 const PacedPacketInfo& pacing_info) override;
  void OnTransportFeedback(const rtcp::TransportFeedback& feedback) override;
  std::vector<PacketFeedback> GetTransportFeedbackVector() const override;

 private:
  class WrappingBitrateEstimator : public RemoteBitrateEstimator {
   public:
    WrappingBitrateEstimator(RemoteBitrateObserver* observer,
                             const Clock* clock);

    virtual ~WrappingBitrateEstimator() {}

    void IncomingPacket(int64_t arrival_time_ms,
                        size_t payload_size,
                        const RTPHeader& header) override;

    void Process() override;

    int64_t TimeUntilNextProcess() override;

    void OnRttUpdate(int64_t avg_rtt_ms, int64_t max_rtt_ms) override;

    void RemoveStream(unsigned int ssrc) override;

    bool LatestEstimate(std::vector<unsigned int>* ssrcs,
                        unsigned int* bitrate_bps) const override;

    void SetMinBitrate(int min_bitrate_bps) override;

   private:
    void PickEstimatorFromHeader(const RTPHeader& header)
        EXCLUSIVE_LOCKS_REQUIRED(crit_sect_);
    void PickEstimator() EXCLUSIVE_LOCKS_REQUIRED(crit_sect_);
    RemoteBitrateObserver* observer_;
    const Clock* const clock_;
    rtc::CriticalSection crit_sect_;
    std::unique_ptr<RemoteBitrateEstimator> rbe_;
    bool using_absolute_send_time_;
    uint32_t packets_since_absolute_send_time_;
    int min_bitrate_bps_;

    RTC_DISALLOW_IMPLICIT_CONSTRUCTORS(WrappingBitrateEstimator);
  };

  void MaybeTriggerOnNetworkChanged();

  bool IsSendQueueFull() const;
  bool IsNetworkDown() const;
  bool HasNetworkParametersToReportChanged(uint32_t bitrate_bps,
                                           uint8_t fraction_loss,
                                           int64_t rtt);
  const Clock* const clock_;
  Observer* const observer_;
  RtcEventLog* const event_log_;
  PacketRouter* const packet_router_;
  const std::unique_ptr<PacedSender> pacer_;
  const std::unique_ptr<BitrateController> bitrate_controller_;
  const std::unique_ptr<ProbeController> probe_controller_;
  const std::unique_ptr<RateLimiter> retransmission_rate_limiter_;
  WrappingBitrateEstimator remote_bitrate_estimator_;
  RemoteEstimatorProxy remote_estimator_proxy_;
  TransportFeedbackAdapter transport_feedback_adapter_;
  int min_bitrate_bps_;
  int max_bitrate_bps_;
  rtc::CriticalSection network_state_lock_;
  uint32_t last_reported_bitrate_bps_ GUARDED_BY(network_state_lock_);
  uint8_t last_reported_fraction_loss_ GUARDED_BY(network_state_lock_);
  int64_t last_reported_rtt_ GUARDED_BY(network_state_lock_);
  NetworkState network_state_ GUARDED_BY(network_state_lock_);
  rtc::CriticalSection bwe_lock_;
  std::unique_ptr<DelayBasedBwe> delay_based_bwe_ GUARDED_BY(bwe_lock_);

  rtc::ThreadChecker worker_thread_checker_;

  RTC_DISALLOW_IMPLICIT_CONSTRUCTORS(CongestionController);
};

}  // namespace webrtc

#endif  // WEBRTC_MODULES_CONGESTION_CONTROLLER_INCLUDE_CONGESTION_CONTROLLER_H_
