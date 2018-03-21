/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "sdk/android/native_api/audio_device_module/audio_device_android.h"

#include <stdlib.h>
#include "rtc_base/logging.h"
#include "rtc_base/refcount.h"
#include "rtc_base/refcountedobject.h"
#include "system_wrappers/include/metrics.h"
#include "sdk/android/src/jni/audio_device/audio_manager.h"

namespace webrtc {

#if defined(AUDIO_DEVICE_INCLUDE_ANDROID_AAUDIO)
rtc::scoped_refptr<AudioDeviceModule> CreateAAudioAudioDeviceModule(
    JNIEnv* env,
    jobject application_context) {
  return android_adm::AudioManager::CreateAAudioAudioDeviceModule(
      env, JavaParamRef<jobject>(application_context));
}
#endif

rtc::scoped_refptr<AudioDeviceModule> CreateAudioDeviceModule(
    JNIEnv* env,
    jobject application_context,
    bool use_opensles_input,
    bool use_opensles_output) {
  return android_adm::AudioManager::CreateAudioDeviceModule(
      env, JavaParamRef<jobject>(application_context), use_opensles_input,
      use_opensles_output);
}

}  // namespace webrtc
