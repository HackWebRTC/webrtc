/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#include "webrtc/modules/video_coding/main/source/jitter_buffer.h"

#include <algorithm>
#include <cassert>

#include "webrtc/modules/video_coding/main/interface/video_coding.h"
#include "webrtc/modules/video_coding/main/source/frame_buffer.h"
#include "webrtc/modules/video_coding/main/source/inter_frame_delay.h"
#include "webrtc/modules/video_coding/main/source/internal_defines.h"
#include "webrtc/modules/video_coding/main/source/jitter_buffer_common.h"
#include "webrtc/modules/video_coding/main/source/jitter_estimator.h"
#include "webrtc/modules/video_coding/main/source/packet.h"
#include "webrtc/system_wrappers/interface/clock.h"
#include "webrtc/system_wrappers/interface/critical_section_wrapper.h"
#include "webrtc/system_wrappers/interface/event_wrapper.h"
#include "webrtc/system_wrappers/interface/logging.h"
#include "webrtc/system_wrappers/interface/trace.h"
#include "webrtc/system_wrappers/interface/trace_event.h"

namespace webrtc {

// Use this rtt if no value has been reported.
static const uint32_t kDefaultRtt = 200;

// Predicates used when searching for frames in the frame buffer list
class FrameSmallerTimestamp {
 public:
  explicit FrameSmallerTimestamp(uint32_t timestamp) : timestamp_(timestamp) {}
  bool operator()(VCMFrameBuffer* frame) {
    return IsNewerTimestamp(timestamp_, frame->TimeStamp());
  }

 private:
  uint32_t timestamp_;
};

class FrameEqualTimestamp {
 public:
  explicit FrameEqualTimestamp(uint32_t timestamp) : timestamp_(timestamp) {}
  bool operator()(VCMFrameBuffer* frame) {
    return (timestamp_ == frame->TimeStamp());
  }

