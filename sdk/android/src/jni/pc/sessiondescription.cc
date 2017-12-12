/*
 *  Copyright 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "sdk/android/src/jni/pc/sessiondescription.h"

#include <string>

#include "rtc_base/logging.h"
#include "sdk/android/generated_peerconnection_jni/jni/SessionDescription_jni.h"
#include "sdk/android/src/jni/jni_helpers.h"

namespace webrtc {
namespace jni {

std::unique_ptr<SessionDescriptionInterface> JavaToNativeSessionDescription(
    JNIEnv* jni,
    jobject j_sdp) {
  std::string std_type = JavaToStdString(
      jni, Java_SessionDescription_getTypeInCanonicalForm(jni, j_sdp));
  std::string std_description =
      JavaToStdString(jni, Java_SessionDescription_getDescription(jni, j_sdp));
  rtc::Optional<SdpType> sdp_type_maybe = SdpTypeFromString(std_type);
  if (!sdp_type_maybe) {
    RTC_LOG(LS_ERROR) << "Unexpected SDP type: " << std_type;
    return nullptr;
  }
  return CreateSessionDescription(*sdp_type_maybe, std_description);
}

jobject NativeToJavaSessionDescription(
    JNIEnv* jni,
    const SessionDescriptionInterface* desc) {
  std::string sdp;
  RTC_CHECK(desc->ToString(&sdp)) << "got so far: " << sdp;
  jstring j_description = NativeToJavaString(jni, sdp);
  jobject j_type =
      Java_Type_fromCanonicalForm(jni, NativeToJavaString(jni, desc->type()));
  jobject j_sdp =
      Java_SessionDescription_Constructor(jni, j_type, j_description);
  CHECK_EXCEPTION(jni) << "error during NewObject";
  return j_sdp;
}

}  // namespace jni
}  // namespace webrtc
