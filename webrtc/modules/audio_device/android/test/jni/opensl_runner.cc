/*
 *  Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/modules/audio_device/android/test/jni/opensl_runner.h"

#include <assert.h>

#include "webrtc/modules/audio_device/android/audio_manager_jni.h"

// Java globals
static JavaVM* g_vm = NULL;
static jclass g_osr = NULL;

// Global class implementing native code.
static webrtc::OpenSlRunner* g_runner = NULL;

jint JNI_OnLoad(JavaVM* vm, void* reserved) {
  // Only called once.
  assert(!g_vm);
  JNIEnv* env;
  if (vm->GetEnv(reinterpret_cast<void**>(&env), JNI_VERSION_1_6) != JNI_OK) {
    return -1;
  }

  jclass local_osr = env->FindClass("org/webrtc/app/OpenSlRunner");
  assert(local_osr != NULL);
  g_osr = static_cast<jclass>(env->NewGlobalRef(local_osr));
  JNINativeMethod nativeFunctions[] = {
    {"RegisterApplicationContext", "(Landroid/content/Context;)V",
     reinterpret_cast<void*>(
         &webrtc::OpenSlRunner::RegisterApplicationContext)},
    {"Start", "()V", reinterpret_cast<void*>(&webrtc::OpenSlRunner::Start)},
    {"Stop",  "()V", reinterpret_cast<void*>(&webrtc::OpenSlRunner::Stop)}
  };
  int ret_val = env->RegisterNatives(g_osr, nativeFunctions, 3);
  if (ret_val != 0) {
    assert(false);
  }
  g_vm = vm;
  return JNI_VERSION_1_6;
}

namespace webrtc {

OpenSlRunner::OpenSlRunner()
  : output_(0),
    input_(0, &output_) {
  output_.AttachAudioBuffer(&audio_buffer_);
  if (output_.Init() != 0) {
    assert(false);
  }
  if (output_.InitPlayout() != 0) {
    assert(false);
  }
  input_.AttachAudioBuffer(&audio_buffer_);
  if (input_.Init() != 0) {
    assert(false);
  }
  if (input_.InitRecording() != 0) {
    assert(false);
  }
}

void OpenSlRunner::StartPlayRecord() {
  output_.StartPlayout();
  input_.StartRecording();
}

void OpenSlRunner::StopPlayRecord() {
  // There are large enough buffers to compensate for recording and playing
  // jitter such that the timing of stopping playing or recording should not
  // result in over or underrun.
  input_.StopRecording();
  output_.StopPlayout();
  audio_buffer_.ClearBuffer();
}

JNIEXPORT void JNICALL OpenSlRunner::RegisterApplicationContext(
    JNIEnv * env,
    jobject,
    jobject context) {
  assert(!g_runner);  // Should only be called once.
  AudioManagerJni::SetAndroidAudioDeviceObjects(g_vm, env, context);
  // Might as well create the global instance since everything is set up at this
  // point.
  g_runner = new webrtc::OpenSlRunner();
}

JNIEXPORT void JNICALL OpenSlRunner::Start(JNIEnv * env, jobject) {
  g_runner->StartPlayRecord();
}

JNIEXPORT void JNICALL OpenSlRunner::Stop(JNIEnv * env, jobject) {
  g_runner->StopPlayRecord();
}

}  // namespace webrtc
