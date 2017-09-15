/*
 *  Copyright 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef SDK_ANDROID_SRC_JNI_PC_MEDIACONSTRAINTS_JNI_H_
#define SDK_ANDROID_SRC_JNI_PC_MEDIACONSTRAINTS_JNI_H_

#include "api/mediaconstraintsinterface.h"
#include "sdk/android/src/jni/jni_helpers.h"

namespace webrtc {
namespace jni {

// Wrapper for a Java MediaConstraints object.  Copies all needed data so when
// the constructor returns the Java object is no longer needed.
class MediaConstraintsJni : public MediaConstraintsInterface {
 public:
  MediaConstraintsJni(JNIEnv* jni, jobject j_constraints);
  virtual ~MediaConstraintsJni() {}

  // MediaConstraintsInterface.
  const Constraints& GetMandatory() const override { return mandatory_; }
  const Constraints& GetOptional() const override { return optional_; }

 private:
  // Helper for translating a List<Pair<String, String>> to a Constraints.
  static void PopulateConstraintsFromJavaPairList(JNIEnv* jni,
                                                  jobject j_constraints,
                                                  const char* field_name,
                                                  Constraints* field);

  Constraints mandatory_;
  Constraints optional_;
};

}  // namespace jni
}  // namespace webrtc

#endif  // SDK_ANDROID_SRC_JNI_PC_MEDIACONSTRAINTS_JNI_H_
