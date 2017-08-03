/*
 *  Copyright 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/sdk/android/src/jni/pc/statsobserver_jni.h"

#include "webrtc/sdk/android/src/jni/classreferenceholder.h"

namespace webrtc_jni {

// Convenience, used since callbacks occur on the signaling thread, which may
// be a non-Java thread.
static JNIEnv* jni() {
  return AttachCurrentThreadIfNeeded();
}

StatsObserverJni::StatsObserverJni(JNIEnv* jni, jobject j_observer)
    : j_observer_global_(jni, j_observer),
      j_observer_class_(jni, GetObjectClass(jni, j_observer)),
      j_stats_report_class_(jni, FindClass(jni, "org/webrtc/StatsReport")),
      j_stats_report_ctor_(GetMethodID(jni,
                                       *j_stats_report_class_,
                                       "<init>",
                                       "(Ljava/lang/String;Ljava/lang/String;D"
                                       "[Lorg/webrtc/StatsReport$Value;)V")),
      j_value_class_(jni, FindClass(jni, "org/webrtc/StatsReport$Value")),
      j_value_ctor_(GetMethodID(jni,
                                *j_value_class_,
                                "<init>",
                                "(Ljava/lang/String;Ljava/lang/String;)V")) {}

void StatsObserverJni::OnComplete(const webrtc::StatsReports& reports) {
  ScopedLocalRefFrame local_ref_frame(jni());
  jobjectArray j_reports = ReportsToJava(jni(), reports);
  jmethodID m = GetMethodID(jni(), *j_observer_class_, "onComplete",
                            "([Lorg/webrtc/StatsReport;)V");
  jni()->CallVoidMethod(*j_observer_global_, m, j_reports);
  CHECK_EXCEPTION(jni()) << "error during CallVoidMethod";
}

jobjectArray StatsObserverJni::ReportsToJava(
    JNIEnv* jni,
    const webrtc::StatsReports& reports) {
  jobjectArray reports_array =
      jni->NewObjectArray(reports.size(), *j_stats_report_class_, NULL);
  int i = 0;
  for (const auto* report : reports) {
    ScopedLocalRefFrame local_ref_frame(jni);
    jstring j_id = JavaStringFromStdString(jni, report->id()->ToString());
    jstring j_type = JavaStringFromStdString(jni, report->TypeToString());
    jobjectArray j_values = ValuesToJava(jni, report->values());
    jobject j_report =
        jni->NewObject(*j_stats_report_class_, j_stats_report_ctor_, j_id,
                       j_type, report->timestamp(), j_values);
    jni->SetObjectArrayElement(reports_array, i++, j_report);
  }
  return reports_array;
}

jobjectArray StatsObserverJni::ValuesToJava(
    JNIEnv* jni,
    const webrtc::StatsReport::Values& values) {
  jobjectArray j_values =
      jni->NewObjectArray(values.size(), *j_value_class_, NULL);
  int i = 0;
  for (const auto& it : values) {
    ScopedLocalRefFrame local_ref_frame(jni);
    // Should we use the '.name' enum value here instead of converting the
    // name to a string?
    jstring j_name = JavaStringFromStdString(jni, it.second->display_name());
    jstring j_value = JavaStringFromStdString(jni, it.second->ToString());
    jobject j_element_value =
        jni->NewObject(*j_value_class_, j_value_ctor_, j_name, j_value);
    jni->SetObjectArrayElement(j_values, i++, j_element_value);
  }
  return j_values;
}

}  // namespace webrtc_jni
