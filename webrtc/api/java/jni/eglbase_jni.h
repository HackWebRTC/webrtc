/*
 *  Copyright 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
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
