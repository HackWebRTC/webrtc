/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "video/stream_synchronization.h"

#include <algorithm>

#include "system_wrappers/include/clock.h"
#include "system_wrappers/include/ntp_time.h"
#include "test/gtest.h"

namespace webrtc {
namespace {
constexpr int kMaxAudioDiffMs = 80;  // From stream_synchronization.cc
constexpr int kDefaultAudioFrequency = 8000;
constexpr int kDefaultVideoFrequency = 90000;
constexpr int kSmoothingFilter = 4 * 2;
}  // namespace

class StreamSynchronizationTest : public ::testing::Test {
 public:
  StreamSynchronizationTest()
      : sync_(0, 0), clock_sender_(98765000), clock_receiver_(43210000) {}

 protected:
  // Generates the necessary RTCP measurements and RTP timestamps and computes
  // the audio and video delays needed to get the two streams in sync.
  // |audio_delay_ms| and |video_delay_ms| are the number of milliseconds after
  // capture which the frames are rendered.
  // |current_audio_delay_ms| is the number of milliseconds which audio is
  // currently being delayed by the receiver.
  bool DelayedStreams(int audio_delay_ms,
                      int video_delay_ms,
                      int current_audio_delay_ms,
                      int* extra_audio_delay_ms,
                      int* total_video_delay_ms) {
    int audio_frequency =
        static_cast<int>(kDefaultAudioFrequency * audio_clock_drift_ + 0.5);
    int video_frequency =
        static_cast<int>(kDefaultVideoFrequency * video_clock_drift_ + 0.5);

    // Generate NTP/RTP timestamp pair for both streams corresponding to RTCP.
    bool new_sr;
    StreamSynchronization::Measurements audio;
    StreamSynchronization::Measurements video;
    NtpTime ntp_time = clock_sender_.CurrentNtpTime();
    uint32_t rtp_timestamp =
        clock_sender_.CurrentTime().ms() * audio_frequency / 1000;
    EXPECT_TRUE(audio.rtp_to_ntp.UpdateMeasurements(
        ntp_time.seconds(), ntp_time.fractions(), rtp_timestamp, &new_sr));
    clock_sender_.AdvanceTimeMilliseconds(100);
    clock_receiver_.AdvanceTimeMilliseconds(100);
    ntp_time = clock_sender_.CurrentNtpTime();
    rtp_timestamp = clock_sender_.CurrentTime().ms() * video_frequency / 1000;
    EXPECT_TRUE(video.rtp_to_ntp.UpdateMeasurements(
        ntp_time.seconds(), ntp_time.fractions(), rtp_timestamp, &new_sr));
    clock_sender_.AdvanceTimeMilliseconds(900);
    clock_receiver_.AdvanceTimeMilliseconds(900);
    ntp_time = clock_sender_.CurrentNtpTime();
    rtp_timestamp = clock_sender_.CurrentTime().ms() * audio_frequency / 1000;
    EXPECT_TRUE(audio.rtp_to_ntp.UpdateMeasurements(
        ntp_time.seconds(), ntp_time.fractions(), rtp_timestamp, &new_sr));
    clock_sender_.AdvanceTimeMilliseconds(100);
    clock_receiver_.AdvanceTimeMilliseconds(100);
    ntp_time = clock_sender_.CurrentNtpTime();
    rtp_timestamp = clock_sender_.CurrentTime().ms() * video_frequency / 1000;
    EXPECT_TRUE(video.rtp_to_ntp.UpdateMeasurements(
        ntp_time.seconds(), ntp_time.fractions(), rtp_timestamp, &new_sr));
    clock_sender_.AdvanceTimeMilliseconds(900);
    clock_receiver_.AdvanceTimeMilliseconds(900);

    // Capture an audio and a video frame at the same time.
    audio.latest_timestamp =
        clock_sender_.CurrentTime().ms() * audio_frequency / 1000;
    video.latest_timestamp =
        clock_sender_.CurrentTime().ms() * video_frequency / 1000;

    if (audio_delay_ms > video_delay_ms) {
      // Audio later than video.
      clock_receiver_.AdvanceTimeMilliseconds(video_delay_ms);
      video.latest_receive_time_ms = clock_receiver_.CurrentTime().ms();
      clock_receiver_.AdvanceTimeMilliseconds(audio_delay_ms - video_delay_ms);
      audio.latest_receive_time_ms = clock_receiver_.CurrentTime().ms();
    } else {
      // Video later than audio.
      clock_receiver_.AdvanceTimeMilliseconds(audio_delay_ms);
      audio.latest_receive_time_ms = clock_receiver_.CurrentTime().ms();
      clock_receiver_.AdvanceTimeMilliseconds(video_delay_ms - audio_delay_ms);
      video.latest_receive_time_ms = clock_receiver_.CurrentTime().ms();
    }
    int relative_delay_ms;
    StreamSynchronization::ComputeRelativeDelay(audio, video,
                                                &relative_delay_ms);
    EXPECT_EQ(video_delay_ms - audio_delay_ms, relative_delay_ms);
    return sync_.ComputeDelays(relative_delay_ms, current_audio_delay_ms,
                               extra_audio_delay_ms, total_video_delay_ms);
  }

