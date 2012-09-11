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

const int kMaxVideoDiffMs = 80;
const int kMaxAudioDiffMs = 80;
const int kMaxDelay = 1500;

const double kNtpFracPerMs = 4.294967296E6;

namespace synchronization {

RtcpMeasurement::RtcpMeasurement()
    : ntp_secs(0), ntp_frac(0), rtp_timestamp(0) {}

RtcpMeasurement::RtcpMeasurement(uint32_t ntp_secs, uint32_t ntp_frac,
                                 uint32_t timestamp)
    : ntp_secs(ntp_secs), ntp_frac(ntp_frac), rtp_timestamp(timestamp) {}

// Calculates the RTP timestamp frequency from two pairs of NTP and RTP
// timestamps.
bool CalculateFrequency(
    int64_t rtcp_ntp_ms1,
    uint32_t rtp_timestamp1,
    int64_t rtcp_ntp_ms2,
    uint32_t rtp_timestamp2,
    double* frequency_khz) {
  if (rtcp_ntp_ms1 == rtcp_ntp_ms2) {
    return false;
  }
  assert(rtcp_ntp_ms1 > rtcp_ntp_ms2);
  *frequency_khz = static_cast<double>(rtp_timestamp1 - rtp_timestamp2) /
      static_cast<double>(rtcp_ntp_ms1 - rtcp_ntp_ms2);
  return true;
}

// Detects if there has been a wraparound between |old_timestamp| and
// |new_timestamp|, and compensates by adding 2^32 if that is the case.
bool CompensateForWrapAround(uint32_t new_timestamp,
                             uint32_t old_timestamp,
                             int64_t* compensated_timestamp) {
  assert(compensated_timestamp);
  int64_t wraps = synchronization::CheckForWrapArounds(new_timestamp,
                                                       old_timestamp);
  if (wraps < 0) {
    // Reordering, don't use this packet.
    return false;
  }
  *compensated_timestamp = new_timestamp + (wraps << 32);
  return true;
}

// Converts an NTP timestamp to a millisecond timestamp.
int64_t NtpToMs(uint32_t ntp_secs, uint32_t ntp_frac) {
  const double ntp_frac_ms = static_cast<double>(ntp_frac) / kNtpFracPerMs;
  return ntp_secs * 1000 + ntp_frac_ms + 0.5;
}

// Converts |rtp_timestamp| to the NTP time base using the NTP and RTP timestamp
// pairs in |rtcp|. The converted timestamp is returned in
// |rtp_timestamp_in_ms|. This function compensates for wrap arounds in RTP
// timestamps and returns false if it can't do the conversion due to reordering.
bool RtpToNtpMs(int64_t rtp_timestamp,
                const synchronization::RtcpList& rtcp,
                int64_t* rtp_timestamp_in_ms) {
  assert(rtcp.size() == 2);
  int64_t rtcp_ntp_ms_new = synchronization::NtpToMs(rtcp.front().ntp_secs,
                                                     rtcp.front().ntp_frac);
  int64_t rtcp_ntp_ms_old = synchronization::NtpToMs(rtcp.back().ntp_secs,
                                                     rtcp.back().ntp_frac);
  int64_t rtcp_timestamp_new = rtcp.front().rtp_timestamp;
  int64_t rtcp_timestamp_old = rtcp.back().rtp_timestamp;
  if (!CompensateForWrapAround(rtcp_timestamp_new,
                               rtcp_timestamp_old,
                               &rtcp_timestamp_new)) {
    return false;
  }
  double freq_khz;
  if (!CalculateFrequency(rtcp_ntp_ms_new,
                          rtcp_timestamp_new,
                          rtcp_ntp_ms_old,
                          rtcp_timestamp_old,
                          &freq_khz)) {
    return false;
  }
  double offset = rtcp_timestamp_new - freq_khz * rtcp_ntp_ms_new;
  int64_t rtp_timestamp_unwrapped;
  if (!CompensateForWrapAround(rtp_timestamp, rtcp_timestamp_old,
                               &rtp_timestamp_unwrapped)) {
    return false;
  }
  double rtp_timestamp_ntp_ms = (static_cast<double>(rtp_timestamp_unwrapped) -
      offset) / freq_khz + 0.5f;
  assert(rtp_timestamp_ntp_ms >= 0);
  *rtp_timestamp_in_ms = rtp_timestamp_ntp_ms;
  return true;
}

int CheckForWrapArounds(uint32_t new_timestamp, uint32_t old_timestamp) {
  if (new_timestamp < old_timestamp) {
    // This difference should be less than -2^31 if we have had a wrap around
    // (e.g. |new_timestamp| = 1, |rtcp_rtp_timestamp| = 2^32 - 1). Since it is
    // cast to a int32_t, it should be positive.
    if (static_cast<int32_t>(new_timestamp - old_timestamp) > 0) {
      // Forward wrap around.
      return 1;
    }
  } else if (static_cast<int32_t>(old_timestamp - new_timestamp) > 0) {
    // This difference should be less than -2^31 if we have had a backward wrap
    // around. Since it is cast to a int32_t, it should be positive.
    return -1;
  }
  return 0;
}
}  // namespace synchronization

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
      video_channel_id_(video_channel_id) {}

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
  if (*relative_delay_ms > 1000 || *relative_delay_ms < -1000) {
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

  int video_delay_ms = 0;
  if (current_diff_ms > 0) {
    // The minimum video delay is longer than the current audio delay.
    // We need to decrease extra video delay, if we have added extra delay
    // earlier, or add extra audio delay.
    if (channel_delay_->extra_video_delay_ms > 0) {
      // We have extra delay added to ViE. Reduce this delay before adding
      // extra delay to VoE.

      // This is the desired delay, we can't reduce more than this.
      video_delay_ms = *total_video_delay_target_ms;

      // Check that we don't reduce the delay more than what is allowed.
      if (video_delay_ms <
          channel_delay_->last_video_delay_ms - kMaxVideoDiffMs) {
        video_delay_ms =
            channel_delay_->last_video_delay_ms - kMaxVideoDiffMs;
        channel_delay_->extra_video_delay_ms =
            video_delay_ms - *total_video_delay_target_ms;
      } else {
        channel_delay_->extra_video_delay_ms = 0;
      }
      channel_delay_->last_video_delay_ms = video_delay_ms;
      channel_delay_->last_sync_delay = -1;
      channel_delay_->extra_audio_delay_ms = 0;
    } else {  // channel_delay_->extra_video_delay_ms > 0
      // We have no extra video delay to remove, increase the audio delay.
      if (channel_delay_->last_sync_delay >= 0) {
        // We have increased the audio delay earlier, increase it even more.
        int audio_diff_ms = current_diff_ms / 2;
        if (audio_diff_ms > kMaxAudioDiffMs) {
          // We only allow a maximum change of KMaxAudioDiffMS for audio
          // due to NetEQ maximum changes.
          audio_diff_ms = kMaxAudioDiffMs;
        }
        // Increase the audio delay
        channel_delay_->extra_audio_delay_ms += audio_diff_ms;

        // Don't set a too high delay.
        if (channel_delay_->extra_audio_delay_ms > kMaxDelay) {
          channel_delay_->extra_audio_delay_ms = kMaxDelay;
        }

        // Don't add any extra video delay.
        video_delay_ms = *total_video_delay_target_ms;
        channel_delay_->extra_video_delay_ms = 0;
        channel_delay_->last_video_delay_ms = video_delay_ms;
        channel_delay_->last_sync_delay = 1;
      } else {  // channel_delay_->last_sync_delay >= 0
        // First time after a delay change, don't add any extra delay.
        // This is to not toggle back and forth too much.
        channel_delay_->extra_audio_delay_ms = 0;
        // Set minimum video delay
        video_delay_ms = *total_video_delay_target_ms;
        channel_delay_->extra_video_delay_ms = 0;
        channel_delay_->last_video_delay_ms = video_delay_ms;
        channel_delay_->last_sync_delay = 0;
      }
    }
  } else {  // if (current_diffMS > 0)
    // The minimum video delay is lower than the current audio delay.
    // We need to decrease possible extra audio delay, or
    // add extra video delay.

    if (channel_delay_->extra_audio_delay_ms > 0) {
      // We have extra delay in VoiceEngine
      // Start with decreasing the voice delay
      int audio_diff_ms = current_diff_ms / 2;
      if (audio_diff_ms < -1 * kMaxAudioDiffMs) {
        // Don't change the delay too much at once.
        audio_diff_ms = -1 * kMaxAudioDiffMs;
      }
      // Add the negative difference.
      channel_delay_->extra_audio_delay_ms += audio_diff_ms;

      if (channel_delay_->extra_audio_delay_ms < 0) {
        // Negative values not allowed.
        channel_delay_->extra_audio_delay_ms = 0;
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
      channel_delay_->extra_audio_delay_ms = 0;

      // Make the difference positive.
      int video_diff_ms = -1 * current_diff_ms;

      // This is the desired delay.
      video_delay_ms = *total_video_delay_target_ms + video_diff_ms;
      if (video_delay_ms > channel_delay_->last_video_delay_ms) {
        if (video_delay_ms >
            channel_delay_->last_video_delay_ms + kMaxVideoDiffMs) {
          // Don't increase the delay too much at once
          video_delay_ms =
              channel_delay_->last_video_delay_ms + kMaxVideoDiffMs;
        }
        // Verify we don't go above the maximum allowed delay
        if (video_delay_ms > kMaxDelay) {
          video_delay_ms = kMaxDelay;
        }
      } else {
        if (video_delay_ms <
            channel_delay_->last_video_delay_ms - kMaxVideoDiffMs) {
          // Don't decrease the delay too much at once
          video_delay_ms =
              channel_delay_->last_video_delay_ms - kMaxVideoDiffMs;
        }
        // Verify we don't go below the minimum delay
        if (video_delay_ms < *total_video_delay_target_ms) {
          video_delay_ms = *total_video_delay_target_ms;
        }
      }
      // Store the values
      channel_delay_->extra_video_delay_ms =
          video_delay_ms - *total_video_delay_target_ms;
      channel_delay_->last_video_delay_ms = video_delay_ms;
      channel_delay_->last_sync_delay = -1;
    }
  }

  WEBRTC_TRACE(webrtc::kTraceInfo, webrtc::kTraceVideo, video_channel_id_,
      "Sync video delay %d ms for video channel and audio delay %d for audio "
      "channel %d",
      video_delay_ms, channel_delay_->extra_audio_delay_ms, audio_channel_id_);

  *extra_audio_delay_ms = channel_delay_->extra_audio_delay_ms;

  if (video_delay_ms < 0) {
    video_delay_ms = 0;
  }
  *total_video_delay_target_ms =
      (*total_video_delay_target_ms  >  video_delay_ms) ?
      *total_video_delay_target_ms : video_delay_ms;
  return true;
}
}  // namespace webrtc
