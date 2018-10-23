/*
 *  Copyright 2018 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <array>
#include <cstring>

#include "api/audio/echo_canceller3_config.h"
#include "api/audio/echo_canceller3_config_json.h"
#include "rtc_base/random.h"
#include "test/gtest.h"

namespace webrtc {
namespace {
EchoCanceller3Config GenerateRandomConfig(Random* prng) {
  std::array<uint8_t, sizeof(EchoCanceller3Config)> random_bytes;
  for (uint8_t& byte : random_bytes) {
    byte = prng->Rand<uint8_t>();
  }
  auto* config = reinterpret_cast<EchoCanceller3Config*>(random_bytes.data());
  EchoCanceller3Config::Validate(config);
  return *config;
}
}  // namespace

TEST(EchoCanceller3JsonHelpers, ToStringAndParseJson) {
  Random prng(7297352569823ull);
  for (int i = 0; i < 10; ++i) {
    EchoCanceller3Config cfg = GenerateRandomConfig(&prng);
    std::string json_string = Aec3ConfigToJsonString(cfg);
    EchoCanceller3Config cfg_transformed =
        Aec3ConfigFromJsonString(json_string);

    // Expect an arbitrary subset of values to carry through the transformation.
    constexpr float kEpsilon = 1e-4;
    EXPECT_NEAR(cfg.filter.main.error_floor,
                cfg_transformed.filter.main.error_floor, kEpsilon);
    EXPECT_NEAR(cfg.ep_strength.default_len,
                cfg_transformed.ep_strength.default_len, kEpsilon);
    EXPECT_NEAR(cfg.suppressor.normal_tuning.mask_lf.enr_suppress,
                cfg_transformed.suppressor.normal_tuning.mask_lf.enr_suppress,
                kEpsilon);
    EXPECT_EQ(cfg.delay.down_sampling_factor,
              cfg_transformed.delay.down_sampling_factor);
    EXPECT_EQ(cfg.filter.shadow_initial.length_blocks,
              cfg_transformed.filter.shadow_initial.length_blocks);
    EXPECT_NEAR(cfg.suppressor.normal_tuning.mask_hf.enr_suppress,
                cfg_transformed.suppressor.normal_tuning.mask_hf.enr_suppress,
                kEpsilon);
  }
}

TEST(EchoCanceller3JsonHelpers, IteratedToStringGivesIdenticalStrings) {
  Random prng(7297352569824ull);
  for (int i = 0; i < 10; ++i) {
    EchoCanceller3Config config = GenerateRandomConfig(&prng);
    std::string json = Aec3ConfigToJsonString(config);
    std::string iterated_json =
        Aec3ConfigToJsonString(Aec3ConfigFromJsonString(json));
    EXPECT_EQ(json, iterated_json);
  }
}
}  // namespace webrtc
