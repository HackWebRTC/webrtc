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

JOW(void, MediaSource_free)(JNIEnv*, jclass, jlong j_p) {
  reinterpret_cast<rtc::RefCountInterface*>(j_p)->Release();
}

JOW(jobject, MediaSource_nativeState)(JNIEnv* jni, jclass, jlong j_p) {
  rtc::scoped_refptr<webrtc::MediaSourceInterface> p(
      reinterpret_cast<webrtc::MediaSourceInterface*>(j_p));
  return JavaEnumFromIndexAndClassName(jni, "MediaSource$State", p->state());
}

}  // namespace webrtc_jni
