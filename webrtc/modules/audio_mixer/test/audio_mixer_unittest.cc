/*
 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <string.h>

#include <memory>
#include <utility>

#include "testing/gmock/include/gmock/gmock.h"
#include "webrtc/base/bind.h"
#include "webrtc/base/thread.h"
#include "webrtc/modules/audio_mixer/audio_mixer_defines.h"
#include "webrtc/modules/audio_mixer/audio_mixer.h"

using testing::_;
using testing::Exactly;
using testing::Invoke;
using testing::Return;

namespace webrtc {

namespace {

constexpr int kDefaultSampleRateHz = 48000;
constexpr int kId = 1;

// Utility function that resets the frame member variables with
// sensible defaults.
void ResetFrame(AudioFrame* frame) {
  frame->id_ = kId;
  frame->sample_rate_hz_ = kDefaultSampleRateHz;
  frame->num_channels_ = 1;

  // Frame duration 10ms.
  frame->samples_per_channel_ = kDefaultSampleRateHz / 100;
  frame->vad_activity_ = AudioFrame::kVadActive;
  frame->speech_type_ = AudioFrame::kNormalSpeech;
}

AudioFrame frame_for_mixing;

}  // namespace

class MockMixerAudioSource : public MixerAudioSource {
 public:
  MockMixerAudioSource()
      : fake_audio_frame_info_(MixerAudioSource::AudioFrameInfo::kNormal) {
    ON_CALL(*this, GetAudioFrameWithMuted(_, _))
        .WillByDefault(
            Invoke(this, &MockMixerAudioSource::FakeAudioFrameWithMuted));
  }

  MOCK_METHOD2(GetAudioFrameWithMuted,
               AudioFrameWithMuted(const int32_t id, int sample_rate_hz));

  AudioFrame* fake_frame() { return &fake_frame_; }
  AudioFrameInfo fake_info() { return fake_audio_frame_info_; }
  void set_fake_info(const AudioFrameInfo audio_frame_info) {
    fake_audio_frame_info_ = audio_frame_info;
  }

 private:
  AudioFrame fake_frame_, fake_output_frame_;
  AudioFrameInfo fake_audio_frame_info_;
  AudioFrameWithMuted FakeAudioFrameWithMuted(const int32_t id,
                                              int sample_rate_hz) {
    fake_output_frame_.CopyFrom(fake_frame_);
    return {
        &fake_output_frame_,  // audio_frame_pointer
        fake_info(),          // audio_frame_info
    };
  }
};

// Creates participants from |frames| and |frame_info| and adds them
// to the mixer. Compares mixed status with |expected_status|
void MixAndCompare(
    const std::vector<AudioFrame>& frames,
    const std::vector<MixerAudioSource::AudioFrameInfo>& frame_info,
    const std::vector<bool>& expected_status) {
  int num_audio_sources = frames.size();
  RTC_DCHECK(frames.size() == frame_info.size());
  RTC_DCHECK(frame_info.size() == expected_status.size());

  const std::unique_ptr<AudioMixer> mixer(AudioMixer::Create(kId));
  std::vector<MockMixerAudioSource> participants(num_audio_sources);

  for (int i = 0; i < num_audio_sources; i++) {
    participants[i].fake_frame()->CopyFrom(frames[i]);
    participants[i].set_fake_info(frame_info[i]);
  }

  for (int i = 0; i < num_audio_sources; i++) {
    EXPECT_EQ(0, mixer->SetMixabilityStatus(&participants[i], true));
    EXPECT_CALL(participants[i],
                GetAudioFrameWithMuted(_, kDefaultSampleRateHz))
        .Times(Exactly(1));
  }

  mixer->Mix(kDefaultSampleRateHz, 1, &frame_for_mixing);

  for (int i = 0; i < num_audio_sources; i++) {
    EXPECT_EQ(expected_status[i], participants[i].IsMixed())
        << "Mixed status of AudioSource #" << i << " wrong.";
  }
}

TEST(AudioMixer, AnonymousAndNamed) {
  // Should not matter even if partipants are more than
  // kMaximumAmountOfMixedAudioSources.
  constexpr int kNamed = AudioMixer::kMaximumAmountOfMixedAudioSources + 1;
  constexpr int kAnonymous = AudioMixer::kMaximumAmountOfMixedAudioSources + 1;

  const std::unique_ptr<AudioMixer> mixer(AudioMixer::Create(kId));

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
  constexpr int kAudioSources =
      AudioMixer::kMaximumAmountOfMixedAudioSources + 3;

  const std::unique_ptr<AudioMixer> mixer(AudioMixer::Create(kId));

  MockMixerAudioSource participants[kAudioSources];

  for (int i = 0; i < kAudioSources; ++i) {
    ResetFrame(participants[i].fake_frame());

    // We set the 80-th sample value since the first 80 samples may be
    // modified by a ramped-in window.
    participants[i].fake_frame()->data_[80] = i;

    EXPECT_EQ(0, mixer->SetMixabilityStatus(&participants[i], true));
    EXPECT_CALL(participants[i], GetAudioFrameWithMuted(_, _))
        .Times(Exactly(1));
  }

  // Last participant gives audio frame with passive VAD, although it has the
  // largest energy.
  participants[kAudioSources - 1].fake_frame()->vad_activity_ =
      AudioFrame::kVadPassive;

  AudioFrame audio_frame;
  mixer->Mix(kDefaultSampleRateHz,
             1,  // number of channels
             &audio_frame);

  for (int i = 0; i < kAudioSources; ++i) {
    bool is_mixed = participants[i].IsMixed();
    if (i == kAudioSources - 1 ||
        i < kAudioSources - 1 - AudioMixer::kMaximumAmountOfMixedAudioSources) {
      EXPECT_FALSE(is_mixed) << "Mixing status of AudioSource #" << i
                             << " wrong.";
    } else {
      EXPECT_TRUE(is_mixed) << "Mixing status of AudioSource #" << i
                            << " wrong.";
    }
  }
}

TEST(AudioMixer, FrameNotModifiedForSingleParticipant) {
  const std::unique_ptr<AudioMixer> mixer(AudioMixer::Create(kId));

  MockMixerAudioSource participant;

  ResetFrame(participant.fake_frame());
  const int n_samples = participant.fake_frame()->samples_per_channel_;

  // Modify the frame so that it's not zero.
  for (int j = 0; j < n_samples; j++) {
    participant.fake_frame()->data_[j] = j;
  }

  EXPECT_EQ(0, mixer->SetMixabilityStatus(&participant, true));
  EXPECT_CALL(participant, GetAudioFrameWithMuted(_, _)).Times(Exactly(2));

  AudioFrame audio_frame;
  // Two mix iteration to compare after the ramp-up step.
  for (int i = 0; i < 2; i++) {
    mixer->Mix(kDefaultSampleRateHz,
               1,  // number of channels
               &audio_frame);
  }

  EXPECT_EQ(
      0, memcmp(participant.fake_frame()->data_, audio_frame.data_, n_samples));
}

TEST(AudioMixer, FrameNotModifiedForSingleAnonymousParticipant) {
  const std::unique_ptr<AudioMixer> mixer(AudioMixer::Create(kId));

  MockMixerAudioSource participant;

  ResetFrame(participant.fake_frame());
  const int n_samples = participant.fake_frame()->samples_per_channel_;

  // Modify the frame so that it's not zero.
  for (int j = 0; j < n_samples; j++) {
    participant.fake_frame()->data_[j] = j;
  }

  EXPECT_EQ(0, mixer->SetMixabilityStatus(&participant, true));
  EXPECT_EQ(0, mixer->SetAnonymousMixabilityStatus(&participant, true));
  EXPECT_CALL(participant, GetAudioFrameWithMuted(_, _)).Times(Exactly(2));

  AudioFrame audio_frame;
  // Two mix iteration to compare after the ramp-up step.
  for (int i = 0; i < 2; i++) {
    mixer->Mix(kDefaultSampleRateHz,
               1,  // number of channels
               &audio_frame);
  }

  EXPECT_EQ(
      0, memcmp(participant.fake_frame()->data_, audio_frame.data_, n_samples));
}

TEST(AudioMixer, ParticipantSampleRate) {
  const std::unique_ptr<AudioMixer> mixer(AudioMixer::Create(kId));

  MockMixerAudioSource participant;
  ResetFrame(participant.fake_frame());

  EXPECT_EQ(0, mixer->SetMixabilityStatus(&participant, true));
  for (auto frequency : {8000, 16000, 32000, 48000}) {
    EXPECT_CALL(participant, GetAudioFrameWithMuted(_, frequency))
        .Times(Exactly(1));
    participant.fake_frame()->sample_rate_hz_ = frequency;
    participant.fake_frame()->samples_per_channel_ = frequency / 100;
    mixer->Mix(frequency, 1, &frame_for_mixing);
    EXPECT_EQ(frequency, frame_for_mixing.sample_rate_hz_);
  }
}

TEST(AudioMixer, ParticipantNumberOfChannels) {
  const std::unique_ptr<AudioMixer> mixer(AudioMixer::Create(kId));

  MockMixerAudioSource participant;
  ResetFrame(participant.fake_frame());

  EXPECT_EQ(0, mixer->SetMixabilityStatus(&participant, true));
  for (size_t number_of_channels : {1, 2}) {
    EXPECT_CALL(participant, GetAudioFrameWithMuted(_, kDefaultSampleRateHz))
        .Times(Exactly(1));
    mixer->Mix(kDefaultSampleRateHz, number_of_channels, &frame_for_mixing);
    EXPECT_EQ(number_of_channels, frame_for_mixing.num_channels_);
  }
}

// Test that the volume is reported as zero when the mixer input
// comprises only zero values.
TEST(AudioMixer, LevelIsZeroWhenMixingZeroes) {
  const std::unique_ptr<AudioMixer> mixer(AudioMixer::Create(kId));

  MockMixerAudioSource participant;
  ResetFrame(participant.fake_frame());

  EXPECT_EQ(0, mixer->SetMixabilityStatus(&participant, true));
  for (int i = 0; i < 11; i++) {
    EXPECT_CALL(participant, GetAudioFrameWithMuted(_, kDefaultSampleRateHz))
        .Times(Exactly(1));
    mixer->Mix(kDefaultSampleRateHz, 1, &frame_for_mixing);
  }

  EXPECT_EQ(0, mixer->GetOutputAudioLevel());
  EXPECT_EQ(0, mixer->GetOutputAudioLevelFullRange());
}

// Test that the reported volume is maximal when the mixer
// input comprises frames with maximal values.
TEST(AudioMixer, LevelIsMaximalWhenMixingMaximalValues) {
  const std::unique_ptr<AudioMixer> mixer(AudioMixer::Create(kId));

  MockMixerAudioSource participant;
  ResetFrame(participant.fake_frame());

  // Fill participant frame data with maximal sound.
  std::fill(participant.fake_frame()->data_,
            participant.fake_frame()->data_ + kDefaultSampleRateHz / 100,
            std::numeric_limits<int16_t>::max());

  EXPECT_EQ(0, mixer->SetMixabilityStatus(&participant, true));

  // We do >10 iterations, because the audio level indicator only
  // updates once every 10 calls.
  for (int i = 0; i < 11; i++) {
    EXPECT_CALL(participant, GetAudioFrameWithMuted(_, kDefaultSampleRateHz))
        .Times(Exactly(1));
    mixer->Mix(kDefaultSampleRateHz, 1, &frame_for_mixing);
  }

  // 9 is the highest possible audio level
  EXPECT_EQ(9, mixer->GetOutputAudioLevel());

  // 0x7fff = 32767 is the highest full range audio level.
  EXPECT_EQ(std::numeric_limits<int16_t>::max(),
            mixer->GetOutputAudioLevelFullRange());
}

// Maximal amount of participants are mixed one iteration, then
// another participant with higher energy is added.
TEST(AudioMixer, RampedOutSourcesShouldNotBeMarkedMixed) {
  constexpr int kAudioSources =
      AudioMixer::kMaximumAmountOfMixedAudioSources + 1;

  const std::unique_ptr<AudioMixer> mixer(AudioMixer::Create(kId));
  MockMixerAudioSource participants[kAudioSources];

  for (int i = 0; i < kAudioSources; i++) {
    ResetFrame(participants[i].fake_frame());
    // Set the participant audio energy to increase with the index
    // |i|.
    participants[i].fake_frame()->data_[0] = 100 * i;
  }

  // Add all participants but the loudest for mixing.
  for (int i = 0; i < kAudioSources - 1; i++) {
    EXPECT_EQ(0, mixer->SetMixabilityStatus(&participants[i], true));
    EXPECT_CALL(participants[i],
                GetAudioFrameWithMuted(_, kDefaultSampleRateHz))
        .Times(Exactly(1));
  }

  // First mixer iteration
  mixer->Mix(kDefaultSampleRateHz, 1, &frame_for_mixing);

  // All participants but the loudest should have been mixed.
  for (int i = 0; i < kAudioSources - 1; i++) {
    EXPECT_TRUE(participants[i].IsMixed()) << "Mixed status of AudioSource #"
                                           << i << " wrong.";
  }

  // Add new participant with higher energy.
  EXPECT_EQ(0,
            mixer->SetMixabilityStatus(&participants[kAudioSources - 1], true));
  for (int i = 0; i < kAudioSources; i++) {
    EXPECT_CALL(participants[i],
                GetAudioFrameWithMuted(_, kDefaultSampleRateHz))
        .Times(Exactly(1));
  }

  mixer->Mix(kDefaultSampleRateHz, 1, &frame_for_mixing);

  // The most quiet participant should not have been mixed.
  EXPECT_FALSE(participants[0].IsMixed())
      << "Mixed status of AudioSource #0 wrong.";

  // The loudest participants should have been mixed.
  for (int i = 1; i < kAudioSources; i++) {
    EXPECT_EQ(true, participants[i].IsMixed())
        << "Mixed status of AudioSource #" << i << " wrong.";
  }
}

// This test checks that the initialization and participant addition
// can be done on a different thread.
TEST(AudioMixer, ConstructFromOtherThread) {
  std::unique_ptr<rtc::Thread> init_thread = rtc::Thread::Create();
  std::unique_ptr<rtc::Thread> participant_thread = rtc::Thread::Create();
  init_thread->Start();
  std::unique_ptr<AudioMixer> mixer(
      init_thread->Invoke<std::unique_ptr<AudioMixer>>(
          RTC_FROM_HERE, std::bind(&AudioMixer::Create, kId)));
  MockMixerAudioSource participant;

  ResetFrame(participant.fake_frame());

  participant_thread->Start();
  EXPECT_EQ(0, participant_thread->Invoke<int>(
                   RTC_FROM_HERE, rtc::Bind(&AudioMixer::SetMixabilityStatus,
                                            mixer.get(), &participant, true)));

  EXPECT_EQ(
      0, participant_thread->Invoke<int>(
             RTC_FROM_HERE, rtc::Bind(&AudioMixer::SetAnonymousMixabilityStatus,
                                      mixer.get(), &participant, true)));

  EXPECT_CALL(participant, GetAudioFrameWithMuted(_, kDefaultSampleRateHz))
      .Times(Exactly(1));

  // Do one mixer iteration
  mixer->Mix(kDefaultSampleRateHz, 1, &frame_for_mixing);
}

TEST(AudioMixer, MutedShouldMixAfterUnmuted) {
  constexpr int kAudioSources =
      AudioMixer::kMaximumAmountOfMixedAudioSources + 1;

  std::vector<AudioFrame> frames(kAudioSources);
  for (auto& frame : frames) {
    ResetFrame(&frame);
  }

  std::vector<MixerAudioSource::AudioFrameInfo> frame_info(
      kAudioSources, MixerAudioSource::AudioFrameInfo::kNormal);
  frame_info[0] = MixerAudioSource::AudioFrameInfo::kMuted;
  std::vector<bool> expected_status(kAudioSources, true);
  expected_status[0] = false;

  MixAndCompare(frames, frame_info, expected_status);
}

TEST(AudioMixer, PassiveShouldMixAfterNormal) {
  constexpr int kAudioSources =
      AudioMixer::kMaximumAmountOfMixedAudioSources + 1;

  std::vector<AudioFrame> frames(kAudioSources);
  for (auto& frame : frames) {
    ResetFrame(&frame);
  }

  std::vector<MixerAudioSource::AudioFrameInfo> frame_info(
      kAudioSources, MixerAudioSource::AudioFrameInfo::kNormal);
  frames[0].vad_activity_ = AudioFrame::kVadPassive;
  std::vector<bool> expected_status(kAudioSources, true);
  expected_status[0] = false;

  MixAndCompare(frames, frame_info, expected_status);
}

TEST(AudioMixer, ActiveShouldMixBeforeLoud) {
  constexpr int kAudioSources =
      AudioMixer::kMaximumAmountOfMixedAudioSources + 1;

  std::vector<AudioFrame> frames(kAudioSources);
  for (auto& frame : frames) {
    ResetFrame(&frame);
  }

  std::vector<MixerAudioSource::AudioFrameInfo> frame_info(
      kAudioSources, MixerAudioSource::AudioFrameInfo::kNormal);
  frames[0].vad_activity_ = AudioFrame::kVadPassive;
  std::fill(frames[0].data_, frames[0].data_ + kDefaultSampleRateHz / 100,
            std::numeric_limits<int16_t>::max());
  std::vector<bool> expected_status(kAudioSources, true);
  expected_status[0] = false;

  MixAndCompare(frames, frame_info, expected_status);
}

TEST(AudioMixer, UnmutedShouldMixBeforeLoud) {
  constexpr int kAudioSources =
      AudioMixer::kMaximumAmountOfMixedAudioSources + 1;

  std::vector<AudioFrame> frames(kAudioSources);
  for (auto& frame : frames) {
    ResetFrame(&frame);
  }

  std::vector<MixerAudioSource::AudioFrameInfo> frame_info(
      kAudioSources, MixerAudioSource::AudioFrameInfo::kNormal);
  frame_info[0] = MixerAudioSource::AudioFrameInfo::kMuted;
  std::fill(frames[0].data_, frames[0].data_ + kDefaultSampleRateHz / 100,
            std::numeric_limits<int16_t>::max());
  std::vector<bool> expected_status(kAudioSources, true);
  expected_status[0] = false;

  MixAndCompare(frames, frame_info, expected_status);
}
}  // namespace webrtc
