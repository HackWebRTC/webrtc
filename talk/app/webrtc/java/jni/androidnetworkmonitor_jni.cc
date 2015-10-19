/*
 * libjingle
 * Copyright 2015 Google Inc.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 *  1. Redistributions of source code must retain the above copyright notice,
 *     this list of conditions and the following disclaimer.
 *  2. Redistributions in binary form must reproduce the above copyright notice,
 *     this list of conditions and the following disclaimer in the documentation
 *     and/or other materials provided with the distribution.
 *  3. The name of the author may not be used to endorse or promote products
 *     derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO
 * EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "talk/app/webrtc/java/jni/androidnetworkmonitor_jni.h"

#include "webrtc/base/common.h"
#include "talk/app/webrtc/java/jni/classreferenceholder.h"
#include "talk/app/webrtc/java/jni/jni_helpers.h"

namespace webrtc_jni {
jobject AndroidNetworkMonitor::application_context_ = nullptr;

// static
void AndroidNetworkMonitor::SetAndroidContext(JNIEnv* jni, jobject context) {
  if (application_context_) {
    jni->DeleteGlobalRef(application_context_);
  }
  application_context_ = NewGlobalRef(jni, context);
}

AndroidNetworkMonitor::AndroidNetworkMonitor()
    : j_network_monitor_class_(jni(),
                               FindClass(jni(), "org/webrtc/NetworkMonitor")),
      j_network_monitor_(
          jni(),
          jni()->CallStaticObjectMethod(
              *j_network_monitor_class_,
              GetStaticMethodID(
                  jni(),
                  *j_network_monitor_class_,
                  "init",
                  "(Landroid/content/Context;)Lorg/webrtc/NetworkMonitor;"),
              application_context_)) {
  ASSERT(application_context_ != nullptr);
  CHECK_EXCEPTION(jni()) << "Error during NetworkMonitor.init";
}

void AndroidNetworkMonitor::Start() {
  RTC_CHECK(thread_checker_.CalledOnValidThread());
  jmethodID m =
      GetMethodID(jni(), *j_network_monitor_class_, "startMonitoring", "(J)V");
  jni()->CallVoidMethod(*j_network_monitor_, m, jlongFromPointer(this));
  CHECK_EXCEPTION(jni()) << "Error during NetworkMonitor.startMonitoring";
}

void AndroidNetworkMonitor::Stop() {
  RTC_CHECK(thread_checker_.CalledOnValidThread());
  jmethodID m =
      GetMethodID(jni(), *j_network_monitor_class_, "stopMonitoring", "(J)V");
  jni()->CallVoidMethod(*j_network_monitor_, m, jlongFromPointer(this));
  CHECK_EXCEPTION(jni()) << "Error during NetworkMonitor.stopMonitoring";
}

JOW(void, NetworkMonitor_nativeNotifyConnectionTypeChanged)(
    JNIEnv* jni, jobject j_monitor, jlong j_native_monitor) {
  rtc::NetworkMonitorInterface* network_monitor =
      reinterpret_cast<rtc::NetworkMonitorInterface*>(j_native_monitor);
  network_monitor->OnNetworksChanged();
}

}  // namespace webrtc_jni
