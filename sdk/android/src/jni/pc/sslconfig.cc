
/*
 *  Copyright 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "sdk/android/src/jni/pc/sslconfig.h"

#include <string>

#include "rtc_base/ssladapter.h"
#include "sdk/android/generated_peerconnection_jni/jni/SslConfig_jni.h"
#include "sdk/android/native_api/jni/java_types.h"
#include "sdk/android/src/jni/jni_helpers.h"

namespace webrtc {
namespace jni {

rtc::TlsCertPolicy JavaToNativeRtcTlsCertPolicy(
    JNIEnv* jni,
    const JavaRef<jobject>& j_ssl_config_tls_cert_policy) {
  std::string enum_name = GetJavaEnumName(jni, j_ssl_config_tls_cert_policy);

  if (enum_name == "TLS_CERT_POLICY_SECURE")
    return rtc::TlsCertPolicy::TLS_CERT_POLICY_SECURE;

  if (enum_name == "TLS_CERT_POLICY_INSECURE_NO_CHECK")
    return rtc::TlsCertPolicy::TLS_CERT_POLICY_INSECURE_NO_CHECK;

  RTC_NOTREACHED();
  return rtc::TlsCertPolicy::TLS_CERT_POLICY_SECURE;
}

rtc::SSLConfig JavaToNativeSslConfig(JNIEnv* jni,
                                     const JavaRef<jobject>& j_ssl_config) {
  rtc::SSLConfig ssl_config;
  ssl_config.enable_ocsp_stapling =
      Java_SslConfig_getEnableOcspStapling(jni, j_ssl_config);
  ssl_config.enable_signed_cert_timestamp =
      Java_SslConfig_getEnableSignedCertTimestamp(jni, j_ssl_config);
  ssl_config.enable_tls_channel_id =
      Java_SslConfig_getEnableTlsChannelId(jni, j_ssl_config);
  ssl_config.enable_grease = Java_SslConfig_getEnableGrease(jni, j_ssl_config);

  ScopedJavaLocalRef<jobject> j_ssl_config_max_ssl_version =
      Java_SslConfig_getMaxSslVersion(jni, j_ssl_config);
  ssl_config.max_ssl_version =
      JavaToNativeOptionalInt(jni, j_ssl_config_max_ssl_version);

  ScopedJavaLocalRef<jobject> j_ssl_config_tls_cert_policy =
      Java_SslConfig_getTlsCertPolicy(jni, j_ssl_config);
  ssl_config.tls_cert_policy =
      JavaToNativeRtcTlsCertPolicy(jni, j_ssl_config_tls_cert_policy);

  ScopedJavaLocalRef<jobject> j_ssl_config_tls_alpn_protocols =
      Java_SslConfig_getTlsAlpnProtocols(jni, j_ssl_config);
  if (!IsNull(jni, j_ssl_config_tls_alpn_protocols)) {
    ssl_config.tls_alpn_protocols =
        JavaListToNativeVector<std::string, jstring>(
            jni, j_ssl_config_tls_alpn_protocols, &JavaToNativeString);
  }
  ScopedJavaLocalRef<jobject> j_ssl_config_tls_elliptic_curves =
      Java_SslConfig_getTlsEllipticCurves(jni, j_ssl_config);
  if (!IsNull(jni, j_ssl_config_tls_elliptic_curves)) {
    ssl_config.tls_elliptic_curves =
        JavaListToNativeVector<std::string, jstring>(
            jni, j_ssl_config_tls_elliptic_curves, &JavaToNativeString);
  }
  return ssl_config;
}

}  // namespace jni
}  // namespace webrtc
