/*
 *  Copyright (c) 2022 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "system_wrappers/include/field_trial.h"

#include "sdk/android/generated_field_trial_jni/FieldTrial_jni.h"
#include "sdk/android/src/jni/jni_helpers.h"

namespace webrtc {
namespace jni {

static ScopedJavaLocalRef<jstring> JNI_FieldTrial_FindFieldTrialsFullName(
    JNIEnv* jni,
    const JavaParamRef<jstring>& j_name) {
  return NativeToJavaString(
      jni, field_trial::FindFullName(JavaToStdString(jni, j_name)));
}

}  // namespace jni
}  // namespace webrtc
