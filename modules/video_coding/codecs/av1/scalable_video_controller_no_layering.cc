/*
 *  Copyright (c) 2020 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#include "modules/video_coding/codecs/av1/scalable_video_controller_no_layering.h"

#include <utility>
#include <vector>

#include "api/transport/rtp/dependency_descriptor.h"
#include "rtc_base/checks.h"

namespace webrtc {

ScalableVideoControllerNoLayering::~ScalableVideoControllerNoLayering() =
    default;

ScalableVideoController::StreamLayersConfig
ScalableVideoControllerNoLayering::StreamConfig() const {
  StreamLayersConfig result;
  result.num_spatial_layers = 1;
  result.num_temporal_layers = 1;
  return result;
}

FrameDependencyStructure
ScalableVideoControllerNoLayering::DependencyStructure() const {
  FrameDependencyStructure structure;
  structure.num_decode_targets = 1;
  FrameDependencyTemplate a_template;
  a_template.decode_target_indications = {DecodeTargetIndication::kSwitch};
  structure.templates.push_back(a_template);
  return structure;
}

std::vector<ScalableVideoController::LayerFrameConfig>
ScalableVideoControllerNoLayering::NextFrameConfig(bool restart) {
  if (restart) {
    start_ = true;
  }
  std::vector<LayerFrameConfig> result(1);
  result[0].id = 0;
  result[0].is_keyframe = start_;
  result[0].buffers = {{/*id=*/0, /*references=*/!start_, /*updates=*/true}};

  start_ = false;
  return result;
}

absl::optional<GenericFrameInfo>
ScalableVideoControllerNoLayering::OnEncodeDone(LayerFrameConfig config) {
  RTC_DCHECK_EQ(config.id, 0);
  absl::optional<GenericFrameInfo> frame_info(absl::in_place);
  frame_info->encoder_buffers = std::move(config.buffers);
  if (config.is_keyframe) {
    for (auto& buffer : frame_info->encoder_buffers) {
      buffer.referenced = false;
    }
  }
  frame_info->decode_target_indications = {DecodeTargetIndication::kSwitch};
  return frame_info;
}

}  // namespace webrtc
