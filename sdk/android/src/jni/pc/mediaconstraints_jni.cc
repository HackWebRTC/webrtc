/*
 *  Copyright 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "sdk/android/src/jni/pc/mediaconstraints_jni.h"

namespace webrtc {
namespace jni {

MediaConstraintsJni::MediaConstraintsJni(JNIEnv* jni, jobject j_constraints) {
  PopulateConstraintsFromJavaPairList(jni, j_constraints, "mandatory",
                                      &mandatory_);
  PopulateConstraintsFromJavaPairList(jni, j_constraints, "optional",
                                      &optional_);
}

// static
void MediaConstraintsJni::PopulateConstraintsFromJavaPairList(
    JNIEnv* jni,
    jobject j_constraints,
    const char* field_name,
    Constraints* field) {
  jfieldID j_id = GetFieldID(jni, GetObjectClass(jni, j_constraints),
                             field_name, "Ljava/util/List;");
  jobject j_list = GetObjectField(jni, j_constraints, j_id);
  for (jobject entry : Iterable(jni, j_list)) {
    jmethodID get_key = GetMethodID(jni, GetObjectClass(jni, entry), "getKey",
                                    "()Ljava/lang/String;");
    jstring j_key =
        reinterpret_cast<jstring>(jni->CallObjectMethod(entry, get_key));
    CHECK_EXCEPTION(jni) << "error during CallObjectMethod";
    jmethodID get_value = GetMethodID(jni, GetObjectClass(jni, entry),
                                      "getValue", "()Ljava/lang/String;");
    jstring j_value =
        reinterpret_cast<jstring>(jni->CallObjectMethod(entry, get_value));
    CHECK_EXCEPTION(jni) << "error during CallObjectMethod";
    field->push_back(
        Constraint(JavaToStdString(jni, j_key), JavaToStdString(jni, j_value)));
  }
}

}  // namespace jni
}  // namespace webrtc
