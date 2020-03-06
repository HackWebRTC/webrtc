/*
 *  Copyright 2020 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "video/adaptation/video_stream_adapter.h"

#include <algorithm>
#include <limits>

#include "absl/types/optional.h"
#include "rtc_base/constructor_magic.h"
#include "rtc_base/logging.h"
#include "rtc_base/numerics/safe_conversions.h"

namespace webrtc {

// VideoSourceRestrictor is responsible for keeping track of current
// VideoSourceRestrictions. It suggests higher and lower frame rates and
// resolutions (used by "maintain-resolution" and "maintain-framerate"), but is
// ultimately not reponsible for determining when or how we should adapt up or
// down (e.g. "balanced" mode also uses BalancedDegradationPreference).
class VideoStreamAdapter::VideoSourceRestrictor {
 public:
  // For frame rate, the steps we take are 2/3 (down) and 3/2 (up).
  static int GetLowerFrameRateThan(int fps) {
    RTC_DCHECK(fps != std::numeric_limits<int>::max());
    return (fps * 2) / 3;
  }
  // TODO(hbos): Use absl::optional<> instead?
  static int GetHigherFrameRateThan(int fps) {
    return fps != std::numeric_limits<int>::max()
               ? (fps * 3) / 2
               : std::numeric_limits<int>::max();
  }

  // For resolution, the steps we take are 3/5 (down) and 5/3 (up).
  // Notice the asymmetry of which restriction property is set depending on if
  // we are adapting up or down:
  // - DecreaseResolution() sets the max_pixels_per_frame() to the desired
  //   target and target_pixels_per_frame() to null.
  // - IncreaseResolutionTo() sets the target_pixels_per_frame() to the desired
  //   target, and max_pixels_per_frame() is set according to
  //   GetIncreasedMaxPixelsWanted().
  static int GetLowerResolutionThan(int pixel_count) {
    RTC_DCHECK(pixel_count != std::numeric_limits<int>::max());
    return (pixel_count * 3) / 5;
  }
  // TODO(hbos): Use absl::optional<> instead?
  static int GetHigherResolutionThan(int pixel_count) {
    return pixel_count != std::numeric_limits<int>::max()
               ? (pixel_count * 5) / 3
               : std::numeric_limits<int>::max();
  }

  VideoSourceRestrictor() {}

  VideoSourceRestrictions source_restrictions() const {
    return source_restrictions_;
  }
  const AdaptationCounters& adaptation_counters() const { return adaptations_; }
  void ClearRestrictions() {
    source_restrictions_ = VideoSourceRestrictions();
    adaptations_ = AdaptationCounters();
  }

  bool CanDecreaseResolutionTo(int target_pixels, int min_pixels_per_frame) {
    int max_pixels_per_frame = rtc::dchecked_cast<int>(
        source_restrictions_.max_pixels_per_frame().value_or(
            std::numeric_limits<int>::max()));
    return target_pixels < max_pixels_per_frame &&
           target_pixels >= min_pixels_per_frame;
  }
  void DecreaseResolutionTo(int target_pixels, int min_pixels_per_frame) {
    RTC_DCHECK(CanDecreaseResolutionTo(target_pixels, min_pixels_per_frame));
    RTC_LOG(LS_INFO) << "Scaling down resolution, max pixels: "
                     << target_pixels;
    source_restrictions_.set_max_pixels_per_frame(
        target_pixels != std::numeric_limits<int>::max()
            ? absl::optional<size_t>(target_pixels)
            : absl::nullopt);
    source_restrictions_.set_target_pixels_per_frame(absl::nullopt);
    ++adaptations_.resolution_adaptations;
  }

  bool CanIncreaseResolutionTo(int target_pixels) {
    int max_pixels_wanted = GetIncreasedMaxPixelsWanted(target_pixels);
    int max_pixels_per_frame = rtc::dchecked_cast<int>(
        source_restrictions_.max_pixels_per_frame().value_or(
            std::numeric_limits<int>::max()));
    return max_pixels_wanted > max_pixels_per_frame;
  }
  void IncreaseResolutionTo(int target_pixels) {
    RTC_DCHECK(CanIncreaseResolutionTo(target_pixels));
    int max_pixels_wanted = GetIncreasedMaxPixelsWanted(target_pixels);
    RTC_LOG(LS_INFO) << "Scaling up resolution, max pixels: "
                     << max_pixels_wanted;
    source_restrictions_.set_max_pixels_per_frame(
        max_pixels_wanted != std::numeric_limits<int>::max()
            ? absl::optional<size_t>(max_pixels_wanted)
            : absl::nullopt);
    source_restrictions_.set_target_pixels_per_frame(
        max_pixels_wanted != std::numeric_limits<int>::max()
            ? absl::optional<size_t>(target_pixels)
            : absl::nullopt);
    --adaptations_.resolution_adaptations;
    RTC_DCHECK_GE(adaptations_.resolution_adaptations, 0);
  }

  bool CanDecreaseFrameRateTo(int max_frame_rate) {
    const int fps_wanted = std::max(kMinFramerateFps, max_frame_rate);
    return fps_wanted < rtc::dchecked_cast<int>(
                            source_restrictions_.max_frame_rate().value_or(
                                std::numeric_limits<int>::max()));
  }
  void DecreaseFrameRateTo(int max_frame_rate) {
    RTC_DCHECK(CanDecreaseFrameRateTo(max_frame_rate));
    max_frame_rate = std::max(kMinFramerateFps, max_frame_rate);
    RTC_LOG(LS_INFO) << "Scaling down framerate: " << max_frame_rate;
    source_restrictions_.set_max_frame_rate(
        max_frame_rate != std::numeric_limits<int>::max()
            ? absl::optional<double>(max_frame_rate)
            : absl::nullopt);
    ++adaptations_.fps_adaptations;
  }

  bool CanIncreaseFrameRateTo(int max_frame_rate) {
    return max_frame_rate > rtc::dchecked_cast<int>(
                                source_restrictions_.max_frame_rate().value_or(
                                    std::numeric_limits<int>::max()));
  }
  void IncreaseFrameRateTo(int max_frame_rate) {
    RTC_DCHECK(CanIncreaseFrameRateTo(max_frame_rate));
    RTC_LOG(LS_INFO) << "Scaling up framerate: " << max_frame_rate;
    source_restrictions_.set_max_frame_rate(
        max_frame_rate != std::numeric_limits<int>::max()
            ? absl::optional<double>(max_frame_rate)
            : absl::nullopt);
    --adaptations_.fps_adaptations;
    RTC_DCHECK_GE(adaptations_.fps_adaptations, 0);
  }

 private:
  static int GetIncreasedMaxPixelsWanted(int target_pixels) {
    if (target_pixels == std::numeric_limits<int>::max())
      return std::numeric_limits<int>::max();
    // When we decrease resolution, we go down to at most 3/5 of current pixels.
    // Thus to increase resolution, we need 3/5 to get back to where we started.
    // When going up, the desired max_pixels_per_frame() has to be significantly
    // higher than the target because the source's native resolutions might not
    // match the target. We pick 12/5 of the target.
    //
    // (This value was historically 4 times the old target, which is (3/5)*4 of
    // the new target - or 12/5 - assuming the target is adjusted according to
    // the above steps.)
    RTC_DCHECK(target_pixels != std::numeric_limits<int>::max());
    return (target_pixels * 12) / 5;
  }

  VideoSourceRestrictions source_restrictions_;
  AdaptationCounters adaptations_;

  RTC_DISALLOW_COPY_AND_ASSIGN(VideoSourceRestrictor);
};

const int VideoStreamAdapter::kMinFramerateFps = 2;

// static
int VideoStreamAdapter::GetLowerFrameRateThan(int fps) {
  return VideoSourceRestrictor::GetLowerFrameRateThan(fps);
}

// static
int VideoStreamAdapter::GetHigherFrameRateThan(int fps) {
  return VideoSourceRestrictor::GetHigherFrameRateThan(fps);
}

// static
int VideoStreamAdapter::GetLowerResolutionThan(int pixel_count) {
  return VideoSourceRestrictor::GetLowerResolutionThan(pixel_count);
}

// static
int VideoStreamAdapter::GetHigherResolutionThan(int pixel_count) {
  return VideoSourceRestrictor::GetHigherResolutionThan(pixel_count);
}

VideoStreamAdapter::VideoStreamAdapter()
    : source_restrictor_(std::make_unique<VideoSourceRestrictor>()) {}

VideoStreamAdapter::~VideoStreamAdapter() {}

VideoSourceRestrictions VideoStreamAdapter::source_restrictions() const {
  return source_restrictor_->source_restrictions();
}

const AdaptationCounters& VideoStreamAdapter::adaptation_counters() const {
  return source_restrictor_->adaptation_counters();
}

void VideoStreamAdapter::ClearRestrictions() {
  source_restrictor_->ClearRestrictions();
}

bool VideoStreamAdapter::CanDecreaseResolutionTo(int target_pixels,
                                                 int min_pixels_per_frame) {
  return source_restrictor_->CanDecreaseResolutionTo(target_pixels,
                                                     min_pixels_per_frame);
}

void VideoStreamAdapter::DecreaseResolutionTo(int target_pixels,
                                              int min_pixels_per_frame) {
  source_restrictor_->DecreaseResolutionTo(target_pixels, min_pixels_per_frame);
}

bool VideoStreamAdapter::CanIncreaseResolutionTo(int target_pixels) {
  return source_restrictor_->CanIncreaseResolutionTo(target_pixels);
}

void VideoStreamAdapter::IncreaseResolutionTo(int target_pixels) {
  source_restrictor_->IncreaseResolutionTo(target_pixels);
}

bool VideoStreamAdapter::CanDecreaseFrameRateTo(int max_frame_rate) {
  return source_restrictor_->CanDecreaseFrameRateTo(max_frame_rate);
}

void VideoStreamAdapter::DecreaseFrameRateTo(int max_frame_rate) {
  source_restrictor_->DecreaseFrameRateTo(max_frame_rate);
}

bool VideoStreamAdapter::CanIncreaseFrameRateTo(int max_frame_rate) {
  return source_restrictor_->CanIncreaseFrameRateTo(max_frame_rate);
}

void VideoStreamAdapter::IncreaseFrameRateTo(int max_frame_rate) {
  source_restrictor_->IncreaseFrameRateTo(max_frame_rate);
}

}  // namespace webrtc
