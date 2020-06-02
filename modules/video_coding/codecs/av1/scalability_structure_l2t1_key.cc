/*
 *  Copyright (c) 2020 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#include "modules/video_coding/codecs/av1/scalability_structure_l2t1_key.h"

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

constexpr DecodeTargetIndication kDtis[3][2] = {
    {kSwitch, kSwitch},      // Key, S0
    {kSwitch, kNotPresent},  // Delta, S0
    {kNotPresent, kSwitch},  // Key and Delta, S1
};

}  // namespace

ScalabilityStructureL2T1Key::~ScalabilityStructureL2T1Key() = default;

ScalableVideoController::StreamLayersConfig
ScalabilityStructureL2T1Key::StreamConfig() const {
  StreamLayersConfig result;
  result.num_spatial_layers = 2;
  result.num_temporal_layers = 1;
  return result;
}

FrameDependencyStructure ScalabilityStructureL2T1Key::DependencyStructure()
    const {
  using Builder = GenericFrameInfo::Builder;
  FrameDependencyStructure structure;
  structure.num_decode_targets = 2;
  structure.num_chains = 2;
  structure.decode_target_protected_by_chain = {0, 1};
  structure.templates = {
      Builder().S(0).Dtis("S-").Fdiffs({2}).ChainDiffs({2, 1}).Build(),
      Builder().S(0).Dtis("SS").ChainDiffs({0, 0}).Build(),
      Builder().S(1).Dtis("-S").Fdiffs({2}).ChainDiffs({1, 2}).Build(),
      Builder().S(1).Dtis("-S").Fdiffs({1}).ChainDiffs({1, 1}).Build(),
  };
  return structure;
}

ScalableVideoController::LayerFrameConfig
ScalabilityStructureL2T1Key::KeyFrameConfig() const {
  LayerFrameConfig result;
  result.id = 0;
  result.spatial_id = 0;
  result.is_keyframe = true;
  result.buffers = {{/*id=*/0, /*references=*/false, /*updates=*/true}};
  return result;
}

std::vector<ScalableVideoController::LayerFrameConfig>
ScalabilityStructureL2T1Key::NextFrameConfig(bool restart) {
  std::vector<LayerFrameConfig> result(2);

  // Buffer0 keeps latest S0T0 frame, Buffer1 keeps latest S1T0 frame.
  if (restart || keyframe_) {
    result[0] = KeyFrameConfig();

    result[1].id = 2;
    result[1].spatial_id = 1;
    result[1].is_keyframe = false;
    result[1].buffers = {{/*id=*/0, /*references=*/true, /*updates=*/false},
                         {/*id=*/1, /*references=*/false, /*updates=*/true}};

    keyframe_ = false;
  } else {
    result[0].id = 1;
    result[0].spatial_id = 0;
    result[0].is_keyframe = false;
    result[0].buffers = {{/*id=*/0, /*references=*/true, /*updates=*/true}};

    result[1].id = 2;
    result[1].spatial_id = 1;
    result[1].is_keyframe = false;
    result[1].buffers = {{/*id=*/0, /*references=*/false, /*updates=*/false},
                         {/*id=*/1, /*references=*/true, /*updates=*/true}};
  }
  return result;
}

absl::optional<GenericFrameInfo> ScalabilityStructureL2T1Key::OnEncodeDone(
    LayerFrameConfig config) {
  absl::optional<GenericFrameInfo> frame_info;
  if (config.is_keyframe) {
    config = KeyFrameConfig();
  }

  if (config.id < 0 || config.id >= int{ABSL_ARRAYSIZE(kDtis)}) {
    RTC_LOG(LS_ERROR) << "Unexpected config id " << config.id;
    return frame_info;
  }
  frame_info.emplace();
  frame_info->spatial_id = config.spatial_id;
  frame_info->temporal_id = config.temporal_id;
  frame_info->encoder_buffers = std::move(config.buffers);
  frame_info->decode_target_indications.assign(std::begin(kDtis[config.id]),
                                               std::end(kDtis[config.id]));
  if (config.is_keyframe) {
    frame_info->part_of_chain = {true, true};
  } else {
    frame_info->part_of_chain = {config.spatial_id == 0,
                                 config.spatial_id == 1};
  }
  return frame_info;
}

}  // namespace webrtc
