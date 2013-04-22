/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "video_engine/stream_synchronization.h"

#include <assert.h>
#include <algorithm>
#include <cmath>

#include "system_wrappers/interface/trace.h"

namespace webrtc {

static const int kMaxChangeMs = 80;
static const int kMaxDeltaDelayMs = 10000;
static const int kFilterLength = 4;
// Minimum difference between audio and video to warrant a change.
static const int kMinDeltaMs = 30;

struct ViESyncDelay {
  ViESyncDelay() {
    extra_video_delay_ms = 0;
    last_video_delay_ms = 0;
    extra_audio_delay_ms = 0;
    last_sync_delay = 0;
    network_delay = 120;
  }

  int extra_video_delay_ms;
  int last_video_delay_ms;
  int extra_audio_delay_ms;
  int last_sync_delay;
  int network_delay;
};

StreamSynchronization::StreamSynchronization(int audio_channel_id,
                                             int video_channel_id)
    : channel_delay_(new ViESyncDelay),
      audio_channel_id_(audio_channel_id),
      video_channel_id_(video_channel_id),
      base_target_delay_ms_(0),
      avg_diff_ms_(0) {}

StreamSynchronization::~StreamSynchronization() {
  delete channel_delay_;
}

bool StreamSynchronization::ComputeRelativeDelay(
    const Measurements& audio_measurement,
    const Measurements& video_measurement,
    int* relative_delay_ms) {
  assert(relative_delay_ms);
  if (audio_measurement.rtcp.size() < 2 || video_measurement.rtcp.size() < 2) {
    // We need two RTCP SR reports per stream to do synchronization.
    return false;
  }
  int64_t audio_last_capture_time_ms;
  if (!synchronization::RtpToNtpMs(audio_measurement.latest_timestamp,
                                   audio_measurement.rtcp,
                                   &audio_last_capture_time_ms)) {
    return false;
  }
  int64_t video_last_capture_time_ms;
  if (!synchronization::RtpToNtpMs(video_measurement.latest_timestamp,
                                   video_measurement.rtcp,
                                   &video_last_capture_time_ms)) {
    return false;
  }
  if (video_last_capture_time_ms < 0) {
    return false;
  }
  // Positive diff means that video_measurement is behind audio_measurement.
  *relative_delay_ms = video_measurement.latest_receive_time_ms -
      audio_measurement.latest_receive_time_ms -
      (video_last_capture_time_ms - audio_last_capture_time_ms);
  if (*relative_delay_ms > kMaxDeltaDelayMs ||
      *relative_delay_ms < -kMaxDeltaDelayMs) {
    return false;
  }
  return true;
}

bool StreamSynchronization::ComputeDelays(int relative_delay_ms,
                                          int current_audio_delay_ms,
                                          int* extra_audio_delay_ms,
                                          int* total_video_delay_target_ms) {
  assert(extra_audio_delay_ms && total_video_delay_target_ms);
  WEBRTC_TRACE(webrtc::kTraceInfo, webrtc::kTraceVideo, video_channel_id_,
               "Audio delay is: %d for voice channel: %d",
               current_audio_delay_ms, audio_channel_id_);
  WEBRTC_TRACE(webrtc::kTraceInfo, webrtc::kTraceVideo, video_channel_id_,
               "Network delay diff is: %d for voice channel: %d",
               channel_delay_->network_delay, audio_channel_id_);
  // Calculate the difference between the lowest possible video delay and
  // the current audio delay.
  WEBRTC_TRACE(webrtc::kTraceInfo, webrtc::kTraceVideo, video_channel_id_,
               "Current diff is: %d for audio channel: %d",
               relative_delay_ms, audio_channel_id_);

  int current_diff_ms = *total_video_delay_target_ms - current_audio_delay_ms +
      relative_delay_ms;

  avg_diff_ms_ = ((kFilterLength - 1) * avg_diff_ms_ +
      current_diff_ms) / kFilterLength;
  if (abs(avg_diff_ms_) < kMinDeltaMs) {
    // Don't adjust if the diff is within our margin.
    return false;
  }

  // Make sure we don't move too fast.
  int diff_ms = avg_diff_ms_ / 2;
  diff_ms = std::min(diff_ms, kMaxChangeMs);
  diff_ms = std::max(diff_ms, -kMaxChangeMs);

  int video_delay_ms = base_target_delay_ms_;
  if (diff_ms > 0) {
    // The minimum video delay is longer than the current audio delay.
    // We need to decrease extra video delay, if we have added extra delay
    // earlier, or add extra audio delay.
    if (channel_delay_->extra_video_delay_ms > 0) {
      // We have extra delay added to ViE. Reduce this delay before adding
      // extra delay to VoE.

      // This is the desired delay, we can't reduce more than this.
      video_delay_ms = *total_video_delay_target_ms;

      // Check that we don't reduce the delay more than what is allowed.
      if (video_delay_ms < channel_delay_->last_video_delay_ms - diff_ms) {
        video_delay_ms = channel_delay_->last_video_delay_ms - diff_ms;
        channel_delay_->extra_video_delay_ms =
            video_delay_ms - *total_video_delay_target_ms;
      } else {
        channel_delay_->extra_video_delay_ms = 0;
      }
      channel_delay_->last_video_delay_ms = video_delay_ms;
      channel_delay_->last_sync_delay = -1;
      channel_delay_->extra_audio_delay_ms = base_target_delay_ms_;
    } else {  // channel_delay_->extra_video_delay_ms > 0
      // We have no extra video delay to remove, increase the audio delay.
      if (channel_delay_->last_sync_delay >= 0) {
        // We have increased the audio delay earlier, increase it even more.
        // Increase the audio delay.
        channel_delay_->extra_audio_delay_ms += diff_ms;

        // Don't set a too high delay.
        channel_delay_->extra_audio_delay_ms = std::min(
            channel_delay_->extra_audio_delay_ms,
            base_target_delay_ms_ + kMaxDeltaDelayMs);

        // Don't add any extra video delay.
        video_delay_ms = *total_video_delay_target_ms;
        channel_delay_->extra_video_delay_ms = 0;
        channel_delay_->last_video_delay_ms = video_delay_ms;
        channel_delay_->last_sync_delay = 1;
      } else {  // channel_delay_->last_sync_delay >= 0
        // First time after a delay change, don't add any extra delay.
        // This is to not toggle back and forth too much.
        channel_delay_->extra_audio_delay_ms = base_target_delay_ms_;
        // Set minimum video delay
        video_delay_ms = *total_video_delay_target_ms;
        channel_delay_->extra_video_delay_ms = 0;
        channel_delay_->last_video_delay_ms = video_delay_ms;
        channel_delay_->last_sync_delay = 0;
      }
    }
  } else {  // if (diff_ms > 0)
    // The minimum video delay is lower than the current audio delay.
    // We need to decrease possible extra audio delay, or
    // add extra video delay.
    if (channel_delay_->extra_audio_delay_ms > base_target_delay_ms_) {
      // We have extra delay in VoiceEngine.
      // Start with decreasing the voice delay.
      // Note: diff_ms is negative; add the negative difference.
      channel_delay_->extra_audio_delay_ms += diff_ms;

      if (channel_delay_->extra_audio_delay_ms < 0) {
        // Negative values not allowed.
        channel_delay_->extra_audio_delay_ms = base_target_delay_ms_;
        channel_delay_->last_sync_delay = 0;
      } else {
        // There is more audio delay to use for the next round.
        channel_delay_->last_sync_delay = 1;
      }

      // Keep the video delay at the minimum values.
      video_delay_ms = *total_video_delay_target_ms;
      channel_delay_->extra_video_delay_ms = 0;
      channel_delay_->last_video_delay_ms = video_delay_ms;
    } else {  // channel_delay_->extra_audio_delay_ms > 0
      // We have no extra delay in VoiceEngine, increase the video delay.
      channel_delay_->extra_audio_delay_ms = base_target_delay_ms_;

      // This is the desired delay.
      // Note: diff_ms is negative.
      video_delay_ms = std::max(*total_video_delay_target_ms,
                                channel_delay_->last_video_delay_ms) - diff_ms;

      // Verify we don't go above the maximum allowed delay.
      video_delay_ms = std::min(video_delay_ms,
                                base_target_delay_ms_ + kMaxDeltaDelayMs);

      // Store the values.
      channel_delay_->extra_video_delay_ms =
          video_delay_ms - *total_video_delay_target_ms;
      channel_delay_->last_video_delay_ms = video_delay_ms;
      channel_delay_->last_sync_delay = -1;
    }
  }
  avg_diff_ms_ = 0;
  WEBRTC_TRACE(webrtc::kTraceInfo, webrtc::kTraceVideo, video_channel_id_,
      "Sync video delay %d ms for video channel and audio delay %d for audio "
      "channel %d",
      video_delay_ms, channel_delay_->extra_audio_delay_ms, audio_channel_id_);

  *extra_audio_delay_ms = channel_delay_->extra_audio_delay_ms;
  video_delay_ms = std::max(video_delay_ms, 0);
  *total_video_delay_target_ms = std::max(*total_video_delay_target_ms,
                                          video_delay_ms);
  return true;
}

void StreamSynchronization::SetTargetBufferingDelay(int target_delay_ms) {
  // Initial extra delay for audio (accounting for existing extra delay).
  channel_delay_->extra_audio_delay_ms +=
      target_delay_ms - base_target_delay_ms_;
  // The video delay is compared to the last value (and how much we can update
  // is limited by that as well).
  channel_delay_->last_video_delay_ms +=
      target_delay_ms - base_target_delay_ms_;
  // Video is already delayed by the desired amount.
  base_target_delay_ms_ = target_delay_ms;
}

}  // namespace webrtc
