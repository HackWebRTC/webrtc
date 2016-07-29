/*
 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <memory>

#include "testing/gmock/include/gmock/gmock.h"

#include "webrtc/modules/audio_conference_mixer/source/audio_frame_manipulator.h"
#include "webrtc/modules/audio_mixer/audio_mixer.h"
#include "webrtc/modules/audio_mixer/include/audio_mixer_defines.h"
#include "webrtc/modules/audio_mixer/include/new_audio_conference_mixer.h"
#include "webrtc/modules/audio_mixer/source/new_audio_conference_mixer_impl.h"

using testing::_;
using testing::Exactly;
using testing::Invoke;
using testing::Return;

using webrtc::voe::AudioMixer;

namespace webrtc {
class MockMixerAudioSource : public MixerAudioSource {
 public:
  MockMixerAudioSource() {
    ON_CALL(*this, GetAudioFrameWithMuted(_, _))
        .WillByDefault(
            Invoke(this, &MockMixerAudioSource::FakeAudioFrameWithMuted));
  }
  MOCK_METHOD2(GetAudioFrameWithMuted,
               AudioFrameWithInfo(const int32_t id, int sample_rate_hz));
  MOCK_CONST_METHOD1(NeededFrequency, int32_t(const int32_t id));

  AudioFrame* fake_frame() { return &fake_frame_; }

 private:
  AudioFrame fake_frame_;
  AudioFrameWithInfo FakeAudioFrameWithMuted(const int32_t id,
                                             int sample_rate_hz) {
    return {
        fake_frame(),             // audio_frame_pointer
        AudioFrameInfo::kNormal,  // audio_frame_info
    };
  }
};

class BothMixersTest : public testing::Test {
 protected:
  BothMixersTest() {
    // Create an OutputMixer.
    AudioMixer::Create(audio_mixer_, kId);

    // Create one mixer participant and add it to the mixer.
    EXPECT_EQ(0, audio_mixer_->SetMixabilityStatus(participant_, true));

    // Each iteration, the participant will return a frame with this content:
    participant_.fake_frame()->id_ = 1;
    participant_.fake_frame()->sample_rate_hz_ = kSampleRateHz;
    participant_.fake_frame()->speech_type_ = AudioFrame::kNormalSpeech;
    participant_.fake_frame()->vad_activity_ = AudioFrame::kVadActive;
    participant_.fake_frame()->num_channels_ = 1;

    // We modify one sample within the RampIn window and one sample
    // outside of it.
    participant_.fake_frame()->data_[10] = 100;
    participant_.fake_frame()->data_[20] = -200;
    participant_.fake_frame()->data_[30] = 300;
    participant_.fake_frame()->data_[90] = -400;

    // Frame duration 10ms.
    participant_.fake_frame()->samples_per_channel_ = kSampleRateHz / 100;
    EXPECT_CALL(participant_, NeededFrequency(_))
        .WillRepeatedly(Return(kSampleRateHz));
  }

  ~BothMixersTest() { AudioMixer::Destroy(audio_mixer_); }

  // Mark the participant as 'unmixed' last round.
  void ResetAudioSource() { participant_._mixHistory->SetIsMixed(false); }

  AudioMixer* audio_mixer_;
  MockMixerAudioSource participant_;
  AudioFrame mixing_round_frame, mixed_results_frame_;

  constexpr static int kSampleRateHz = 48000;
  constexpr static int kId = 1;
};

TEST(AudioMixer, AnonymousAndNamed) {
  constexpr int kId = 1;
  // Should not matter even if partipants are more than
  // kMaximumAmountOfMixedAudioSources.
  constexpr int kNamed =
      NewAudioConferenceMixer::kMaximumAmountOfMixedAudioSources + 1;
  constexpr int kAnonymous =
      NewAudioConferenceMixer::kMaximumAmountOfMixedAudioSources + 1;

  std::unique_ptr<NewAudioConferenceMixer> mixer(
      NewAudioConferenceMixer::Create(kId));

  MockMixerAudioSource named[kNamed];
  MockMixerAudioSource anonymous[kAnonymous];

  for (int i = 0; i < kNamed; ++i) {
    EXPECT_EQ(0, mixer->SetMixabilityStatus(&named[i], true));
    EXPECT_TRUE(mixer->MixabilityStatus(named[i]));
  }

  for (int i = 0; i < kAnonymous; ++i) {
    // AudioSource must be registered before turning it into anonymous.
    EXPECT_EQ(-1, mixer->SetAnonymousMixabilityStatus(&anonymous[i], true));
    EXPECT_EQ(0, mixer->SetMixabilityStatus(&anonymous[i], true));
    EXPECT_TRUE(mixer->MixabilityStatus(anonymous[i]));
    EXPECT_FALSE(mixer->AnonymousMixabilityStatus(anonymous[i]));

    EXPECT_EQ(0, mixer->SetAnonymousMixabilityStatus(&anonymous[i], true));
    EXPECT_TRUE(mixer->AnonymousMixabilityStatus(anonymous[i]));

    // Anonymous participants do not show status by MixabilityStatus.
    EXPECT_FALSE(mixer->MixabilityStatus(anonymous[i]));
  }

  for (int i = 0; i < kNamed; ++i) {
    EXPECT_EQ(0, mixer->SetMixabilityStatus(&named[i], false));
    EXPECT_FALSE(mixer->MixabilityStatus(named[i]));
  }

  for (int i = 0; i < kAnonymous - 1; i++) {
    EXPECT_EQ(0, mixer->SetAnonymousMixabilityStatus(&anonymous[i], false));
    EXPECT_FALSE(mixer->AnonymousMixabilityStatus(anonymous[i]));

    // SetAnonymousMixabilityStatus(anonymous, false) moves anonymous to the
    // named group.
    EXPECT_TRUE(mixer->MixabilityStatus(anonymous[i]));
  }

  // SetMixabilityStatus(anonymous, false) will remove anonymous from both
  // anonymous and named groups.
  EXPECT_EQ(0, mixer->SetMixabilityStatus(&anonymous[kAnonymous - 1], false));
  EXPECT_FALSE(mixer->AnonymousMixabilityStatus(anonymous[kAnonymous - 1]));
  EXPECT_FALSE(mixer->MixabilityStatus(anonymous[kAnonymous - 1]));
}

TEST(AudioMixer, LargestEnergyVadActiveMixed) {
  const int kId = 1;
  const int kAudioSources =
      NewAudioConferenceMixer::kMaximumAmountOfMixedAudioSources + 3;
  const int kSampleRateHz = 32000;

  std::unique_ptr<NewAudioConferenceMixer> mixer(
      NewAudioConferenceMixer::Create(kId));

  MockMixerAudioSource participants[kAudioSources];

  for (int i = 0; i < kAudioSources; ++i) {
    participants[i].fake_frame()->id_ = i;
    participants[i].fake_frame()->sample_rate_hz_ = kSampleRateHz;
    participants[i].fake_frame()->speech_type_ = AudioFrame::kNormalSpeech;
    participants[i].fake_frame()->vad_activity_ = AudioFrame::kVadActive;
    participants[i].fake_frame()->num_channels_ = 1;

    // Frame duration 10ms.
    participants[i].fake_frame()->samples_per_channel_ = kSampleRateHz / 100;

    // We set the 80-th sample value since the first 80 samples may be
    // modified by a ramped-in window.
    participants[i].fake_frame()->data_[80] = i;

    EXPECT_EQ(0, mixer->SetMixabilityStatus(&participants[i], true));
    EXPECT_CALL(participants[i], GetAudioFrameWithMuted(_, _))
        .Times(Exactly(1));
    EXPECT_CALL(participants[i], NeededFrequency(_))
        .WillRepeatedly(Return(kSampleRateHz));
  }

  // Last participant gives audio frame with passive VAD, although it has the
  // largest energy.
  participants[kAudioSources - 1].fake_frame()->vad_activity_ =
      AudioFrame::kVadPassive;

  AudioFrame audio_frame;
  mixer->Mix(&audio_frame);

  for (int i = 0; i < kAudioSources; ++i) {
    bool is_mixed = participants[i].IsMixed();
    if (i == kAudioSources - 1 ||
        i < kAudioSources - 1 -
                NewAudioConferenceMixer::kMaximumAmountOfMixedAudioSources) {
      EXPECT_FALSE(is_mixed) << "Mixing status of AudioSource #" << i
                             << " wrong.";
    } else {
      EXPECT_TRUE(is_mixed) << "Mixing status of AudioSource #" << i
                            << " wrong.";
    }
  }
}

TEST_F(BothMixersTest, CompareInitialFrameAudio) {
  EXPECT_CALL(participant_, GetAudioFrameWithMuted(_, _)).Times(Exactly(1));

  // Make sure the participant is marked as 'non-mixed' so that it is
  // ramped in next round.
  ResetAudioSource();

  // Construct the expected sound for the first mixing round.
  mixing_round_frame.CopyFrom(*participant_.fake_frame());
  RampIn(mixing_round_frame);

  // Mix frames and put the result into a frame.
  audio_mixer_->MixActiveChannels();
  audio_mixer_->GetMixedAudio(kSampleRateHz, 1, &mixed_results_frame_);

  // Compare the received frame with the expected.
  EXPECT_EQ(mixing_round_frame.sample_rate_hz_,
            mixed_results_frame_.sample_rate_hz_);
  EXPECT_EQ(mixing_round_frame.num_channels_,
            mixed_results_frame_.num_channels_);
  EXPECT_EQ(mixing_round_frame.samples_per_channel_,
            mixed_results_frame_.samples_per_channel_);
  EXPECT_EQ(0, memcmp(mixing_round_frame.data_, mixed_results_frame_.data_,
                      sizeof(mixing_round_frame.data_)));
}

TEST_F(BothMixersTest, CompareSecondFrameAudio) {
  EXPECT_CALL(participant_, GetAudioFrameWithMuted(_, _)).Times(Exactly(2));

  // Make sure the participant is marked as 'non-mixed' so that it is
  // ramped in next round.
  ResetAudioSource();

  // Do one mixing iteration.
  audio_mixer_->MixActiveChannels();

  // Mix frames a second time and compare with the expected frame
  // (which is the participant's frame).
  audio_mixer_->MixActiveChannels();
  audio_mixer_->GetMixedAudio(kSampleRateHz, 1, &mixed_results_frame_);
  EXPECT_EQ(0,
            memcmp(participant_.fake_frame()->data_, mixed_results_frame_.data_,
                   sizeof(mixing_round_frame.data_)));
}

}  // namespace webrtc
