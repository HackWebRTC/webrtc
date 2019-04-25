/*
 *  Copyright (c) 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "video/frame_encode_timer.h"

#include <algorithm>

#include "modules/include/module_common_types_public.h"
#include "modules/video_coding/include/video_coding_defines.h"
#include "rtc_base/logging.h"
#include "rtc_base/time_utils.h"

namespace webrtc {
namespace {
const int kMessagesThrottlingThreshold = 2;
const int kThrottleRatio = 100000;
}  // namespace

FrameEncodeTimer::TimingFramesLayerInfo::TimingFramesLayerInfo() = default;
FrameEncodeTimer::TimingFramesLayerInfo::~TimingFramesLayerInfo() = default;

FrameEncodeTimer::FrameEncodeTimer(EncodedImageCallback* frame_drop_callback)
    : frame_drop_callback_(frame_drop_callback),
      internal_source_(false),
      framerate_fps_(0),
      last_timing_frame_time_ms_(-1),
      incorrect_capture_time_logged_messages_(0),
      reordered_frames_logged_messages_(0),
      stalled_encoder_logged_messages_(0) {
  codec_settings_.timing_frame_thresholds = {-1, 0};
}
FrameEncodeTimer::~FrameEncodeTimer() {}

void FrameEncodeTimer::OnEncoderInit(const VideoCodec& codec,
                                     bool internal_source) {
  rtc::CritScope cs(&lock_);
  codec_settings_ = codec;
  internal_source_ = internal_source;
}

void FrameEncodeTimer::OnSetRates(
    const VideoBitrateAllocation& bitrate_allocation,
    uint32_t framerate_fps) {
  rtc::CritScope cs(&lock_);
  framerate_fps_ = framerate_fps;
  const size_t num_spatial_layers = NumSpatialLayers();
  if (timing_frames_info_.size() < num_spatial_layers) {
    timing_frames_info_.resize(num_spatial_layers);
  }
  for (size_t i = 0; i < num_spatial_layers; ++i) {
    timing_frames_info_[i].target_bitrate_bytes_per_sec =
        bitrate_allocation.GetSpatialLayerSum(i) / 8;
  }
}

void FrameEncodeTimer::OnEncodeStarted(uint32_t rtp_timestamp,
                                       int64_t capture_time_ms) {
  rtc::CritScope cs(&lock_);
  if (internal_source_) {
    return;
  }

  const size_t num_spatial_layers = NumSpatialLayers();
  timing_frames_info_.resize(num_spatial_layers);
  for (size_t si = 0; si < num_spatial_layers; ++si) {
    RTC_DCHECK(
        timing_frames_info_[si].encode_start_list.empty() ||
        rtc::TimeDiff(
            capture_time_ms,
            timing_frames_info_[si].encode_start_list.back().capture_time_ms) >=
            0);
    // If stream is disabled due to low bandwidth OnEncodeStarted still will be
    // called and have to be ignored.
    if (timing_frames_info_[si].target_bitrate_bytes_per_sec == 0)
      return;
    if (timing_frames_info_[si].encode_start_list.size() ==
        kMaxEncodeStartTimeListSize) {
      ++stalled_encoder_logged_messages_;
      if (stalled_encoder_logged_messages_ <= kMessagesThrottlingThreshold ||
          stalled_encoder_logged_messages_ % kThrottleRatio == 0) {
        RTC_LOG(LS_WARNING) << "Too many frames in the encode_start_list."
                               " Did encoder stall?";
        if (stalled_encoder_logged_messages_ == kMessagesThrottlingThreshold) {
          RTC_LOG(LS_WARNING)
              << "Too many log messages. Further stalled encoder"
                 "warnings will be throttled.";
        }
      }
      frame_drop_callback_->OnDroppedFrame(
          EncodedImageCallback::DropReason::kDroppedByEncoder);
      timing_frames_info_[si].encode_start_list.pop_front();
    }
    timing_frames_info_[si].encode_start_list.emplace_back(
        rtp_timestamp, capture_time_ms, rtc::TimeMillis());
  }
}

void FrameEncodeTimer::FillTimingInfo(size_t simulcast_svc_idx,
                                      EncodedImage* encoded_image,
                                      int64_t encode_done_ms) {
  rtc::CritScope cs(&lock_);
  absl::optional<size_t> outlier_frame_size;
  absl::optional<int64_t> encode_start_ms;
  uint8_t timing_flags = VideoSendTiming::kNotTriggered;

  // Encoders with internal sources do not call OnEncodeStarted
  // |timing_frames_info_| may be not filled here.
  if (!internal_source_) {
    encode_start_ms = ExtractEncodeStartTime(simulcast_svc_idx, encoded_image);
  }

  if (timing_frames_info_.size() > simulcast_svc_idx) {
    size_t target_bitrate =
        timing_frames_info_[simulcast_svc_idx].target_bitrate_bytes_per_sec;
    if (framerate_fps_ > 0 && target_bitrate > 0) {
      // framerate and target bitrate were reported by encoder.
      size_t average_frame_size = target_bitrate / framerate_fps_;
      outlier_frame_size.emplace(
          average_frame_size *
          codec_settings_.timing_frame_thresholds.outlier_ratio_percent / 100);
    }
  }

  // Outliers trigger timing frames, but do not affect scheduled timing
  // frames.
  if (outlier_frame_size && encoded_image->size() >= *outlier_frame_size) {
    timing_flags |= VideoSendTiming::kTriggeredBySize;
  }

  // Check if it's time to send a timing frame.
  int64_t timing_frame_delay_ms =
      encoded_image->capture_time_ms_ - last_timing_frame_time_ms_;
  // Trigger threshold if it's a first frame, too long passed since the last
  // timing frame, or we already sent timing frame on a different simulcast
  // stream with the same capture time.
  if (last_timing_frame_time_ms_ == -1 ||
      timing_frame_delay_ms >=
          codec_settings_.timing_frame_thresholds.delay_ms ||
      timing_frame_delay_ms == 0) {
    timing_flags |= VideoSendTiming::kTriggeredByTimer;
    last_timing_frame_time_ms_ = encoded_image->capture_time_ms_;
  }

  // Workaround for chromoting encoder: it passes encode start and finished
  // timestamps in |timing_| field, but they (together with capture timestamp)
  // are not in the WebRTC clock.
  if (internal_source_ && encoded_image->timing_.encode_finish_ms > 0 &&
      encoded_image->timing_.encode_start_ms > 0) {
    int64_t clock_offset_ms =
        encode_done_ms - encoded_image->timing_.encode_finish_ms;
    // Translate capture timestamp to local WebRTC clock.
    encoded_image->capture_time_ms_ += clock_offset_ms;
    encoded_image->SetTimestamp(
        static_cast<uint32_t>(encoded_image->capture_time_ms_ * 90));
    encode_start_ms.emplace(encoded_image->timing_.encode_start_ms +
                            clock_offset_ms);
  }

  // If encode start is not available that means that encoder uses internal
  // source. In that case capture timestamp may be from a different clock with a
  // drift relative to rtc::TimeMillis(). We can't use it for Timing frames,
  // because to being sent in the network capture time required to be less than
  // all the other timestamps.
  if (encode_start_ms) {
    encoded_image->SetEncodeTime(*encode_start_ms, encode_done_ms);
    encoded_image->timing_.flags = timing_flags;
  } else {
    encoded_image->timing_.flags = VideoSendTiming::kInvalid;
  }
}

void FrameEncodeTimer::Reset() {
  rtc::CritScope cs(&lock_);
  timing_frames_info_.clear();
  last_timing_frame_time_ms_ = -1;
  reordered_frames_logged_messages_ = 0;
  stalled_encoder_logged_messages_ = 0;
}

absl::optional<int64_t> FrameEncodeTimer::ExtractEncodeStartTime(
    size_t simulcast_svc_idx,
    EncodedImage* encoded_image) {
  absl::optional<int64_t> result;
  size_t num_simulcast_svc_streams = timing_frames_info_.size();
  if (simulcast_svc_idx < num_simulcast_svc_streams) {
    auto encode_start_list =
        &timing_frames_info_[simulcast_svc_idx].encode_start_list;
    // Skip frames for which there was OnEncodeStarted but no OnEncodedImage
    // call. These are dropped by encoder internally.
    // Because some hardware encoders don't preserve capture timestamp we
    // use RTP timestamps here.
    while (!encode_start_list->empty() &&
           IsNewerTimestamp(encoded_image->Timestamp(),
                            encode_start_list->front().rtp_timestamp)) {
      frame_drop_callback_->OnDroppedFrame(
          EncodedImageCallback::DropReason::kDroppedByEncoder);
      encode_start_list->pop_front();
    }
    if (!encode_start_list->empty() &&
        encode_start_list->front().rtp_timestamp ==
            encoded_image->Timestamp()) {
      result.emplace(encode_start_list->front().encode_start_time_ms);
      if (encoded_image->capture_time_ms_ !=
          encode_start_list->front().capture_time_ms) {
        // Force correct capture timestamp.
        encoded_image->capture_time_ms_ =
            encode_start_list->front().capture_time_ms;
        ++incorrect_capture_time_logged_messages_;
        if (incorrect_capture_time_logged_messages_ <=
                kMessagesThrottlingThreshold ||
            incorrect_capture_time_logged_messages_ % kThrottleRatio == 0) {
          RTC_LOG(LS_WARNING)
              << "Encoder is not preserving capture timestamps.";
          if (incorrect_capture_time_logged_messages_ ==
              kMessagesThrottlingThreshold) {
            RTC_LOG(LS_WARNING) << "Too many log messages. Further incorrect "
                                   "timestamps warnings will be throttled.";
          }
        }
      }
      encode_start_list->pop_front();
    } else {
      ++reordered_frames_logged_messages_;
      if (reordered_frames_logged_messages_ <= kMessagesThrottlingThreshold ||
          reordered_frames_logged_messages_ % kThrottleRatio == 0) {
        RTC_LOG(LS_WARNING) << "Frame with no encode started time recordings. "
                               "Encoder may be reordering frames "
                               "or not preserving RTP timestamps.";
        if (reordered_frames_logged_messages_ == kMessagesThrottlingThreshold) {
          RTC_LOG(LS_WARNING) << "Too many log messages. Further frames "
                                 "reordering warnings will be throttled.";
        }
      }
    }
  }
  return result;
}

size_t FrameEncodeTimer::NumSpatialLayers() const {
  size_t num_spatial_layers = codec_settings_.numberOfSimulcastStreams;
  if (codec_settings_.codecType == kVideoCodecVP9) {
    num_spatial_layers = std::max(
        num_spatial_layers,
        static_cast<size_t>(codec_settings_.VP9().numberOfSpatialLayers));
  }
  return std::max(num_spatial_layers, size_t{1});
}

}  // namespace webrtc
