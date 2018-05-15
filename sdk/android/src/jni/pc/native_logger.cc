/*
 *  Copyright 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <memory>

#include "rtc_base/logging.h"
#include "sdk/android/native_api/jni/java_types.h"
#include "sdk/android/src/jni/jni_helpers.h"

namespace webrtc {
namespace jni {

JNI_FUNCTION_DECLARATION(void,
                         NativeLogger_nativeEnableLogToDebugOutput,
                         JNIEnv* jni,
                         jclass,
                         jint native_severity) {
  // Check that native_severity is a valid enum.
  if (native_severity >= rtc::LS_SENSITIVE && native_severity <= rtc::LS_NONE) {
    rtc::LogMessage::LogToDebug(
        static_cast<rtc::LoggingSeverity>(native_severity));
  }
}

JNI_FUNCTION_DECLARATION(void,
                         NativeLogger_nativeLog,
                         JNIEnv* jni,
                         jclass,
                         jint j_severity,
                         jstring j_tag,
                         jstring j_message) {
  std::string message = JavaToStdString(jni, JavaParamRef<jstring>(j_message));
  std::string tag = JavaToStdString(jni, JavaParamRef<jstring>(j_tag));
  RTC_LOG_TAG(static_cast<rtc::LoggingSeverity>(j_severity), tag.c_str())
      << message;
}

}  // namespace jni
}  // namespace webrtc
