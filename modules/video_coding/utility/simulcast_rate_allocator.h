/*
 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_VIDEO_CODING_UTILITY_SIMULCAST_RATE_ALLOCATOR_H_
#define MODULES_VIDEO_CODING_UTILITY_SIMULCAST_RATE_ALLOCATOR_H_

#include <stdint.h>

#include <map>
#include <memory>
#include <vector>

#include "api/video_codecs/video_encoder.h"
#include "common_types.h"  // NOLINT(build/include)
#include "common_video/include/video_bitrate_allocator.h"
#include "rtc_base/constructormagic.h"

namespace webrtc {

// Ratio allocation between temporal streams:
// Values as required for the VP8 codec (accumulating).
static const float
    kLayerRateAllocation[kMaxSimulcastStreams][kMaxTemporalStreams] = {
        {1.0f, 1.0f, 1.0f, 1.0f},  // 1 layer
        {0.6f, 1.0f, 1.0f, 1.0f},  // 2 layers {60%, 40%}
        {0.4f, 0.6f, 1.0f, 1.0f},  // 3 layers {40%, 20%, 40%}
        {0.25f, 0.4f, 0.6f, 1.0f}  // 4 layers {25%, 15%, 20%, 40%}
};

class SimulcastRateAllocator : public VideoBitrateAllocator {
 public:
  explicit SimulcastRateAllocator(const VideoCodec& codec);

  VideoBitrateAllocation GetAllocation(uint32_t total_bitrate_bps,
                                       uint32_t framerate) override;
  const VideoCodec& GetCodec() const;

 private:
  void DistributeAllocationToSimulcastLayers(
      uint32_t total_bitrate_bps,
      VideoBitrateAllocation* allocated_bitrates_bps) const;
  void DistributeAllocationToTemporalLayers(
      uint32_t framerate,
      VideoBitrateAllocation* allocated_bitrates_bps) const;
  std::vector<uint32_t> DefaultTemporalLayerAllocation(int bitrate_kbps,
                                                       int max_bitrate_kbps,
                                                       int framerate,
                                                       int simulcast_id) const;
  std::vector<uint32_t> ScreenshareTemporalLayerAllocation(
      int bitrate_kbps,
      int max_bitrate_kbps,
      int framerate,
      int simulcast_id) const;
  int NumTemporalStreams(size_t simulcast_id) const;

  const VideoCodec codec_;

  RTC_DISALLOW_COPY_AND_ASSIGN(SimulcastRateAllocator);
};

}  // namespace webrtc

#endif  // MODULES_VIDEO_CODING_UTILITY_SIMULCAST_RATE_ALLOCATOR_H_
