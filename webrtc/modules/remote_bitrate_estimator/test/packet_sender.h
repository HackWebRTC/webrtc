/*
 *  Copyright (c) 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MODULES_REMOTE_BITRATE_ESTIMATOR_TEST_PACKET_SENDER_H_
#define WEBRTC_MODULES_REMOTE_BITRATE_ESTIMATOR_TEST_PACKET_SENDER_H_

#include <list>
#include <string>

#include "webrtc/base/constructormagic.h"
#include "webrtc/base/scoped_ptr.h"
#include "webrtc/modules/interface/module.h"
#include "webrtc/modules/remote_bitrate_estimator/test/bwe.h"
#include "webrtc/modules/remote_bitrate_estimator/test/bwe_test_framework.h"

namespace webrtc {
namespace testing {
namespace bwe {

class PacketSender : public PacketProcessor {
 public:
  PacketSender(PacketProcessorListener* listener, int flow_id)
      : PacketProcessor(listener, flow_id, kSender) {}
  virtual ~PacketSender() {}
  // Call GiveFeedback() with the returned interval in milliseconds, provided
  // there is a new estimate available.
  // Note that changing the feedback interval affects the timing of when the
  // output of the estimators is sampled and therefore the baseline files may
  // have to be regenerated.
  virtual int GetFeedbackIntervalMs() const = 0;
};

class VideoSender : public PacketSender, public BitrateObserver {
 public:
  VideoSender(PacketProcessorListener* listener,
              VideoSource* source,
              BandwidthEstimatorType estimator);
  virtual ~VideoSender();

  int GetFeedbackIntervalMs() const override;
  void RunFor(int64_t time_ms, Packets* in_out) override;

  virtual VideoSource* source() const { return source_; }

  // Implements BitrateObserver.
  void OnNetworkChanged(uint32_t target_bitrate_bps,
                        uint8_t fraction_lost,
                        int64_t rtt) override;

 protected:
  void ProcessFeedbackAndGeneratePackets(int64_t time_ms,
                                         std::list<FeedbackPacket*>* feedbacks,
                                         Packets* generated);

  SimulatedClock clock_;
  VideoSource* source_;
  rtc::scoped_ptr<BweSender> bwe_;
  int64_t start_of_run_ms_;
  std::list<Module*> modules_;

 private:
  DISALLOW_COPY_AND_ASSIGN(VideoSender);
};

class PacedVideoSender : public VideoSender, public PacedSender::Callback {
 public:
  PacedVideoSender(PacketProcessorListener* listener,
                   VideoSource* source,
                   BandwidthEstimatorType estimator);
  virtual ~PacedVideoSender();

  void RunFor(int64_t time_ms, Packets* in_out) override;

  // Implements PacedSender::Callback.
  bool TimeToSendPacket(uint32_t ssrc,
                        uint16_t sequence_number,
                        int64_t capture_time_ms,
                        bool retransmission) override;
  size_t TimeToSendPadding(size_t bytes) override;

  // Implements BitrateObserver.
  void OnNetworkChanged(uint32_t target_bitrate_bps,
                        uint8_t fraction_lost,
                        int64_t rtt) override;

 private:
  int64_t TimeUntilNextProcess(const std::list<Module*>& modules);
  void CallProcess(const std::list<Module*>& modules);
  void QueuePackets(Packets* batch, int64_t end_of_batch_time_us);

  PacedSender pacer_;
  Packets queue_;
  Packets pacer_queue_;

  DISALLOW_IMPLICIT_CONSTRUCTORS(PacedVideoSender);
};

class TcpSender : public PacketSender {
 public:
  TcpSender(PacketProcessorListener* listener, int flow_id)
      : PacketSender(listener, flow_id),
        now_ms_(0),
        in_slow_start_(false),
        cwnd_(1),
        in_flight_(0),
        ack_received_(false),
        last_acked_seq_num_(0),
        next_sequence_number_(0) {}

  void RunFor(int64_t time_ms, Packets* in_out) override;
  int GetFeedbackIntervalMs() const override { return 10; }

 private:
  void SendPackets(Packets* in_out);
  void UpdateCongestionControl(const FeedbackPacket* fb);
  bool LossEvent(const std::vector<uint16_t>& acked_packets);
  Packets GeneratePackets(size_t num_packets);

  int64_t now_ms_;
  bool in_slow_start_;
  float cwnd_;
  int in_flight_;
  bool ack_received_;
  uint16_t last_acked_seq_num_;
  uint16_t next_sequence_number_;
};
}  // namespace bwe
}  // namespace testing
}  // namespace webrtc
#endif  // WEBRTC_MODULES_REMOTE_BITRATE_ESTIMATOR_TEST_PACKET_SENDER_H_
