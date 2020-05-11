/*
 *  Copyright (c) 2020 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "call/adaptation/resource_adaptation_processor.h"

#include "api/video/video_adaptation_counters.h"
#include "call/adaptation/resource.h"
#include "call/adaptation/resource_adaptation_processor_interface.h"
#include "call/adaptation/test/fake_frame_rate_provider.h"
#include "call/adaptation/test/fake_resource.h"
#include "call/adaptation/video_source_restrictions.h"
#include "call/adaptation/video_stream_input_state_provider.h"
#include "test/gtest.h"

namespace webrtc {

namespace {

const int kDefaultFrameRate = 30;
const int kDefaultFrameSize = 1280 * 720;

class ResourceAdaptationProcessorListenerForTesting
    : public ResourceAdaptationProcessorListener {
 public:
  ResourceAdaptationProcessorListenerForTesting()
      : restrictions_updated_count_(0),
        restrictions_(),
        adaptation_counters_(),
        reason_(nullptr) {}
  ~ResourceAdaptationProcessorListenerForTesting() override {}

  size_t restrictions_updated_count() const {
    return restrictions_updated_count_;
  }
  const VideoSourceRestrictions& restrictions() const { return restrictions_; }
  const VideoAdaptationCounters& adaptation_counters() const {
    return adaptation_counters_;
  }
  const Resource* reason() const { return reason_; }

  // ResourceAdaptationProcessorListener implementation.
  void OnVideoSourceRestrictionsUpdated(
      VideoSourceRestrictions restrictions,
      const VideoAdaptationCounters& adaptation_counters,
      const Resource* reason) override {
    ++restrictions_updated_count_;
    restrictions_ = restrictions;
    adaptation_counters_ = adaptation_counters;
    reason_ = reason;
  }

 private:
  size_t restrictions_updated_count_;
  VideoSourceRestrictions restrictions_;
  VideoAdaptationCounters adaptation_counters_;
  const Resource* reason_;
};

class ResourceAdaptationProcessorTest : public ::testing::Test {
 public:
  ResourceAdaptationProcessorTest()
      : frame_rate_provider_(),
        input_state_provider_(&frame_rate_provider_),
        resource_("FakeResource"),
        other_resource_("OtherFakeResource"),
        processor_(&input_state_provider_,
                   /*encoder_stats_observer=*/&frame_rate_provider_) {
    processor_.AddAdaptationListener(&processor_listener_);
    processor_.AddResource(&resource_);
    processor_.AddResource(&other_resource_);
  }
  ~ResourceAdaptationProcessorTest() override {
    processor_.StopResourceAdaptation();
  }

  void SetInputStates(bool has_input, int fps, int frame_size) {
    input_state_provider_.OnHasInputChanged(has_input);
    frame_rate_provider_.set_fps(fps);
    input_state_provider_.OnFrameSizeObserved(frame_size);
  }

  void RestrictSource(VideoSourceRestrictions restrictions) {
    SetInputStates(
        true, restrictions.max_frame_rate().value_or(kDefaultFrameRate),
        restrictions.target_pixels_per_frame().has_value()
            ? restrictions.target_pixels_per_frame().value()
            : restrictions.max_pixels_per_frame().value_or(kDefaultFrameSize));
  }

 protected:
  FakeFrameRateProvider frame_rate_provider_;
  VideoStreamInputStateProvider input_state_provider_;
  FakeResource resource_;
  FakeResource other_resource_;
  ResourceAdaptationProcessor processor_;
  ResourceAdaptationProcessorListenerForTesting processor_listener_;
};

}  // namespace

TEST_F(ResourceAdaptationProcessorTest, DisabledByDefault) {
  EXPECT_EQ(DegradationPreference::DISABLED,
            processor_.degradation_preference());
  EXPECT_EQ(DegradationPreference::DISABLED,
            processor_.effective_degradation_preference());
  SetInputStates(true, kDefaultFrameRate, kDefaultFrameSize);
  processor_.StartResourceAdaptation();
  // Adaptation does not happen when disabled.
  resource_.set_usage_state(ResourceUsageState::kOveruse);
  EXPECT_EQ(0u, processor_listener_.restrictions_updated_count());
}

