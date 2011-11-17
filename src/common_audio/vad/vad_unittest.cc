/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <stddef.h> // size_t
#include <stdlib.h>

#include "gtest/gtest.h"
#include "typedefs.h"
#include "webrtc_vad.h"

namespace webrtc {
namespace {
const int16_t kModes[] = { 0, 1, 2, 3 };
const size_t kModesSize = sizeof(kModes) / sizeof(*kModes);

// Rates we support.
const int16_t kRates[] = { 8000, 16000, 32000 };
const size_t kRatesSize = sizeof(kRates) / sizeof(*kRates);
// Frame lengths we support.
const int16_t kMaxFrameLength = 960;
const int16_t kFrameLengths[] = { 80, 160, 240, 320, 480, 640, 960 };
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

// TODO(bjornv): Add a process test, run on file.

}  // namespace
}  // namespace webrtc
