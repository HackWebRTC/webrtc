/*
 *  Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <jni.h>

#include "webrtc/modules/audio_device/android/test/fake_audio_device_buffer.h"
#include "webrtc/modules/audio_device/android/opensles_input.h"
#include "webrtc/modules/audio_device/android/opensles_output.h"
#include "webrtc/system_wrappers/interface/scoped_ptr.h"

#ifndef WEBRTC_MODULES_AUDIO_DEVICE_ANDROID_JNI_OPENSL_RUNNER_H_
#define WEBRTC_MODULES_AUDIO_DEVICE_ANDROID_JNI_OPENSL_RUNNER_H_

namespace webrtc {

class FakeAudioDeviceBuffer;

class OpenSlRunner {
 public:
  OpenSlRunner();
  ~OpenSlRunner() {}

  void StartPlayRecord();
  void StopPlayRecord();

  static JNIEXPORT void JNICALL RegisterApplicationContext(JNIEnv * env,
                                                           jobject,
                                                           jobject context);
  static JNIEXPORT void JNICALL Start(JNIEnv * env, jobject);
  static JNIEXPORT void JNICALL Stop(JNIEnv * env, jobject);

 private:
  OpenSlesOutput output_;
  OpenSlesInput input_;
  FakeAudioDeviceBuffer audio_buffer_;
};

}  // namespace webrtc

#endif  // WEBRTC_MODULES_AUDIO_DEVICE_ANDROID_JNI_OPENSL_RUNNER_H_
