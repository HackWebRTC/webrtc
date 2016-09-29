/*
 *  Copyright (c) 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MODULES_REMOTE_BITRATE_ESTIMATOR_TRANSPORT_FEEDBACK_ADAPTER_H_
#define WEBRTC_MODULES_REMOTE_BITRATE_ESTIMATOR_TRANSPORT_FEEDBACK_ADAPTER_H_

#include <memory>
#include <vector>

#include "webrtc/base/criticalsection.h"
#include "webrtc/base/thread_annotations.h"
#include "webrtc/modules/include/module_common_types.h"
#include "webrtc/modules/remote_bitrate_estimator/include/remote_bitrate_estimator.h"
#include "webrtc/modules/remote_bitrate_estimator/include/send_time_history.h"

namespace webrtc {

class ProcessThread;

class TransportFeedbackAdapter : public TransportFeedbackObserver,
                                 public CallStatsObserver {
 public:
  explicit TransportFeedbackAdapter(Clock* clock);
  virtual ~TransportFeedbackAdapter();

  void SetBitrateEstimator(RemoteBitrateEstimator* rbe);
  RemoteBitrateEstimator* GetBitrateEstimator() const {
    return bitrate_estimator_.get();
  }

  // Implements TransportFeedbackObserver.
  void AddPacket(uint16_t sequence_number,
                 size_t length,
                 int probe_cluster_id) override;
  void OnSentPacket(uint16_t sequence_number, int64_t send_time_ms);

  void OnTransportFeedback(const rtcp::TransportFeedback& feedback) override;
  std::vector<PacketInfo> GetTransportFeedbackVector() const override;

  // Implements CallStatsObserver.
  void OnRttUpdate(int64_t avg_rtt_ms, int64_t max_rtt_ms) override;

 private:
  std::vector<PacketInfo> GetPacketFeedbackVector(
      const rtcp::TransportFeedback& feedback);

  rtc::CriticalSection lock_;
  SendTimeHistory send_time_history_ GUARDED_BY(&lock_);
  std::unique_ptr<RemoteBitrateEstimator> bitrate_estimator_;
  Clock* const clock_;
  int64_t current_offset_ms_;
  int64_t last_timestamp_us_;
  std::vector<PacketInfo> last_packet_feedback_vector_;
};

}  // namespace webrtc

#endif  // WEBRTC_MODULES_REMOTE_BITRATE_ESTIMATOR_TRANSPORT_FEEDBACK_ADAPTER_H_
