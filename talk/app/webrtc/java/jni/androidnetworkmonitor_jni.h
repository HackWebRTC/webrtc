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

#ifndef TALK_APP_WEBRTC_JAVA_JNI_ANDROIDNETWORKMONITOR_JNI_H_
#define TALK_APP_WEBRTC_JAVA_JNI_ANDROIDNETWORKMONITOR_JNI_H_

#include "webrtc/base/networkmonitor.h"

#include <map>

#include "webrtc/base/basictypes.h"
#include "webrtc/base/thread_checker.h"
#include "talk/app/webrtc/java/jni/jni_helpers.h"

namespace webrtc_jni {

typedef uint32_t NetworkHandle;

// c++ equivalent of java NetworkMonitorAutoDetect.ConnectionType.
enum NetworkType {
  NETWORK_UNKNOWN,
  NETWORK_ETHERNET,
  NETWORK_WIFI,
  NETWORK_4G,
  NETWORK_3G,
  NETWORK_2G,
  NETWORK_BLUETOOTH,
  NETWORK_NONE
};

// The information is collected from Android OS so that the native code can get
// the network type and handle (Android network ID) for each interface.
struct NetworkInformation {
  std::string interface_name;
  NetworkHandle handle;
  NetworkType type;
  std::vector<rtc::IPAddress> ip_addresses;

  std::string ToString() const;
};

class AndroidNetworkMonitor : public rtc::NetworkMonitorBase,
                              public rtc::NetworkBinderInterface {
 public:
  AndroidNetworkMonitor();

  static void SetAndroidContext(JNIEnv* jni, jobject context);

  void Start() override;
  void Stop() override;

  int BindSocketToNetwork(int socket_fd,
                          const rtc::IPAddress& address) override;
  void OnNetworkAvailable(const NetworkInformation& network_info);

 private:
  JNIEnv* jni() { return AttachCurrentThreadIfNeeded(); }

  void OnNetworkAvailable_w(const NetworkInformation& network_info);

  ScopedGlobalRef<jclass> j_network_monitor_class_;
  ScopedGlobalRef<jobject> j_network_monitor_;
  rtc::ThreadChecker thread_checker_;
  static jobject application_context_;
  bool started_ = false;
  std::map<rtc::IPAddress, NetworkInformation> network_info_by_address_;
};

class AndroidNetworkMonitorFactory : public rtc::NetworkMonitorFactory {
 public:
  AndroidNetworkMonitorFactory() {}

  rtc::NetworkMonitorInterface* CreateNetworkMonitor() override;
};

}  // namespace webrtc_jni

#endif  // TALK_APP_WEBRTC_JAVA_JNI_ANDROIDNETWORKMONITOR_JNI_H_