TEST_F(ResourceAdaptationProcessorTest, InsufficientInput) {
  processor_.SetDegradationPreference(
      DegradationPreference::MAINTAIN_FRAMERATE);
  processor_.StartResourceAdaptation();
  // Adaptation does not happen if input is insufficient.
  // When frame size is missing (OnFrameSizeObserved not called yet).
  input_state_provider_.OnHasInputChanged(true);
  resource_.set_usage_state(ResourceUsageState::kOveruse);
  EXPECT_EQ(0u, processor_listener_.restrictions_updated_count());
  // When "has input" is missing.
  SetInputStates(false, kDefaultFrameRate, kDefaultFrameSize);
  resource_.set_usage_state(ResourceUsageState::kOveruse);
  EXPECT_EQ(0u, processor_listener_.restrictions_updated_count());
  // Note: frame rate cannot be missing, if unset it is 0.
}

// These tests verify that restrictions are applied, but not exactly how much
// the source is restricted. This ensures that the VideoStreamAdapter is wired
// up correctly but not exactly how the VideoStreamAdapter generates
// restrictions. For that, see video_stream_adapter_unittest.cc.
TEST_F(ResourceAdaptationProcessorTest,
       OveruseTriggersRestrictingResolutionInMaintainFrameRate) {
  processor_.SetDegradationPreference(
      DegradationPreference::MAINTAIN_FRAMERATE);
  processor_.StartResourceAdaptation();
  SetInputStates(true, kDefaultFrameRate, kDefaultFrameSize);
  resource_.set_usage_state(ResourceUsageState::kOveruse);
  EXPECT_EQ(1u, processor_listener_.restrictions_updated_count());
  EXPECT_TRUE(
      processor_listener_.restrictions().max_pixels_per_frame().has_value());
}

TEST_F(ResourceAdaptationProcessorTest,
       OveruseTriggersRestrictingFrameRateInMaintainResolution) {
  processor_.SetDegradationPreference(
      DegradationPreference::MAINTAIN_RESOLUTION);
  processor_.StartResourceAdaptation();
  SetInputStates(true, kDefaultFrameRate, kDefaultFrameSize);
  resource_.set_usage_state(ResourceUsageState::kOveruse);
  EXPECT_EQ(1u, processor_listener_.restrictions_updated_count());
  EXPECT_TRUE(processor_listener_.restrictions().max_frame_rate().has_value());
}

TEST_F(ResourceAdaptationProcessorTest,
       OveruseTriggersRestrictingFrameRateAndResolutionInBalanced) {
  processor_.SetDegradationPreference(DegradationPreference::BALANCED);
  processor_.StartResourceAdaptation();
  SetInputStates(true, kDefaultFrameRate, kDefaultFrameSize);
  // Adapting multiple times eventually resticts both frame rate and resolution.
  // Exactly many times we need to adapt depends on BalancedDegradationSettings,
  // VideoStreamAdapter and default input states. This test requires it to be
  // achieved within 4 adaptations.
  for (size_t i = 0; i < 4; ++i) {
    resource_.set_usage_state(ResourceUsageState::kOveruse);
    EXPECT_EQ(i + 1, processor_listener_.restrictions_updated_count());
    RestrictSource(processor_listener_.restrictions());
  }
  EXPECT_TRUE(
      processor_listener_.restrictions().max_pixels_per_frame().has_value());
  EXPECT_TRUE(processor_listener_.restrictions().max_frame_rate().has_value());
}

TEST_F(ResourceAdaptationProcessorTest, AwaitingPreviousAdaptation) {
  processor_.SetDegradationPreference(
      DegradationPreference::MAINTAIN_FRAMERATE);
  processor_.StartResourceAdaptation();
  SetInputStates(true, kDefaultFrameRate, kDefaultFrameSize);
  resource_.set_usage_state(ResourceUsageState::kOveruse);
  EXPECT_EQ(1u, processor_listener_.restrictions_updated_count());
  // If we don't restrict the source then adaptation will not happen again due
  // to "awaiting previous adaptation". This prevents "double-adapt".
  resource_.set_usage_state(ResourceUsageState::kOveruse);
  EXPECT_EQ(1u, processor_listener_.restrictions_updated_count());
}

