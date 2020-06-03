/*
 *  Copyright (c) 2020 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <stddef.h>
#include <stdint.h>

#include <functional>
#include <memory>
#include <ostream>
#include <string>

#include "absl/types/optional.h"
#include "modules/video_coding/codecs/av1/scalability_structure_l1t2.h"
#include "modules/video_coding/codecs/av1/scalability_structure_l2t1.h"
#include "modules/video_coding/codecs/av1/scalability_structure_l2t1_key.h"
#include "modules/video_coding/codecs/av1/scalability_structure_s2t1.h"
#include "modules/video_coding/codecs/av1/scalable_video_controller.h"
#include "test/gmock.h"
#include "test/gtest.h"

namespace webrtc {
namespace {

using ::testing::AllOf;
using ::testing::Each;
using ::testing::Field;
using ::testing::Ge;
using ::testing::IsEmpty;
using ::testing::Le;
using ::testing::Lt;
using ::testing::Not;
using ::testing::SizeIs;
using ::testing::TestWithParam;
using ::testing::Values;

struct SvcTestParam {
  friend std::ostream& operator<<(std::ostream& os, const SvcTestParam& param) {
    return os << param.name;
  }

  std::string name;
  std::function<std::unique_ptr<ScalableVideoController>()> svc_factory;
};

class ScalabilityStructureTest : public TestWithParam<SvcTestParam> {};

TEST_P(ScalabilityStructureTest,
       NumberOfDecodeTargetsAndChainsAreInRangeAndConsistent) {
  FrameDependencyStructure structure =
      GetParam().svc_factory()->DependencyStructure();
  EXPECT_GT(structure.num_decode_targets, 0);
  EXPECT_LE(structure.num_decode_targets, 32);
  EXPECT_GE(structure.num_chains, 0);
  EXPECT_LE(structure.num_chains, structure.num_decode_targets);
  if (structure.num_chains == 0) {
    EXPECT_THAT(structure.decode_target_protected_by_chain, IsEmpty());
  } else {
    EXPECT_THAT(structure.decode_target_protected_by_chain,
                AllOf(SizeIs(structure.num_decode_targets), Each(Ge(0)),
                      Each(Le(structure.num_chains))));
  }
  EXPECT_THAT(structure.templates, SizeIs(Lt(size_t{64})));
}

TEST_P(ScalabilityStructureTest, TemplatesAreSortedByLayerId) {
  FrameDependencyStructure structure =
      GetParam().svc_factory()->DependencyStructure();
  ASSERT_THAT(structure.templates, Not(IsEmpty()));
  const auto& first_templates = structure.templates.front();
  EXPECT_EQ(first_templates.spatial_id, 0);
  EXPECT_EQ(first_templates.temporal_id, 0);
  for (size_t i = 1; i < structure.templates.size(); ++i) {
    const auto& prev_template = structure.templates[i - 1];
    const auto& next_template = structure.templates[i];
    if (next_template.spatial_id == prev_template.spatial_id &&
        next_template.temporal_id == prev_template.temporal_id) {
      // Same layer, next_layer_idc == 0
    } else if (next_template.spatial_id == prev_template.spatial_id &&
               next_template.temporal_id == prev_template.temporal_id + 1) {
      // Next temporal layer, next_layer_idc == 1
    } else if (next_template.spatial_id == prev_template.spatial_id + 1 &&
               next_template.temporal_id == 0) {
      // Next spatial layer, next_layer_idc == 2
    } else {
      // everything else is invalid.
      ADD_FAILURE() << "Invalid templates order. Template #" << i
                    << " with layer (" << next_template.spatial_id << ","
                    << next_template.temporal_id
                    << ") follows template with layer ("
                    << prev_template.spatial_id << ","
                    << prev_template.temporal_id << ").";
    }
  }
}

TEST_P(ScalabilityStructureTest, TemplatesMatchNumberOfDecodeTargetsAndChains) {
  FrameDependencyStructure structure =
      GetParam().svc_factory()->DependencyStructure();
  EXPECT_THAT(
      structure.templates,
      Each(AllOf(Field(&FrameDependencyTemplate::decode_target_indications,
                       SizeIs(structure.num_decode_targets)),
                 Field(&FrameDependencyTemplate::chain_diffs,
                       SizeIs(structure.num_chains)))));
}

INSTANTIATE_TEST_SUITE_P(
    Svc,
    ScalabilityStructureTest,
    Values(SvcTestParam{"L1T2", std::make_unique<ScalabilityStructureL1T2>},
           SvcTestParam{"L2T1", std::make_unique<ScalabilityStructureL2T1>},
           SvcTestParam{"L2T1Key",
                        std::make_unique<ScalabilityStructureL2T1Key>},
           SvcTestParam{"S2T1", std::make_unique<ScalabilityStructureS2T1>}),
    [](const testing::TestParamInfo<SvcTestParam>& info) {
      return info.param.name;
    });

}  // namespace
}  // namespace webrtc