  // Simulate audio playback 300 ms after capture and video rendering 100 ms
  // after capture. Verify that the correct extra delays are calculated for
  // audio and video, and that they change correctly when we simulate that
  // NetEQ or the VCM adds more delay to the streams.
  // TODO(holmer): This is currently wrong! We should simply change
  // audio_delay_ms or video_delay_ms since those now include VCM and NetEQ
  // delays.
  void BothDelayedAudioLaterTest(int base_target_delay) {
    int current_audio_delay_ms = base_target_delay;
    int audio_delay_ms = base_target_delay + 300;
    int video_delay_ms = base_target_delay + 100;
    int extra_audio_delay_ms = 0;
    int total_video_delay_ms = base_target_delay;
    int filtered_move = (audio_delay_ms - video_delay_ms) / kSmoothingFilter;
    const int kNeteqDelayIncrease = 50;
    const int kNeteqDelayDecrease = 10;

    EXPECT_TRUE(DelayedStreams(audio_delay_ms, video_delay_ms,
                               current_audio_delay_ms, &extra_audio_delay_ms,
                               &total_video_delay_ms));
    EXPECT_EQ(base_target_delay + filtered_move, total_video_delay_ms);
    EXPECT_EQ(base_target_delay, extra_audio_delay_ms);
    current_audio_delay_ms = extra_audio_delay_ms;

    clock_sender_.AdvanceTimeMilliseconds(1000);
    clock_receiver_.AdvanceTimeMilliseconds(
        1000 - std::max(audio_delay_ms, video_delay_ms));
    // Simulate base_target_delay minimum delay in the VCM.
    total_video_delay_ms = base_target_delay;
    EXPECT_TRUE(DelayedStreams(audio_delay_ms, video_delay_ms,
                               current_audio_delay_ms, &extra_audio_delay_ms,
                               &total_video_delay_ms));
    EXPECT_EQ(base_target_delay + 2 * filtered_move, total_video_delay_ms);
    EXPECT_EQ(base_target_delay, extra_audio_delay_ms);
    current_audio_delay_ms = extra_audio_delay_ms;

    clock_sender_.AdvanceTimeMilliseconds(1000);
    clock_receiver_.AdvanceTimeMilliseconds(
        1000 - std::max(audio_delay_ms, video_delay_ms));
    // Simulate base_target_delay minimum delay in the VCM.
    total_video_delay_ms = base_target_delay;
    EXPECT_TRUE(DelayedStreams(audio_delay_ms, video_delay_ms,
                               current_audio_delay_ms, &extra_audio_delay_ms,
                               &total_video_delay_ms));
    EXPECT_EQ(base_target_delay + 3 * filtered_move, total_video_delay_ms);
    EXPECT_EQ(base_target_delay, extra_audio_delay_ms);

    // Simulate that NetEQ introduces some audio delay.
    current_audio_delay_ms = base_target_delay + kNeteqDelayIncrease;
    clock_sender_.AdvanceTimeMilliseconds(1000);
    clock_receiver_.AdvanceTimeMilliseconds(
        1000 - std::max(audio_delay_ms, video_delay_ms));
    // Simulate base_target_delay minimum delay in the VCM.
    total_video_delay_ms = base_target_delay;
    EXPECT_TRUE(DelayedStreams(audio_delay_ms, video_delay_ms,
                               current_audio_delay_ms, &extra_audio_delay_ms,
                               &total_video_delay_ms));
    filtered_move = 3 * filtered_move +
                    (kNeteqDelayIncrease + audio_delay_ms - video_delay_ms) /
                        kSmoothingFilter;
    EXPECT_EQ(base_target_delay + filtered_move, total_video_delay_ms);
    EXPECT_EQ(base_target_delay, extra_audio_delay_ms);

    // Simulate that NetEQ reduces its delay.
    current_audio_delay_ms = base_target_delay + kNeteqDelayDecrease;
    clock_sender_.AdvanceTimeMilliseconds(1000);
    clock_receiver_.AdvanceTimeMilliseconds(
        1000 - std::max(audio_delay_ms, video_delay_ms));
    // Simulate base_target_delay minimum delay in the VCM.
    total_video_delay_ms = base_target_delay;
    EXPECT_TRUE(DelayedStreams(audio_delay_ms, video_delay_ms,
                               current_audio_delay_ms, &extra_audio_delay_ms,
                               &total_video_delay_ms));

    filtered_move = filtered_move +
                    (kNeteqDelayDecrease + audio_delay_ms - video_delay_ms) /
                        kSmoothingFilter;

    EXPECT_EQ(base_target_delay + filtered_move, total_video_delay_ms);
    EXPECT_EQ(base_target_delay, extra_audio_delay_ms);
  }