TEST_F(ResourceAdaptationProcessorTest, CannotAdaptUpWhenUnrestricted) {
  processor_.SetDegradationPreference(
      DegradationPreference::MAINTAIN_FRAMERATE);
  processor_.StartResourceAdaptation();
  SetInputStates(true, kDefaultFrameRate, kDefaultFrameSize);
  resource_.set_usage_state(ResourceUsageState::kUnderuse);
  EXPECT_EQ(0u, processor_listener_.restrictions_updated_count());
}

TEST_F(ResourceAdaptationProcessorTest, UnderuseTakesUsBackToUnrestricted) {
  processor_.SetDegradationPreference(
      DegradationPreference::MAINTAIN_FRAMERATE);
  processor_.StartResourceAdaptation();
  SetInputStates(true, kDefaultFrameRate, kDefaultFrameSize);
  resource_.set_usage_state(ResourceUsageState::kOveruse);
  EXPECT_EQ(1u, processor_listener_.restrictions_updated_count());
  RestrictSource(processor_listener_.restrictions());
  resource_.set_usage_state(ResourceUsageState::kUnderuse);
  EXPECT_EQ(2u, processor_listener_.restrictions_updated_count());
  EXPECT_EQ(VideoSourceRestrictions(), processor_listener_.restrictions());
}

TEST_F(ResourceAdaptationProcessorTest, ResourcesCanPreventAdaptingUp) {
  processor_.SetDegradationPreference(
      DegradationPreference::MAINTAIN_FRAMERATE);
  processor_.StartResourceAdaptation();
  SetInputStates(true, kDefaultFrameRate, kDefaultFrameSize);
  // Adapt down so that we can adapt up.
  resource_.set_usage_state(ResourceUsageState::kOveruse);
  EXPECT_EQ(1u, processor_listener_.restrictions_updated_count());
  RestrictSource(processor_listener_.restrictions());
  // Adapting up is prevented.
  resource_.set_is_adaptation_up_allowed(false);
  resource_.set_usage_state(ResourceUsageState::kUnderuse);
  EXPECT_EQ(1u, processor_listener_.restrictions_updated_count());
}

TEST_F(ResourceAdaptationProcessorTest,
       ResourcesCanNotAdaptUpIfNeverAdaptedDown) {
  processor_.SetDegradationPreference(
      DegradationPreference::MAINTAIN_FRAMERATE);
  processor_.StartResourceAdaptation();
  SetInputStates(true, kDefaultFrameRate, kDefaultFrameSize);
  resource_.set_usage_state(ResourceUsageState::kOveruse);
  EXPECT_EQ(1u, processor_listener_.restrictions_updated_count());
  RestrictSource(processor_listener_.restrictions());

  // Other resource signals under-use
  other_resource_.set_usage_state(ResourceUsageState::kUnderuse);
  EXPECT_EQ(1u, processor_listener_.restrictions_updated_count());
}

TEST_F(ResourceAdaptationProcessorTest,
       ResourcesCanNotAdaptUpIfNotAdaptedDownAfterReset) {
  processor_.SetDegradationPreference(
      DegradationPreference::MAINTAIN_FRAMERATE);
  processor_.StartResourceAdaptation();
  SetInputStates(true, kDefaultFrameRate, kDefaultFrameSize);
  resource_.set_usage_state(ResourceUsageState::kOveruse);
  EXPECT_EQ(1u, processor_listener_.restrictions_updated_count());

  processor_.ResetVideoSourceRestrictions();
  EXPECT_EQ(0, processor_listener_.adaptation_counters().Total());
  other_resource_.set_usage_state(ResourceUsageState::kOveruse);
  EXPECT_EQ(1, processor_listener_.adaptation_counters().Total());
  RestrictSource(processor_listener_.restrictions());

  // resource_ did not overuse after we reset the restrictions, so adapt up
  // should be disallowed.
  resource_.set_usage_state(ResourceUsageState::kUnderuse);
  EXPECT_EQ(1, processor_listener_.adaptation_counters().Total());
}

