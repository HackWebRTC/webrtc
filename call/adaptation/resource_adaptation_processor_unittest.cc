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

#include "api/adaptation/resource.h"
#include "api/scoped_refptr.h"
#include "api/video/video_adaptation_counters.h"
#include "call/adaptation/resource_adaptation_processor_interface.h"
#include "call/adaptation/test/fake_adaptation_constraint.h"
#include "call/adaptation/test/fake_adaptation_listener.h"
#include "call/adaptation/test/fake_frame_rate_provider.h"
#include "call/adaptation/test/fake_resource.h"
#include "call/adaptation/video_source_restrictions.h"
#include "call/adaptation/video_stream_input_state_provider.h"
#include "rtc_base/critical_section.h"
#include "rtc_base/event.h"
#include "rtc_base/gunit.h"
#include "rtc_base/task_queue_for_test.h"
#include "test/gtest.h"

namespace webrtc {

namespace {

const int kDefaultFrameRate = 30;
const int kDefaultFrameSize = 1280 * 720;
const int kDefaultTimeoutMs = 5000;

class VideoSourceRestrictionsListenerForTesting
    : public VideoSourceRestrictionsListener {
 public:
  VideoSourceRestrictionsListenerForTesting()
      : restrictions_updated_count_(0),
        restrictions_(),
        adaptation_counters_(),
        reason_(nullptr) {}
  ~VideoSourceRestrictionsListenerForTesting() override {}

  size_t restrictions_updated_count() const {
    rtc::CritScope crit(&lock_);
    return restrictions_updated_count_;
  }
  VideoSourceRestrictions restrictions() const {
    rtc::CritScope crit(&lock_);
    return restrictions_;
  }
  VideoAdaptationCounters adaptation_counters() const {
    rtc::CritScope crit(&lock_);
    return adaptation_counters_;
  }
  rtc::scoped_refptr<Resource> reason() const {
    rtc::CritScope crit(&lock_);
    return reason_;
  }

  // VideoSourceRestrictionsListener implementation.
  void OnVideoSourceRestrictionsUpdated(
      VideoSourceRestrictions restrictions,
      const VideoAdaptationCounters& adaptation_counters,
      rtc::scoped_refptr<Resource> reason) override {
    rtc::CritScope crit(&lock_);
    ++restrictions_updated_count_;
    restrictions_ = restrictions;
    adaptation_counters_ = adaptation_counters;
    reason_ = reason;
  }

 private:
  rtc::CriticalSection lock_;
  size_t restrictions_updated_count_ RTC_GUARDED_BY(lock_);
  VideoSourceRestrictions restrictions_ RTC_GUARDED_BY(lock_);
  VideoAdaptationCounters adaptation_counters_ RTC_GUARDED_BY(lock_);
  rtc::scoped_refptr<Resource> reason_ RTC_GUARDED_BY(lock_);
};

class ResourceAdaptationProcessorTest : public ::testing::Test {
 public:
  ResourceAdaptationProcessorTest()
      : frame_rate_provider_(),
        input_state_provider_(&frame_rate_provider_),
        resource_(FakeResource::Create("FakeResource")),
        other_resource_(FakeResource::Create("OtherFakeResource")),
        adaptation_constraint_("FakeAdaptationConstraint"),
        adaptation_listener_(),
        processor_(std::make_unique<ResourceAdaptationProcessor>(
            &input_state_provider_,
            /*encoder_stats_observer=*/&frame_rate_provider_)) {
    processor_->SetResourceAdaptationQueue(TaskQueueBase::Current());
    processor_->AddRestrictionsListener(&restrictions_listener_);
    processor_->AddResource(resource_);
    processor_->AddResource(other_resource_);
    processor_->AddAdaptationConstraint(&adaptation_constraint_);
    processor_->AddAdaptationListener(&adaptation_listener_);
  }
  ~ResourceAdaptationProcessorTest() override {
    if (processor_) {
      DestroyProcessor();
    }
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

  void DestroyProcessor() {
    processor_->StopResourceAdaptation();
    processor_->RemoveRestrictionsListener(&restrictions_listener_);
    processor_->RemoveResource(resource_);
    processor_->RemoveResource(other_resource_);
    processor_->RemoveAdaptationConstraint(&adaptation_constraint_);
    processor_->RemoveAdaptationListener(&adaptation_listener_);
    processor_.reset();
  }

 protected:
  FakeFrameRateProvider frame_rate_provider_;
  VideoStreamInputStateProvider input_state_provider_;
  rtc::scoped_refptr<FakeResource> resource_;
  rtc::scoped_refptr<FakeResource> other_resource_;
  FakeAdaptationConstraint adaptation_constraint_;
  FakeAdaptationListener adaptation_listener_;
  std::unique_ptr<ResourceAdaptationProcessor> processor_;
  VideoSourceRestrictionsListenerForTesting restrictions_listener_;
};

}  // namespace

TEST_F(ResourceAdaptationProcessorTest, DisabledByDefault) {
  EXPECT_EQ(DegradationPreference::DISABLED,
            processor_->degradation_preference());
  EXPECT_EQ(DegradationPreference::DISABLED,
            processor_->effective_degradation_preference());
  SetInputStates(true, kDefaultFrameRate, kDefaultFrameSize);
  processor_->StartResourceAdaptation();
  // Adaptation does not happen when disabled.
  resource_->SetUsageState(ResourceUsageState::kOveruse);
  EXPECT_EQ(0u, restrictions_listener_.restrictions_updated_count());
}

TEST_F(ResourceAdaptationProcessorTest, InsufficientInput) {
  processor_->SetDegradationPreference(
      DegradationPreference::MAINTAIN_FRAMERATE);
  processor_->StartResourceAdaptation();
  // Adaptation does not happen if input is insufficient.
  // When frame size is missing (OnFrameSizeObserved not called yet).
  input_state_provider_.OnHasInputChanged(true);
  resource_->SetUsageState(ResourceUsageState::kOveruse);
  EXPECT_EQ(0u, restrictions_listener_.restrictions_updated_count());
  // When "has input" is missing.
  SetInputStates(false, kDefaultFrameRate, kDefaultFrameSize);
  resource_->SetUsageState(ResourceUsageState::kOveruse);
  EXPECT_EQ(0u, restrictions_listener_.restrictions_updated_count());
  // Note: frame rate cannot be missing, if unset it is 0.
}

// These tests verify that restrictions are applied, but not exactly how much
// the source is restricted. This ensures that the VideoStreamAdapter is wired
// up correctly but not exactly how the VideoStreamAdapter generates
// restrictions. For that, see video_stream_adapter_unittest.cc.
TEST_F(ResourceAdaptationProcessorTest,
       OveruseTriggersRestrictingResolutionInMaintainFrameRate) {
  processor_->SetDegradationPreference(
      DegradationPreference::MAINTAIN_FRAMERATE);
  processor_->StartResourceAdaptation();
  SetInputStates(true, kDefaultFrameRate, kDefaultFrameSize);
  resource_->SetUsageState(ResourceUsageState::kOveruse);
  EXPECT_EQ(1u, restrictions_listener_.restrictions_updated_count());
  EXPECT_TRUE(
      restrictions_listener_.restrictions().max_pixels_per_frame().has_value());
}

TEST_F(ResourceAdaptationProcessorTest,
       OveruseTriggersRestrictingFrameRateInMaintainResolution) {
  processor_->SetDegradationPreference(
      DegradationPreference::MAINTAIN_RESOLUTION);
  processor_->StartResourceAdaptation();
  SetInputStates(true, kDefaultFrameRate, kDefaultFrameSize);
  resource_->SetUsageState(ResourceUsageState::kOveruse);
  EXPECT_EQ(1u, restrictions_listener_.restrictions_updated_count());
  EXPECT_TRUE(
      restrictions_listener_.restrictions().max_frame_rate().has_value());
}

TEST_F(ResourceAdaptationProcessorTest,
       OveruseTriggersRestrictingFrameRateAndResolutionInBalanced) {
  processor_->SetDegradationPreference(DegradationPreference::BALANCED);
  processor_->StartResourceAdaptation();
  SetInputStates(true, kDefaultFrameRate, kDefaultFrameSize);
  // Adapting multiple times eventually resticts both frame rate and
  // resolution. Exactly many times we need to adapt depends on
  // BalancedDegradationSettings, VideoStreamAdapter and default input
  // states. This test requires it to be achieved within 4 adaptations.
  for (size_t i = 0; i < 4; ++i) {
    resource_->SetUsageState(ResourceUsageState::kOveruse);
    EXPECT_EQ(i + 1, restrictions_listener_.restrictions_updated_count());
    RestrictSource(restrictions_listener_.restrictions());
  }
  EXPECT_TRUE(
      restrictions_listener_.restrictions().max_pixels_per_frame().has_value());
  EXPECT_TRUE(
      restrictions_listener_.restrictions().max_frame_rate().has_value());
}

TEST_F(ResourceAdaptationProcessorTest, AwaitingPreviousAdaptation) {
  processor_->SetDegradationPreference(
      DegradationPreference::MAINTAIN_FRAMERATE);
  processor_->StartResourceAdaptation();
  SetInputStates(true, kDefaultFrameRate, kDefaultFrameSize);
  resource_->SetUsageState(ResourceUsageState::kOveruse);
  EXPECT_EQ(1u, restrictions_listener_.restrictions_updated_count());
  // If we don't restrict the source then adaptation will not happen again
  // due to "awaiting previous adaptation". This prevents "double-adapt".
  resource_->SetUsageState(ResourceUsageState::kOveruse);
  EXPECT_EQ(1u, restrictions_listener_.restrictions_updated_count());
}

TEST_F(ResourceAdaptationProcessorTest, CannotAdaptUpWhenUnrestricted) {
  processor_->SetDegradationPreference(
      DegradationPreference::MAINTAIN_FRAMERATE);
  processor_->StartResourceAdaptation();
  SetInputStates(true, kDefaultFrameRate, kDefaultFrameSize);
  resource_->SetUsageState(ResourceUsageState::kUnderuse);
  EXPECT_EQ(0u, restrictions_listener_.restrictions_updated_count());
}

TEST_F(ResourceAdaptationProcessorTest, UnderuseTakesUsBackToUnrestricted) {
  processor_->SetDegradationPreference(
      DegradationPreference::MAINTAIN_FRAMERATE);
  processor_->StartResourceAdaptation();
  SetInputStates(true, kDefaultFrameRate, kDefaultFrameSize);
  resource_->SetUsageState(ResourceUsageState::kOveruse);
  EXPECT_EQ(1u, restrictions_listener_.restrictions_updated_count());
  RestrictSource(restrictions_listener_.restrictions());
  resource_->SetUsageState(ResourceUsageState::kUnderuse);
  EXPECT_EQ(2u, restrictions_listener_.restrictions_updated_count());
  EXPECT_EQ(VideoSourceRestrictions(), restrictions_listener_.restrictions());
}

TEST_F(ResourceAdaptationProcessorTest, ResourcesCanPreventAdaptingUp) {
  processor_->SetDegradationPreference(
      DegradationPreference::MAINTAIN_FRAMERATE);
  processor_->StartResourceAdaptation();
  SetInputStates(true, kDefaultFrameRate, kDefaultFrameSize);
  // Adapt down so that we can adapt up.
  resource_->SetUsageState(ResourceUsageState::kOveruse);
  EXPECT_EQ(1u, restrictions_listener_.restrictions_updated_count());
  RestrictSource(restrictions_listener_.restrictions());
  // Adapting up is prevented.
  adaptation_constraint_.set_is_adaptation_up_allowed(false);
  resource_->SetUsageState(ResourceUsageState::kUnderuse);
  EXPECT_EQ(1u, restrictions_listener_.restrictions_updated_count());
}

TEST_F(ResourceAdaptationProcessorTest,
       ResourcesCanNotAdaptUpIfNeverAdaptedDown) {
  processor_->SetDegradationPreference(
      DegradationPreference::MAINTAIN_FRAMERATE);
  processor_->StartResourceAdaptation();
  SetInputStates(true, kDefaultFrameRate, kDefaultFrameSize);
  resource_->SetUsageState(ResourceUsageState::kOveruse);
  EXPECT_EQ(1u, restrictions_listener_.restrictions_updated_count());
  RestrictSource(restrictions_listener_.restrictions());

  // Other resource signals under-use
  other_resource_->SetUsageState(ResourceUsageState::kUnderuse);
  EXPECT_EQ(1u, restrictions_listener_.restrictions_updated_count());
}

TEST_F(ResourceAdaptationProcessorTest,
       ResourcesCanNotAdaptUpIfNotAdaptedDownAfterReset) {
  processor_->SetDegradationPreference(
      DegradationPreference::MAINTAIN_FRAMERATE);
  processor_->StartResourceAdaptation();
  SetInputStates(true, kDefaultFrameRate, kDefaultFrameSize);
  resource_->SetUsageState(ResourceUsageState::kOveruse);
  EXPECT_EQ(1u, restrictions_listener_.restrictions_updated_count());

  processor_->ResetVideoSourceRestrictions();
  EXPECT_EQ(0, restrictions_listener_.adaptation_counters().Total());
  other_resource_->SetUsageState(ResourceUsageState::kOveruse);
  EXPECT_EQ(1, restrictions_listener_.adaptation_counters().Total());
  RestrictSource(restrictions_listener_.restrictions());

  // resource_ did not overuse after we reset the restrictions, so adapt
  // up should be disallowed.
  resource_->SetUsageState(ResourceUsageState::kUnderuse);
  EXPECT_EQ(1, restrictions_listener_.adaptation_counters().Total());
}

TEST_F(ResourceAdaptationProcessorTest, OnlyMostLimitedResourceMayAdaptUp) {
  processor_->SetDegradationPreference(
      DegradationPreference::MAINTAIN_FRAMERATE);
  processor_->StartResourceAdaptation();
  SetInputStates(true, kDefaultFrameRate, kDefaultFrameSize);
  resource_->SetUsageState(ResourceUsageState::kOveruse);
  EXPECT_EQ(1, restrictions_listener_.adaptation_counters().Total());
  RestrictSource(restrictions_listener_.restrictions());
  other_resource_->SetUsageState(ResourceUsageState::kOveruse);
  EXPECT_EQ(2, restrictions_listener_.adaptation_counters().Total());
  RestrictSource(restrictions_listener_.restrictions());

  // |other_resource_| is most limited, resource_ can't adapt up.
  resource_->SetUsageState(ResourceUsageState::kUnderuse);
  EXPECT_EQ(2, restrictions_listener_.adaptation_counters().Total());
  RestrictSource(restrictions_listener_.restrictions());
  other_resource_->SetUsageState(ResourceUsageState::kUnderuse);
  EXPECT_EQ(1, restrictions_listener_.adaptation_counters().Total());
  RestrictSource(restrictions_listener_.restrictions());

  // |resource_| and |other_resource_| are now most limited, so both must
  // signal underuse to adapt up.
  other_resource_->SetUsageState(ResourceUsageState::kUnderuse);
  EXPECT_EQ(1, restrictions_listener_.adaptation_counters().Total());
  RestrictSource(restrictions_listener_.restrictions());
  resource_->SetUsageState(ResourceUsageState::kUnderuse);
  EXPECT_EQ(0, restrictions_listener_.adaptation_counters().Total());
  RestrictSource(restrictions_listener_.restrictions());
}

TEST_F(ResourceAdaptationProcessorTest,
       MultipleResourcesCanTriggerMultipleAdaptations) {
  processor_->SetDegradationPreference(
      DegradationPreference::MAINTAIN_FRAMERATE);
  processor_->StartResourceAdaptation();
  SetInputStates(true, kDefaultFrameRate, kDefaultFrameSize);
  resource_->SetUsageState(ResourceUsageState::kOveruse);
  EXPECT_EQ(1, restrictions_listener_.adaptation_counters().Total());
  RestrictSource(restrictions_listener_.restrictions());
  other_resource_->SetUsageState(ResourceUsageState::kOveruse);
  EXPECT_EQ(2, restrictions_listener_.adaptation_counters().Total());
  RestrictSource(restrictions_listener_.restrictions());
  other_resource_->SetUsageState(ResourceUsageState::kOveruse);
  EXPECT_EQ(3, restrictions_listener_.adaptation_counters().Total());
  RestrictSource(restrictions_listener_.restrictions());

  // resource_ is not most limited so can't adapt from underuse.
  resource_->SetUsageState(ResourceUsageState::kUnderuse);
  EXPECT_EQ(3, restrictions_listener_.adaptation_counters().Total());
  RestrictSource(restrictions_listener_.restrictions());
  other_resource_->SetUsageState(ResourceUsageState::kUnderuse);
  EXPECT_EQ(2, restrictions_listener_.adaptation_counters().Total());
  RestrictSource(restrictions_listener_.restrictions());
  // resource_ is still not most limited so can't adapt from underuse.
  resource_->SetUsageState(ResourceUsageState::kUnderuse);
  EXPECT_EQ(2, restrictions_listener_.adaptation_counters().Total());
  RestrictSource(restrictions_listener_.restrictions());

  // However it will be after overuse
  resource_->SetUsageState(ResourceUsageState::kOveruse);
  EXPECT_EQ(3, restrictions_listener_.adaptation_counters().Total());
  RestrictSource(restrictions_listener_.restrictions());

  // Now other_resource_ can't adapt up as it is not most restricted.
  other_resource_->SetUsageState(ResourceUsageState::kUnderuse);
  EXPECT_EQ(3, restrictions_listener_.adaptation_counters().Total());
  RestrictSource(restrictions_listener_.restrictions());

  // resource_ is limited at 3 adaptations and other_resource_ 2.
  // With the most limited resource signalling underuse in the following
  // order we get back to unrestricted video.
  resource_->SetUsageState(ResourceUsageState::kUnderuse);
  EXPECT_EQ(2, restrictions_listener_.adaptation_counters().Total());
  RestrictSource(restrictions_listener_.restrictions());
  // Both resource_ and other_resource_ are most limited.
  other_resource_->SetUsageState(ResourceUsageState::kUnderuse);
  EXPECT_EQ(2, restrictions_listener_.adaptation_counters().Total());
  RestrictSource(restrictions_listener_.restrictions());
  resource_->SetUsageState(ResourceUsageState::kUnderuse);
  EXPECT_EQ(1, restrictions_listener_.adaptation_counters().Total());
  RestrictSource(restrictions_listener_.restrictions());
  // Again both are most limited.
  resource_->SetUsageState(ResourceUsageState::kUnderuse);
  EXPECT_EQ(1, restrictions_listener_.adaptation_counters().Total());
  RestrictSource(restrictions_listener_.restrictions());
  other_resource_->SetUsageState(ResourceUsageState::kUnderuse);
  EXPECT_EQ(0, restrictions_listener_.adaptation_counters().Total());
}

TEST_F(ResourceAdaptationProcessorTest,
       MostLimitedResourceAdaptationWorksAfterChangingDegradataionPreference) {
  processor_->SetDegradationPreference(
      DegradationPreference::MAINTAIN_FRAMERATE);
  processor_->StartResourceAdaptation();
  SetInputStates(true, kDefaultFrameRate, kDefaultFrameSize);
  // Adapt down until we can't anymore.
  resource_->SetUsageState(ResourceUsageState::kOveruse);
  RestrictSource(restrictions_listener_.restrictions());
  resource_->SetUsageState(ResourceUsageState::kOveruse);
  RestrictSource(restrictions_listener_.restrictions());
  resource_->SetUsageState(ResourceUsageState::kOveruse);
  RestrictSource(restrictions_listener_.restrictions());
  resource_->SetUsageState(ResourceUsageState::kOveruse);
  RestrictSource(restrictions_listener_.restrictions());
  resource_->SetUsageState(ResourceUsageState::kOveruse);
  RestrictSource(restrictions_listener_.restrictions());
  int last_total = restrictions_listener_.adaptation_counters().Total();

  processor_->SetDegradationPreference(
      DegradationPreference::MAINTAIN_RESOLUTION);
  // resource_ can not adapt up since we have never reduced FPS.
  resource_->SetUsageState(ResourceUsageState::kUnderuse);
  EXPECT_EQ(last_total, restrictions_listener_.adaptation_counters().Total());

  other_resource_->SetUsageState(ResourceUsageState::kOveruse);
  EXPECT_EQ(last_total + 1,
            restrictions_listener_.adaptation_counters().Total());
  RestrictSource(restrictions_listener_.restrictions());
  // other_resource_ is most limited so should be able to adapt up.
  other_resource_->SetUsageState(ResourceUsageState::kUnderuse);
  EXPECT_EQ(last_total, restrictions_listener_.adaptation_counters().Total());
}

TEST_F(ResourceAdaptationProcessorTest, AdaptingTriggersOnAdaptationApplied) {
  processor_->SetDegradationPreference(
      DegradationPreference::MAINTAIN_FRAMERATE);
  processor_->StartResourceAdaptation();
  SetInputStates(true, kDefaultFrameRate, kDefaultFrameSize);
  resource_->SetUsageState(ResourceUsageState::kOveruse);
  EXPECT_EQ(1u, adaptation_listener_.num_adaptations_applied());
}

TEST_F(ResourceAdaptationProcessorTest,
       AdaptsDownWhenOtherResourceIsAlwaysUnderused) {
  processor_->SetDegradationPreference(
      DegradationPreference::MAINTAIN_FRAMERATE);
  processor_->StartResourceAdaptation();
  SetInputStates(true, kDefaultFrameRate, kDefaultFrameSize);
  other_resource_->SetUsageState(ResourceUsageState::kUnderuse);
  // Does not trigger adapataion because there's no restriction.
  EXPECT_EQ(0, restrictions_listener_.adaptation_counters().Total());

  RestrictSource(restrictions_listener_.restrictions());
  resource_->SetUsageState(ResourceUsageState::kOveruse);
  // Adapts down even if other resource asked for adapting up.
  EXPECT_EQ(1, restrictions_listener_.adaptation_counters().Total());

  RestrictSource(restrictions_listener_.restrictions());
  other_resource_->SetUsageState(ResourceUsageState::kUnderuse);
  // Doesn't adapt up because adaptation is due to another resource.
  EXPECT_EQ(1, restrictions_listener_.adaptation_counters().Total());
  RestrictSource(restrictions_listener_.restrictions());
}

TEST_F(ResourceAdaptationProcessorTest,
       TriggerOveruseNotOnAdaptationTaskQueue) {
  processor_->SetDegradationPreference(
      DegradationPreference::MAINTAIN_FRAMERATE);
  processor_->StartResourceAdaptation();
  SetInputStates(true, kDefaultFrameRate, kDefaultFrameSize);

  TaskQueueForTest resource_task_queue("ResourceTaskQueue");
  resource_task_queue.PostTask(ToQueuedTask(
      [&]() { resource_->SetUsageState(ResourceUsageState::kOveruse); }));

  EXPECT_EQ_WAIT(1u, restrictions_listener_.restrictions_updated_count(),
                 kDefaultTimeoutMs);
}

TEST_F(ResourceAdaptationProcessorTest,
       DestroyProcessorWhileResourceListenerDelegateHasTaskInFlight) {
  processor_->SetDegradationPreference(
      DegradationPreference::MAINTAIN_FRAMERATE);
  processor_->StartResourceAdaptation();
  SetInputStates(true, kDefaultFrameRate, kDefaultFrameSize);

  // Wait for |resource_| to signal oversue first so we know that the delegate
  // has passed it on to the processor's task queue.
  rtc::Event resource_event;
  TaskQueueForTest resource_task_queue("ResourceTaskQueue");
  resource_task_queue.PostTask(ToQueuedTask([&]() {
    resource_->SetUsageState(ResourceUsageState::kOveruse);
    resource_event.Set();
  }));

  EXPECT_TRUE(resource_event.Wait(kDefaultTimeoutMs));
  // Now destroy the processor while handling the overuse is in flight.
  DestroyProcessor();

  // Because the processor was destroyed by the time the delegate's task ran,
  // the overuse signal must not have been handled.
  EXPECT_EQ(0u, restrictions_listener_.restrictions_updated_count());
}

}  // namespace webrtc
