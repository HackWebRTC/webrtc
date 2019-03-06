/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef API_VIDEO_CODECS_VP8_TEMPORAL_LAYERS_H_
#define API_VIDEO_CODECS_VP8_TEMPORAL_LAYERS_H_

#include <vector>

#include "api/video_codecs/vp8_frame_buffer_controller.h"
#include "api/video_codecs/vp8_frame_config.h"

namespace webrtc {

// Two different flavors of temporal layers are currently available:
// kFixedPattern uses a fixed repeating pattern of 1-4 layers.
// kBitrateDynamic can allocate frames dynamically to 1 or 2 layers, based on
// the bitrate produced.
enum class Vp8TemporalLayersType { kFixedPattern, kBitrateDynamic };

// This interface defines a way of getting the encoder settings needed to
// realize a temporal layer structure.
class Vp8TemporalLayers : public Vp8FrameBufferController {
 public:
  ~Vp8TemporalLayers() override = default;

  bool SupportsEncoderFrameDropping() const override = 0;

  void OnRatesUpdated(const std::vector<uint32_t>& bitrates_bps,
                      int framerate_fps) override = 0;

  bool UpdateConfiguration(Vp8EncoderConfig* cfg) override = 0;

  Vp8FrameConfig UpdateLayerConfig(uint32_t rtp_timestamp) override = 0;

  void OnEncodeDone(uint32_t rtp_timestamp,
                    size_t size_bytes,
                    bool is_keyframe,
                    int qp,
                    CodecSpecificInfo* info) override = 0;
};

}  // namespace webrtc

#endif  // API_VIDEO_CODECS_VP8_TEMPORAL_LAYERS_H_
