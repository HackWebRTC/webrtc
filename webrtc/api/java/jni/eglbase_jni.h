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

#ifndef WEBRTC_API_JAVA_JNI_EGLBASE_JNI_H_
#define WEBRTC_API_JAVA_JNI_EGLBASE_JNI_H_

#include <jni.h>

#include "webrtc/base/constructormagic.h"

namespace webrtc_jni {

// Helper class used for creating a Java instance of org/webrtc/EglBase.
class EglBase {
 public:
  EglBase();
  ~EglBase();

  // Creates an new java EglBase instance. |egl_base_context| must be a valid
  // EglBase$Context.
  // Returns false if |egl_base_context| is a null Java object or if an
  // exception occur in Java.
  bool CreateEglBase(JNIEnv* jni, jobject egl_base_context);
  jobject egl_base_context() const { return egl_base_context_; }

 private:
  jobject egl_base_ = nullptr;  // instance of org/webrtc/EglBase
  jobject egl_base_context_ = nullptr;  // instance of EglBase$Context

  RTC_DISALLOW_COPY_AND_ASSIGN(EglBase);
};

}  // namespace webrtc_jni

#endif  // WEBRTC_API_JAVA_JNI_EGLBASE_JNI_H_
