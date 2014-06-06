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

#include <assert.h>

#include "webrtc/examples/android/media_demo/jni/jni_helpers.h"
#include "webrtc/examples/android/media_demo/jni/video_engine_jni.h"
#include "webrtc/examples/android/media_demo/jni/voice_engine_jni.h"
#include "webrtc/video_engine/include/vie_base.h"
#include "webrtc/voice_engine/include/voe_base.h"

// Macro for native functions that can be found by way of jni-auto discovery.
// Note extern "C" is needed for "discovery" of native methods to work.
#define JOWW(rettype, name)                                             \
  extern "C" rettype JNIEXPORT JNICALL Java_org_webrtc_webrtcdemo_##name

static JavaVM* g_vm = NULL;

extern "C" jint JNIEXPORT JNICALL JNI_OnLoad(JavaVM* vm, void* reserved) {
  // Only called once.
  CHECK(!g_vm, "OnLoad called more than once");
  g_vm = vm;
  return JNI_VERSION_1_4;
}

JOWW(void, NativeWebRtcContextRegistry_register)(
    JNIEnv* jni,
    jclass,
    jobject context) {
  webrtc_examples::SetVoeDeviceObjects(g_vm);
  webrtc_examples::SetVieDeviceObjects(g_vm);
  CHECK(webrtc::VideoEngine::SetAndroidObjects(g_vm, context) == 0,
        "Failed to register android objects to video engine");
  CHECK(webrtc::VoiceEngine::SetAndroidObjects(g_vm, jni, context) == 0,
        "Failed to register android objects to voice engine");
}

JOWW(void, NativeWebRtcContextRegistry_unRegister)(
    JNIEnv* jni,
    jclass) {
  CHECK(webrtc::VideoEngine::SetAndroidObjects(NULL, NULL) == 0,
        "Failed to unregister android objects from video engine");
  CHECK(webrtc::VoiceEngine::SetAndroidObjects(NULL, NULL, NULL) == 0,
        "Failed to unregister android objects from voice engine");
  webrtc_examples::ClearVieDeviceObjects();
  webrtc_examples::ClearVoeDeviceObjects();
}
