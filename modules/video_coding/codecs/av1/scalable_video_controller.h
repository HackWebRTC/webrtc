/*
 *  Copyright (c) 2020 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#ifndef MODULES_VIDEO_CODING_CODECS_AV1_SCALABLE_VIDEO_CONTROLLER_H_
#define MODULES_VIDEO_CODING_CODECS_AV1_SCALABLE_VIDEO_CONTROLLER_H_

#include <vector>

#include "absl/container/inlined_vector.h"
#include "absl/types/optional.h"
#include "api/transport/rtp/dependency_descriptor.h"
#include "common_video/generic_frame_descriptor/generic_frame_info.h"

namespace webrtc {

// Controls how video should be encoded to be scalable. Outputs results as
// buffer usage configuration for encoder and enough details to communicate the
// scalability structure via dependency descriptor rtp header extension.
class ScalableVideoController {
 public:
  struct StreamLayersConfig {
    int num_spatial_layers = 1;
    int num_temporal_layers = 1;
  };
  struct LayerFrameConfig {
    // Id to match configuration returned by NextFrameConfig with
    // (possibly modified) configuration passed back via OnEncoderDone.
    // The meaning of the id is an implementation detail of
    // the ScalableVideoController.
    int id = 0;

    // Indication frame should be encoded as a key frame. In particular when
    // `is_keyframe=true` property `CodecBufferUsage::referenced` should be
    // ignored and treated as false.
    bool is_keyframe = false;

    int spatial_id = 0;
    int temporal_id = 0;
    // Describes how encoder which buffers encoder allowed to reference and
    // which buffers encoder should update.
    absl::InlinedVector<CodecBufferUsage, kMaxEncoderBuffers> buffers;
  };

  virtual ~ScalableVideoController() = default;

  // Returns video structure description for encoder to configure itself.
  virtual StreamLayersConfig StreamConfig() const = 0;

  // Returns video structure description in format compatible with
  // dependency descriptor rtp header extension.
  virtual FrameDependencyStructure DependencyStructure() const = 0;

  // When `restart` is true, first `LayerFrameConfig` should have `is_keyframe`
  // set to true.
  // Returned vector shouldn't be empty.
  virtual std::vector<LayerFrameConfig> NextFrameConfig(bool restart) = 0;

  // Returns configuration to pass to EncoderCallback.
  virtual absl::optional<GenericFrameInfo> OnEncodeDone(
      LayerFrameConfig config) = 0;
};

}  // namespace webrtc

#endif  // MODULES_VIDEO_CODING_CODECS_AV1_SCALABLE_VIDEO_CONTROLLER_H_
