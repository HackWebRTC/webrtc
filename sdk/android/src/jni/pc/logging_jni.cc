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

#include "webrtc/rtc_base/logging.h"
#include "webrtc/sdk/android/src/jni/jni_helpers.h"
#include "webrtc/system_wrappers/include/logcat_trace_context.h"
#include "webrtc/system_wrappers/include/trace.h"

namespace webrtc {
namespace jni {

JNI_FUNCTION_DECLARATION(void,
                         Logging_nativeEnableTracing,
                         JNIEnv* jni,
                         jclass,
                         jstring j_path,
                         jint nativeLevels) {
  std::string path = JavaToStdString(jni, j_path);
  if (nativeLevels != kTraceNone) {
    Trace::set_level_filter(nativeLevels);
    if (path != "logcat:") {
      RTC_CHECK_EQ(0, Trace::SetTraceFile(path.c_str(), false))
          << "SetTraceFile failed";
    } else {
      // Intentionally leak this to avoid needing to reason about its lifecycle.
      // It keeps no state and functions only as a dispatch point.
      static LogcatTraceContext* g_trace_callback = new LogcatTraceContext();
    }
  }
}

JNI_FUNCTION_DECLARATION(void,
                         Logging_nativeEnableLogToDebugOutput,
                         JNIEnv* jni,
                         jclass,
                         jint nativeSeverity) {
  if (nativeSeverity >= rtc::LS_SENSITIVE && nativeSeverity <= rtc::LS_NONE) {
    rtc::LogMessage::LogToDebug(
        static_cast<rtc::LoggingSeverity>(nativeSeverity));
  }
}

JNI_FUNCTION_DECLARATION(void,
                         Logging_nativeEnableLogThreads,
                         JNIEnv* jni,
                         jclass) {
  rtc::LogMessage::LogThreads(true);
}

JNI_FUNCTION_DECLARATION(void,
                         Logging_nativeEnableLogTimeStamps,
                         JNIEnv* jni,
                         jclass) {
  rtc::LogMessage::LogTimestamps(true);
}

JNI_FUNCTION_DECLARATION(void,
                         Logging_nativeLog,
                         JNIEnv* jni,
                         jclass,
                         jint j_severity,
                         jstring j_tag,
                         jstring j_message) {
  std::string message = JavaToStdString(jni, j_message);
  std::string tag = JavaToStdString(jni, j_tag);
  LOG_TAG(static_cast<rtc::LoggingSeverity>(j_severity), tag) << message;
}

}  // namespace jni
}  // namespace webrtc
