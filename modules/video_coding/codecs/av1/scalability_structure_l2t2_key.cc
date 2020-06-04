/*
 *  Copyright (c) 2020 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#include "modules/video_coding/codecs/av1/scalability_structure_l2t2_key.h"

#include <utility>
#include <vector>

#include "absl/base/macros.h"
#include "api/transport/rtp/dependency_descriptor.h"
#include "rtc_base/checks.h"
#include "rtc_base/logging.h"

namespace webrtc {
namespace {

constexpr auto kNotPresent = DecodeTargetIndication::kNotPresent;
constexpr auto kDiscardable = DecodeTargetIndication::kDiscardable;
constexpr auto kSwitch = DecodeTargetIndication::kSwitch;

// decode targets: S0T0, S0T1, S1T0, S1T1
constexpr DecodeTargetIndication kDtis[6][4] = {
    {kSwitch, kSwitch, kSwitch, kSwitch},                   //    kKey, S0
    {kNotPresent, kNotPresent, kSwitch, kSwitch},           //    kKey, S1
    {kNotPresent, kDiscardable, kNotPresent, kNotPresent},  //    kDeltaT1, S0
    {kNotPresent, kNotPresent, kNotPresent, kDiscardable},  //    kDeltaT1, S1
    {kSwitch, kSwitch, kNotPresent, kNotPresent},           //    kDeltaT0, S0
    {kNotPresent, kNotPresent, kSwitch, kSwitch},           //    kDeltaT0, S1
};

}  // namespace

ScalabilityStructureL2T2Key::~ScalabilityStructureL2T2Key() = default;

ScalableVideoController::StreamLayersConfig
ScalabilityStructureL2T2Key::StreamConfig() const {
  StreamLayersConfig result;
  result.num_spatial_layers = 2;
  result.num_temporal_layers = 2;
  return result;
}

FrameDependencyStructure ScalabilityStructureL2T2Key::DependencyStructure()
    const {
  using Builder = GenericFrameInfo::Builder;
  FrameDependencyStructure structure;
  structure.num_decode_targets = 4;
  structure.num_chains = 2;
  structure.decode_target_protected_by_chain = {0, 0, 1, 1};
  structure.templates = {
      Builder().S(0).T(0).Dtis("SSSS").ChainDiffs({0, 0}).Build(),
      Builder().S(0).T(0).Dtis("SS--").Fdiffs({4}).ChainDiffs({4, 3}).Build(),
      Builder().S(0).T(1).Dtis("-D--").Fdiffs({2}).ChainDiffs({2, 1}).Build(),
      Builder().S(1).T(0).Dtis("--SS").Fdiffs({1}).ChainDiffs({1, 1}).Build(),
      Builder().S(1).T(0).Dtis("--SS").Fdiffs({4}).ChainDiffs({1, 4}).Build(),
      Builder().S(1).T(1).Dtis("---D").Fdiffs({2}).ChainDiffs({3, 2}).Build(),
  };
  return structure;
}

ScalableVideoController::LayerFrameConfig
ScalabilityStructureL2T2Key::KeyFrameConfig() const {
  LayerFrameConfig result;
  result.id = 0;
  result.is_keyframe = true;
  result.spatial_id = 0;
  result.temporal_id = 0;
  result.buffers = {{/*id=*/0, /*referenced=*/false, /*updated=*/true}};
  return result;
}

std::vector<ScalableVideoController::LayerFrameConfig>
ScalabilityStructureL2T2Key::NextFrameConfig(bool restart) {
  if (restart) {
    next_pattern_ = kKey;
  }
  std::vector<LayerFrameConfig> result(2);

  // Buffer0 keeps latest S0T0 frame,
  // Buffer1 keeps latest S1T0 frame.
  switch (next_pattern_) {
    case kKey:
      result[0] = KeyFrameConfig();

      result[1].id = 1;
      result[1].is_keyframe = false;
      result[1].spatial_id = 1;
      result[1].temporal_id = 0;
      result[1].buffers = {{/*id=*/0, /*referenced=*/true, /*updated=*/false},
                           {/*id=*/1, /*referenced=*/false, /*updated=*/true}};

      next_pattern_ = kDeltaT1;
      break;
    case kDeltaT1:
      result[0].id = 2;
      result[0].is_keyframe = false;
      result[0].spatial_id = 0;
      result[0].temporal_id = 1;
      result[0].buffers = {{/*id=*/0, /*referenced=*/true, /*updated=*/false}};

      result[1].id = 3;
      result[1].is_keyframe = false;
      result[1].spatial_id = 1;
      result[1].temporal_id = 1;
      result[1].buffers = {{/*id=*/1, /*referenced=*/true, /*updated=*/false}};

      next_pattern_ = kDeltaT0;
      break;
    case kDeltaT0:
      result[0].id = 4;
      result[0].is_keyframe = false;
      result[0].spatial_id = 0;
      result[0].temporal_id = 0;
      result[0].buffers = {{/*id=*/0, /*referenced=*/true, /*updated=*/true}};

      result[1].id = 5;
      result[1].is_keyframe = false;
      result[1].spatial_id = 1;
      result[1].temporal_id = 0;
      result[1].buffers = {{/*id=*/1, /*referenced=*/true, /*updated=*/true}};

      next_pattern_ = kDeltaT1;
      break;
  }
  return result;
}

absl::optional<GenericFrameInfo> ScalabilityStructureL2T2Key::OnEncodeDone(
    LayerFrameConfig config) {
  if (config.is_keyframe) {
    config = KeyFrameConfig();
  }

  absl::optional<GenericFrameInfo> frame_info;
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
  } else if (config.temporal_id == 0) {
    frame_info->part_of_chain = {config.spatial_id == 0,
                                 config.spatial_id == 1};
  } else {
    frame_info->part_of_chain = {false, false};
  }
  return frame_info;
}

}  // namespace webrtc
