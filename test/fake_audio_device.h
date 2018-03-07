/*
 *  Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#ifndef TEST_FAKE_AUDIO_DEVICE_H_
#define TEST_FAKE_AUDIO_DEVICE_H_

#include "modules/audio_device/include/test_audio_device_impl.h"

namespace webrtc {

namespace test {

// This class is deprecated. Use webrtc::TestAudioDevice from
// modules/audio_device/include/test_audio_device.h instead.
using FakeAudioDevice = webrtc::webrtc_impl::TestAudioDeviceModuleImpl;

}  // namespace test
}  // namespace webrtc

#endif  // TEST_FAKE_AUDIO_DEVICE_H_
