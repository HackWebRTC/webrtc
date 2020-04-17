/*
 *  Copyright (c) 2020 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "call/adaptation/video_stream_adapter.h"

#include <string>
#include <utility>

#include "absl/types/optional.h"
#include "api/video/video_adaptation_reason.h"
#include "api/video_codecs/video_codec.h"
#include "api/video_codecs/video_encoder.h"
#include "api/video_codecs/video_encoder_config.h"
#include "call/adaptation/encoder_settings.h"
#include "call/adaptation/video_source_restrictions.h"
#include "rtc_base/string_encode.h"
#include "test/field_trial.h"
#include "test/gmock.h"
#include "test/gtest.h"
#include "test/testsupport/rtc_expect_death.h"

namespace webrtc {

namespace {

const int kBalancedHighResolutionPixels = 1280 * 720;
const int kBalancedHighFrameRateFps = 30;

const int kBalancedMediumResolutionPixels = 640 * 480;
const int kBalancedMediumFrameRateFps = 20;

const int kBalancedLowResolutionPixels = 320 * 240;
const int kBalancedLowFrameRateFps = 10;

std::string BalancedFieldTrialConfig() {
  return "WebRTC-Video-BalancedDegradationSettings/pixels:" +
         rtc::ToString(kBalancedLowResolutionPixels) + "|" +
         rtc::ToString(kBalancedMediumResolutionPixels) + "|" +
         rtc::ToString(kBalancedHighResolutionPixels) +
         ",fps:" + rtc::ToString(kBalancedLowFrameRateFps) + "|" +
         rtc::ToString(kBalancedMediumFrameRateFps) + "|" +
         rtc::ToString(kBalancedHighFrameRateFps) + "/";
}

VideoStreamInputState InputState(int input_pixels,
                                 int input_fps,
                                 int min_pixels_per_frame) {
  VideoStreamInputState input_state;
  input_state.set_has_input(true);
  input_state.set_frame_size_pixels(input_pixels);
  input_state.set_frames_per_second(input_fps);
  input_state.set_min_pixels_per_frame(min_pixels_per_frame);
  return input_state;
}

// Responsible for adjusting the inputs to VideoStreamAdapter (SetInput), such
// as pixels and frame rate, according to the most recent source restrictions.
// This helps tests that apply adaptations multiple times: if the input is not
// adjusted between adaptations, the subsequent adaptations fail with
// kAwaitingPreviousAdaptation.
class FakeVideoStream {
 public:
  FakeVideoStream(VideoStreamAdapter* adapter,
                  int input_pixels,
                  int input_fps,
                  int min_pixels_per_frame)
      : adapter_(adapter),
        input_pixels_(input_pixels),
        input_fps_(input_fps),
        min_pixels_per_frame_(min_pixels_per_frame) {
    adapter_->SetInput(
        InputState(input_pixels_, input_fps_, min_pixels_per_frame_));
  }

  int input_pixels() const { return input_pixels_; }
  int input_fps() const { return input_fps_; }

  // Performs ApplyAdaptation() followed by SetInput() with input pixels and
  // frame rate adjusted according to the resulting restrictions.
  void ApplyAdaptation(Adaptation adaptation) {
    adapter_->ApplyAdaptation(adaptation);
    // Update input pixels and fps according to the resulting restrictions.
    auto restrictions = adapter_->source_restrictions();
    if (restrictions.target_pixels_per_frame().has_value()) {
      RTC_DCHECK(!restrictions.max_pixels_per_frame().has_value() ||
                 restrictions.max_pixels_per_frame().value() >=
                     restrictions.target_pixels_per_frame().value());
      input_pixels_ = restrictions.target_pixels_per_frame().value();
    } else if (restrictions.max_pixels_per_frame().has_value()) {
      input_pixels_ = restrictions.max_pixels_per_frame().value();
    }
    if (restrictions.max_frame_rate().has_value()) {
      input_fps_ = restrictions.max_frame_rate().value();
    }
    adapter_->SetInput(
        InputState(input_pixels_, input_fps_, min_pixels_per_frame_));
  }

 private:
  VideoStreamAdapter* adapter_;
  int input_pixels_;
  int input_fps_;
  int min_pixels_per_frame_;
};

}  // namespace

TEST(VideoStreamAdapterTest, NoRestrictionsByDefault) {
  VideoStreamAdapter adapter;
  EXPECT_EQ(VideoSourceRestrictions(), adapter.source_restrictions());
  EXPECT_EQ(0, adapter.adaptation_counters().Total());
}

TEST(VideoStreamAdapterTest, MaintainFramerate_DecreasesPixelsToThreeFifths) {
  const int kInputPixels = 1280 * 720;
  VideoStreamAdapter adapter;
  adapter.SetDegradationPreference(DegradationPreference::MAINTAIN_FRAMERATE);
  adapter.SetInput(InputState(kInputPixels, 30, kDefaultMinPixelsPerFrame));
  Adaptation adaptation = adapter.GetAdaptationDown();
  EXPECT_EQ(Adaptation::Status::kValid, adaptation.status());
  EXPECT_FALSE(adaptation.min_pixel_limit_reached());
  adapter.ApplyAdaptation(adaptation);
  EXPECT_EQ(static_cast<size_t>((kInputPixels * 3) / 5),
            adapter.source_restrictions().max_pixels_per_frame());
  EXPECT_EQ(absl::nullopt,
            adapter.source_restrictions().target_pixels_per_frame());
  EXPECT_EQ(absl::nullopt, adapter.source_restrictions().max_frame_rate());
  EXPECT_EQ(1, adapter.adaptation_counters().resolution_adaptations);
}

TEST(VideoStreamAdapterTest, MaintainFramerate_DecreasesPixelsToLimitReached) {
  const int kMinPixelsPerFrame = 640 * 480;
  VideoStreamAdapter adapter;
  adapter.SetDegradationPreference(DegradationPreference::MAINTAIN_FRAMERATE);
  adapter.SetInput(InputState(kMinPixelsPerFrame + 1, 30, kMinPixelsPerFrame));
  // Even though we are above kMinPixelsPerFrame, because adapting down would
  // have exceeded the limit, we are said to have reached the limit already.
  // This differs from the frame rate adaptation logic, which would have clamped
  // to the limit in the first step and reported kLimitReached in the second
  // step.
  Adaptation adaptation = adapter.GetAdaptationDown();
  EXPECT_EQ(Adaptation::Status::kLimitReached, adaptation.status());
  EXPECT_TRUE(adaptation.min_pixel_limit_reached());
}

TEST(VideoStreamAdapterTest, MaintainFramerate_IncreasePixelsToFiveThirds) {
  VideoStreamAdapter adapter;
  adapter.SetDegradationPreference(DegradationPreference::MAINTAIN_FRAMERATE);
  FakeVideoStream fake_stream(&adapter, 1280 * 720, 30,
                              kDefaultMinPixelsPerFrame);
  // Go down twice, ensuring going back up is still a restricted resolution.
  fake_stream.ApplyAdaptation(adapter.GetAdaptationDown());
  fake_stream.ApplyAdaptation(adapter.GetAdaptationDown());
  EXPECT_EQ(2, adapter.adaptation_counters().resolution_adaptations);
  int input_pixels = fake_stream.input_pixels();
  // Go up once. The target is 5/3 and the max is 12/5 of the target.
  const int target = (input_pixels * 5) / 3;
  fake_stream.ApplyAdaptation(adapter.GetAdaptationUp());
  EXPECT_EQ(static_cast<size_t>((target * 12) / 5),
            adapter.source_restrictions().max_pixels_per_frame());
  EXPECT_EQ(static_cast<size_t>(target),
            adapter.source_restrictions().target_pixels_per_frame());
  EXPECT_EQ(absl::nullopt, adapter.source_restrictions().max_frame_rate());
  EXPECT_EQ(1, adapter.adaptation_counters().resolution_adaptations);
}

TEST(VideoStreamAdapterTest, MaintainFramerate_IncreasePixelsToUnrestricted) {
  VideoStreamAdapter adapter;
  adapter.SetDegradationPreference(DegradationPreference::MAINTAIN_FRAMERATE);
  FakeVideoStream fake_stream(&adapter, 1280 * 720, 30,
                              kDefaultMinPixelsPerFrame);
  // We are unrestricted by default and should not be able to adapt up.
  EXPECT_EQ(Adaptation::Status::kLimitReached,
            adapter.GetAdaptationUp().status());
  // If we go down once and then back up we should not have any restrictions.
  fake_stream.ApplyAdaptation(adapter.GetAdaptationDown());
  EXPECT_EQ(1, adapter.adaptation_counters().resolution_adaptations);
  fake_stream.ApplyAdaptation(adapter.GetAdaptationUp());
  EXPECT_EQ(VideoSourceRestrictions(), adapter.source_restrictions());
  EXPECT_EQ(0, adapter.adaptation_counters().Total());
}

TEST(VideoStreamAdapterTest, MaintainResolution_DecreasesFpsToTwoThirds) {
  const int kInputFps = 30;
  VideoStreamAdapter adapter;
  adapter.SetDegradationPreference(DegradationPreference::MAINTAIN_RESOLUTION);
  adapter.SetInput(
      InputState(1280 * 720, kInputFps, kDefaultMinPixelsPerFrame));
  Adaptation adaptation = adapter.GetAdaptationDown();
  EXPECT_EQ(Adaptation::Status::kValid, adaptation.status());
  adapter.ApplyAdaptation(adaptation);
  EXPECT_EQ(absl::nullopt,
            adapter.source_restrictions().max_pixels_per_frame());
  EXPECT_EQ(absl::nullopt,
            adapter.source_restrictions().target_pixels_per_frame());
  EXPECT_EQ(static_cast<double>((kInputFps * 2) / 3),
            adapter.source_restrictions().max_frame_rate());
  EXPECT_EQ(1, adapter.adaptation_counters().fps_adaptations);
}

TEST(VideoStreamAdapterTest, MaintainResolution_DecreasesFpsToLimitReached) {
  VideoStreamAdapter adapter;
  adapter.SetDegradationPreference(DegradationPreference::MAINTAIN_RESOLUTION);
  FakeVideoStream fake_stream(&adapter, 1280 * 720, kMinFrameRateFps + 1,
                              kDefaultMinPixelsPerFrame);
  // If we are not yet at the limit and the next step would exceed it, the step
  // is clamped such that we end up exactly on the limit.
  Adaptation adaptation = adapter.GetAdaptationDown();
  EXPECT_EQ(Adaptation::Status::kValid, adaptation.status());
  fake_stream.ApplyAdaptation(adaptation);
  EXPECT_EQ(static_cast<double>(kMinFrameRateFps),
            adapter.source_restrictions().max_frame_rate());
  EXPECT_EQ(1, adapter.adaptation_counters().fps_adaptations);
  // Having reached the limit, the next adaptation down is not valid.
  EXPECT_EQ(Adaptation::Status::kLimitReached,
            adapter.GetAdaptationDown().status());
}

TEST(VideoStreamAdapterTest, MaintainResolution_IncreaseFpsToThreeHalves) {
  VideoStreamAdapter adapter;
  adapter.SetDegradationPreference(DegradationPreference::MAINTAIN_RESOLUTION);
  FakeVideoStream fake_stream(&adapter, 1280 * 720, 30,
                              kDefaultMinPixelsPerFrame);
  // Go down twice, ensuring going back up is still a restricted frame rate.
  fake_stream.ApplyAdaptation(adapter.GetAdaptationDown());
  fake_stream.ApplyAdaptation(adapter.GetAdaptationDown());
  EXPECT_EQ(2, adapter.adaptation_counters().fps_adaptations);
  int input_fps = fake_stream.input_fps();
  // Go up once. The target is 3/2 of the input.
  Adaptation adaptation = adapter.GetAdaptationUp();
  EXPECT_EQ(Adaptation::Status::kValid, adaptation.status());
  fake_stream.ApplyAdaptation(adaptation);
  EXPECT_EQ(absl::nullopt,
            adapter.source_restrictions().max_pixels_per_frame());
  EXPECT_EQ(absl::nullopt,
            adapter.source_restrictions().target_pixels_per_frame());
  EXPECT_EQ(static_cast<double>((input_fps * 3) / 2),
            adapter.source_restrictions().max_frame_rate());
  EXPECT_EQ(1, adapter.adaptation_counters().fps_adaptations);
}

TEST(VideoStreamAdapterTest, MaintainResolution_IncreaseFpsToUnrestricted) {
  VideoStreamAdapter adapter;
  adapter.SetDegradationPreference(DegradationPreference::MAINTAIN_RESOLUTION);
  FakeVideoStream fake_stream(&adapter, 1280 * 720, 30,
                              kDefaultMinPixelsPerFrame);
  // We are unrestricted by default and should not be able to adapt up.
  EXPECT_EQ(Adaptation::Status::kLimitReached,
            adapter.GetAdaptationUp().status());
  // If we go down once and then back up we should not have any restrictions.
  fake_stream.ApplyAdaptation(adapter.GetAdaptationDown());
  EXPECT_EQ(1, adapter.adaptation_counters().fps_adaptations);
  fake_stream.ApplyAdaptation(adapter.GetAdaptationUp());
  EXPECT_EQ(VideoSourceRestrictions(), adapter.source_restrictions());
  EXPECT_EQ(0, adapter.adaptation_counters().Total());
}

TEST(VideoStreamAdapterTest, Balanced_DecreaseFrameRate) {
  webrtc::test::ScopedFieldTrials balanced_field_trials(
      BalancedFieldTrialConfig());
  VideoStreamAdapter adapter;
  adapter.SetDegradationPreference(DegradationPreference::BALANCED);
  adapter.SetInput(InputState(kBalancedMediumResolutionPixels,
                              kBalancedHighFrameRateFps,
                              kDefaultMinPixelsPerFrame));
  // If our frame rate is higher than the frame rate associated with our
  // resolution we should try to adapt to the frame rate associated with our
  // resolution: kBalancedMediumFrameRateFps.
  Adaptation adaptation = adapter.GetAdaptationDown();
  EXPECT_EQ(Adaptation::Status::kValid, adaptation.status());
  adapter.ApplyAdaptation(adaptation);
  EXPECT_EQ(absl::nullopt,
            adapter.source_restrictions().max_pixels_per_frame());
  EXPECT_EQ(absl::nullopt,
            adapter.source_restrictions().target_pixels_per_frame());
  EXPECT_EQ(static_cast<double>(kBalancedMediumFrameRateFps),
            adapter.source_restrictions().max_frame_rate());
  EXPECT_EQ(0, adapter.adaptation_counters().resolution_adaptations);
  EXPECT_EQ(1, adapter.adaptation_counters().fps_adaptations);
}

TEST(VideoStreamAdapterTest, Balanced_DecreaseResolution) {
  webrtc::test::ScopedFieldTrials balanced_field_trials(
      BalancedFieldTrialConfig());
  VideoStreamAdapter adapter;
  adapter.SetDegradationPreference(DegradationPreference::BALANCED);
  FakeVideoStream fake_stream(&adapter, kBalancedHighResolutionPixels,
                              kBalancedHighFrameRateFps,
                              kDefaultMinPixelsPerFrame);
  // If we are not below the current resolution's frame rate limit, we should
  // adapt resolution according to "maintain-framerate" logic (three fifths).
  //
  // However, since we are unlimited at the start and input frame rate is not
  // below kBalancedHighFrameRateFps, we first restrict the frame rate to
  // kBalancedHighFrameRateFps even though that is our current frame rate. This
  // does prevent the source from going higher, though, so it's technically not
  // a NO-OP.
  {
    Adaptation adaptation = adapter.GetAdaptationDown();
    EXPECT_EQ(Adaptation::Status::kValid, adaptation.status());
    fake_stream.ApplyAdaptation(adaptation);
  }
  EXPECT_EQ(absl::nullopt,
            adapter.source_restrictions().max_pixels_per_frame());
  EXPECT_EQ(absl::nullopt,
            adapter.source_restrictions().target_pixels_per_frame());
  EXPECT_EQ(static_cast<double>(kBalancedHighFrameRateFps),
            adapter.source_restrictions().max_frame_rate());
  EXPECT_EQ(0, adapter.adaptation_counters().resolution_adaptations);
  EXPECT_EQ(1, adapter.adaptation_counters().fps_adaptations);
  // Verify "maintain-framerate" logic the second time we adapt: Frame rate
  // restrictions remains the same and resolution goes down.
  {
    Adaptation adaptation = adapter.GetAdaptationDown();
    EXPECT_EQ(Adaptation::Status::kValid, adaptation.status());
    fake_stream.ApplyAdaptation(adaptation);
  }
  constexpr size_t kReducedPixelsFirstStep =
      static_cast<size_t>((kBalancedHighResolutionPixels * 3) / 5);
  EXPECT_EQ(kReducedPixelsFirstStep,
            adapter.source_restrictions().max_pixels_per_frame());
  EXPECT_EQ(absl::nullopt,
            adapter.source_restrictions().target_pixels_per_frame());
  EXPECT_EQ(static_cast<double>(kBalancedHighFrameRateFps),
            adapter.source_restrictions().max_frame_rate());
  EXPECT_EQ(1, adapter.adaptation_counters().resolution_adaptations);
  EXPECT_EQ(1, adapter.adaptation_counters().fps_adaptations);
  // If we adapt again, because the balanced settings' proposed frame rate is
  // still kBalancedHighFrameRateFps, "maintain-framerate" will trigger again.
  static_assert(kReducedPixelsFirstStep > kBalancedMediumResolutionPixels,
                "The reduced resolution is still greater than the next lower "
                "balanced setting resolution");
  constexpr size_t kReducedPixelsSecondStep = (kReducedPixelsFirstStep * 3) / 5;
  {
    Adaptation adaptation = adapter.GetAdaptationDown();
    EXPECT_EQ(Adaptation::Status::kValid, adaptation.status());
    fake_stream.ApplyAdaptation(adaptation);
  }
  EXPECT_EQ(kReducedPixelsSecondStep,
            adapter.source_restrictions().max_pixels_per_frame());
  EXPECT_EQ(absl::nullopt,
            adapter.source_restrictions().target_pixels_per_frame());
  EXPECT_EQ(static_cast<double>(kBalancedHighFrameRateFps),
            adapter.source_restrictions().max_frame_rate());
  EXPECT_EQ(2, adapter.adaptation_counters().resolution_adaptations);
  EXPECT_EQ(1, adapter.adaptation_counters().fps_adaptations);
}

// Testing when to adapt frame rate and when to adapt resolution is quite
// entangled, so this test covers both cases.
//
// There is an asymmetry: When we adapt down we do it in one order, but when we
// adapt up we don't do it in the reverse order. Instead we always try to adapt
// frame rate first according to balanced settings' configs and only when the
// frame rate is already achieved do we adjust the resolution.
TEST(VideoStreamAdapterTest, Balanced_IncreaseFrameRateAndResolution) {
  webrtc::test::ScopedFieldTrials balanced_field_trials(
      BalancedFieldTrialConfig());
  VideoStreamAdapter adapter;
  adapter.SetDegradationPreference(DegradationPreference::BALANCED);
  FakeVideoStream fake_stream(&adapter, kBalancedHighResolutionPixels,
                              kBalancedHighFrameRateFps,
                              kDefaultMinPixelsPerFrame);
  // The desired starting point of this test is having adapted frame rate twice.
  // This requires performing a number of adaptations.
  constexpr size_t kReducedPixelsFirstStep =
      static_cast<size_t>((kBalancedHighResolutionPixels * 3) / 5);
  constexpr size_t kReducedPixelsSecondStep = (kReducedPixelsFirstStep * 3) / 5;
  constexpr size_t kReducedPixelsThirdStep = (kReducedPixelsSecondStep * 3) / 5;
  static_assert(kReducedPixelsFirstStep > kBalancedMediumResolutionPixels,
                "The first pixel reduction is greater than the balanced "
                "settings' medium pixel configuration");
  static_assert(kReducedPixelsSecondStep > kBalancedMediumResolutionPixels,
                "The second pixel reduction is greater than the balanced "
                "settings' medium pixel configuration");
  static_assert(kReducedPixelsThirdStep <= kBalancedMediumResolutionPixels,
                "The third pixel reduction is NOT greater than the balanced "
                "settings' medium pixel configuration");
  // The first adaptation should affect the frame rate: See
  // Balanced_DecreaseResolution for explanation why.
  fake_stream.ApplyAdaptation(adapter.GetAdaptationDown());
  EXPECT_EQ(static_cast<double>(kBalancedHighFrameRateFps),
            adapter.source_restrictions().max_frame_rate());
  // The next three adaptations affects the resolution, because we have to reach
  // kBalancedMediumResolutionPixels before a lower frame rate is considered by
  // BalancedDegradationSettings. The number three is derived from the
  // static_asserts above.
  fake_stream.ApplyAdaptation(adapter.GetAdaptationDown());
  EXPECT_EQ(kReducedPixelsFirstStep,
            adapter.source_restrictions().max_pixels_per_frame());
  fake_stream.ApplyAdaptation(adapter.GetAdaptationDown());
  EXPECT_EQ(kReducedPixelsSecondStep,
            adapter.source_restrictions().max_pixels_per_frame());
  fake_stream.ApplyAdaptation(adapter.GetAdaptationDown());
  EXPECT_EQ(kReducedPixelsThirdStep,
            adapter.source_restrictions().max_pixels_per_frame());
  // Thus, the next adaptation will reduce frame rate to
  // kBalancedMediumFrameRateFps.
  fake_stream.ApplyAdaptation(adapter.GetAdaptationDown());
  EXPECT_EQ(static_cast<double>(kBalancedMediumFrameRateFps),
            adapter.source_restrictions().max_frame_rate());
  EXPECT_EQ(3, adapter.adaptation_counters().resolution_adaptations);
  EXPECT_EQ(2, adapter.adaptation_counters().fps_adaptations);
  // Adapt up!
  // While our resolution is in the medium-range, the frame rate associated with
  // the next resolution configuration up ("high") is kBalancedHighFrameRateFps
  // and "balanced" prefers adapting frame rate if not already applied.
  {
    Adaptation adaptation = adapter.GetAdaptationUp();
    EXPECT_EQ(Adaptation::Status::kValid, adaptation.status());
    fake_stream.ApplyAdaptation(adaptation);
    EXPECT_EQ(static_cast<double>(kBalancedHighFrameRateFps),
              adapter.source_restrictions().max_frame_rate());
    EXPECT_EQ(3, adapter.adaptation_counters().resolution_adaptations);
    EXPECT_EQ(1, adapter.adaptation_counters().fps_adaptations);
  }
  // Now that we have already achieved the next frame rate up, we act according
  // to "maintain-framerate". We go back up in resolution. Due to rounding
  // errors we don't end up back at kReducedPixelsSecondStep. Rather we get to
  // kReducedPixelsSecondStepUp, which is off by one compared to
  // kReducedPixelsSecondStep.
  constexpr size_t kReducedPixelsSecondStepUp =
      (kReducedPixelsThirdStep * 5) / 3;
  {
    Adaptation adaptation = adapter.GetAdaptationUp();
    EXPECT_EQ(Adaptation::Status::kValid, adaptation.status());
    fake_stream.ApplyAdaptation(adaptation);
    EXPECT_EQ(kReducedPixelsSecondStepUp,
              adapter.source_restrictions().target_pixels_per_frame());
    EXPECT_EQ(2, adapter.adaptation_counters().resolution_adaptations);
    EXPECT_EQ(1, adapter.adaptation_counters().fps_adaptations);
  }
  // Now that our resolution is back in the high-range, the next frame rate to
  // try out is "unlimited".
  {
    Adaptation adaptation = adapter.GetAdaptationUp();
    EXPECT_EQ(Adaptation::Status::kValid, adaptation.status());
    fake_stream.ApplyAdaptation(adaptation);
    EXPECT_EQ(absl::nullopt, adapter.source_restrictions().max_frame_rate());
    EXPECT_EQ(2, adapter.adaptation_counters().resolution_adaptations);
    EXPECT_EQ(0, adapter.adaptation_counters().fps_adaptations);
  }
  // Now only adapting resolution remains.
  constexpr size_t kReducedPixelsFirstStepUp =
      (kReducedPixelsSecondStepUp * 5) / 3;
  {
    Adaptation adaptation = adapter.GetAdaptationUp();
    EXPECT_EQ(Adaptation::Status::kValid, adaptation.status());
    fake_stream.ApplyAdaptation(adaptation);
    EXPECT_EQ(kReducedPixelsFirstStepUp,
              adapter.source_restrictions().target_pixels_per_frame());
    EXPECT_EQ(1, adapter.adaptation_counters().resolution_adaptations);
    EXPECT_EQ(0, adapter.adaptation_counters().fps_adaptations);
  }
  // The last step up should make us entirely unrestricted.
  {
    Adaptation adaptation = adapter.GetAdaptationUp();
    EXPECT_EQ(Adaptation::Status::kValid, adaptation.status());
    fake_stream.ApplyAdaptation(adaptation);
    EXPECT_EQ(VideoSourceRestrictions(), adapter.source_restrictions());
    EXPECT_EQ(0, adapter.adaptation_counters().Total());
  }
}

TEST(VideoStreamAdapterTest, Balanced_LimitReached) {
  webrtc::test::ScopedFieldTrials balanced_field_trials(
      BalancedFieldTrialConfig());
  VideoStreamAdapter adapter;
  adapter.SetDegradationPreference(DegradationPreference::BALANCED);
  FakeVideoStream fake_stream(&adapter, kBalancedLowResolutionPixels,
                              kBalancedLowFrameRateFps,
                              kDefaultMinPixelsPerFrame);
  // Attempting to adapt up while unrestricted should result in kLimitReached.
  EXPECT_EQ(Adaptation::Status::kLimitReached,
            adapter.GetAdaptationUp().status());
  // Adapting down once result in restricted frame rate, in this case we reach
  // the lowest possible frame rate immediately: kBalancedLowFrameRateFps.
  fake_stream.ApplyAdaptation(adapter.GetAdaptationDown());
  EXPECT_EQ(static_cast<double>(kBalancedLowFrameRateFps),
            adapter.source_restrictions().max_frame_rate());
  EXPECT_EQ(1, adapter.adaptation_counters().fps_adaptations);
  // Any further adaptation must follow "maintain-framerate" rules (these are
  // covered in more depth by the MaintainFramerate tests). This test does not
  // assert exactly how resolution is adjusted, only that resolution always
  // decreases and that we eventually reach kLimitReached.
  size_t previous_resolution = kBalancedLowResolutionPixels;
  bool did_reach_limit = false;
  // If we have not reached the limit within 5 adaptations something is wrong...
  for (int i = 0; i < 5; i++) {
    Adaptation adaptation = adapter.GetAdaptationDown();
    if (adaptation.status() == Adaptation::Status::kLimitReached) {
      did_reach_limit = true;
      break;
    }
    EXPECT_EQ(Adaptation::Status::kValid, adaptation.status());
    fake_stream.ApplyAdaptation(adaptation);
    EXPECT_LT(adapter.source_restrictions().max_pixels_per_frame().value(),
              previous_resolution);
    previous_resolution =
        adapter.source_restrictions().max_pixels_per_frame().value();
  }
  EXPECT_TRUE(did_reach_limit);
  // Frame rate restrictions are the same as before.
  EXPECT_EQ(static_cast<double>(kBalancedLowFrameRateFps),
            adapter.source_restrictions().max_frame_rate());
  EXPECT_EQ(1, adapter.adaptation_counters().fps_adaptations);
}

// kAwaitingPreviousAdaptation is only supported in "maintain-framerate".
TEST(VideoStreamAdapterTest, MaintainFramerate_AwaitingPreviousAdaptationDown) {
  VideoStreamAdapter adapter;
  adapter.SetDegradationPreference(DegradationPreference::MAINTAIN_FRAMERATE);
  adapter.SetInput(InputState(1280 * 720, 30, kDefaultMinPixelsPerFrame));
  // Adapt down once, but don't update the input.
  adapter.ApplyAdaptation(adapter.GetAdaptationDown());
  EXPECT_EQ(1, adapter.adaptation_counters().resolution_adaptations);
  {
    // Having performed the adaptation, but not updated the input based on the
    // new restrictions, adapting again in the same direction will not work.
    Adaptation adaptation = adapter.GetAdaptationDown();
    EXPECT_EQ(Adaptation::Status::kAwaitingPreviousAdaptation,
              adaptation.status());
  }
}

// kAwaitingPreviousAdaptation is only supported in "maintain-framerate".
TEST(VideoStreamAdapterTest, MaintainFramerate_AwaitingPreviousAdaptationUp) {
  VideoStreamAdapter adapter;
  adapter.SetDegradationPreference(DegradationPreference::MAINTAIN_FRAMERATE);
  FakeVideoStream fake_stream(&adapter, 1280 * 720, 30,
                              kDefaultMinPixelsPerFrame);
  // Perform two adaptation down so that adapting up twice is possible.
  fake_stream.ApplyAdaptation(adapter.GetAdaptationDown());
  fake_stream.ApplyAdaptation(adapter.GetAdaptationDown());
  EXPECT_EQ(2, adapter.adaptation_counters().resolution_adaptations);
  // Adapt up once, but don't update the input.
  adapter.ApplyAdaptation(adapter.GetAdaptationUp());
  EXPECT_EQ(1, adapter.adaptation_counters().resolution_adaptations);
  {
    // Having performed the adaptation, but not updated the input based on the
    // new restrictions, adapting again in the same direction will not work.
    Adaptation adaptation = adapter.GetAdaptationUp();
    EXPECT_EQ(Adaptation::Status::kAwaitingPreviousAdaptation,
              adaptation.status());
  }
}

TEST(VideoStreamAdapterTest, PeekNextRestrictions) {
  VideoStreamAdapter adapter;
  // Any non-disabled DegradationPreference will do.
  adapter.SetDegradationPreference(DegradationPreference::MAINTAIN_FRAMERATE);
  FakeVideoStream fake_stream(&adapter, 1280 * 720, 30,
                              kDefaultMinPixelsPerFrame);
  // When adaptation is not possible.
  {
    Adaptation adaptation = adapter.GetAdaptationUp();
    EXPECT_EQ(Adaptation::Status::kLimitReached, adaptation.status());
    EXPECT_EQ(adapter.PeekNextRestrictions(adaptation),
              adapter.source_restrictions());
  }
  // When we adapt down.
  {
    Adaptation adaptation = adapter.GetAdaptationDown();
    EXPECT_EQ(Adaptation::Status::kValid, adaptation.status());
    VideoSourceRestrictions next_restrictions =
        adapter.PeekNextRestrictions(adaptation);
    fake_stream.ApplyAdaptation(adaptation);
    EXPECT_EQ(next_restrictions, adapter.source_restrictions());
  }
  // When we adapt up.
  {
    Adaptation adaptation = adapter.GetAdaptationUp();
    EXPECT_EQ(Adaptation::Status::kValid, adaptation.status());
    VideoSourceRestrictions next_restrictions =
        adapter.PeekNextRestrictions(adaptation);
    fake_stream.ApplyAdaptation(adaptation);
    EXPECT_EQ(next_restrictions, adapter.source_restrictions());
  }
}

TEST(VideoStreamAdapterTest,
     SetDegradationPreferenceToOrFromBalancedClearsRestrictions) {
  VideoStreamAdapter adapter;
  adapter.SetDegradationPreference(DegradationPreference::MAINTAIN_FRAMERATE);
  adapter.SetInput(InputState(1280 * 720, 30, kDefaultMinPixelsPerFrame));
  adapter.ApplyAdaptation(adapter.GetAdaptationDown());
  EXPECT_NE(VideoSourceRestrictions(), adapter.source_restrictions());
  EXPECT_NE(0, adapter.adaptation_counters().Total());
  // Changing from non-balanced to balanced clears the restrictions.
  adapter.SetDegradationPreference(DegradationPreference::BALANCED);
  EXPECT_EQ(VideoSourceRestrictions(), adapter.source_restrictions());
  EXPECT_EQ(0, adapter.adaptation_counters().Total());
  // Apply adaptation again.
  adapter.ApplyAdaptation(adapter.GetAdaptationDown());
  EXPECT_NE(VideoSourceRestrictions(), adapter.source_restrictions());
  EXPECT_NE(0, adapter.adaptation_counters().Total());
  // Changing from balanced to non-balanced clears the restrictions.
  adapter.SetDegradationPreference(DegradationPreference::MAINTAIN_RESOLUTION);
  EXPECT_EQ(VideoSourceRestrictions(), adapter.source_restrictions());
  EXPECT_EQ(0, adapter.adaptation_counters().Total());
}

// Death tests.
// Disabled on Android because death tests misbehave on Android, see
// base/test/gtest_util.h.
#if RTC_DCHECK_IS_ON && GTEST_HAS_DEATH_TEST && !defined(WEBRTC_ANDROID)

TEST(VideoStreamAdapterDeathTest,
     SetDegradationPreferenceInvalidatesAdaptations) {
  VideoStreamAdapter adapter;
  adapter.SetDegradationPreference(DegradationPreference::MAINTAIN_FRAMERATE);
  adapter.SetInput(InputState(1280 * 720, 30, kDefaultMinPixelsPerFrame));
  Adaptation adaptation = adapter.GetAdaptationDown();
  adapter.SetDegradationPreference(DegradationPreference::MAINTAIN_RESOLUTION);
  EXPECT_DEATH(adapter.ApplyAdaptation(adaptation), "");
}

TEST(VideoStreamAdapterDeathTest, SetInputInvalidatesAdaptations) {
  VideoStreamAdapter adapter;
  adapter.SetDegradationPreference(DegradationPreference::MAINTAIN_RESOLUTION);
  adapter.SetInput(InputState(1280 * 720, 30, kDefaultMinPixelsPerFrame));
  Adaptation adaptation = adapter.GetAdaptationDown();
  adapter.SetInput(InputState(1280 * 720, 31, kDefaultMinPixelsPerFrame));
  EXPECT_DEATH(adapter.PeekNextRestrictions(adaptation), "");
}

#endif  // RTC_DCHECK_IS_ON && GTEST_HAS_DEATH_TEST && !defined(WEBRTC_ANDROID)

}  // namespace webrtc
