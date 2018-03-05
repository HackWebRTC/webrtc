/*
 *  Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_AUDIO_DEVICE_INCLUDE_FAKE_AUDIO_DEVICE_H_
#define MODULES_AUDIO_DEVICE_INCLUDE_FAKE_AUDIO_DEVICE_H_

#include "modules/audio_device/include/audio_device.h"
#include "modules/audio_device/include/audio_device_default.h"

namespace webrtc {

class FakeAudioDeviceModule
    : public webrtc_impl::AudioDeviceModuleDefault<AudioDeviceModule> {};

}  // namespace webrtc

#endif  // MODULES_AUDIO_DEVICE_INCLUDE_FAKE_AUDIO_DEVICE_H_
