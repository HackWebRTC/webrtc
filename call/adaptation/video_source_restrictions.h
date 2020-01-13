/*
 *  Copyright 2020 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef CALL_ADAPTATION_VIDEO_SOURCE_RESTRICTIONS_H_
#define CALL_ADAPTATION_VIDEO_SOURCE_RESTRICTIONS_H_

#include <utility>

#include "absl/types/optional.h"

namespace webrtc {

// Describes optional restrictions to the resolution and frame rate of a video
// source.
class VideoSourceRestrictions {
 public:
  // Constructs without any restrictions.
  VideoSourceRestrictions();
  // All values must be positive or nullopt.
  // TODO(hbos): Support expressing "disable this stream"?
  VideoSourceRestrictions(absl::optional<size_t> max_pixels_per_frame,
                          absl::optional<size_t> target_pixels_per_frame,
                          absl::optional<double> max_frame_rate);

  bool operator==(const VideoSourceRestrictions& rhs) const {
    return max_pixels_per_frame_ == rhs.max_pixels_per_frame_ &&
           target_pixels_per_frame_ == rhs.target_pixels_per_frame_ &&
           max_frame_rate_ == rhs.max_frame_rate_;
  }
  bool operator!=(const VideoSourceRestrictions& rhs) const {
    return !(*this == rhs);
  }

  const absl::optional<size_t>& max_pixels_per_frame() const;
  const absl::optional<size_t>& target_pixels_per_frame() const;
  const absl::optional<double>& max_frame_rate() const;

  void set_max_pixels_per_frame(absl::optional<size_t> max_pixels_per_frame);
  void set_target_pixels_per_frame(
      absl::optional<size_t> target_pixels_per_frame);
  void set_max_frame_rate(absl::optional<double> max_frame_rate);

 private:
  // These map to rtc::VideoSinkWants's |max_pixel_count| and
  // |target_pixel_count|.
  // TODO(hbos): It's not clear what "target" means; either make it well-defined
  // or remove it in favor of only using |max_pixels_per_frame_|.
  absl::optional<size_t> max_pixels_per_frame_;
  absl::optional<size_t> target_pixels_per_frame_;
  absl::optional<double> max_frame_rate_;
};

}  // namespace webrtc

#endif  // CALL_ADAPTATION_VIDEO_SOURCE_RESTRICTIONS_H_
