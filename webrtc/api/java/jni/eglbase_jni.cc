/*
 * libjingle
 * Copyright 2016 Google Inc.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 *  1. Redistributions of source code must retain the above copyright notice,
 *     this list of conditions and the following disclaimer.
 *  2. Redistributions in binary form must reproduce the above copyright notice,
 *     this list of conditions and the following disclaimer in the documentation
 *     and/or other materials provided with the distribution.
 *  3. The name of the author may not be used to endorse or promote products
 *     derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO
 * EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
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
