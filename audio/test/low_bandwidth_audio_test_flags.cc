/*
 *  Copyright (c) 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
// #ifndef AUDIO_TEST_LOW_BANDWIDTH_AUDIO_TEST_FLAGS_H_
// #define AUDIO_TEST_LOW_BANDWIDTH_AUDIO_TEST_FLAGS_H_

#include "rtc_base/flags.h"

WEBRTC_DEFINE_int(sample_rate_hz,
                  16000,
                  "Sample rate (Hz) of the produced audio files.");

WEBRTC_DEFINE_bool(
    quick,
    false,
    "Don't do the full audio recording. "
    "Used to quickly check that the test runs without crashing.");

WEBRTC_DEFINE_string(test_case_prefix, "", "Test case prefix.");

// #endif  // AUDIO_TEST_LOW_BANDWIDTH_AUDIO_TEST_FLAGS_H_
