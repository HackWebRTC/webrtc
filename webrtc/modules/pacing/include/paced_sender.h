/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MODULES_PACED_SENDER_H_
#define WEBRTC_MODULES_PACED_SENDER_H_

#include <list>
#include <set>

#include "webrtc/modules/interface/module.h"
#include "webrtc/system_wrappers/interface/scoped_ptr.h"
#include "webrtc/system_wrappers/interface/thread_annotations.h"
#include "webrtc/typedefs.h"

namespace webrtc {
class Clock;
class CriticalSectionWrapper;

namespace paced_sender {
class IntervalBudget;
struct Packet;
class PacketList;
}  // namespace paced_sender

class PacedSender : public Module {
 public:
  enum Priority {
    kHighPriority = 0,  // Pass through; will be sent immediately.
    kNormalPriority = 2,  // Put in back of the line.
    kLowPriority = 3,  // Put in back of the low priority line.
  };
  // Low priority packets are mixed with the normal priority packets
  // while we are paused.

  class Callback {
   public:
    // Note: packets sent as a result of a callback should not pass by this
    // module again.
    // Called when it's time to send a queued packet.
    // Returns false if packet cannot be sent.
    virtual bool TimeToSendPacket(uint32_t ssrc,
                                  uint16_t sequence_number,
                                  int64_t capture_time_ms,
                                  bool retransmission) = 0;
    // Called when it's a good time to send a padding data.
    // Returns the number of bytes sent.
    virtual int TimeToSendPadding(int bytes) = 0;

   protected:
    virtual ~Callback() {}
  };

  static const int kDefaultMaxQueueLengthMs = 2000;
  // Pace in kbits/s until we receive first estimate.
  static const int kDefaultInitialPaceKbps = 2000;
  // Pacing-rate relative to our target send rate.
  // Multiplicative factor that is applied to the target bitrate to calculate
  // the number of bytes that can be transmitted per interval.
  // Increasing this factor will result in lower delays in cases of bitrate
  // overshoots from the encoder.
  static const float kDefaultPaceMultiplier;

  PacedSender(Clock* clock, Callback* callback, int max_bitrate_kbps,
              int min_bitrate_kbps);

  virtual ~PacedSender();

  // Enable/disable pacing.
  void SetStatus(bool enable);

  bool Enabled() const;

  // Temporarily pause all sending.
  void Pause();

  // Resume sending packets.
  void Resume();

  // Set target bitrates for the pacer. Padding packets will be utilized to
  // reach |min_bitrate| unless enough media packets are available.
  void UpdateBitrate(int max_bitrate_kbps, int min_bitrate_kbps);

  // Returns true if we send the packet now, else it will add the packet
  // information to the queue and call TimeToSendPacket when it's time to send.
  virtual bool SendPacket(Priority priority,
                          uint32_t ssrc,
                          uint16_t sequence_number,
                          int64_t capture_time_ms,
                          int bytes,
                          bool retransmission);

  // Sets the max length of the pacer queue in milliseconds.
  // A negative queue size is interpreted as infinite.
  virtual void set_max_queue_length_ms(int max_queue_length_ms);

  // Returns the time since the oldest queued packet was enqueued.
  virtual int QueueInMs() const;

  // Returns the number of milliseconds until the module want a worker thread
  // to call Process.
  virtual int32_t TimeUntilNextProcess() OVERRIDE;

  // Process any pending packets in the queue(s).
  virtual int32_t Process() OVERRIDE;

 private:
  // Return true if next packet in line should be transmitted.
  // Return packet list that contains the next packet.
  bool ShouldSendNextPacket(paced_sender::PacketList** packet_list)
      EXCLUSIVE_LOCKS_REQUIRED(critsect_);

  // Local helper function to GetNextPacket.
  paced_sender::Packet GetNextPacketFromList(paced_sender::PacketList* packets)
      EXCLUSIVE_LOCKS_REQUIRED(critsect_);

  bool SendPacketFromList(paced_sender::PacketList* packet_list)
      EXCLUSIVE_LOCKS_REQUIRED(critsect_);

  // Updates the number of bytes that can be sent for the next time interval.
  void UpdateBytesPerInterval(uint32_t delta_time_in_ms)
      EXCLUSIVE_LOCKS_REQUIRED(critsect_);

  // Updates the buffers with the number of bytes that we sent.
  void UpdateMediaBytesSent(int num_bytes) EXCLUSIVE_LOCKS_REQUIRED(critsect_);

  Clock* const clock_;
  Callback* const callback_;

  scoped_ptr<CriticalSectionWrapper> critsect_;
  bool enabled_ GUARDED_BY(critsect_);
  bool paused_ GUARDED_BY(critsect_);
  int max_queue_length_ms_ GUARDED_BY(critsect_);
  // This is the media budget, keeping track of how many bits of media
  // we can pace out during the current interval.
  scoped_ptr<paced_sender::IntervalBudget> media_budget_ GUARDED_BY(critsect_);
  // This is the padding budget, keeping track of how many bits of padding we're
  // allowed to send out during the current interval. This budget will be
  // utilized when there's no media to send.
  scoped_ptr<paced_sender::IntervalBudget> padding_budget_
      GUARDED_BY(critsect_);

  int64_t time_last_update_us_ GUARDED_BY(critsect_);
  int64_t time_last_send_us_ GUARDED_BY(critsect_);
  int64_t capture_time_ms_last_queued_ GUARDED_BY(critsect_);
  int64_t capture_time_ms_last_sent_ GUARDED_BY(critsect_);

  scoped_ptr<paced_sender::PacketList> high_priority_packets_
      GUARDED_BY(critsect_);
  scoped_ptr<paced_sender::PacketList> normal_priority_packets_
      GUARDED_BY(critsect_);
  scoped_ptr<paced_sender::PacketList> low_priority_packets_
      GUARDED_BY(critsect_);
};
}  // namespace webrtc
#endif  // WEBRTC_MODULES_PACED_SENDER_H_