 private:
  uint32_t timestamp_;
};

class KeyFrameCriteria {
 public:
  bool operator()(VCMFrameBuffer* frame) {
    return frame->FrameType() == kVideoFrameKey;
  }
};

class CompleteKeyFrameCriteria {
 public:
  bool operator()(VCMFrameBuffer* frame) {
    return (frame->FrameType() == kVideoFrameKey &&
        frame->GetState() == kStateComplete);
  }
};

bool HasNonEmptyState(VCMFrameBuffer* frame) {
  return frame->GetState() != kStateEmpty;
}

VCMJitterBuffer::VCMJitterBuffer(Clock* clock,
                                 EventFactory* event_factory,
                                 int vcm_id,
                                 int receiver_id,
                                 bool master)
    : vcm_id_(vcm_id),
      receiver_id_(receiver_id),
      clock_(clock),
      running_(false),
      crit_sect_(CriticalSectionWrapper::CreateCriticalSection()),
      master_(master),
      frame_event_(event_factory->CreateEvent()),
      packet_event_(event_factory->CreateEvent()),
      max_number_of_frames_(kStartNumberOfFrames),
      frame_buffers_(),
      frame_list_(),
      last_decoded_state_(),
      first_packet_(true),
      num_not_decodable_packets_(0),
      receive_statistics_(),
      incoming_frame_rate_(0),
      incoming_frame_count_(0),
      time_last_incoming_frame_count_(0),
      incoming_bit_count_(0),
      incoming_bit_rate_(0),
      drop_count_(0),
      num_consecutive_old_frames_(0),
      num_consecutive_old_packets_(0),
      num_discarded_packets_(0),
      jitter_estimate_(vcm_id, receiver_id),
      inter_frame_delay_(clock_->TimeInMilliseconds()),
      rtt_ms_(kDefaultRtt),
      nack_mode_(kNoNack),
      low_rtt_nack_threshold_ms_(-1),
      high_rtt_nack_threshold_ms_(-1),
      missing_sequence_numbers_(SequenceNumberLessThan()),
      nack_seq_nums_(),
      max_nack_list_size_(0),
      max_packet_age_to_nack_(0),
      max_incomplete_time_ms_(0),
      decode_with_errors_(false) {
  memset(frame_buffers_, 0, sizeof(frame_buffers_));
  memset(receive_statistics_, 0, sizeof(receive_statistics_));

  for (int i = 0; i < kStartNumberOfFrames; i++) {
    frame_buffers_[i] = new VCMFrameBuffer();
  }
}

VCMJitterBuffer::~VCMJitterBuffer() {
  Stop();
  for (int i = 0; i < kMaxNumberOfFrames; i++) {
    if (frame_buffers_[i]) {
      delete frame_buffers_[i];
    }
  }
  delete crit_sect_;
}

void VCMJitterBuffer::CopyFrom(const VCMJitterBuffer& rhs) {
  if (this != &rhs) {
    crit_sect_->Enter();
    rhs.crit_sect_->Enter();
    vcm_id_ = rhs.vcm_id_;
    receiver_id_ = rhs.receiver_id_;
    running_ = rhs.running_;
    master_ = !rhs.master_;
    max_number_of_frames_ = rhs.max_number_of_frames_;
    incoming_frame_rate_ = rhs.incoming_frame_rate_;
    incoming_frame_count_ = rhs.incoming_frame_count_;
    time_last_incoming_frame_count_ = rhs.time_last_incoming_frame_count_;
    incoming_bit_count_ = rhs.incoming_bit_count_;
    incoming_bit_rate_ = rhs.incoming_bit_rate_;
    drop_count_ = rhs.drop_count_;
    num_consecutive_old_frames_ = rhs.num_consecutive_old_frames_;
    num_consecutive_old_packets_ = rhs.num_consecutive_old_packets_;
    num_discarded_packets_ = rhs.num_discarded_packets_;
    jitter_estimate_ = rhs.jitter_estimate_;
    inter_frame_delay_ = rhs.inter_frame_delay_;
    waiting_for_completion_ = rhs.waiting_for_completion_;
    rtt_ms_ = rhs.rtt_ms_;
    first_packet_ = rhs.first_packet_;
    last_decoded_state_ =  rhs.last_decoded_state_;
    num_not_decodable_packets_ = rhs.num_not_decodable_packets_;
    decode_with_errors_ = rhs.decode_with_errors_;
    assert(max_nack_list_size_ == rhs.max_nack_list_size_);
    assert(max_packet_age_to_nack_ == rhs.max_packet_age_to_nack_);
    assert(max_incomplete_time_ms_ == rhs.max_incomplete_time_ms_);
    memcpy(receive_statistics_, rhs.receive_statistics_,
           sizeof(receive_statistics_));
    nack_seq_nums_.resize(rhs.nack_seq_nums_.size());
    missing_sequence_numbers_ = rhs.missing_sequence_numbers_;
    latest_received_sequence_number_ = rhs.latest_received_sequence_number_;
    for (int i = 0; i < kMaxNumberOfFrames; i++) {
      if (frame_buffers_[i] != NULL) {
        delete frame_buffers_[i];
        frame_buffers_[i] = NULL;
      }
    }
    frame_list_.clear();
    for (int i = 0; i < max_number_of_frames_; i++) {
      frame_buffers_[i] = new VCMFrameBuffer(*(rhs.frame_buffers_[i]));
      if (frame_buffers_[i]->Length() > 0) {
        FrameList::reverse_iterator rit = std::find_if(
            frame_list_.rbegin(), frame_list_.rend(),
            FrameSmallerTimestamp(frame_buffers_[i]->TimeStamp()));
        frame_list_.insert(rit.base(), frame_buffers_[i]);
      }
    }
    rhs.crit_sect_->Leave();
    crit_sect_->Leave();
  }
}

void VCMJitterBuffer::Start() {
  CriticalSectionScoped cs(crit_sect_);
  running_ = true;
  incoming_frame_count_ = 0;
  incoming_frame_rate_ = 0;
  incoming_bit_count_ = 0;
  incoming_bit_rate_ = 0;
  time_last_incoming_frame_count_ = clock_->TimeInMilliseconds();
  memset(receive_statistics_, 0, sizeof(receive_statistics_));

  num_consecutive_old_frames_ = 0;
  num_consecutive_old_packets_ = 0;
  num_discarded_packets_ = 0;

  // Start in a non-signaled state.
  frame_event_->Reset();
  packet_event_->Reset();
  waiting_for_completion_.frame_size = 0;
  waiting_for_completion_.timestamp = 0;
  waiting_for_completion_.latest_packet_time = -1;
  first_packet_ = true;
  rtt_ms_ = kDefaultRtt;
  num_not_decodable_packets_ = 0;
  last_decoded_state_.Reset();

  WEBRTC_TRACE(webrtc::kTraceDebug, webrtc::kTraceVideoCoding,
               VCMId(vcm_id_, receiver_id_), "JB(0x%x): Jitter buffer: start",
               this);
}

void VCMJitterBuffer::Stop() {
  crit_sect_->Enter();
  running_ = false;
  last_decoded_state_.Reset();
  frame_list_.clear();
  TRACE_EVENT_INSTANT1("webrtc", "JB::FrameListEmptied", "type", "Stop");
  for (int i = 0; i < kMaxNumberOfFrames; i++) {
    if (frame_buffers_[i] != NULL) {
      static_cast<VCMFrameBuffer*>(frame_buffers_[i])->SetState(kStateFree);
    }
  }

  crit_sect_->Leave();
  // Make sure we wake up any threads waiting on these events.
  frame_event_->Set();
  packet_event_->Set();
  WEBRTC_TRACE(webrtc::kTraceDebug, webrtc::kTraceVideoCoding,
               VCMId(vcm_id_, receiver_id_), "JB(0x%x): Jitter buffer: stop",
               this);
}

bool VCMJitterBuffer::Running() const {
  CriticalSectionScoped cs(crit_sect_);
  return running_;
}

void VCMJitterBuffer::Flush() {
  CriticalSectionScoped cs(crit_sect_);
  // Erase all frames from the sorted list and set their state to free.
  frame_list_.clear();
  TRACE_EVENT_INSTANT2("webrtc", "JB::FrameListEmptied", "type", "Flush",
                       "frames", max_number_of_frames_);
  for (int i = 0; i < max_number_of_frames_; i++) {
    ReleaseFrameIfNotDecoding(frame_buffers_[i]);
  }
  last_decoded_state_.Reset();  // TODO(mikhal): sync reset.
  num_not_decodable_packets_ = 0;
  frame_event_->Reset();
  packet_event_->Reset();
  num_consecutive_old_frames_ = 0;
  num_consecutive_old_packets_ = 0;
  // Also reset the jitter and delay estimates
  jitter_estimate_.Reset();
  inter_frame_delay_.Reset(clock_->TimeInMilliseconds());
  waiting_for_completion_.frame_size = 0;
  waiting_for_completion_.timestamp = 0;
  waiting_for_completion_.latest_packet_time = -1;
  first_packet_ = true;
  missing_sequence_numbers_.clear();
  WEBRTC_TRACE(webrtc::kTraceDebug, webrtc::kTraceVideoCoding,
               VCMId(vcm_id_, receiver_id_), "JB(0x%x): Jitter buffer: flush",
               this);
}

// Get received key and delta frames
void VCMJitterBuffer::FrameStatistics(uint32_t* received_delta_frames,
                                      uint32_t* received_key_frames) const {
  assert(received_delta_frames);
  assert(received_key_frames);
  CriticalSectionScoped cs(crit_sect_);
  *received_delta_frames = receive_statistics_[1] + receive_statistics_[3];
  *received_key_frames = receive_statistics_[0] + receive_statistics_[2];
}

int VCMJitterBuffer::num_not_decodable_packets() const {
  CriticalSectionScoped cs(crit_sect_);
  return num_not_decodable_packets_;
}

int VCMJitterBuffer::num_discarded_packets() const {
  CriticalSectionScoped cs(crit_sect_);
  return num_discarded_packets_;
}

// Calculate framerate and bitrate.
void VCMJitterBuffer::IncomingRateStatistics(unsigned int* framerate,
                                             unsigned int* bitrate) {
  assert(framerate);
  assert(bitrate);
  CriticalSectionScoped cs(crit_sect_);
  const int64_t now = clock_->TimeInMilliseconds();
  int64_t diff = now - time_last_incoming_frame_count_;
  if (diff < 1000 && incoming_frame_rate_ > 0 && incoming_bit_rate_ > 0) {
    // Make sure we report something even though less than
    // 1 second has passed since last update.
    *framerate = incoming_frame_rate_;
    *bitrate = incoming_bit_rate_;
  } else if (incoming_frame_count_ != 0) {
    // We have received frame(s) since last call to this function

    // Prepare calculations
    if (diff <= 0) {
      diff = 1;
    }
    // we add 0.5f for rounding
    float rate = 0.5f + ((incoming_frame_count_ * 1000.0f) / diff);
    if (rate < 1.0f) {
      rate = 1.0f;
    }

    // Calculate frame rate
    // Let r be rate.
    // r(0) = 1000*framecount/delta_time.
    // (I.e. frames per second since last calculation.)
    // frame_rate = r(0)/2 + r(-1)/2
    // (I.e. fr/s average this and the previous calculation.)
    *framerate = (incoming_frame_rate_ + static_cast<unsigned int>(rate)) / 2;
    incoming_frame_rate_ = static_cast<unsigned int>(rate);

    // Calculate bit rate
    if (incoming_bit_count_ == 0) {
      *bitrate = 0;
    } else {
      *bitrate = 10 * ((100 * incoming_bit_count_) /
                       static_cast<unsigned int>(diff));
    }
    incoming_bit_rate_ = *bitrate;

    // Reset count
    incoming_frame_count_ = 0;
    incoming_bit_count_ = 0;
    time_last_incoming_frame_count_ = now;

  } else {
    // No frames since last call
    time_last_incoming_frame_count_ = clock_->TimeInMilliseconds();
    *framerate = 0;
    *bitrate = 0;
    incoming_frame_rate_ = 0;
    incoming_bit_rate_ = 0;
  }
  TRACE_COUNTER1("webrtc", "JBIncomingFramerate", incoming_frame_rate_);
  TRACE_COUNTER1("webrtc", "JBIncomingBitrate", incoming_bit_rate_);
}

// Answers the question:
// Will the packet sequence be complete if the next frame is grabbed for
// decoding right now? That is, have we lost a frame between the last decoded
// frame and the next, or is the next
// frame missing one or more packets?
bool VCMJitterBuffer::CompleteSequenceWithNextFrame() {
  CriticalSectionScoped cs(crit_sect_);
  // Finding oldest frame ready for decoder, check sequence number and size
  CleanUpOldOrEmptyFrames();

  if (frame_list_.empty())
    return true;

  VCMFrameBuffer* oldest_frame = frame_list_.front();
  if (frame_list_.size() <= 1 &&
      oldest_frame->GetState() != kStateComplete) {
    // Frame not ready to be decoded.
    return true;
  }
  if (oldest_frame->GetState() != kStateComplete) {
    return false;
  }

  // See if we have lost a frame before this one.
  if (last_decoded_state_.in_initial_state()) {
    // Following start, reset or flush -> check for key frame.
    if (oldest_frame->FrameType() != kVideoFrameKey) {
      return false;
    }
  } else if (oldest_frame->GetLowSeqNum() == -1) {
    return false;
  } else if (!last_decoded_state_.ContinuousFrame(oldest_frame)) {
    return false;
  }
  return true;
}

// Returns immediately or a |max_wait_time_ms| ms event hang waiting for a
// complete frame, |max_wait_time_ms| decided by caller.
bool VCMJitterBuffer::NextCompleteTimestamp(
    uint32_t max_wait_time_ms, uint32_t* timestamp) {
  TRACE_EVENT0("webrtc", "JB::NextCompleteTimestamp");
  crit_sect_->Enter();
  if (!running_) {
    return false;
  }
  CleanUpOldOrEmptyFrames();

  FrameList::iterator it = FindOldestCompleteContinuousFrame(
      frame_list_.begin(), &last_decoded_state_);
  if (it == frame_list_.end()) {
    const int64_t end_wait_time_ms = clock_->TimeInMilliseconds() +
        max_wait_time_ms;
    int64_t wait_time_ms = max_wait_time_ms;
    while (wait_time_ms > 0) {
      crit_sect_->Leave();
      const EventTypeWrapper ret =
        frame_event_->Wait(static_cast<uint32_t>(wait_time_ms));
      crit_sect_->Enter();
      if (ret == kEventSignaled) {
        // Are we closing down the Jitter buffer?
        if (!running_) {
          crit_sect_->Leave();
          return false;
        }
        // Finding oldest frame ready for decoder, but check
        // sequence number and size
        CleanUpOldOrEmptyFrames();
        it = FindOldestCompleteContinuousFrame(
            frame_list_.begin(), &last_decoded_state_);
        if (it == frame_list_.end()) {
          wait_time_ms = end_wait_time_ms - clock_->TimeInMilliseconds();
        } else {
          break;
        }
      } else {
        break;
      }
    }
    // Inside |crit_sect_|.
  } else {
    // We already have a frame reset the event.
    frame_event_->Reset();
  }

  if (!decode_with_errors_ && it == frame_list_.end()) {
    // We're still missing a complete continuous frame.
    // Look for a complete key frame if we're not decoding with errors.
    it = find_if(frame_list_.begin(), frame_list_.end(),
        CompleteKeyFrameCriteria());
  }

  if (it == frame_list_.end()) {
      crit_sect_->Leave();
      return false;
  }

  VCMFrameBuffer* oldest_frame = *it;

  *timestamp = oldest_frame->TimeStamp();
  crit_sect_->Leave();
  return true;
}

bool VCMJitterBuffer::NextMaybeIncompleteTimestamp(
    uint32_t* timestamp) {
  TRACE_EVENT0("webrtc", "JB::NextMaybeIncompleteTimestamp");
  CriticalSectionScoped cs(crit_sect_);
  if (!running_) {
    return false;
  }
  if (!decode_with_errors_) {
    // No point to continue, as we are not decoding with errors.
    return false;
  }

  CleanUpOldOrEmptyFrames();

  if (frame_list_.empty()) {
    return false;
  }

  VCMFrameBuffer* oldest_frame = frame_list_.front();
  // If we have only one frame in the buffer, release it only if it is complete.
  if (frame_list_.size() <= 1 && oldest_frame->GetState() != kStateComplete) {
    return false;
  }

  // Always start with a key frame.
  if (last_decoded_state_.in_initial_state() &&
      oldest_frame->FrameType() != kVideoFrameKey) {
    return false;
  }

  *timestamp = oldest_frame->TimeStamp();
  return true;
}

VCMEncodedFrame* VCMJitterBuffer::ExtractAndSetDecode(uint32_t timestamp) {
  TRACE_EVENT0("webrtc", "JB::ExtractAndSetDecode");
  CriticalSectionScoped cs(crit_sect_);

  if (!running_) {
    return NULL;
  }
  // Extract the frame with the desired timestamp.
  FrameList::iterator it = std::find_if(
      frame_list_.begin(),
      frame_list_.end(),
      FrameEqualTimestamp(timestamp));

  if (it == frame_list_.end()) {
    return NULL;
  }
  // We got the frame.
  VCMFrameBuffer* frame = *it;
  frame_list_.erase(it);
  if (frame_list_.empty()) {
    TRACE_EVENT_INSTANT1("webrtc", "JB::FrameListEmptied",
                         "type", "ExtractAndSetDecode");
  }
  // Frame pulled out from jitter buffer, update the jitter estimate.
  const bool retransmitted = (frame->GetNackCount() > 0);
  if (retransmitted) {
    jitter_estimate_.FrameNacked();
  } else if (frame->Length() > 0) {
    // Ignore retransmitted and empty frames.
    if (waiting_for_completion_.latest_packet_time >= 0) {
      UpdateJitterEstimate(waiting_for_completion_, true);
    }
    if (frame->GetState() == kStateComplete) {
      UpdateJitterEstimate(*frame, false);
    } else {
      // Wait for this one to get complete.
      waiting_for_completion_.frame_size = frame->Length();
      waiting_for_completion_.latest_packet_time =
          frame->LatestPacketTimeMs();
      waiting_for_completion_.timestamp = frame->TimeStamp();
    }
  }
  // Look for previous frame loss.
  VerifyAndSetPreviousFrameLost(frame);

  // The state must be changed to decoding before cleaning up zero sized
  // frames to avoid empty frames being cleaned up and then given to the
  // decoder. Propagates the missing_frame bit.
  frame->SetState(kStateDecoding);

  num_not_decodable_packets_ += frame->NotDecodablePackets();

  // We have a frame - update the last decoded state and nack list.
  last_decoded_state_.SetState(frame);
  DropPacketsFromNackList(last_decoded_state_.sequence_num());
  return frame;
}

// Release frame when done with decoding. Should never be used to release
// frames from within the jitter buffer.
void VCMJitterBuffer::ReleaseFrame(VCMEncodedFrame* frame) {
  CriticalSectionScoped cs(crit_sect_);
  VCMFrameBuffer* frame_buffer = static_cast<VCMFrameBuffer*>(frame);
  if (frame_buffer)
    frame_buffer->SetState(kStateFree);
}

// Gets frame to use for this timestamp. If no match, get empty frame.
int VCMJitterBuffer::GetFrame(const VCMPacket& packet,
                              VCMEncodedFrame*& frame) {
  if (!running_) {  // Don't accept incoming packets until we are started.
    return VCM_UNINITIALIZED;
  }

  crit_sect_->Enter();
  // Does this packet belong to an old frame?
  if (last_decoded_state_.IsOldPacket(&packet)) {
    // Account only for media packets.
    if (packet.sizeBytes > 0) {
      num_discarded_packets_++;
      num_consecutive_old_packets_++;
      TRACE_EVENT_INSTANT2("webrtc", "JB::OldPacketDropped",
                           "seqnum", packet.seqNum,
                           "timestamp", packet.timestamp);
      TRACE_COUNTER1("webrtc", "JBDroppedOldPackets", num_discarded_packets_);
    }
    // Update last decoded sequence number if the packet arrived late and
    // belongs to a frame with a timestamp equal to the last decoded
    // timestamp.
    last_decoded_state_.UpdateOldPacket(&packet);
    DropPacketsFromNackList(last_decoded_state_.sequence_num());

    if (num_consecutive_old_packets_ > kMaxConsecutiveOldPackets) {
      Flush();
      crit_sect_->Leave();
      return VCM_FLUSH_INDICATOR;
    }
    crit_sect_->Leave();
    return VCM_OLD_PACKET_ERROR;
  }
  num_consecutive_old_packets_ = 0;

  FrameList::iterator it = std::find_if(
                             frame_list_.begin(),
                             frame_list_.end(),
                             FrameEqualTimestamp(packet.timestamp));

  if (it != frame_list_.end()) {
    frame = *it;
    crit_sect_->Leave();
    return VCM_OK;
  }

  crit_sect_->Leave();

  // No match, return empty frame.
  frame = GetEmptyFrame();
  if (frame != NULL) {
    return VCM_OK;
  }
  // No free frame! Try to reclaim some...
  crit_sect_->Enter();
  RecycleFramesUntilKeyFrame();
  crit_sect_->Leave();

  frame = GetEmptyFrame();
  if (frame != NULL) {
    return VCM_OK;
  }
  return VCM_JITTER_BUFFER_ERROR;
}

// Deprecated! Kept for testing purposes.
VCMEncodedFrame* VCMJitterBuffer::GetFrame(const VCMPacket& packet) {
  VCMEncodedFrame* frame = NULL;
  if (GetFrame(packet, frame) < 0) {
    return NULL;
  }
  return frame;
}

int64_t VCMJitterBuffer::LastPacketTime(VCMEncodedFrame* frame,
                                        bool* retransmitted) const {
  assert(retransmitted);
  CriticalSectionScoped cs(crit_sect_);
  *retransmitted = (static_cast<VCMFrameBuffer*>(frame)->GetNackCount() > 0);
  return static_cast<VCMFrameBuffer*>(frame)->LatestPacketTimeMs();
}

VCMFrameBufferEnum VCMJitterBuffer::InsertPacket(VCMEncodedFrame* encoded_frame,
                                                 const VCMPacket& packet) {
  assert(encoded_frame);
  bool request_key_frame = false;
  CriticalSectionScoped cs(crit_sect_);
  int64_t now_ms = clock_->TimeInMilliseconds();
  VCMFrameBufferEnum buffer_return = kSizeError;
  VCMFrameBufferEnum ret = kSizeError;
  VCMFrameBuffer* frame = static_cast<VCMFrameBuffer*>(encoded_frame);

  // If this packet belongs to an old, already decoded frame, we want to update
  // the last decoded sequence number.
  last_decoded_state_.UpdateOldPacket(&packet);

  // We are keeping track of the first seq num, the latest seq num and
  // the number of wraps to be able to calculate how many packets we expect.
  if (first_packet_) {
    // Now it's time to start estimating jitter
    // reset the delay estimate.
    inter_frame_delay_.Reset(clock_->TimeInMilliseconds());
    first_packet_ = false;
    latest_received_sequence_number_ = packet.seqNum;
  } else {
    if (IsPacketRetransmitted(packet)) {
      frame->IncrementNackCount();
    }
    if (!UpdateNackList(packet.seqNum)) {
      LOG_F(LS_INFO) << "Requesting key frame due to flushed NACK list.";
      request_key_frame = true;
    }
    latest_received_sequence_number_ = LatestSequenceNumber(
        latest_received_sequence_number_, packet.seqNum);
  }

  // Empty packets may bias the jitter estimate (lacking size component),
  // therefore don't let empty packet trigger the following updates:
  if (packet.frameType != kFrameEmpty) {
    if (waiting_for_completion_.timestamp == packet.timestamp) {
      // This can get bad if we have a lot of duplicate packets,
      // we will then count some packet multiple times.
      waiting_for_completion_.frame_size += packet.sizeBytes;
      waiting_for_completion_.latest_packet_time = now_ms;
    } else if (waiting_for_completion_.latest_packet_time >= 0 &&
               waiting_for_completion_.latest_packet_time + 2000 <= now_ms) {
      // A packet should never be more than two seconds late
      UpdateJitterEstimate(waiting_for_completion_, true);
      waiting_for_completion_.latest_packet_time = -1;
      waiting_for_completion_.frame_size = 0;
      waiting_for_completion_.timestamp = 0;
    }
  }

  VCMFrameBufferStateEnum state = frame->GetState();
  // Insert packet
  // Check for first packet
  // High sequence number will be -1 if neither an empty packet nor
  // a media packet has been inserted.
  bool first = (frame->GetHighSeqNum() == -1);
  // When in Hybrid mode, we allow for a decodable state
  // Note: Under current version, a decodable frame will never be
  // triggered, as the body of the function is empty.
  // TODO(mikhal): Update when decodable is enabled.
  buffer_return = frame->InsertPacket(packet, now_ms,
                                      decode_with_errors_,
                                      rtt_ms_);
  ret = buffer_return;
  if (buffer_return > 0) {
    incoming_bit_count_ += packet.sizeBytes << 3;

    // Insert each frame once on the arrival of the first packet
    // belonging to that frame (media or empty).
    if (state == kStateEmpty && first) {
      ret = kFirstPacket;
      FrameList::reverse_iterator rit = std::find_if(
          frame_list_.rbegin(),
          frame_list_.rend(),
          FrameSmallerTimestamp(frame->TimeStamp()));
      frame_list_.insert(rit.base(), frame);
    }
  }
  switch (buffer_return) {
    case kStateError:
    case kTimeStampError:
    case kSizeError: {
      if (frame != NULL) {
        // Will be released when it gets old.
        frame->Reset();
        frame->SetState(kStateEmpty);
      }
      break;
    }
    case kCompleteSession: {
      // Don't let the first packet be overridden by a complete session.
      ret = kCompleteSession;
      // Only update return value for a JB flush indicator.
      if (UpdateFrameState(frame) == kFlushIndicator)
        ret = kFlushIndicator;
      // Signal that we have a received packet.
      packet_event_->Set();
      break;
    }
    case kDecodableSession:
    case kIncomplete: {
      // Signal that we have a received packet.
      packet_event_->Set();
      break;
    }
    case kNoError:
    case kDuplicatePacket: {
      break;
    }
    default: {
      assert(false && "JitterBuffer::InsertPacket: Undefined value");
    }
  }
  if (request_key_frame) {
    ret = kFlushIndicator;
  }
  return ret;
}

void VCMJitterBuffer::SetMaxJitterEstimate(bool enable) {
  CriticalSectionScoped cs(crit_sect_);
  jitter_estimate_.SetMaxJitterEstimate(enable);
}

uint32_t VCMJitterBuffer::EstimatedJitterMs() {
  CriticalSectionScoped cs(crit_sect_);
  // Compute RTT multiplier for estimation.
  // low_rtt_nackThresholdMs_ == -1 means no FEC.
  double rtt_mult = 1.0f;
  if (low_rtt_nack_threshold_ms_ >= 0 &&
      static_cast<int>(rtt_ms_) >= low_rtt_nack_threshold_ms_) {
    // For RTTs above low_rtt_nack_threshold_ms_ we don't apply extra delay
    // when waiting for retransmissions.
    rtt_mult = 0.0f;
  }
  return jitter_estimate_.GetJitterEstimate(rtt_mult);
}

void VCMJitterBuffer::UpdateRtt(uint32_t rtt_ms) {
  CriticalSectionScoped cs(crit_sect_);
  rtt_ms_ = rtt_ms;
  jitter_estimate_.UpdateRtt(rtt_ms);
}

void VCMJitterBuffer::SetNackMode(VCMNackMode mode,
                                  int low_rtt_nack_threshold_ms,
                                  int high_rtt_nack_threshold_ms) {
  CriticalSectionScoped cs(crit_sect_);
  nack_mode_ = mode;
  if (mode == kNoNack) {
    missing_sequence_numbers_.clear();
  }
  assert(low_rtt_nack_threshold_ms >= -1 && high_rtt_nack_threshold_ms >= -1);
  assert(high_rtt_nack_threshold_ms == -1 ||
         low_rtt_nack_threshold_ms <= high_rtt_nack_threshold_ms);
  assert(low_rtt_nack_threshold_ms > -1 || high_rtt_nack_threshold_ms == -1);
  low_rtt_nack_threshold_ms_ = low_rtt_nack_threshold_ms;
  high_rtt_nack_threshold_ms_ = high_rtt_nack_threshold_ms;
  // Don't set a high start rtt if high_rtt_nack_threshold_ms_ is used, to not
  // disable NACK in hybrid mode.
  if (rtt_ms_ == kDefaultRtt && high_rtt_nack_threshold_ms_ != -1) {
    rtt_ms_ = 0;
  }
  if (!WaitForRetransmissions()) {
    jitter_estimate_.ResetNackCount();
  }
}

void VCMJitterBuffer::SetNackSettings(size_t max_nack_list_size,
                                      int max_packet_age_to_nack,
                                      int max_incomplete_time_ms) {
  CriticalSectionScoped cs(crit_sect_);
  assert(max_packet_age_to_nack >= 0);
  assert(max_incomplete_time_ms_ >= 0);
  max_nack_list_size_ = max_nack_list_size;
  max_packet_age_to_nack_ = max_packet_age_to_nack;
  max_incomplete_time_ms_ = max_incomplete_time_ms;
  nack_seq_nums_.resize(max_nack_list_size_);
}

VCMNackMode VCMJitterBuffer::nack_mode() const {
  CriticalSectionScoped cs(crit_sect_);
  return nack_mode_;
}

int VCMJitterBuffer::NonContinuousOrIncompleteDuration() {
  if (frame_list_.empty()) {
    return 0;
  }
  FrameList::iterator start_it;
  FrameList::iterator end_it;
  RenderBuffer(&start_it, &end_it);
  if (end_it == frame_list_.end())
    end_it = frame_list_.begin();
  return frame_list_.back()->TimeStamp() -
      (*end_it)->TimeStamp();
}

uint16_t VCMJitterBuffer::EstimatedLowSequenceNumber(
    const VCMFrameBuffer& frame) const {
  assert(frame.GetLowSeqNum() >= 0);
  if (frame.HaveFirstPacket())
    return frame.GetLowSeqNum();
  return frame.GetLowSeqNum() - 1;
}

uint16_t* VCMJitterBuffer::GetNackList(uint16_t* nack_list_size,
                                       bool* request_key_frame) {
  CriticalSectionScoped cs(crit_sect_);
  *request_key_frame = false;
  if (nack_mode_ == kNoNack) {
    *nack_list_size = 0;
    return NULL;
  }
  if (last_decoded_state_.in_initial_state()) {
    bool first_frame_is_key = !frame_list_.empty() &&
        frame_list_.front()->FrameType() == kVideoFrameKey &&
        frame_list_.front()->HaveFirstPacket();
    if (!first_frame_is_key) {
      const bool have_non_empty_frame = frame_list_.end() != find_if(
            frame_list_.begin(), frame_list_.end(), HasNonEmptyState);
      bool found_key_frame = RecycleFramesUntilKeyFrame();
      if (!found_key_frame) {
        *request_key_frame = have_non_empty_frame;
        *nack_list_size = 0;
        return NULL;
      }
    }
  }
  if (TooLargeNackList()) {
    TRACE_EVENT_INSTANT1("webrtc", "JB::NackListTooLarge",
                         "size", missing_sequence_numbers_.size());
    *request_key_frame = !HandleTooLargeNackList();
  }
  if (max_incomplete_time_ms_ > 0) {
    int non_continuous_incomplete_duration =
        NonContinuousOrIncompleteDuration();
    if (non_continuous_incomplete_duration > 90 * max_incomplete_time_ms_) {
      TRACE_EVENT_INSTANT1("webrtc", "JB::NonContinuousOrIncompleteDuration",
                           "duration", non_continuous_incomplete_duration);
      LOG_F(LS_INFO) << "Too long non-decodable duration: " <<
          non_continuous_incomplete_duration << " > " <<
          90 * max_incomplete_time_ms_;
      FrameList::reverse_iterator rit = find_if(frame_list_.rbegin(),
                                                frame_list_.rend(),
                                                KeyFrameCriteria());
      if (rit == frame_list_.rend()) {
        // Request a key frame if we don't have one already.
        *request_key_frame = true;
        *nack_list_size = 0;
        return NULL;
      } else {
        // Skip to the last key frame. If it's incomplete we will start
        // NACKing it.
        last_decoded_state_.Reset();
        DropPacketsFromNackList(EstimatedLowSequenceNumber(**rit));
      }
    }
  }
  unsigned int i = 0;
  SequenceNumberSet::iterator it = missing_sequence_numbers_.begin();
  for (; it != missing_sequence_numbers_.end(); ++it, ++i) {
    nack_seq_nums_[i] = *it;
  }
  *nack_list_size = i;
  return &nack_seq_nums_[0];
}

bool VCMJitterBuffer::UpdateNackList(uint16_t sequence_number) {
  if (nack_mode_ == kNoNack) {
    return true;
  }
  // Make sure we don't add packets which are already too old to be decoded.
  if (!last_decoded_state_.in_initial_state()) {
    latest_received_sequence_number_ = LatestSequenceNumber(
        latest_received_sequence_number_,
        last_decoded_state_.sequence_num());
  }
  if (IsNewerSequenceNumber(sequence_number,
                            latest_received_sequence_number_)) {
    // Push any missing sequence numbers to the NACK list.
    for (uint16_t i = latest_received_sequence_number_ + 1;
         IsNewerSequenceNumber(sequence_number, i); ++i) {
      missing_sequence_numbers_.insert(missing_sequence_numbers_.end(), i);
      TRACE_EVENT_INSTANT1("webrtc", "AddNack", "seqnum", i);
    }
    if (TooLargeNackList() && !HandleTooLargeNackList()) {
      return false;
    }
    if (MissingTooOldPacket(sequence_number) &&
        !HandleTooOldPackets(sequence_number)) {
      return false;
    }
  } else {
    missing_sequence_numbers_.erase(sequence_number);
    TRACE_EVENT_INSTANT1("webrtc", "RemoveNack", "seqnum", sequence_number);
  }
  return true;
}

bool VCMJitterBuffer::TooLargeNackList() const {
  return missing_sequence_numbers_.size() > max_nack_list_size_;
}

bool VCMJitterBuffer::HandleTooLargeNackList() {
  // Recycle frames until the NACK list is small enough. It is likely cheaper to
  // request a key frame than to retransmit this many missing packets.
  LOG_F(LS_INFO) << "NACK list has grown too large: " <<
      missing_sequence_numbers_.size() << " > " << max_nack_list_size_;
  bool key_frame_found = false;
  while (TooLargeNackList()) {
    key_frame_found = RecycleFramesUntilKeyFrame();
  }
  return key_frame_found;
}

bool VCMJitterBuffer::MissingTooOldPacket(
    uint16_t latest_sequence_number) const {
  if (missing_sequence_numbers_.empty()) {
    return false;
  }
  const uint16_t age_of_oldest_missing_packet = latest_sequence_number -
      *missing_sequence_numbers_.begin();
  // Recycle frames if the NACK list contains too old sequence numbers as
  // the packets may have already been dropped by the sender.
  return age_of_oldest_missing_packet > max_packet_age_to_nack_;
}

bool VCMJitterBuffer::HandleTooOldPackets(uint16_t latest_sequence_number) {
  bool key_frame_found = false;
  const uint16_t age_of_oldest_missing_packet = latest_sequence_number -
      *missing_sequence_numbers_.begin();
  LOG_F(LS_INFO) << "NACK list contains too old sequence numbers: " <<
      age_of_oldest_missing_packet << " > " << max_packet_age_to_nack_;
  while (MissingTooOldPacket(latest_sequence_number)) {
    key_frame_found = RecycleFramesUntilKeyFrame();
  }
  return key_frame_found;
}

void VCMJitterBuffer::DropPacketsFromNackList(
    uint16_t last_decoded_sequence_number) {
 TRACE_EVENT_INSTANT1("webrtc", "JB::DropPacketsFromNackList",
                      "seqnum", last_decoded_sequence_number);
  // Erase all sequence numbers from the NACK list which we won't need any
  // longer.
  missing_sequence_numbers_.erase(missing_sequence_numbers_.begin(),
                                  missing_sequence_numbers_.upper_bound(
                                      last_decoded_sequence_number));
}

int64_t VCMJitterBuffer::LastDecodedTimestamp() const {
  CriticalSectionScoped cs(crit_sect_);
  return last_decoded_state_.time_stamp();
}

FrameList::iterator VCMJitterBuffer::FindLastContinuousAndComplete(
    FrameList::iterator start_it) {
  // Search for a complete and continuous sequence (starting from the last
  // decoded state or current frame if in initial state).
  VCMDecodingState previous_state;
  previous_state.SetState(*start_it);
  FrameList::iterator previous_it = start_it;
  ++start_it;
  while (start_it != frame_list_.end()) {
    start_it = FindOldestCompleteContinuousFrame(start_it, &previous_state);
    if (start_it == frame_list_.end())
      break;
    previous_state.SetState(*start_it);
    previous_it = start_it;
    ++start_it;
  }
  // Desired frame is the previous one.
  return previous_it;
}

void VCMJitterBuffer::RenderBuffer(FrameList::iterator* start_it,
                                   FrameList::iterator* end_it) {
  *start_it = FindOldestCompleteContinuousFrame(
      frame_list_.begin(), &last_decoded_state_);
  if (!decode_with_errors_ && *start_it == frame_list_.end()) {
    // No complete continuous frame found.
    // Look for a complete key frame if we're not decoding with errors.
    *start_it = find_if(frame_list_.begin(), frame_list_.end(),
        CompleteKeyFrameCriteria());
  }
  if (*start_it == frame_list_.end()) {
    *end_it = *start_it;
  } else {
    *end_it = *start_it;
    // Look for the last complete key frame and use that as the end of the
    // render buffer it's later than the last complete continuous frame.
    FrameList::reverse_iterator rend(*end_it);
    FrameList::reverse_iterator rit = find_if(frame_list_.rbegin(),
                                              rend,
                                              CompleteKeyFrameCriteria());
    if (rit != rend) {
      // A key frame was found. The reverse iterator base points to the
      // frame after it, so subtracting 1.
      *end_it = rit.base();
      --*end_it;
    }
    *end_it = FindLastContinuousAndComplete(*end_it);
  }
}

void VCMJitterBuffer::RenderBufferSize(uint32_t* timestamp_start,
                                       uint32_t* timestamp_end) {
  CriticalSectionScoped cs(crit_sect_);
  CleanUpOldOrEmptyFrames();
  *timestamp_start = 0;
  *timestamp_end = 0;
  if (frame_list_.empty()) {
    return;
  }
  FrameList::iterator start_it;
  FrameList::iterator end_it;
  RenderBuffer(&start_it, &end_it);
  if (start_it == frame_list_.end()) {
    return;
  }
  *timestamp_start = (*start_it)->TimeStamp();
  *timestamp_end = (*end_it)->TimeStamp();
}

// Set the frame state to free and remove it from the sorted
// frame list. Must be called from inside the critical section crit_sect_.
void VCMJitterBuffer::ReleaseFrameIfNotDecoding(VCMFrameBuffer* frame) {
  if (frame != NULL && frame->GetState() != kStateDecoding) {
    frame->SetState(kStateFree);
  }
}

VCMFrameBuffer* VCMJitterBuffer::GetEmptyFrame() {
  if (!running_) {
    return NULL;
  }

  crit_sect_->Enter();

  for (int i = 0; i < max_number_of_frames_; ++i) {
    if (kStateFree == frame_buffers_[i]->GetState()) {
      // found a free buffer
      frame_buffers_[i]->SetState(kStateEmpty);
      crit_sect_->Leave();
      return frame_buffers_[i];
    }
  }

  // Check if we can increase JB size
  if (max_number_of_frames_ < kMaxNumberOfFrames) {
    VCMFrameBuffer* ptr_new_buffer = new VCMFrameBuffer();
    ptr_new_buffer->SetState(kStateEmpty);
    frame_buffers_[max_number_of_frames_] = ptr_new_buffer;
    max_number_of_frames_++;

    crit_sect_->Leave();
    WEBRTC_TRACE(webrtc::kTraceDebug, webrtc::kTraceVideoCoding,
                 VCMId(vcm_id_, receiver_id_),
                 "JB(0x%x) FB(0x%x): Jitter buffer  increased to:%d frames",
                 this, ptr_new_buffer, max_number_of_frames_);
    TRACE_COUNTER1("webrtc", "JBMaxFrames", max_number_of_frames_);
    return ptr_new_buffer;
  }
  crit_sect_->Leave();

  // We have reached max size, cannot increase JB size
  return NULL;
}

// Recycle oldest frames up to a key frame, used if jitter buffer is completely
// full.
bool VCMJitterBuffer::RecycleFramesUntilKeyFrame() {
  // Remove up to oldest key frame
  while (!frame_list_.empty()) {
    // Throw at least one frame.
    drop_count_++;
    FrameList::iterator it = frame_list_.begin();
    WEBRTC_TRACE(webrtc::kTraceWarning, webrtc::kTraceVideoCoding,
                 VCMId(vcm_id_, receiver_id_),
                 "Jitter buffer drop count:%d, low_seq %d", drop_count_,
                 (*it)->GetLowSeqNum());
    TRACE_EVENT_INSTANT0("webrtc", "JB::RecycleFramesUntilKeyFrame");
    ReleaseFrameIfNotDecoding(*it);
    it = frame_list_.erase(it);
    if (it != frame_list_.end() && (*it)->FrameType() == kVideoFrameKey) {
      // Reset last decoded state to make sure the next frame decoded is a key
      // frame, and start NACKing from here.
      last_decoded_state_.Reset();
      DropPacketsFromNackList(EstimatedLowSequenceNumber(**it));
      return true;
    }
  }
  if (frame_list_.empty()) {
    TRACE_EVENT_INSTANT1("webrtc", "JB::FrameListEmptied",
                         "type", "RecycleFramesUntilKeyFrame");
  }
  last_decoded_state_.Reset();  // TODO(mikhal): No sync.
  missing_sequence_numbers_.clear();
  return false;
}

// Must be called under the critical section |crit_sect_|.
VCMFrameBufferEnum VCMJitterBuffer::UpdateFrameState(VCMFrameBuffer* frame) {
  if (frame == NULL) {
    WEBRTC_TRACE(webrtc::kTraceWarning, webrtc::kTraceVideoCoding,
                 VCMId(vcm_id_, receiver_id_), "JB(0x%x) FB(0x%x): "
                 "UpdateFrameState NULL frame pointer", this, frame);
    return kNoError;
  }

  int length = frame->Length();
  if (master_) {
    // Only trace the primary jitter buffer to make it possible to parse
    // and plot the trace file.
    WEBRTC_TRACE(webrtc::kTraceDebug, webrtc::kTraceVideoCoding,
                 VCMId(vcm_id_, receiver_id_),
                 "JB(0x%x) FB(0x%x): Complete frame added to jitter buffer,"
                 " size:%d type %d",
                 this, frame, length, frame->FrameType());
  }

  bool frame_counted = false;
  if (length != 0 && !frame->GetCountedFrame()) {
    // Ignore ACK frames.
    incoming_frame_count_++;
    frame->SetCountedFrame(true);
    frame_counted = true;
  }

  // Check if we should drop the frame. A complete frame can arrive too late.
  if (last_decoded_state_.IsOldFrame(frame)) {
    // Frame is older than the latest decoded frame, drop it. Will be
    // released by CleanUpOldFrames later.
    TRACE_EVENT_INSTANT1("webrtc", "JB::DropLateFrame",
                         "timestamp", frame->TimeStamp());
    frame->Reset();
    frame->SetState(kStateEmpty);
    WEBRTC_TRACE(webrtc::kTraceDebug, webrtc::kTraceVideoCoding,
                 VCMId(vcm_id_, receiver_id_),
                 "JB(0x%x) FB(0x%x): Dropping old frame in Jitter buffer",
                 this, frame);
    drop_count_++;
    WEBRTC_TRACE(webrtc::kTraceWarning, webrtc::kTraceVideoCoding,
                 VCMId(vcm_id_, receiver_id_),
                 "Jitter buffer drop count: %d, consecutive drops: %u",
                 drop_count_, num_consecutive_old_frames_);
    // Flush() if this happens consistently.
    num_consecutive_old_frames_++;
    if (num_consecutive_old_frames_ > kMaxConsecutiveOldFrames) {
      Flush();
      return kFlushIndicator;
    }
    return kNoError;
  }
  num_consecutive_old_frames_ = 0;
  frame->SetState(kStateComplete);
  if (frame->FrameType() == kVideoFrameKey) {
    TRACE_EVENT_INSTANT2("webrtc", "JB::AddKeyFrame",
                         "timestamp", frame->TimeStamp(),
                         "retransmit", !frame_counted);
  } else {
    TRACE_EVENT_INSTANT2("webrtc", "JB::AddFrame",
                         "timestamp", frame->TimeStamp(),
                         "retransmit", !frame_counted);
  }

  // Update receive statistics. We count all layers, thus when you use layers
  // adding all key and delta frames might differ from frame count.
  if (frame->IsSessionComplete()) {
    switch (frame->FrameType()) {
      case kVideoFrameKey: {
        receive_statistics_[0]++;
        break;
      }
      case kVideoFrameDelta: {
        receive_statistics_[1]++;
        break;
      }
      case kVideoFrameGolden: {
        receive_statistics_[2]++;
        break;
      }
      case kVideoFrameAltRef: {
        receive_statistics_[3]++;
        break;
      }
      default:
        assert(false);
    }
  }
  const FrameList::iterator it = FindOldestCompleteContinuousFrame(
      frame_list_.begin(), &last_decoded_state_);
  VCMFrameBuffer* old_frame = NULL;
  if (it != frame_list_.end()) {
    old_frame = *it;
  }

  // Only signal if this is the oldest frame.
  // Not necessarily the case due to packet reordering or NACK.
  if (!WaitForRetransmissions() || (old_frame != NULL && old_frame == frame)) {
    frame_event_->Set();
  }
  return kNoError;
}

// Find oldest complete frame used for getting next frame to decode
// Must be called under critical section
FrameList::iterator VCMJitterBuffer::FindOldestCompleteContinuousFrame(
    FrameList::iterator start_it,
    const VCMDecodingState* decoding_state) {
  // If we have more than one frame done since last time, pick oldest.
  VCMFrameBuffer* oldest_frame = NULL;

  // When temporal layers are available, we search for a complete or decodable
  // frame until we hit one of the following:
  // 1. Continuous base or sync layer.
  // 2. The end of the list was reached.
  for (; start_it != frame_list_.end(); ++start_it)  {
    oldest_frame = *start_it;
    VCMFrameBufferStateEnum state = oldest_frame->GetState();
    // Is this frame complete or decodable and continuous?
    if ((state == kStateComplete ||
         (decode_with_errors_ && state == kStateDecodable)) &&
         decoding_state->ContinuousFrame(oldest_frame)) {
      break;
    } else {
      int temporal_id = oldest_frame->TemporalId();
      oldest_frame = NULL;
      if (temporal_id <= 0) {
        // When temporal layers are disabled or we have hit a base layer
        // we break (regardless of continuity and completeness).
        break;
      }
    }
  }

  if (oldest_frame == NULL) {
    // No complete frame no point to continue.
    return frame_list_.end();
  }

  // We have a complete continuous frame.
  return start_it;
}

// Must be called under the critical section |crit_sect_|.
void VCMJitterBuffer::CleanUpOldOrEmptyFrames() {
  while (frame_list_.size() > 0) {
    VCMFrameBuffer* oldest_frame = frame_list_.front();
    if (oldest_frame->GetState() == kStateEmpty && frame_list_.size() > 1) {
      // This frame is empty, mark it as decoded, thereby making it old.
      last_decoded_state_.UpdateEmptyFrame(oldest_frame);
    }
    if (last_decoded_state_.IsOldFrame(oldest_frame)) {
      ReleaseFrameIfNotDecoding(frame_list_.front());
      TRACE_EVENT_INSTANT1("webrtc", "JB::OldFrameDropped",
                           "timestamp", oldest_frame->TimeStamp());
      TRACE_COUNTER1("webrtc", "JBDroppedLateFrames", drop_count_);
      frame_list_.erase(frame_list_.begin());
    } else {
      break;
    }
  }
  if (frame_list_.empty()) {
    TRACE_EVENT_INSTANT1("webrtc", "JB::FrameListEmptied",
                         "type", "CleanUpOldOrEmptyFrames");
  }
  if (!last_decoded_state_.in_initial_state()) {
    DropPacketsFromNackList(last_decoded_state_.sequence_num());
  }
}

void VCMJitterBuffer::VerifyAndSetPreviousFrameLost(VCMFrameBuffer* frame) {
  assert(frame);
  frame->MakeSessionDecodable();  // Make sure the session can be decoded.
  if (frame->FrameType() == kVideoFrameKey)
    return;

  if (!last_decoded_state_.ContinuousFrame(frame))
    frame->SetPreviousFrameLoss();
}

// Must be called from within |crit_sect_|.
bool VCMJitterBuffer::IsPacketRetransmitted(const VCMPacket& packet) const {
  return missing_sequence_numbers_.find(packet.seqNum) !=
      missing_sequence_numbers_.end();
}

// Must be called under the critical section |crit_sect_|. Should never be
// called with retransmitted frames, they must be filtered out before this
// function is called.
void VCMJitterBuffer::UpdateJitterEstimate(const VCMJitterSample& sample,
                                           bool incomplete_frame) {
  if (sample.latest_packet_time == -1) {
    return;
  }
  if (incomplete_frame) {
    WEBRTC_TRACE(webrtc::kTraceDebug, webrtc::kTraceVideoCoding,
                 VCMId(vcm_id_, receiver_id_), "Received incomplete frame "
                 "timestamp %u frame size %u at time %u",
                 sample.timestamp, sample.frame_size,
                 MaskWord64ToUWord32(sample.latest_packet_time));
  } else {
    WEBRTC_TRACE(webrtc::kTraceDebug, webrtc::kTraceVideoCoding,
                 VCMId(vcm_id_, receiver_id_), "Received complete frame "
                 "timestamp %u frame size %u at time %u",
                 sample.timestamp, sample.frame_size,
                 MaskWord64ToUWord32(sample.latest_packet_time));
  }
  UpdateJitterEstimate(sample.latest_packet_time, sample.timestamp,
                       sample.frame_size, incomplete_frame);
}

// Must be called under the critical section crit_sect_. Should never be
// called with retransmitted frames, they must be filtered out before this
// function is called.
void VCMJitterBuffer::UpdateJitterEstimate(const VCMFrameBuffer& frame,
                                           bool incomplete_frame) {
  if (frame.LatestPacketTimeMs() == -1) {
    return;
  }
  // No retransmitted frames should be a part of the jitter
  // estimate.
  if (incomplete_frame) {
    WEBRTC_TRACE(webrtc::kTraceDebug, webrtc::kTraceVideoCoding,
                 VCMId(vcm_id_, receiver_id_),
                 "Received incomplete frame timestamp %u frame type %d "
                 "frame size %u at time %u, jitter estimate was %u",
                 frame.TimeStamp(), frame.FrameType(), frame.Length(),
                 MaskWord64ToUWord32(frame.LatestPacketTimeMs()),
                 EstimatedJitterMs());
  } else {
    WEBRTC_TRACE(webrtc::kTraceDebug, webrtc::kTraceVideoCoding,
                 VCMId(vcm_id_, receiver_id_), "Received complete frame "
                 "timestamp %u frame type %d frame size %u at time %u, "
                 "jitter estimate was %u",
                 frame.TimeStamp(), frame.FrameType(), frame.Length(),
                 MaskWord64ToUWord32(frame.LatestPacketTimeMs()),
                 EstimatedJitterMs());
  }
  UpdateJitterEstimate(frame.LatestPacketTimeMs(), frame.TimeStamp(),
                       frame.Length(), incomplete_frame);
}

// Must be called under the critical section |crit_sect_|. Should never be
// called with retransmitted frames, they must be filtered out before this
// function is called.
void VCMJitterBuffer::UpdateJitterEstimate(
    int64_t latest_packet_time_ms,
    uint32_t timestamp,
    unsigned int frame_size,
    bool incomplete_frame) {
  if (latest_packet_time_ms == -1) {
    return;
  }
  int64_t frame_delay;
  // Calculate the delay estimate
  WEBRTC_TRACE(webrtc::kTraceDebug, webrtc::kTraceVideoCoding,
               VCMId(vcm_id_, receiver_id_),
               "Packet received and sent to jitter estimate with: "
               "timestamp=%u wall_clock=%u", timestamp,
               MaskWord64ToUWord32(latest_packet_time_ms));
  bool not_reordered = inter_frame_delay_.CalculateDelay(timestamp,
                                                      &frame_delay,
                                                      latest_packet_time_ms);
  // Filter out frames which have been reordered in time by the network
  if (not_reordered) {
    // Update the jitter estimate with the new samples
    jitter_estimate_.UpdateEstimate(frame_delay, frame_size, incomplete_frame);
  }
}

bool VCMJitterBuffer::WaitForRetransmissions() {
  if (nack_mode_ == kNoNack) {
    // NACK disabled -> don't wait for retransmissions.
    return false;
  }
  // Evaluate if the RTT is higher than |high_rtt_nack_threshold_ms_|, and in
  // that case we don't wait for retransmissions.
  if (high_rtt_nack_threshold_ms_ >= 0 &&
      rtt_ms_ >= static_cast<unsigned int>(high_rtt_nack_threshold_ms_)) {
    return false;
  }
  return true;
}
}  // namespace webrtc