  void BothDelayedVideoLaterTest(int base_target_delay) {
    int current_audio_delay_ms = base_target_delay;
    int audio_delay_ms = base_target_delay + 100;
    int video_delay_ms = base_target_delay + 300;
    int extra_audio_delay_ms = 0;
    int total_video_delay_ms = base_target_delay;

    EXPECT_TRUE(DelayedStreams(audio_delay_ms, video_delay_ms,
                               current_audio_delay_ms, &extra_audio_delay_ms,
                               &total_video_delay_ms));
    EXPECT_EQ(base_target_delay, total_video_delay_ms);
    // The audio delay is not allowed to change more than this in 1 second.
    EXPECT_GE(base_target_delay + kMaxAudioDiffMs, extra_audio_delay_ms);
    current_audio_delay_ms = extra_audio_delay_ms;
    int current_extra_delay_ms = extra_audio_delay_ms;

    clock_sender_.AdvanceTimeMilliseconds(1000);
    clock_receiver_.AdvanceTimeMilliseconds(800);
    EXPECT_TRUE(DelayedStreams(audio_delay_ms, video_delay_ms,
                               current_audio_delay_ms, &extra_audio_delay_ms,
                               &total_video_delay_ms));
    EXPECT_EQ(base_target_delay, total_video_delay_ms);
    // The audio delay is not allowed to change more than the half of the
    // required change in delay.
    EXPECT_EQ(current_extra_delay_ms +
                  MaxAudioDelayIncrease(
                      current_audio_delay_ms,
                      base_target_delay + video_delay_ms - audio_delay_ms),
              extra_audio_delay_ms);
    current_audio_delay_ms = extra_audio_delay_ms;
    current_extra_delay_ms = extra_audio_delay_ms;

    clock_sender_.AdvanceTimeMilliseconds(1000);
    clock_receiver_.AdvanceTimeMilliseconds(800);
    EXPECT_TRUE(DelayedStreams(audio_delay_ms, video_delay_ms,
                               current_audio_delay_ms, &extra_audio_delay_ms,
                               &total_video_delay_ms));
    EXPECT_EQ(base_target_delay, total_video_delay_ms);
    // The audio delay is not allowed to change more than the half of the
    // required change in delay.
    EXPECT_EQ(current_extra_delay_ms +
                  MaxAudioDelayIncrease(
                      current_audio_delay_ms,
                      base_target_delay + video_delay_ms - audio_delay_ms),
              extra_audio_delay_ms);
    current_extra_delay_ms = extra_audio_delay_ms;

    // Simulate that NetEQ for some reason reduced the delay.
    current_audio_delay_ms = base_target_delay + 10;
    clock_sender_.AdvanceTimeMilliseconds(1000);
    clock_receiver_.AdvanceTimeMilliseconds(800);
    EXPECT_TRUE(DelayedStreams(audio_delay_ms, video_delay_ms,
                               current_audio_delay_ms, &extra_audio_delay_ms,
                               &total_video_delay_ms));
    EXPECT_EQ(base_target_delay, total_video_delay_ms);
    // Since we only can ask NetEQ for a certain amount of extra delay, and
    // we only measure the total NetEQ delay, we will ask for additional delay
    // here to try to stay in sync.
    EXPECT_EQ(current_extra_delay_ms +
                  MaxAudioDelayIncrease(
                      current_audio_delay_ms,
                      base_target_delay + video_delay_ms - audio_delay_ms),
              extra_audio_delay_ms);
    current_extra_delay_ms = extra_audio_delay_ms;

    // Simulate that NetEQ for some reason significantly increased the delay.
    current_audio_delay_ms = base_target_delay + 350;
    clock_sender_.AdvanceTimeMilliseconds(1000);
    clock_receiver_.AdvanceTimeMilliseconds(800);
    EXPECT_TRUE(DelayedStreams(audio_delay_ms, video_delay_ms,
                               current_audio_delay_ms, &extra_audio_delay_ms,
                               &total_video_delay_ms));
    EXPECT_EQ(base_target_delay, total_video_delay_ms);
    // The audio delay is not allowed to change more than the half of the
    // required change in delay.
    EXPECT_EQ(current_extra_delay_ms +
                  MaxAudioDelayIncrease(
                      current_audio_delay_ms,
                      base_target_delay + video_delay_ms - audio_delay_ms),
              extra_audio_delay_ms);
  }

