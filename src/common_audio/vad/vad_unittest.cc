/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <stddef.h>  // size_t
#include <stdlib.h>

#include "gtest/gtest.h"
#include "typedefs.h"
#include "webrtc_vad.h"

// TODO(bjornv): Move the internal unit tests to separate files.
extern "C" {
#include "vad_core.h"
#include "vad_gmm.h"
#include "vad_sp.h"
}

namespace webrtc {
namespace {
const int16_t kModes[] = { 0, 1, 2, 3 };
const size_t kModesSize = sizeof(kModes) / sizeof(*kModes);

// Rates we support.
const int16_t kRates[] = { 8000, 12000, 16000, 24000, 32000 };
const size_t kRatesSize = sizeof(kRates) / sizeof(*kRates);
// Frame lengths we support.
const int16_t kMaxFrameLength = 960;
const int16_t kFrameLengths[] = { 80, 120, 160, 240, 320, 480, 640,
    kMaxFrameLength };
const size_t kFrameLengthsSize = sizeof(kFrameLengths) / sizeof(*kFrameLengths);

// Returns true if the rate and frame length combination is valid.
bool ValidRatesAndFrameLengths(int16_t rate, int16_t frame_length) {
  if (rate == 8000) {
    if (frame_length == 80 || frame_length == 160 || frame_length == 240) {
      return true;
    }
    return false;
  } else if (rate == 16000) {
    if (frame_length == 160 || frame_length == 320 || frame_length == 480) {
      return true;
    }
    return false;
  }
  if (rate == 32000) {
    if (frame_length == 320 || frame_length == 640 || frame_length == 960) {
      return true;
    }
    return false;
  }

  return false;
}

class VadTest : public ::testing::Test {
 protected:
  VadTest();
  virtual void SetUp();
  virtual void TearDown();
};

VadTest::VadTest() {
}

void VadTest::SetUp() {
}

void VadTest::TearDown() {
}

TEST_F(VadTest, ApiTest) {
  // This API test runs through the APIs for all possible valid and invalid
  // combinations.

  VadInst* handle = NULL;
  int16_t zeros[kMaxFrameLength] = { 0 };

  // Construct a speech signal that will trigger the VAD in all modes. It is
  // known that (i * i) will wrap around, but that doesn't matter in this case.
  int16_t speech[kMaxFrameLength];
  for (int16_t i = 0; i < kMaxFrameLength; i++) {
    speech[i] = (i * i);
  }

  // WebRtcVad_get_version() tests
  char version[32];
  EXPECT_EQ(-1, WebRtcVad_get_version(NULL, sizeof(version)));
  EXPECT_EQ(-1, WebRtcVad_get_version(version, 1));
  EXPECT_EQ(0, WebRtcVad_get_version(version, sizeof(version)));

  // Null instance tests
  EXPECT_EQ(-1, WebRtcVad_Create(NULL));
  EXPECT_EQ(-1, WebRtcVad_Init(NULL));
  EXPECT_EQ(-1, WebRtcVad_Assign(NULL, NULL));
  EXPECT_EQ(-1, WebRtcVad_Free(NULL));
  EXPECT_EQ(-1, WebRtcVad_set_mode(NULL, kModes[0]));
  EXPECT_EQ(-1, WebRtcVad_Process(NULL, kRates[0], speech, kFrameLengths[0]));

  // WebRtcVad_AssignSize tests
  int handle_size_bytes = 0;
  EXPECT_EQ(0, WebRtcVad_AssignSize(&handle_size_bytes));
  EXPECT_EQ(576, handle_size_bytes);

  // WebRtcVad_Assign tests
  void* tmp_handle = malloc(handle_size_bytes);
  EXPECT_EQ(-1, WebRtcVad_Assign(&handle, NULL));
  EXPECT_EQ(0, WebRtcVad_Assign(&handle, tmp_handle));
  EXPECT_EQ(handle, tmp_handle);
  free(tmp_handle);

  // WebRtcVad_Create()
  ASSERT_EQ(0, WebRtcVad_Create(&handle));

  // Not initialized tests
  EXPECT_EQ(-1, WebRtcVad_Process(handle, kRates[0], speech, kFrameLengths[0]));
  EXPECT_EQ(-1, WebRtcVad_set_mode(handle, kModes[0]));

  // WebRtcVad_Init() test
  ASSERT_EQ(0, WebRtcVad_Init(handle));

  // WebRtcVad_set_mode() invalid modes tests
  EXPECT_EQ(-1, WebRtcVad_set_mode(handle, kModes[0] - 1));
  EXPECT_EQ(-1, WebRtcVad_set_mode(handle, kModes[kModesSize - 1] + 1));

  // WebRtcVad_Process() tests
  // NULL speech pointer
  EXPECT_EQ(-1, WebRtcVad_Process(handle, kRates[0], NULL, kFrameLengths[0]));
  // Invalid sampling rate
  EXPECT_EQ(-1, WebRtcVad_Process(handle, 9999, speech, kFrameLengths[0]));
  // All zeros as input should work
  EXPECT_EQ(0, WebRtcVad_Process(handle, kRates[0], zeros, kFrameLengths[0]));
  for (size_t k = 0; k < kModesSize; k++) {
    // Test valid modes
    EXPECT_EQ(0, WebRtcVad_set_mode(handle, kModes[k]));
    // Loop through sampling rate and frame length combinations
    for (size_t i = 0; i < kRatesSize; i++) {
      for (size_t j = 0; j < kFrameLengthsSize; j++) {
        if (ValidRatesAndFrameLengths(kRates[i], kFrameLengths[j])) {
          EXPECT_EQ(1, WebRtcVad_Process(handle,
                                         kRates[i],
                                         speech,
                                         kFrameLengths[j]));
        } else {
          EXPECT_EQ(-1, WebRtcVad_Process(handle,
                                          kRates[i],
                                          speech,
                                          kFrameLengths[j]));
        }
      }
    }
  }

  EXPECT_EQ(0, WebRtcVad_Free(handle));
}

TEST_F(VadTest, GMMTests) {
  int16_t delta = 0;
  // Input value at mean.
  EXPECT_EQ(1048576, WebRtcVad_GaussianProbability(0, 0, 128, &delta));
  EXPECT_EQ(0, delta);
  EXPECT_EQ(1048576, WebRtcVad_GaussianProbability(16, 128, 128, &delta));
  EXPECT_EQ(0, delta);
  EXPECT_EQ(1048576, WebRtcVad_GaussianProbability(-16, -128, 128, &delta));
  EXPECT_EQ(0, delta);

  // Largest possible input to give non-zero probability.
  EXPECT_EQ(1024, WebRtcVad_GaussianProbability(59, 0, 128, &delta));
  EXPECT_EQ(7552, delta);
  EXPECT_EQ(1024, WebRtcVad_GaussianProbability(75, 128, 128, &delta));
  EXPECT_EQ(7552, delta);
  EXPECT_EQ(1024, WebRtcVad_GaussianProbability(-75, -128, 128, &delta));
  EXPECT_EQ(-7552, delta);

  // Too large input, should give zero probability.
  EXPECT_EQ(0, WebRtcVad_GaussianProbability(105, 0, 128, &delta));
  EXPECT_EQ(13440, delta);
}

TEST_F(VadTest, SPTests) {
  VadInstT* handle = (VadInstT*) malloc(sizeof(VadInstT));
  int16_t zeros[kMaxFrameLength] = { 0 };
  int32_t state[2] = { 0 };
  int16_t data_in[kMaxFrameLength];
  int16_t data_out[kMaxFrameLength];

  const int16_t kReferenceMin[32] = {
      1600, 720, 509, 512, 532, 552, 570, 588,
      606, 624, 642, 659, 675, 691, 707, 723,
      1600, 544, 502, 522, 542, 561, 579, 597,
      615, 633, 651, 667, 683, 699, 715, 731
  };

  // Construct a speech signal that will trigger the VAD in all modes. It is
  // known that (i * i) will wrap around, but that doesn't matter in this case.
  for (int16_t i = 0; i < kMaxFrameLength; ++i) {
    data_in[i] = (i * i);
  }
  // Input values all zeros, expect all zeros out.
  WebRtcVad_Downsampling(zeros, data_out, state, (int) kMaxFrameLength);
  EXPECT_EQ(0, state[0]);
  EXPECT_EQ(0, state[1]);
  for (int16_t i = 0; i < kMaxFrameLength / 2; ++i) {
    EXPECT_EQ(0, data_out[i]);
  }
  // Make a simple non-zero data test.
  WebRtcVad_Downsampling(data_in, data_out, state, (int) kMaxFrameLength);
  EXPECT_EQ(207, state[0]);
  EXPECT_EQ(2270, state[1]);

  ASSERT_EQ(0, WebRtcVad_InitCore(handle, 0));
  for (int16_t i = 0; i < 16; ++i) {
    int16_t value = 500 * (i + 1);
    for (int j = 0; j < NUM_CHANNELS; ++j) {
      // Use values both above and below initialized value.
      EXPECT_EQ(kReferenceMin[i], WebRtcVad_FindMinimum(handle, value, j));
      EXPECT_EQ(kReferenceMin[i + 16], WebRtcVad_FindMinimum(handle, 12000, j));
    }
    handle->frame_counter++;
  }

  free(handle);
}

// TODO(bjornv): Add a process test, run on file.

}  // namespace
}  // namespace webrtc
