/*
 *  Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/examples/android/media_demo/jni/jni_helpers.h"

#include <limits>

jmethodID GetMethodID(JNIEnv* jni, jclass c, const std::string& name,
                      const char* signature) {
  jmethodID m = jni->GetMethodID(c, name.c_str(), signature);
  CHECK_JNI_EXCEPTION(jni, "error during GetMethodID");
  return m;
}

jlong jlongFromPointer(void* ptr) {
  CHECK(sizeof(intptr_t) <= sizeof(jlong), "Time to rethink the use of jlongs");
  // Going through intptr_t to be obvious about the definedness of the
  // conversion from pointer to integral type.  intptr_t to jlong is a standard
  // widening by the COMPILE_ASSERT above.
  jlong ret = reinterpret_cast<intptr_t>(ptr);
  CHECK(reinterpret_cast<void*>(ret) == ptr,
        "jlong does not convert back to pointer");
  return ret;
}

// Given a (UTF-16) jstring return a new UTF-8 native string.
std::string JavaToStdString(JNIEnv* jni, const jstring& j_string) {
  const char* chars = jni->GetStringUTFChars(j_string, NULL);
  CHECK_JNI_EXCEPTION(jni, "Error during GetStringUTFChars");
  std::string str(chars, jni->GetStringUTFLength(j_string));
  CHECK_JNI_EXCEPTION(jni, "Error during GetStringUTFLength");
  jni->ReleaseStringUTFChars(j_string, chars);
  CHECK_JNI_EXCEPTION(jni, "Error during ReleaseStringUTFChars");
  return str;
}

ClassReferenceHolder::ClassReferenceHolder(JNIEnv* jni, const char** classes,
                                           int size) {
  for (int i = 0; i < size; ++i) {
    LoadClass(jni, classes[i]);
  }
}
ClassReferenceHolder::~ClassReferenceHolder() {
  CHECK(classes_.empty(), "Must call FreeReferences() before dtor!");
}

void ClassReferenceHolder::FreeReferences(JNIEnv* jni) {
  for (std::map<std::string, jclass>::const_iterator it = classes_.begin();
       it != classes_.end(); ++it) {
    jni->DeleteGlobalRef(it->second);
  }
  classes_.clear();
}

jclass ClassReferenceHolder::GetClass(const std::string& name) {
  std::map<std::string, jclass>::iterator it = classes_.find(name);
  CHECK(it != classes_.end(), "Could not find class");
  return it->second;
}

void ClassReferenceHolder::LoadClass(JNIEnv* jni, const std::string& name) {
  jclass localRef = jni->FindClass(name.c_str());
  CHECK_JNI_EXCEPTION(jni, "Could not load class");
  CHECK(localRef, name.c_str());
  jclass globalRef = reinterpret_cast<jclass>(jni->NewGlobalRef(localRef));
  CHECK_JNI_EXCEPTION(jni, "error during NewGlobalRef");
  CHECK(globalRef, name.c_str());
  bool inserted = classes_.insert(std::make_pair(name, globalRef)).second;
  CHECK(inserted, "Duplicate class name");
}
