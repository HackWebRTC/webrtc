/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef SDK_ANDROID_NATIVE_API_AUDIO_DEVICE_MODULE_AUDIO_DEVICE_ANDROID_H_
#define SDK_ANDROID_NATIVE_API_AUDIO_DEVICE_MODULE_AUDIO_DEVICE_ANDROID_H_

#include "modules/audio_device/include/audio_device.h"

namespace webrtc {

rtc::scoped_refptr<AudioDeviceModule> CreateAndroidAudioDeviceModule();

}  // namespace webrtc

#endif  // SDK_ANDROID_NATIVE_API_AUDIO_DEVICE_MODULE_AUDIO_DEVICE_ANDROID_H_
