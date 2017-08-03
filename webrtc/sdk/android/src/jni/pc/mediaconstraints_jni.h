/*
 *  Copyright 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_SDK_ANDROID_SRC_JNI_PC_MEDIACONSTRAINTS_JNI_H_
#define WEBRTC_SDK_ANDROID_SRC_JNI_PC_MEDIACONSTRAINTS_JNI_H_

#include "webrtc/api/mediaconstraintsinterface.h"
#include "webrtc/sdk/android/src/jni/jni_helpers.h"

namespace webrtc_jni {

// Wrapper for a Java MediaConstraints object.  Copies all needed data so when
// the constructor returns the Java object is no longer needed.
class MediaConstraintsJni : public webrtc::MediaConstraintsInterface {
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

}  // namespace webrtc_jni

#endif  // WEBRTC_SDK_ANDROID_SRC_JNI_PC_MEDIACONSTRAINTS_JNI_H_
