/*
 *  Copyright 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/api/mediastreaminterface.h"
#include "webrtc/sdk/android/src/jni/jni_helpers.h"

namespace webrtc_jni {

JNI_FUNCTION_DECLARATION(void,
                         MediaStreamTrack_free,
                         JNIEnv*,
                         jclass,
                         jlong j_p) {
  reinterpret_cast<webrtc::MediaStreamTrackInterface*>(j_p)->Release();
}

JNI_FUNCTION_DECLARATION(jstring,
                         MediaStreamTrack_nativeId,
                         JNIEnv* jni,
                         jclass,
                         jlong j_p) {
  return JavaStringFromStdString(
      jni, reinterpret_cast<webrtc::MediaStreamTrackInterface*>(j_p)->id());
}

JNI_FUNCTION_DECLARATION(jstring,
                         MediaStreamTrack_nativeKind,
                         JNIEnv* jni,
                         jclass,
                         jlong j_p) {
  return JavaStringFromStdString(
      jni, reinterpret_cast<webrtc::MediaStreamTrackInterface*>(j_p)->kind());
}

JNI_FUNCTION_DECLARATION(jboolean,
                         MediaStreamTrack_nativeEnabled,
                         JNIEnv* jni,
                         jclass,
                         jlong j_p) {
  return reinterpret_cast<webrtc::MediaStreamTrackInterface*>(j_p)->enabled();
}

JNI_FUNCTION_DECLARATION(jobject,
                         MediaStreamTrack_nativeState,
                         JNIEnv* jni,
                         jclass,
                         jlong j_p) {
  return JavaEnumFromIndexAndClassName(
      jni, "MediaStreamTrack$State",
      reinterpret_cast<webrtc::MediaStreamTrackInterface*>(j_p)->state());
}

JNI_FUNCTION_DECLARATION(jboolean,
                         MediaStreamTrack_nativeSetEnabled,
                         JNIEnv* jni,
                         jclass,
                         jlong j_p,
                         jboolean enabled) {
  return reinterpret_cast<webrtc::MediaStreamTrackInterface*>(j_p)->set_enabled(
      enabled);
}

}  // namespace webrtc_jni
