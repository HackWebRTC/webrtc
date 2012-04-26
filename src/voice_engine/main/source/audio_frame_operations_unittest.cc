/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "gtest/gtest.h"

#include "audio_frame_operations.h"
#include "module_common_types.h"

namespace webrtc {
namespace voe {
namespace {

class AudioFrameOperationsTest : public ::testing::Test {
 protected:
  AudioFrameOperationsTest() {
    // Set typical values.
    frame_._payloadDataLengthInSamples = 320;
    frame_._audioChannel = 2;
  }

  AudioFrame frame_;
};

void SetFrameData(AudioFrame* frame, int16_t left, int16_t right) {
  for (int i = 0; i < frame->_payloadDataLengthInSamples * 2; i += 2) {
    frame->_payloadData[i] = left;
    frame->_payloadData[i + 1] = right;
  }
}

void SetFrameData(AudioFrame* frame, int16_t data) {
  for (int i = 0; i < frame->_payloadDataLengthInSamples; i++) {
    frame->_payloadData[i] = data;
  }
}

void VerifyFramesAreEqual(const AudioFrame& frame1, const AudioFrame& frame2) {
  EXPECT_EQ(frame1._audioChannel, frame2._audioChannel);
  EXPECT_EQ(frame1._payloadDataLengthInSamples,
            frame2._payloadDataLengthInSamples);

  for (int i = 0; i < frame1._payloadDataLengthInSamples * frame1._audioChannel;
      i++) {
    EXPECT_EQ(frame1._payloadData[i], frame2._payloadData[i]);
  }
}

TEST_F(AudioFrameOperationsTest, MonoToStereoFailsWithBadParameters) {
  EXPECT_EQ(-1, AudioFrameOperations::MonoToStereo(frame_));

  frame_._payloadDataLengthInSamples = AudioFrame::kMaxAudioFrameSizeSamples;
  frame_._audioChannel = 1;
  EXPECT_EQ(-1, AudioFrameOperations::MonoToStereo(frame_));
}

TEST_F(AudioFrameOperationsTest, MonoToStereoSucceeds) {
  frame_._audioChannel = 1;
  SetFrameData(&frame_, 1);
  EXPECT_EQ(0, AudioFrameOperations::MonoToStereo(frame_));

  AudioFrame stereo_frame;
  stereo_frame._payloadDataLengthInSamples = 320;
  stereo_frame._audioChannel = 2;
  SetFrameData(&stereo_frame, 1, 1);
  VerifyFramesAreEqual(stereo_frame, frame_);
}

TEST_F(AudioFrameOperationsTest, StereoToMonoFailsWithBadParameters) {
  frame_._audioChannel = 1;
  EXPECT_EQ(-1, AudioFrameOperations::StereoToMono(frame_));
}

TEST_F(AudioFrameOperationsTest, StereoToMonoSucceeds) {
  SetFrameData(&frame_, 4, 2);
  EXPECT_EQ(0, AudioFrameOperations::StereoToMono(frame_));

  AudioFrame mono_frame;
  mono_frame._payloadDataLengthInSamples = 320;
  mono_frame._audioChannel = 1;
  SetFrameData(&mono_frame, 3);
  VerifyFramesAreEqual(mono_frame, frame_);
}

TEST_F(AudioFrameOperationsTest, StereoToMonoDoesNotWrapAround) {
  SetFrameData(&frame_, -32768, -32768);
  EXPECT_EQ(0, AudioFrameOperations::StereoToMono(frame_));

  AudioFrame mono_frame;
  mono_frame._payloadDataLengthInSamples = 320;
  mono_frame._audioChannel = 1;
  SetFrameData(&mono_frame, -32768);
  VerifyFramesAreEqual(mono_frame, frame_);
}

TEST_F(AudioFrameOperationsTest, SwapStereoChannelsSucceedsOnStereo) {
  SetFrameData(&frame_, 0, 1);

  AudioFrame swapped_frame;
  swapped_frame._payloadDataLengthInSamples = 320;
  swapped_frame._audioChannel = 2;
  SetFrameData(&swapped_frame, 1, 0);

  AudioFrameOperations::SwapStereoChannels(&frame_);
  VerifyFramesAreEqual(swapped_frame, frame_);
}

TEST_F(AudioFrameOperationsTest, SwapStereoChannelsFailsOnMono) {
  frame_._audioChannel = 1;
  // Set data to "stereo", despite it being a mono frame.
  SetFrameData(&frame_, 0, 1);

  AudioFrame orig_frame = frame_;
  AudioFrameOperations::SwapStereoChannels(&frame_);
  // Verify that no swap occurred.
  VerifyFramesAreEqual(orig_frame, frame_);
}

TEST_F(AudioFrameOperationsTest, MuteSucceeds) {
  SetFrameData(&frame_, 1000, 1000);
  frame_._energy = 1000 * 1000 * frame_._payloadDataLengthInSamples *
      frame_._audioChannel;
  AudioFrameOperations::Mute(frame_);

  AudioFrame muted_frame;
  muted_frame._payloadDataLengthInSamples = 320;
  muted_frame._audioChannel = 2;
  SetFrameData(&muted_frame, 0, 0);
  muted_frame._energy = 0;
  VerifyFramesAreEqual(muted_frame, frame_);
  EXPECT_EQ(muted_frame._energy, frame_._energy);
}

// TODO(andrew): should not allow negative scales.
TEST_F(AudioFrameOperationsTest, DISABLED_ScaleFailsWithBadParameters) {
  frame_._audioChannel = 1;
  EXPECT_EQ(-1, AudioFrameOperations::Scale(1.0, 1.0, frame_));

  frame_._audioChannel = 3;
  EXPECT_EQ(-1, AudioFrameOperations::Scale(1.0, 1.0, frame_));

  frame_._audioChannel = 2;
  EXPECT_EQ(-1, AudioFrameOperations::Scale(-1.0, 1.0, frame_));
  EXPECT_EQ(-1, AudioFrameOperations::Scale(1.0, -1.0, frame_));
}

// TODO(andrew): fix the wraparound bug. We should always saturate.
TEST_F(AudioFrameOperationsTest, DISABLED_ScaleDoesNotWrapAround) {
  SetFrameData(&frame_, 4000, -4000);
  EXPECT_EQ(0, AudioFrameOperations::Scale(10.0, 10.0, frame_));

  AudioFrame clipped_frame;
  clipped_frame._payloadDataLengthInSamples = 320;
  clipped_frame._audioChannel = 2;
  SetFrameData(&clipped_frame, 32767, -32768);
  VerifyFramesAreEqual(clipped_frame, frame_);
}

TEST_F(AudioFrameOperationsTest, ScaleSucceeds) {
  SetFrameData(&frame_, 1, -1);
  EXPECT_EQ(0, AudioFrameOperations::Scale(2.0, 3.0, frame_));

  AudioFrame scaled_frame;
  scaled_frame._payloadDataLengthInSamples = 320;
  scaled_frame._audioChannel = 2;
  SetFrameData(&scaled_frame, 2, -3);
  VerifyFramesAreEqual(scaled_frame, frame_);
}

// TODO(andrew): should fail with a negative scale.
TEST_F(AudioFrameOperationsTest, DISABLED_ScaleWithSatFailsWithBadParameters) {
  EXPECT_EQ(-1, AudioFrameOperations::ScaleWithSat(-1.0, frame_));
}

TEST_F(AudioFrameOperationsTest, ScaleWithSatDoesNotWrapAround) {
  frame_._audioChannel = 1;
  SetFrameData(&frame_, 4000);
  EXPECT_EQ(0, AudioFrameOperations::ScaleWithSat(10.0, frame_));

  AudioFrame clipped_frame;
  clipped_frame._payloadDataLengthInSamples = 320;
  clipped_frame._audioChannel = 1;
  SetFrameData(&clipped_frame, 32767);
  VerifyFramesAreEqual(clipped_frame, frame_);

  SetFrameData(&frame_, -4000);
  EXPECT_EQ(0, AudioFrameOperations::ScaleWithSat(10.0, frame_));
  SetFrameData(&clipped_frame, -32768);
  VerifyFramesAreEqual(clipped_frame, frame_);
}

TEST_F(AudioFrameOperationsTest, ScaleWithSatSucceeds) {
  frame_._audioChannel = 1;
  SetFrameData(&frame_, 1);
  EXPECT_EQ(0, AudioFrameOperations::ScaleWithSat(2.0, frame_));

  AudioFrame scaled_frame;
  scaled_frame._payloadDataLengthInSamples = 320;
  scaled_frame._audioChannel = 1;
  SetFrameData(&scaled_frame, 2);
  VerifyFramesAreEqual(scaled_frame, frame_);
}

}  // namespace
}  // namespace voe
}  // namespace webrtc
