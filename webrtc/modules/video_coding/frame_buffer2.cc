/*
 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/modules/video_coding/frame_buffer2.h"

#include <algorithm>

#include "webrtc/base/checks.h"
#include "webrtc/modules/video_coding/frame_object.h"
#include "webrtc/modules/video_coding/jitter_estimator.h"
#include "webrtc/modules/video_coding/sequence_number_util.h"
#include "webrtc/modules/video_coding/timing.h"
#include "webrtc/system_wrappers/include/clock.h"

namespace webrtc {
namespace video_coding {

namespace {
// The maximum age of decoded frames tracked by frame buffer, compared to
// |newest_picture_id_|.
constexpr int kMaxFrameAge = 4096;

// The maximum number of decoded frames being tracked by the frame buffer.
constexpr int kMaxNumHistoryFrames = 256;
}  // namespace

bool FrameBuffer::FrameComp::operator()(const FrameKey& f1,
                                        const FrameKey& f2) const {
  // first = picture id
  // second = spatial layer
  if (f1.first == f2.first)
    return f1.second < f2.second;
  return AheadOf(f2.first, f1.first);
}

FrameBuffer::FrameBuffer(Clock* clock,
                         VCMJitterEstimator* jitter_estimator,
                         VCMTiming* timing)
    : clock_(clock),
      frame_inserted_event_(false, false),
      jitter_estimator_(jitter_estimator),
      timing_(timing),
      inter_frame_delay_(clock_->TimeInMilliseconds()),
      newest_picture_id_(-1),
      stopped_(false),
      protection_mode_(kProtectionNack) {}

FrameBuffer::ReturnReason FrameBuffer::NextFrame(
    int64_t max_wait_time_ms,
    std::unique_ptr<FrameObject>* frame_out) {
  int64_t latest_return_time = clock_->TimeInMilliseconds() + max_wait_time_ms;
  int64_t now = clock_->TimeInMilliseconds();
  int64_t wait_ms = max_wait_time_ms;
  while (true) {
    std::map<FrameKey, std::unique_ptr<FrameObject>, FrameComp>::iterator
        next_frame_it;
    {
      rtc::CritScope lock(&crit_);
      frame_inserted_event_.Reset();
      if (stopped_)
        return kStopped;

      now = clock_->TimeInMilliseconds();
      wait_ms = max_wait_time_ms;
      next_frame_it = frames_.end();
      for (auto frame_it = frames_.begin(); frame_it != frames_.end();
           ++frame_it) {
        const FrameObject& frame = *frame_it->second;
        if (IsContinuous(frame)) {
          next_frame_it = frame_it;
          int64_t render_time =
              next_frame_it->second->RenderTime() == -1
                  ? timing_->RenderTimeMs(frame.timestamp, now)
                  : next_frame_it->second->RenderTime();
          wait_ms = timing_->MaxWaitingTime(render_time, now);
          frame_it->second->SetRenderTime(render_time);

          // This will cause the frame buffer to prefer high framerate rather
          // than high resolution in the case of the decoder not decoding fast
          // enough and the stream has multiple spatial and temporal layers.
          if (wait_ms == 0)
            continue;

          break;
        }
      }
    }

    wait_ms = std::min<int64_t>(wait_ms, latest_return_time - now);
    wait_ms = std::max<int64_t>(wait_ms, 0);
    // If the timeout occurs, return. Otherwise a new frame has been inserted
    // and the best frame to decode next will be selected again.
    if (!frame_inserted_event_.Wait(wait_ms)) {
      rtc::CritScope lock(&crit_);
      if (next_frame_it != frames_.end()) {
        int64_t received_timestamp = next_frame_it->second->ReceivedTime();
        uint32_t timestamp = next_frame_it->second->Timestamp();

        int64_t frame_delay;
        if (inter_frame_delay_.CalculateDelay(timestamp, &frame_delay,
                                              received_timestamp)) {
          jitter_estimator_->UpdateEstimate(frame_delay,
                                            next_frame_it->second->size);
        }
        float rtt_mult = protection_mode_ == kProtectionNackFEC ? 0.0 : 1.0;
        timing_->SetJitterDelay(jitter_estimator_->GetJitterEstimate(rtt_mult));
        timing_->UpdateCurrentDelay(next_frame_it->second->RenderTime(),
                                    clock_->TimeInMilliseconds());

        decoded_frames_.insert(next_frame_it->first);
        std::unique_ptr<FrameObject> frame = std::move(next_frame_it->second);
        frames_.erase(frames_.begin(), ++next_frame_it);
        *frame_out = std::move(frame);
        return kFrameFound;
      } else {
        return kTimeout;
      }
    }
  }
}

void FrameBuffer::SetProtectionMode(VCMVideoProtection mode) {
  rtc::CritScope lock(&crit_);
  protection_mode_ = mode;
}

void FrameBuffer::Start() {
  rtc::CritScope lock(&crit_);
  stopped_ = false;
}

void FrameBuffer::Stop() {
  rtc::CritScope lock(&crit_);
  stopped_ = true;
  frame_inserted_event_.Set();
}

void FrameBuffer::InsertFrame(std::unique_ptr<FrameObject> frame) {
  rtc::CritScope lock(&crit_);
  // If |newest_picture_id_| is -1 then this is the first frame we received.
  if (newest_picture_id_ == -1)
    newest_picture_id_ = frame->picture_id;

  if (AheadOf<uint16_t>(frame->picture_id, newest_picture_id_))
    newest_picture_id_ = frame->picture_id;

  // Remove frames as long as we have too many, |kMaxNumHistoryFrames|.
  while (decoded_frames_.size() > kMaxNumHistoryFrames)
    decoded_frames_.erase(decoded_frames_.begin());

  // Remove frames that are too old.
  uint16_t old_picture_id = Subtract<1 << 16>(newest_picture_id_, kMaxFrameAge);
  auto old_decoded_it =
      decoded_frames_.lower_bound(FrameKey(old_picture_id, 0));
  decoded_frames_.erase(decoded_frames_.begin(), old_decoded_it);

  FrameKey key(frame->picture_id, frame->spatial_layer);
  frames_[key] = std::move(frame);
  frame_inserted_event_.Set();
}

bool FrameBuffer::IsContinuous(const FrameObject& frame) const {
  // If a frame with an earlier picture id was inserted compared to the last
  // decoded frames picture id then that frame arrived too late.
  if (!decoded_frames_.empty() &&
      AheadOf(decoded_frames_.rbegin()->first, frame.picture_id)) {
    return false;
  }

  // Have we decoded all frames that this frame depend on?
  for (size_t r = 0; r < frame.num_references; ++r) {
    FrameKey ref_key(frame.references[r], frame.spatial_layer);
    if (decoded_frames_.find(ref_key) == decoded_frames_.end())
      return false;
  }

  // If this is a layer frame, have we decoded the lower layer of this
  // super frame.
  if (frame.inter_layer_predicted) {
    RTC_DCHECK_GT(frame.spatial_layer, 0);
    FrameKey ref_key(frame.picture_id, frame.spatial_layer - 1);
    if (decoded_frames_.find(ref_key) == decoded_frames_.end())
      return false;
  }

  return true;
}

}  // namespace video_coding
}  // namespace webrtc
