/*
 *  Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_EXAMPLES_ANDROID_MEDIA_DEMO_JNI_JNI_HELPERS_H_
#define WEBRTC_EXAMPLES_ANDROID_MEDIA_DEMO_JNI_JNI_HELPERS_H_

// TODO(henrike): this file contains duplication with regards to
// talk/app/webrtc/java/jni/peerconnection_jni.cc. When/if code can be shared
// between trunk/talk and trunk/webrtc remove the duplication.

#include <android/log.h>
#include <jni.h>

#include <map>
#include <string>

#define TAG "WEBRTC-NATIVE"

// Abort the process if |x| is false, emitting |msg| to logcat.
#define CHECK(x, msg)                                                  \
  if (x) {                                                             \
  } else {                                                             \
    __android_log_print(ANDROID_LOG_ERROR, TAG, "%s:%d: %s", __FILE__, \
                        __LINE__, msg);                                \
    abort();                                                           \
  }

// Abort the process if |jni| has a Java exception pending, emitting |msg| to
// logcat.
#define CHECK_EXCEPTION(jni, msg) \
  if (0) {                        \
  } else {                        \
    if (jni->ExceptionCheck()) {  \
      jni->ExceptionDescribe();   \
      jni->ExceptionClear();      \
      CHECK(0, msg);              \
    }                             \
  }

#define ARRAYSIZE(instance)                                     \
  static_cast<int>(sizeof(instance) / sizeof(instance[0]))

// JNIEnv-helper methods that CHECK success: no Java exception thrown and found
// object/class/method/field is non-null.
jmethodID GetMethodID(JNIEnv* jni, jclass c, const std::string& name,
                      const char* signature);

// Return a |jlong| that will automatically convert back to |ptr| when assigned
// to a |uint64|
jlong jlongFromPointer(void* ptr);

// Given a (UTF-16) jstring return a new UTF-8 native string.
std::string JavaToStdString(JNIEnv* jni, const jstring& j_string);

// Android's FindClass() is trickier than usual because the app-specific
// ClassLoader is not consulted when there is no app-specific frame on the
// stack.  Consequently, we only look up classes once in JNI_OnLoad.
// http://developer.android.com/training/articles/perf-jni.html#faq_FindClass
class ClassReferenceHolder {
 public:
  ClassReferenceHolder(JNIEnv* jni, const char** classes, int size);
  ~ClassReferenceHolder();

  void FreeReferences(JNIEnv* jni);

  jclass GetClass(const std::string& name);

 private:
  void LoadClass(JNIEnv* jni, const std::string& name);

  std::map<std::string, jclass> classes_;
};

#endif  // WEBRTC_EXAMPLES_ANDROID_MEDIA_DEMO_JNI_JNI_HELPERS_H_
