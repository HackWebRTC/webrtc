/*
 *  Copyright 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef SDK_ANDROID_SRC_JNI_PC_RTPPARAMETERS_H_
#define SDK_ANDROID_SRC_JNI_PC_RTPPARAMETERS_H_

#include <jni.h>

#include "api/rtpparameters.h"

namespace webrtc {
namespace jni {

RtpParameters JavaToNativeRtpParameters(JNIEnv* jni, jobject j_parameters);
jobject NativeToJavaRtpParameters(JNIEnv* jni, const RtpParameters& parameters);

}  // namespace jni
}  // namespace webrtc

#endif  // SDK_ANDROID_SRC_JNI_PC_RTPPARAMETERS_H_
