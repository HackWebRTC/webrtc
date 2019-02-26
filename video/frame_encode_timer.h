/*
 *  Copyright (c) 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef VIDEO_FRAME_ENCODE_TIMER_H_
#define VIDEO_FRAME_ENCODE_TIMER_H_

#include <list>
#include <vector>

#include "absl/types/optional.h"
#include "api/video/encoded_image.h"
#include "api/video_codecs/video_codec.h"
#include "api/video_codecs/video_encoder.h"
#include "rtc_base/critical_section.h"

namespace webrtc {

class FrameEncodeTimer {
 public:
  explicit FrameEncodeTimer(EncodedImageCallback* frame_drop_callback);
  ~FrameEncodeTimer();

  void OnEncoderInit(const VideoCodec& codec, bool internal_source);
  void OnSetRates(const VideoBitrateAllocation& bitrate_allocation,
                  uint32_t framerate_fps);

  void OnEncodeStarted(uint32_t rtp_timestamp, int64_t capture_time_ms);

  void FillTimingInfo(size_t simulcast_svc_idx,
                      EncodedImage* encoded_image,
                      int64_t encode_done_ms);
  void Reset();

 private:
  size_t NumSpatialLayers() const RTC_EXCLUSIVE_LOCKS_REQUIRED(lock_);

  // For non-internal-source encoders, returns encode started time and fixes
  // capture timestamp for the frame, if corrupted by the encoder.
  absl::optional<int64_t> ExtractEncodeStartTime(size_t simulcast_svc_idx,
                                                 EncodedImage* encoded_image)
      RTC_EXCLUSIVE_LOCKS_REQUIRED(lock_);

  struct EncodeStartTimeRecord {
    EncodeStartTimeRecord(uint32_t timestamp,
                          int64_t capture_time,
                          int64_t encode_start_time)
        : rtp_timestamp(timestamp),
          capture_time_ms(capture_time),
          encode_start_time_ms(encode_start_time) {}
    uint32_t rtp_timestamp;
    int64_t capture_time_ms;
    int64_t encode_start_time_ms;
  };
  struct TimingFramesLayerInfo {
    TimingFramesLayerInfo();
    ~TimingFramesLayerInfo();
    size_t target_bitrate_bytes_per_sec = 0;
    std::list<EncodeStartTimeRecord> encode_start_list;
  };

  rtc::CriticalSection lock_;
  EncodedImageCallback* const frame_drop_callback_;
  VideoCodec codec_settings_ RTC_GUARDED_BY(&lock_);
  bool internal_source_ RTC_GUARDED_BY(&lock_);
  uint32_t framerate_fps_ RTC_GUARDED_BY(&lock_);

  // Separate instance for each simulcast stream or spatial layer.
  std::vector<TimingFramesLayerInfo> timing_frames_info_ RTC_GUARDED_BY(&lock_);
  int64_t last_timing_frame_time_ms_ RTC_GUARDED_BY(&lock_);
  size_t incorrect_capture_time_logged_messages_ RTC_GUARDED_BY(&lock_);
  size_t reordered_frames_logged_messages_ RTC_GUARDED_BY(&lock_);
  size_t stalled_encoder_logged_messages_ RTC_GUARDED_BY(&lock_);
};

}  // namespace webrtc

#endif  // VIDEO_FRAME_ENCODE_TIMER_H_
