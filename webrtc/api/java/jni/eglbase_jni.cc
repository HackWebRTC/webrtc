/*
 *  Copyright 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/api/java/jni/eglbase_jni.h"

#include "webrtc/api/java/jni/androidmediacodeccommon.h"
#include "webrtc/api/java/jni/classreferenceholder.h"
#include "webrtc/api/java/jni/jni_helpers.h"

namespace webrtc_jni {

EglBase::EglBase() {
}

EglBase::~EglBase() {
  if (egl_base_) {
    JNIEnv* jni = AttachCurrentThreadIfNeeded();
    jni->DeleteGlobalRef(egl_base_context_);
    egl_base_context_ = nullptr;
    jni->CallVoidMethod(egl_base_,
                        GetMethodID(jni,
                                    FindClass(jni, "org/webrtc/EglBase"),
                                    "release", "()V"));
    jni->DeleteGlobalRef(egl_base_);
  }
}

bool EglBase::CreateEglBase(JNIEnv* jni, jobject egl_context) {
  if (egl_base_) {
    jni->DeleteGlobalRef(egl_base_context_);
    egl_base_context_ = nullptr;
    jni->CallVoidMethod(egl_base_,
                        GetMethodID(jni,
                                    FindClass(jni, "org/webrtc/EglBase"),
                                    "release", "()V"));
    jni->DeleteGlobalRef(egl_base_);
    egl_base_ = nullptr;
  }

  if (IsNull(jni, egl_context))
    return false;

  jobject egl_base = jni->CallStaticObjectMethod(
      FindClass(jni, "org/webrtc/EglBase"),
      GetStaticMethodID(jni,
                        FindClass(jni, "org/webrtc/EglBase"),
                        "create",
                        "(Lorg/webrtc/EglBase$Context;)Lorg/webrtc/EglBase;"),
                        egl_context);
  if (CheckException(jni))
    return false;

  egl_base_ = jni->NewGlobalRef(egl_base);
  egl_base_context_ =  jni->NewGlobalRef(
      jni->CallObjectMethod(
          egl_base_,
          GetMethodID(jni,
                      FindClass(jni, "org/webrtc/EglBase"),
                      "getEglBaseContext",
                      "()Lorg/webrtc/EglBase$Context;")));
  RTC_CHECK(egl_base_context_);
  return true;
}

}  // namespace webrtc_jni
