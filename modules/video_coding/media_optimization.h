/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_VIDEO_CODING_MEDIA_OPTIMIZATION_H_
#define MODULES_VIDEO_CODING_MEDIA_OPTIMIZATION_H_

#include <list>
#include <memory>

#include "modules/include/module_common_types.h"
#include "modules/video_coding/include/video_coding.h"
#include "modules/video_coding/media_opt_util.h"
#include "rtc_base/criticalsection.h"

namespace webrtc {

class Clock;
class FrameDropper;
class VCMContentMetricsProcessing;

namespace media_optimization {

class MediaOptimization {
 public:
  explicit MediaOptimization(Clock* clock);
  ~MediaOptimization();

  // TODO(andresp): Can Reset and SetEncodingData be done at construction time
  // only?
  void Reset();

  // Informs media optimization of initial encoding state.
  // TODO(perkj): Deprecate SetEncodingData once its not used for stats in
  // VieEncoder.
  void SetEncodingData(int32_t max_bit_rate,
                       uint32_t bit_rate,
                       uint32_t frame_rate);

  // Sets target rates for the encoder given the channel parameters.
  // Input: |target bitrate| - the encoder target bitrate in bits/s.
  uint32_t SetTargetRates(uint32_t target_bitrate);

  void EnableFrameDropper(bool enable);
  bool DropFrame();

  // Informs Media Optimization of encoded output.
  // TODO(perkj): Deprecate SetEncodingData once its not used for stats in
  // VieEncoder.
  int32_t UpdateWithEncodedData(const EncodedImage& encoded_image);

  // InputFrameRate 0 = no frame rate estimate available.
  uint32_t InputFrameRate();
  uint32_t SentFrameRate();
  uint32_t SentBitRate();

 private:
  enum { kFrameCountHistorySize = 90 };
  enum { kFrameHistoryWinMs = 2000 };
  enum { kBitrateAverageWinMs = 1000 };

  struct EncodedFrameSample;
  typedef std::list<EncodedFrameSample> FrameSampleList;

  void UpdateIncomingFrameRate() RTC_EXCLUSIVE_LOCKS_REQUIRED(crit_sect_);
  void PurgeOldFrameSamples(int64_t threshold_ms)
      RTC_EXCLUSIVE_LOCKS_REQUIRED(crit_sect_);
  void UpdateSentFramerate() RTC_EXCLUSIVE_LOCKS_REQUIRED(crit_sect_);

  void ProcessIncomingFrameRate(int64_t now)
      RTC_EXCLUSIVE_LOCKS_REQUIRED(crit_sect_);

  // Checks conditions for suspending the video. The method compares
  // |video_target_bitrate_| with the threshold values for suspension, and
  // changes the state of |video_suspended_| accordingly.
  void CheckSuspendConditions() RTC_EXCLUSIVE_LOCKS_REQUIRED(crit_sect_);

  void SetEncodingDataInternal(int32_t max_bit_rate,
                               uint32_t frame_rate,
                               uint32_t bit_rate)
      RTC_EXCLUSIVE_LOCKS_REQUIRED(crit_sect_);

  uint32_t InputFrameRateInternal() RTC_EXCLUSIVE_LOCKS_REQUIRED(crit_sect_);

  uint32_t SentFrameRateInternal() RTC_EXCLUSIVE_LOCKS_REQUIRED(crit_sect_);

  // Protect all members.
  rtc::CriticalSection crit_sect_;

  Clock* clock_ RTC_GUARDED_BY(crit_sect_);
  int32_t max_bit_rate_ RTC_GUARDED_BY(crit_sect_);
  float user_frame_rate_ RTC_GUARDED_BY(crit_sect_);
  std::unique_ptr<FrameDropper> frame_dropper_ RTC_GUARDED_BY(crit_sect_);
  int video_target_bitrate_ RTC_GUARDED_BY(crit_sect_);
  float incoming_frame_rate_ RTC_GUARDED_BY(crit_sect_);
  int64_t incoming_frame_times_[kFrameCountHistorySize] RTC_GUARDED_BY(
      crit_sect_);
  std::list<EncodedFrameSample> encoded_frame_samples_
      RTC_GUARDED_BY(crit_sect_);
  uint32_t avg_sent_framerate_ RTC_GUARDED_BY(crit_sect_);
};
}  // namespace media_optimization
}  // namespace webrtc

#endif  // MODULES_VIDEO_CODING_MEDIA_OPTIMIZATION_H_
