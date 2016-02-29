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

#include "webrtc/base/scoped_ptr.h"
#include "webrtc/modules/include/module.h"
#include "webrtc/modules/include/module_common_types.h"
#include "webrtc/modules/pacing/packet_router.h"
#include "webrtc/modules/remote_bitrate_estimator/remote_estimator_proxy.h"
#include "webrtc/modules/remote_bitrate_estimator/transport_feedback_adapter.h"
#include "webrtc/stream.h"

namespace rtc {
struct SentPacket;
}

namespace webrtc {

class BitrateController;
class BitrateObserver;
class Clock;
class PacedSender;
class ProcessThread;
class RemoteBitrateEstimator;
class RemoteBitrateObserver;
class TransportFeedbackObserver;

class CongestionController : public CallStatsObserver, public Module {
 public:
  CongestionController(Clock* clock,
                       BitrateObserver* bitrate_observer,
                       RemoteBitrateObserver* remote_bitrate_observer);
  virtual ~CongestionController();

  virtual void SetBweBitrates(int min_bitrate_bps,
                              int start_bitrate_bps,
                              int max_bitrate_bps);
  virtual void SignalNetworkState(NetworkState state);
  virtual BitrateController* GetBitrateController() const;
  virtual RemoteBitrateEstimator* GetRemoteBitrateEstimator(
      bool send_side_bwe);
  virtual int64_t GetPacerQueuingDelayMs() const;
  virtual PacedSender* pacer() { return pacer_.get(); }
  virtual PacketRouter* packet_router() { return &packet_router_; }
  virtual TransportFeedbackObserver* GetTransportFeedbackObserver();

  virtual void UpdatePacerBitrate(int bitrate_kbps,
                                  int max_bitrate_kbps,
                                  int min_bitrate_kbps);

  virtual void OnSentPacket(const rtc::SentPacket& sent_packet);

  // Implements CallStatsObserver.
  void OnRttUpdate(int64_t avg_rtt_ms, int64_t max_rtt_ms) override;

  // Implements Module.
  int64_t TimeUntilNextProcess() override;
  void Process() override;

 private:
  Clock* const clock_;
  const rtc::scoped_ptr<PacedSender> pacer_;
  const rtc::scoped_ptr<RemoteBitrateEstimator> remote_bitrate_estimator_;
  const rtc::scoped_ptr<BitrateController> bitrate_controller_;
  PacketRouter packet_router_;
  RemoteEstimatorProxy remote_estimator_proxy_;
  TransportFeedbackAdapter transport_feedback_adapter_;
  int min_bitrate_bps_;

  RTC_DISALLOW_IMPLICIT_CONSTRUCTORS(CongestionController);
};

}  // namespace webrtc

#endif  // WEBRTC_MODULES_CONGESTION_CONTROLLER_INCLUDE_CONGESTION_CONTROLLER_H_
