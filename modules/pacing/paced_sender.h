/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_PACING_PACED_SENDER_H_
#define MODULES_PACING_PACED_SENDER_H_

#include <stddef.h>
#include <stdint.h>

#include <atomic>
#include <memory>
#include <vector>

#include "absl/types/optional.h"
#include "api/function_view.h"
#include "api/transport/field_trial_based_config.h"
#include "api/transport/network_types.h"
#include "api/transport/webrtc_key_value_config.h"
#include "modules/include/module.h"
#include "modules/pacing/bitrate_prober.h"
#include "modules/pacing/interval_budget.h"
#include "modules/pacing/pacing_controller.h"
#include "modules/pacing/packet_router.h"
#include "modules/pacing/rtp_packet_pacer.h"
#include "modules/rtp_rtcp/include/rtp_packet_sender.h"
#include "modules/rtp_rtcp/source/rtp_packet_to_send.h"
#include "modules/utility/include/process_thread.h"
#include "rtc_base/critical_section.h"
#include "rtc_base/thread_annotations.h"

namespace webrtc {
class Clock;
class RtcEventLog;

class PacedSender : public Module,
                    public RtpPacketPacer,
                    public RtpPacketSender,
                    private PacingController::PacketSender {
 public:
  // Expected max pacer delay in ms. If ExpectedQueueTime() is higher than
  // this value, the packet producers should wait (eg drop frames rather than
  // encoding them). Bitrate sent may temporarily exceed target set by
  // UpdateBitrate() so that this limit will be upheld.
  static const int64_t kMaxQueueLengthMs;
  // Pacing-rate relative to our target send rate.
  // Multiplicative factor that is applied to the target bitrate to calculate
  // the number of bytes that can be transmitted per interval.
  // Increasing this factor will result in lower delays in cases of bitrate
  // overshoots from the encoder.
  static const float kDefaultPaceMultiplier;

  PacedSender(Clock* clock,
              PacketRouter* packet_router,
              RtcEventLog* event_log,
              const WebRtcKeyValueConfig* field_trials = nullptr);

  ~PacedSender() override;

  // Methods implementing RtpPacketSender.

  // Adds the packet information to the queue and calls TimeToSendPacket
  // when it's time to send.
  void InsertPacket(RtpPacketSender::Priority priority,
                    uint32_t ssrc,
                    uint16_t sequence_number,
                    int64_t capture_time_ms,
                    size_t bytes,
                    bool retransmission) override;
  // Adds the packet to the queue and calls PacketRouter::SendPacket() when
  // it's time to send.
  void EnqueuePacket(std::unique_ptr<RtpPacketToSend> packet) override;

  // Methods implementing RtpPacketPacer:

  void CreateProbeCluster(DataRate bitrate, int cluster_id) override;

  // Temporarily pause all sending.
  void Pause() override;

  // Resume sending packets.
  void Resume() override;

  void SetCongestionWindow(DataSize congestion_window_size) override;
  void UpdateOutstandingData(DataSize outstanding_data) override;

  // Sets the pacing rates. Must be called once before packets can be sent.
  void SetPacingRates(DataRate pacing_rate, DataRate padding_rate) override;

  // Currently audio traffic is not accounted by pacer and passed through.
  // With the introduction of audio BWE audio traffic will be accounted for
  // the pacer budget calculation. The audio traffic still will be injected
  // at high priority.
  void SetAccountForAudioPackets(bool account_for_audio) override;

  // Returns the time since the oldest queued packet was enqueued.
  TimeDelta OldestPacketWaitTime() const override;

  size_t QueueSizePackets() const override;
  DataSize QueueSizeData() const override;

  // Returns the time when the first packet was sent;
  absl::optional<Timestamp> FirstSentPacketTime() const override;

  // Returns the number of milliseconds it will take to send the current
  // packets in the queue, given the current size and bitrate, ignoring prio.
  TimeDelta ExpectedQueueTime() const override;

  void SetQueueTimeLimit(TimeDelta limit) override;

  // Below are methods specific to this implementation, such as things related
  // to module processing thread specifics or methods exposed for test.

  // TODO(bugs.webrtc.org/10809): Remove when cleanup up unit tests.
  // Enable bitrate probing. Enabled by default, mostly here to simplify
  // testing. Must be called before any packets are being sent to have an
  // effect.
  void SetProbingEnabled(bool enabled);

  // Methods implementing Module.

  // Returns the number of milliseconds until the module want a worker thread
  // to call Process.
  int64_t TimeUntilNextProcess() override;

  // Process any pending packets in the queue(s).
  void Process() override;

  // Called when the prober is associated with a process thread.
  void ProcessThreadAttached(ProcessThread* process_thread) override;

 private:
  // Methods implementing PacedSenderController:PacketSender.

  void SendRtpPacket(std::unique_ptr<RtpPacketToSend> packet,
                     const PacedPacketInfo& cluster_info) override
      RTC_EXCLUSIVE_LOCKS_REQUIRED(critsect_);

  std::vector<std::unique_ptr<RtpPacketToSend>> GeneratePadding(
      DataSize size) override RTC_EXCLUSIVE_LOCKS_REQUIRED(critsect_);

  // TODO(bugs.webrtc.org/10633): Remove these when old code path is gone.
  RtpPacketSendResult TimeToSendPacket(uint32_t ssrc,
                                       uint16_t sequence_number,
                                       int64_t capture_timestamp,
                                       bool retransmission,
                                       const PacedPacketInfo& packet_info)
      override RTC_EXCLUSIVE_LOCKS_REQUIRED(critsect_);
  DataSize TimeToSendPadding(DataSize size,
                             const PacedPacketInfo& pacing_info) override
      RTC_EXCLUSIVE_LOCKS_REQUIRED(critsect_);

  rtc::CriticalSection critsect_;
  PacingController pacing_controller_ RTC_GUARDED_BY(critsect_);

  PacketRouter* const packet_router_;

  // Lock to avoid race when attaching process thread. This can happen due to
  // the Call class setting network state on RtpTransportControllerSend, which
  // in turn calls Pause/Resume on Pacedsender, before actually starting the
  // pacer process thread. If RtpTransportControllerSend is running on a task
  // queue separate from the thread used by Call, this causes a race.
  rtc::CriticalSection process_thread_lock_;
  ProcessThread* process_thread_ RTC_GUARDED_BY(process_thread_lock_);
};
}  // namespace webrtc
#endif  // MODULES_PACING_PACED_SENDER_H_
