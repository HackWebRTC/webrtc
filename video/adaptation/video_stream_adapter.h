/*
 *  Copyright 2020 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef VIDEO_ADAPTATION_VIDEO_STREAM_ADAPTER_H_
#define VIDEO_ADAPTATION_VIDEO_STREAM_ADAPTER_H_

#include <memory>

#include "call/adaptation/video_source_restrictions.h"
#include "video/adaptation/adaptation_counters.h"

namespace webrtc {

// Owns the VideoSourceRestriction for a single stream and is responsible for
// adapting it up or down when told to do so. This class serves the following
// purposes:
// 1. Keep track of a stream's restrictions.
// 2. Provide valid ways to adapt up or down the stream's restrictions.
// 3. Modify the stream's restrictions in one of the valid ways.
class VideoStreamAdapter {
 public:
  static const int kMinFramerateFps;

  static int GetLowerFrameRateThan(int fps);
  static int GetHigherFrameRateThan(int fps);
  static int GetLowerResolutionThan(int pixel_count);
  static int GetHigherResolutionThan(int pixel_count);

  VideoStreamAdapter();
  ~VideoStreamAdapter();

  // TODO(hbos): Why isn't this const?
  VideoSourceRestrictions source_restrictions() const;
  const AdaptationCounters& adaptation_counters() const;
  void ClearRestrictions();

  // "Can adapt?" and "do adapt!" methods.
  // TODO(https://crbug.com/webrtc/11393): Make the adapter responsible for
  // deciding what the next step are, i.e. taking on degradation preference
  // logic. Then, these can be expressed either as CanAdaptUp() and DoAdaptUp()
  // or as GetNextRestrictionsUp() and ApplyRestrictions().
  bool CanDecreaseResolutionTo(int target_pixels, int min_pixels_per_frame);
  void DecreaseResolutionTo(int target_pixels, int min_pixels_per_frame);
  bool CanIncreaseResolutionTo(int target_pixels);
  void IncreaseResolutionTo(int target_pixels);
  bool CanDecreaseFrameRateTo(int max_frame_rate);
  void DecreaseFrameRateTo(int max_frame_rate);
  bool CanIncreaseFrameRateTo(int max_frame_rate);
  void IncreaseFrameRateTo(int max_frame_rate);

 private:
  class VideoSourceRestrictor;

  const std::unique_ptr<VideoSourceRestrictor> source_restrictor_;
};

}  // namespace webrtc

#endif  // VIDEO_ADAPTATION_VIDEO_STREAM_ADAPTER_H_