TEST_F(ResourceAdaptationProcessorTest,
       MultipleResourcesCanTriggerMultipleAdaptations) {
  processor_.SetDegradationPreference(
      DegradationPreference::MAINTAIN_FRAMERATE);
  processor_.StartResourceAdaptation();
  SetInputStates(true, kDefaultFrameRate, kDefaultFrameSize);
  resource_.set_usage_state(ResourceUsageState::kOveruse);
  EXPECT_EQ(1, processor_listener_.adaptation_counters().Total());
  RestrictSource(processor_listener_.restrictions());
  other_resource_.set_usage_state(ResourceUsageState::kOveruse);
  EXPECT_EQ(2, processor_listener_.adaptation_counters().Total());
  RestrictSource(processor_listener_.restrictions());
  other_resource_.set_usage_state(ResourceUsageState::kOveruse);
  EXPECT_EQ(3, processor_listener_.adaptation_counters().Total());
  RestrictSource(processor_listener_.restrictions());

  resource_.set_usage_state(ResourceUsageState::kUnderuse);
  EXPECT_EQ(2, processor_listener_.adaptation_counters().Total());
  RestrictSource(processor_listener_.restrictions());
  // Does not trigger adaptation since resource has no adaptations left.
  resource_.set_usage_state(ResourceUsageState::kUnderuse);
  EXPECT_EQ(2, processor_listener_.adaptation_counters().Total());
  RestrictSource(processor_listener_.restrictions());

  other_resource_.set_usage_state(ResourceUsageState::kUnderuse);
  EXPECT_EQ(1, processor_listener_.adaptation_counters().Total());
  RestrictSource(processor_listener_.restrictions());
  other_resource_.set_usage_state(ResourceUsageState::kUnderuse);
  EXPECT_EQ(0, processor_listener_.adaptation_counters().Total());
  RestrictSource(processor_listener_.restrictions());
}

TEST_F(ResourceAdaptationProcessorTest, AdaptingTriggersOnAdaptationApplied) {
  processor_.SetDegradationPreference(
      DegradationPreference::MAINTAIN_FRAMERATE);
  processor_.StartResourceAdaptation();
  SetInputStates(true, kDefaultFrameRate, kDefaultFrameSize);
  resource_.set_usage_state(ResourceUsageState::kOveruse);
  EXPECT_EQ(1u, resource_.num_adaptations_applied());
}

TEST_F(ResourceAdaptationProcessorTest, AdaptingClearsResourceUsageState) {
  processor_.SetDegradationPreference(
      DegradationPreference::MAINTAIN_FRAMERATE);
  processor_.StartResourceAdaptation();
  SetInputStates(true, kDefaultFrameRate, kDefaultFrameSize);
  resource_.set_usage_state(ResourceUsageState::kOveruse);
  EXPECT_EQ(1u, processor_listener_.restrictions_updated_count());
  EXPECT_FALSE(resource_.usage_state().has_value());
}

TEST_F(ResourceAdaptationProcessorTest,
       FailingAdaptingAlsoClearsResourceUsageState) {
  processor_.SetDegradationPreference(DegradationPreference::DISABLED);
  processor_.StartResourceAdaptation();
  resource_.set_usage_state(ResourceUsageState::kOveruse);
  EXPECT_EQ(0u, processor_listener_.restrictions_updated_count());
  EXPECT_FALSE(resource_.usage_state().has_value());
}

TEST_F(ResourceAdaptationProcessorTest,
       AdaptsDownWhenOtherResourceIsAlwaysUnderused) {
  processor_.SetDegradationPreference(
      DegradationPreference::MAINTAIN_FRAMERATE);
  processor_.StartResourceAdaptation();
  SetInputStates(true, kDefaultFrameRate, kDefaultFrameSize);
  other_resource_.set_usage_state(ResourceUsageState::kUnderuse);
  // Does not trigger adapataion because there's no restriction.
  EXPECT_EQ(0, processor_listener_.adaptation_counters().Total());

  RestrictSource(processor_listener_.restrictions());
  resource_.set_usage_state(ResourceUsageState::kOveruse);
  // Adapts down even if other resource asked for adapting up.
  EXPECT_EQ(1, processor_listener_.adaptation_counters().Total());

  RestrictSource(processor_listener_.restrictions());
  other_resource_.set_usage_state(ResourceUsageState::kUnderuse);
  // Doesn't adapt up because adaptation is due to another resource.
  EXPECT_EQ(1, processor_listener_.adaptation_counters().Total());
  RestrictSource(processor_listener_.restrictions());
}

}  // namespace webrtc
