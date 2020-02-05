/*
 *  Copyright (c) 2020 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_VIDEO_CODING_FRAME_DEPENDENCIES_CALCULATOR_H_
#define MODULES_VIDEO_CODING_FRAME_DEPENDENCIES_CALCULATOR_H_

#include <stdint.h>

#include <vector>

#include "absl/container/inlined_vector.h"
#include "absl/types/optional.h"
#include "api/array_view.h"
#include "api/video/video_frame_type.h"
#include "common_video/generic_frame_descriptor/generic_frame_info.h"
#include "rtc_base/synchronization/sequence_checker.h"
#include "rtc_base/thread_annotations.h"

namespace webrtc {

class FrameDependenciesCalculator {
 public:
  FrameDependenciesCalculator() = default;
  FrameDependenciesCalculator(FrameDependenciesCalculator&&) = default;
  FrameDependenciesCalculator& operator=(FrameDependenciesCalculator&&) =
      default;

  // Calculates frame dependencies based on previous encoder buffer usage.
  absl::InlinedVector<int64_t, 5> FromBuffersUsage(
      VideoFrameType frame_type,
      int64_t frame_id,
      rtc::ArrayView<const CodecBufferUsage> buffers_usage);

 private:
  struct BufferUsage {
    absl::optional<int64_t> frame_id;
    absl::InlinedVector<int64_t, 4> dependencies;
  };

  SequenceChecker checker_;
  absl::InlinedVector<BufferUsage, 4> buffers_ RTC_GUARDED_BY(checker_);
};

}  // namespace webrtc

#endif  // MODULES_VIDEO_CODING_FRAME_DEPENDENCIES_CALCULATOR_H_
