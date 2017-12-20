/*
 *  Copyright 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <jni.h>

#include "media/base/h264_profile_level_id.h"
#include "sdk/android/generated_video_jni/jni/HardwareVideoEncoderFactory_jni.h"
#include "sdk/android/src/jni/jni_helpers.h"

namespace webrtc {
namespace jni {

static jboolean JNI_HardwareVideoEncoderFactory_IsSameH264Profile(
    JNIEnv* jni,
    const JavaParamRef<jclass>&,
    const JavaParamRef<jobject>& params1,
    const JavaParamRef<jobject>& params2) {
  return H264::IsSameH264Profile(JavaToStdMapStrings(jni, params1),
                                 JavaToStdMapStrings(jni, params2));
}

}  // namespace jni
}  // namespace webrtc
