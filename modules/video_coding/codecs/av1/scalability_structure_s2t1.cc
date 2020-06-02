/*
 *  Copyright (c) 2020 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#include "modules/video_coding/codecs/av1/scalability_structure_s2t1.h"

#include <utility>
#include <vector>

#include "absl/base/macros.h"
#include "api/transport/rtp/dependency_descriptor.h"
#include "rtc_base/checks.h"
#include "rtc_base/logging.h"

namespace webrtc {
namespace {

constexpr auto kNotPresent = DecodeTargetIndication::kNotPresent;
constexpr auto kSwitch = DecodeTargetIndication::kSwitch;

constexpr DecodeTargetIndication kDtis[2][2] = {
    {kSwitch, kNotPresent},  // S0
    {kNotPresent, kSwitch},  // S1
};

}  // namespace

ScalabilityStructureS2T1::~ScalabilityStructureS2T1() = default;

ScalableVideoController::StreamLayersConfig
ScalabilityStructureS2T1::StreamConfig() const {
  StreamLayersConfig result;
  result.num_spatial_layers = 2;
  result.num_temporal_layers = 1;
  return result;
}

FrameDependencyStructure ScalabilityStructureS2T1::DependencyStructure() const {
  using Builder = GenericFrameInfo::Builder;
  FrameDependencyStructure structure;
  structure.num_decode_targets = 2;
  structure.num_chains = 2;
  structure.decode_target_protected_by_chain = {0, 1};
  structure.templates = {
      Builder().S(0).Dtis("S-").Fdiffs({2}).ChainDiffs({2, 1}).Build(),
      Builder().S(0).Dtis("S-").ChainDiffs({0, 0}).Build(),
      Builder().S(1).Dtis("-S").Fdiffs({2}).ChainDiffs({1, 2}).Build(),
      Builder().S(1).Dtis("-S").ChainDiffs({1, 0}).Build(),
  };
  return structure;
}

std::vector<ScalableVideoController::LayerFrameConfig>
ScalabilityStructureS2T1::NextFrameConfig(bool restart) {
  if (restart) {
    keyframe_ = true;
  }
  std::vector<LayerFrameConfig> result(2);

  // Buffer0 keeps latest S0T0 frame, Buffer1 keeps latest S1T0 frame.
  result[0].spatial_id = 0;
  result[0].is_keyframe = keyframe_;
  result[0].buffers = {{/*id=*/0, /*references=*/!keyframe_, /*updates=*/true}};

  result[1].spatial_id = 1;
  result[1].is_keyframe = keyframe_;
  result[1].buffers = {{/*id=*/1, /*references=*/!keyframe_, /*updates=*/true}};

  keyframe_ = false;
  return result;
}

absl::optional<GenericFrameInfo> ScalabilityStructureS2T1::OnEncodeDone(
    LayerFrameConfig config) {
  absl::optional<GenericFrameInfo> frame_info;
  if (config.id != 0) {
    RTC_LOG(LS_ERROR) << "Unexpected config id " << config.id;
  }
  if (config.spatial_id < 0 ||
      config.spatial_id >= int{ABSL_ARRAYSIZE(kDtis)}) {
    RTC_LOG(LS_ERROR) << "Unexpected spatial id " << config.spatial_id;
    return frame_info;
  }
  frame_info.emplace();
  frame_info->spatial_id = config.spatial_id;
  frame_info->temporal_id = config.temporal_id;
  frame_info->encoder_buffers = std::move(config.buffers);
  frame_info->decode_target_indications.assign(
      std::begin(kDtis[config.spatial_id]), std::end(kDtis[config.spatial_id]));
  frame_info->part_of_chain = {config.spatial_id == 0, config.spatial_id == 1};
  return frame_info;
}

}  // namespace webrtc