  int MaxAudioDelayIncrease(int current_audio_delay_ms, int delay_ms) {
    return std::min((delay_ms - current_audio_delay_ms) / kSmoothingFilter,
                    kMaxAudioDiffMs);
  }

  int MaxAudioDelayDecrease(int current_audio_delay_ms, int delay_ms) {
    return std::max((delay_ms - current_audio_delay_ms) / kSmoothingFilter,
                    -kMaxAudioDiffMs);
  }

  StreamSynchronization sync_;
  SimulatedClock clock_sender_;
  SimulatedClock clock_receiver_;
  double audio_clock_drift_ = 1.0;
  double video_clock_drift_ = 1.0;
};

TEST_F(StreamSynchronizationTest, NoDelay) {
  uint32_t current_audio_delay_ms = 0;
  int extra_audio_delay_ms = 0;
  int total_video_delay_ms = 0;

  EXPECT_FALSE(DelayedStreams(0, 0, current_audio_delay_ms,
                              &extra_audio_delay_ms, &total_video_delay_ms));
  EXPECT_EQ(0, extra_audio_delay_ms);
  EXPECT_EQ(0, total_video_delay_ms);
}

TEST_F(StreamSynchronizationTest, VideoDelay) {
  uint32_t current_audio_delay_ms = 0;
  int delay_ms = 200;
  int extra_audio_delay_ms = 0;
  int total_video_delay_ms = 0;

  EXPECT_TRUE(DelayedStreams(delay_ms, 0, current_audio_delay_ms,
                             &extra_audio_delay_ms, &total_video_delay_ms));
  EXPECT_EQ(0, extra_audio_delay_ms);
  // The video delay is not allowed to change more than this in 1 second.
  EXPECT_EQ(delay_ms / kSmoothingFilter, total_video_delay_ms);

  clock_sender_.AdvanceTimeMilliseconds(1000);
  clock_receiver_.AdvanceTimeMilliseconds(800);
  // Simulate 0 minimum delay in the VCM.
  total_video_delay_ms = 0;
  EXPECT_TRUE(DelayedStreams(delay_ms, 0, current_audio_delay_ms,
                             &extra_audio_delay_ms, &total_video_delay_ms));
  EXPECT_EQ(0, extra_audio_delay_ms);
  // The video delay is not allowed to change more than this in 1 second.
  EXPECT_EQ(2 * delay_ms / kSmoothingFilter, total_video_delay_ms);

  clock_sender_.AdvanceTimeMilliseconds(1000);
  clock_receiver_.AdvanceTimeMilliseconds(800);
  // Simulate 0 minimum delay in the VCM.
  total_video_delay_ms = 0;
  EXPECT_TRUE(DelayedStreams(delay_ms, 0, current_audio_delay_ms,
                             &extra_audio_delay_ms, &total_video_delay_ms));
  EXPECT_EQ(0, extra_audio_delay_ms);
  EXPECT_EQ(3 * delay_ms / kSmoothingFilter, total_video_delay_ms);
}

TEST_F(StreamSynchronizationTest, AudioDelay) {
  int current_audio_delay_ms = 0;
  int delay_ms = 200;
  int extra_audio_delay_ms = 0;
  int total_video_delay_ms = 0;

  EXPECT_TRUE(DelayedStreams(0, delay_ms, current_audio_delay_ms,
                             &extra_audio_delay_ms, &total_video_delay_ms));
  EXPECT_EQ(0, total_video_delay_ms);
  // The audio delay is not allowed to change more than this in 1 second.
  EXPECT_EQ(delay_ms / kSmoothingFilter, extra_audio_delay_ms);
  current_audio_delay_ms = extra_audio_delay_ms;
  int current_extra_delay_ms = extra_audio_delay_ms;

  clock_sender_.AdvanceTimeMilliseconds(1000);
  clock_receiver_.AdvanceTimeMilliseconds(800);
  EXPECT_TRUE(DelayedStreams(0, delay_ms, current_audio_delay_ms,
                             &extra_audio_delay_ms, &total_video_delay_ms));
  EXPECT_EQ(0, total_video_delay_ms);
  // The audio delay is not allowed to change more than the half of the required
  // change in delay.
  EXPECT_EQ(current_extra_delay_ms +
                MaxAudioDelayIncrease(current_audio_delay_ms, delay_ms),
            extra_audio_delay_ms);
  current_audio_delay_ms = extra_audio_delay_ms;
  current_extra_delay_ms = extra_audio_delay_ms;

  clock_sender_.AdvanceTimeMilliseconds(1000);
  clock_receiver_.AdvanceTimeMilliseconds(800);
  EXPECT_TRUE(DelayedStreams(0, delay_ms, current_audio_delay_ms,
                             &extra_audio_delay_ms, &total_video_delay_ms));
  EXPECT_EQ(0, total_video_delay_ms);
  // The audio delay is not allowed to change more than the half of the required
  // change in delay.
  EXPECT_EQ(current_extra_delay_ms +
                MaxAudioDelayIncrease(current_audio_delay_ms, delay_ms),
            extra_audio_delay_ms);
  current_extra_delay_ms = extra_audio_delay_ms;

  // Simulate that NetEQ for some reason reduced the delay.
  current_audio_delay_ms = 10;
  clock_sender_.AdvanceTimeMilliseconds(1000);
  clock_receiver_.AdvanceTimeMilliseconds(800);
  EXPECT_TRUE(DelayedStreams(0, delay_ms, current_audio_delay_ms,
                             &extra_audio_delay_ms, &total_video_delay_ms));
  EXPECT_EQ(0, total_video_delay_ms);
  // Since we only can ask NetEQ for a certain amount of extra delay, and
  // we only measure the total NetEQ delay, we will ask for additional delay
  // here to try to
  EXPECT_EQ(current_extra_delay_ms +
                MaxAudioDelayIncrease(current_audio_delay_ms, delay_ms),
            extra_audio_delay_ms);
  current_extra_delay_ms = extra_audio_delay_ms;

  // Simulate that NetEQ for some reason significantly increased the delay.
  current_audio_delay_ms = 350;
  clock_sender_.AdvanceTimeMilliseconds(1000);
  clock_receiver_.AdvanceTimeMilliseconds(800);
  EXPECT_TRUE(DelayedStreams(0, delay_ms, current_audio_delay_ms,
                             &extra_audio_delay_ms, &total_video_delay_ms));
  EXPECT_EQ(0, total_video_delay_ms);
  // The audio delay is not allowed to change more than the half of the required
  // change in delay.
  EXPECT_EQ(current_extra_delay_ms +
                MaxAudioDelayDecrease(current_audio_delay_ms, delay_ms),
            extra_audio_delay_ms);
}

TEST_F(StreamSynchronizationTest, BothDelayedVideoLater) {
  BothDelayedVideoLaterTest(0);
}

TEST_F(StreamSynchronizationTest, BothDelayedVideoLaterAudioClockDrift) {
  audio_clock_drift_ = 1.05;
  BothDelayedVideoLaterTest(0);
}

TEST_F(StreamSynchronizationTest, BothDelayedVideoLaterVideoClockDrift) {
  video_clock_drift_ = 1.05;
  BothDelayedVideoLaterTest(0);
}

TEST_F(StreamSynchronizationTest, BothDelayedAudioLater) {
  BothDelayedAudioLaterTest(0);
}

TEST_F(StreamSynchronizationTest, BothDelayedAudioClockDrift) {
  audio_clock_drift_ = 1.05;
  BothDelayedAudioLaterTest(0);
}

TEST_F(StreamSynchronizationTest, BothDelayedVideoClockDrift) {
  video_clock_drift_ = 1.05;
  BothDelayedAudioLaterTest(0);
}

TEST_F(StreamSynchronizationTest, BaseDelay) {
  int base_target_delay_ms = 2000;
  int current_audio_delay_ms = 2000;
  int extra_audio_delay_ms = 0;
  int total_video_delay_ms = base_target_delay_ms;
  sync_.SetTargetBufferingDelay(base_target_delay_ms);
  // We are in sync don't change.
  EXPECT_FALSE(DelayedStreams(base_target_delay_ms, base_target_delay_ms,
                              current_audio_delay_ms, &extra_audio_delay_ms,
                              &total_video_delay_ms));
  // Triggering another call with the same values. Delay should not be modified.
  base_target_delay_ms = 2000;
  current_audio_delay_ms = base_target_delay_ms;
  total_video_delay_ms = base_target_delay_ms;
  sync_.SetTargetBufferingDelay(base_target_delay_ms);
  // We are in sync don't change.
  EXPECT_FALSE(DelayedStreams(base_target_delay_ms, base_target_delay_ms,
                              current_audio_delay_ms, &extra_audio_delay_ms,
                              &total_video_delay_ms));
  // Changing delay value - intended to test this module only. In practice it
  // would take VoE time to adapt.
  base_target_delay_ms = 5000;
  current_audio_delay_ms = base_target_delay_ms;
  total_video_delay_ms = base_target_delay_ms;
  sync_.SetTargetBufferingDelay(base_target_delay_ms);
  // We are in sync don't change.
  EXPECT_FALSE(DelayedStreams(base_target_delay_ms, base_target_delay_ms,
                              current_audio_delay_ms, &extra_audio_delay_ms,
                              &total_video_delay_ms));
}

TEST_F(StreamSynchronizationTest, BothDelayedAudioLaterWithBaseDelay) {
  int base_target_delay_ms = 3000;
  sync_.SetTargetBufferingDelay(base_target_delay_ms);
  BothDelayedAudioLaterTest(base_target_delay_ms);
}

TEST_F(StreamSynchronizationTest, BothDelayedAudioClockDriftWithBaseDelay) {
  int base_target_delay_ms = 3000;
  sync_.SetTargetBufferingDelay(base_target_delay_ms);
  audio_clock_drift_ = 1.05;
  BothDelayedAudioLaterTest(base_target_delay_ms);
}

TEST_F(StreamSynchronizationTest, BothDelayedVideoClockDriftWithBaseDelay) {
  int base_target_delay_ms = 3000;
  sync_.SetTargetBufferingDelay(base_target_delay_ms);
  video_clock_drift_ = 1.05;
  BothDelayedAudioLaterTest(base_target_delay_ms);
}

TEST_F(StreamSynchronizationTest, BothDelayedVideoLaterWithBaseDelay) {
  int base_target_delay_ms = 2000;
  sync_.SetTargetBufferingDelay(base_target_delay_ms);
  BothDelayedVideoLaterTest(base_target_delay_ms);
}

TEST_F(StreamSynchronizationTest,
       BothDelayedVideoLaterAudioClockDriftWithBaseDelay) {
  int base_target_delay_ms = 2000;
  audio_clock_drift_ = 1.05;
  sync_.SetTargetBufferingDelay(base_target_delay_ms);
  BothDelayedVideoLaterTest(base_target_delay_ms);
}

TEST_F(StreamSynchronizationTest,
       BothDelayedVideoLaterVideoClockDriftWithBaseDelay) {
  int base_target_delay_ms = 2000;
  video_clock_drift_ = 1.05;
  sync_.SetTargetBufferingDelay(base_target_delay_ms);
  BothDelayedVideoLaterTest(base_target_delay_ms);
}

}  // namespace webrtc
