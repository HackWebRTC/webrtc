/*
 *  Copyright 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/sdk/android/src/jni/pc/java_native_conversion.h"

#include <string>

#include "webrtc/pc/webrtcsdp.h"
#include "webrtc/sdk/android/src/jni/classreferenceholder.h"

namespace webrtc {
namespace jni {

DataChannelInit JavaToNativeDataChannelInit(JNIEnv* jni, jobject j_init) {
  DataChannelInit init;

  jclass j_init_class = FindClass(jni, "org/webrtc/DataChannel$Init");
  jfieldID ordered_id = GetFieldID(jni, j_init_class, "ordered", "Z");
  jfieldID max_retransmit_time_id =
      GetFieldID(jni, j_init_class, "maxRetransmitTimeMs", "I");
  jfieldID max_retransmits_id =
      GetFieldID(jni, j_init_class, "maxRetransmits", "I");
  jfieldID protocol_id =
      GetFieldID(jni, j_init_class, "protocol", "Ljava/lang/String;");
  jfieldID negotiated_id = GetFieldID(jni, j_init_class, "negotiated", "Z");
  jfieldID id_id = GetFieldID(jni, j_init_class, "id", "I");

  init.ordered = GetBooleanField(jni, j_init, ordered_id);
  init.maxRetransmitTime = GetIntField(jni, j_init, max_retransmit_time_id);
  init.maxRetransmits = GetIntField(jni, j_init, max_retransmits_id);
  init.protocol =
      JavaToStdString(jni, GetStringField(jni, j_init, protocol_id));
  init.negotiated = GetBooleanField(jni, j_init, negotiated_id);
  init.id = GetIntField(jni, j_init, id_id);

  return init;
}

jobject NativeToJavaMediaType(JNIEnv* jni, cricket::MediaType media_type) {
  jclass j_media_type_class =
      FindClass(jni, "org/webrtc/MediaStreamTrack$MediaType");

  const char* media_type_str = nullptr;
  switch (media_type) {
    case cricket::MEDIA_TYPE_AUDIO:
      media_type_str = "MEDIA_TYPE_AUDIO";
      break;
    case cricket::MEDIA_TYPE_VIDEO:
      media_type_str = "MEDIA_TYPE_VIDEO";
      break;
    case cricket::MEDIA_TYPE_DATA:
      RTC_NOTREACHED();
      break;
  }
  jfieldID j_media_type_fid =
      GetStaticFieldID(jni, j_media_type_class, media_type_str,
                       "Lorg/webrtc/MediaStreamTrack$MediaType;");
  return GetStaticObjectField(jni, j_media_type_class, j_media_type_fid);
}

cricket::MediaType JavaToNativeMediaType(JNIEnv* jni, jobject j_media_type) {
  jclass j_media_type_class =
      FindClass(jni, "org/webrtc/MediaStreamTrack$MediaType");
  jmethodID j_name_id =
      GetMethodID(jni, j_media_type_class, "name", "()Ljava/lang/String;");
  jstring j_type_string =
      (jstring)jni->CallObjectMethod(j_media_type, j_name_id);
  CHECK_EXCEPTION(jni) << "error during CallObjectMethod";
  std::string type_string = JavaToStdString(jni, j_type_string);

  RTC_DCHECK(type_string == "MEDIA_TYPE_AUDIO" ||
             type_string == "MEDIA_TYPE_VIDEO")
      << "Media type: " << type_string;
  return type_string == "MEDIA_TYPE_AUDIO" ? cricket::MEDIA_TYPE_AUDIO
                                           : cricket::MEDIA_TYPE_VIDEO;
}

cricket::Candidate JavaToNativeCandidate(JNIEnv* jni, jobject j_candidate) {
  jclass j_candidate_class = GetObjectClass(jni, j_candidate);
  jfieldID j_sdp_mid_id =
      GetFieldID(jni, j_candidate_class, "sdpMid", "Ljava/lang/String;");
  std::string sdp_mid =
      JavaToStdString(jni, GetStringField(jni, j_candidate, j_sdp_mid_id));
  jfieldID j_sdp_id =
      GetFieldID(jni, j_candidate_class, "sdp", "Ljava/lang/String;");
  std::string sdp =
      JavaToStdString(jni, GetStringField(jni, j_candidate, j_sdp_id));
  cricket::Candidate candidate;
  if (!SdpDeserializeCandidate(sdp_mid, sdp, &candidate, NULL)) {
    LOG(LS_ERROR) << "SdpDescrializeCandidate failed with sdp " << sdp;
  }
  return candidate;
}

jobject NativeToJavaCandidate(JNIEnv* jni,
                              jclass* candidate_class,
                              const cricket::Candidate& candidate) {
  std::string sdp = SdpSerializeCandidate(candidate);
  RTC_CHECK(!sdp.empty()) << "got an empty ICE candidate";
  jmethodID ctor = GetMethodID(jni, *candidate_class, "<init>",
                               "(Ljava/lang/String;ILjava/lang/String;)V");
  jstring j_mid = JavaStringFromStdString(jni, candidate.transport_name());
  jstring j_sdp = JavaStringFromStdString(jni, sdp);
  // sdp_mline_index is not used, pass an invalid value -1.
  jobject j_candidate =
      jni->NewObject(*candidate_class, ctor, j_mid, -1, j_sdp);
  CHECK_EXCEPTION(jni) << "error during Java Candidate NewObject";
  return j_candidate;
}

jobjectArray NativeToJavaCandidateArray(
    JNIEnv* jni,
    const std::vector<cricket::Candidate>& candidates) {
  jclass candidate_class = FindClass(jni, "org/webrtc/IceCandidate");
  jobjectArray java_candidates =
      jni->NewObjectArray(candidates.size(), candidate_class, NULL);
  int i = 0;
  for (const cricket::Candidate& candidate : candidates) {
    jobject j_candidate =
        NativeToJavaCandidate(jni, &candidate_class, candidate);
    jni->SetObjectArrayElement(java_candidates, i++, j_candidate);
  }
  return java_candidates;
}

SessionDescriptionInterface* JavaToNativeSessionDescription(JNIEnv* jni,
                                                            jobject j_sdp) {
  jfieldID j_type_id = GetFieldID(jni, GetObjectClass(jni, j_sdp), "type",
                                  "Lorg/webrtc/SessionDescription$Type;");
  jobject j_type = GetObjectField(jni, j_sdp, j_type_id);
  jmethodID j_canonical_form_id =
      GetMethodID(jni, GetObjectClass(jni, j_type), "canonicalForm",
                  "()Ljava/lang/String;");
  jstring j_type_string =
      (jstring)jni->CallObjectMethod(j_type, j_canonical_form_id);
  CHECK_EXCEPTION(jni) << "error during CallObjectMethod";
  std::string std_type = JavaToStdString(jni, j_type_string);

  jfieldID j_description_id = GetFieldID(jni, GetObjectClass(jni, j_sdp),
                                         "description", "Ljava/lang/String;");
  jstring j_description = (jstring)GetObjectField(jni, j_sdp, j_description_id);
  std::string std_description = JavaToStdString(jni, j_description);

  return CreateSessionDescription(std_type, std_description, NULL);
}

jobject NativeToJavaSessionDescription(
    JNIEnv* jni,
    const SessionDescriptionInterface* desc) {
  std::string sdp;
  RTC_CHECK(desc->ToString(&sdp)) << "got so far: " << sdp;
  jstring j_description = JavaStringFromStdString(jni, sdp);

  jclass j_type_class = FindClass(jni, "org/webrtc/SessionDescription$Type");
  jmethodID j_type_from_canonical = GetStaticMethodID(
      jni, j_type_class, "fromCanonicalForm",
      "(Ljava/lang/String;)Lorg/webrtc/SessionDescription$Type;");
  jstring j_type_string = JavaStringFromStdString(jni, desc->type());
  jobject j_type = jni->CallStaticObjectMethod(
      j_type_class, j_type_from_canonical, j_type_string);
  CHECK_EXCEPTION(jni) << "error during CallObjectMethod";

  jclass j_sdp_class = FindClass(jni, "org/webrtc/SessionDescription");
  jmethodID j_sdp_ctor =
      GetMethodID(jni, j_sdp_class, "<init>",
                  "(Lorg/webrtc/SessionDescription$Type;Ljava/lang/String;)V");
  jobject j_sdp =
      jni->NewObject(j_sdp_class, j_sdp_ctor, j_type, j_description);
  CHECK_EXCEPTION(jni) << "error during NewObject";
  return j_sdp;
}

PeerConnectionFactoryInterface::Options
JavaToNativePeerConnectionFactoryOptions(JNIEnv* jni, jobject options) {
  jclass options_class = jni->GetObjectClass(options);
  jfieldID network_ignore_mask_field =
      jni->GetFieldID(options_class, "networkIgnoreMask", "I");
  int network_ignore_mask =
      jni->GetIntField(options, network_ignore_mask_field);

  jfieldID disable_encryption_field =
      jni->GetFieldID(options_class, "disableEncryption", "Z");
  bool disable_encryption =
      jni->GetBooleanField(options, disable_encryption_field);

  jfieldID disable_network_monitor_field =
      jni->GetFieldID(options_class, "disableNetworkMonitor", "Z");
  bool disable_network_monitor =
      jni->GetBooleanField(options, disable_network_monitor_field);

  PeerConnectionFactoryInterface::Options native_options;

  // This doesn't necessarily match the c++ version of this struct; feel free
  // to add more parameters as necessary.
  native_options.network_ignore_mask = network_ignore_mask;
  native_options.disable_encryption = disable_encryption;
  native_options.disable_network_monitor = disable_network_monitor;
  return native_options;
}

PeerConnectionInterface::IceTransportsType JavaToNativeIceTransportsType(
    JNIEnv* jni,
    jobject j_ice_transports_type) {
  std::string enum_name =
      GetJavaEnumName(jni, "org/webrtc/PeerConnection$IceTransportsType",
                      j_ice_transports_type);

  if (enum_name == "ALL")
    return PeerConnectionInterface::kAll;

  if (enum_name == "RELAY")
    return PeerConnectionInterface::kRelay;

  if (enum_name == "NOHOST")
    return PeerConnectionInterface::kNoHost;

  if (enum_name == "NONE")
    return PeerConnectionInterface::kNone;

  RTC_CHECK(false) << "Unexpected IceTransportsType enum_name " << enum_name;
  return PeerConnectionInterface::kAll;
}

PeerConnectionInterface::BundlePolicy JavaToNativeBundlePolicy(
    JNIEnv* jni,
    jobject j_bundle_policy) {
  std::string enum_name = GetJavaEnumName(
      jni, "org/webrtc/PeerConnection$BundlePolicy", j_bundle_policy);

  if (enum_name == "BALANCED")
    return PeerConnectionInterface::kBundlePolicyBalanced;

  if (enum_name == "MAXBUNDLE")
    return PeerConnectionInterface::kBundlePolicyMaxBundle;

  if (enum_name == "MAXCOMPAT")
    return PeerConnectionInterface::kBundlePolicyMaxCompat;

  RTC_CHECK(false) << "Unexpected BundlePolicy enum_name " << enum_name;
  return PeerConnectionInterface::kBundlePolicyBalanced;
}

PeerConnectionInterface::RtcpMuxPolicy JavaToNativeRtcpMuxPolicy(
    JNIEnv* jni,
    jobject j_rtcp_mux_policy) {
  std::string enum_name = GetJavaEnumName(
      jni, "org/webrtc/PeerConnection$RtcpMuxPolicy", j_rtcp_mux_policy);

  if (enum_name == "NEGOTIATE")
    return PeerConnectionInterface::kRtcpMuxPolicyNegotiate;

  if (enum_name == "REQUIRE")
    return PeerConnectionInterface::kRtcpMuxPolicyRequire;

  RTC_CHECK(false) << "Unexpected RtcpMuxPolicy enum_name " << enum_name;
  return PeerConnectionInterface::kRtcpMuxPolicyNegotiate;
}

PeerConnectionInterface::TcpCandidatePolicy JavaToNativeTcpCandidatePolicy(
    JNIEnv* jni,
    jobject j_tcp_candidate_policy) {
  std::string enum_name =
      GetJavaEnumName(jni, "org/webrtc/PeerConnection$TcpCandidatePolicy",
                      j_tcp_candidate_policy);

  if (enum_name == "ENABLED")
    return PeerConnectionInterface::kTcpCandidatePolicyEnabled;

  if (enum_name == "DISABLED")
    return PeerConnectionInterface::kTcpCandidatePolicyDisabled;

  RTC_CHECK(false) << "Unexpected TcpCandidatePolicy enum_name " << enum_name;
  return PeerConnectionInterface::kTcpCandidatePolicyEnabled;
}

PeerConnectionInterface::CandidateNetworkPolicy
JavaToNativeCandidateNetworkPolicy(JNIEnv* jni,
                                   jobject j_candidate_network_policy) {
  std::string enum_name =
      GetJavaEnumName(jni, "org/webrtc/PeerConnection$CandidateNetworkPolicy",
                      j_candidate_network_policy);

  if (enum_name == "ALL")
    return PeerConnectionInterface::kCandidateNetworkPolicyAll;

  if (enum_name == "LOW_COST")
    return PeerConnectionInterface::kCandidateNetworkPolicyLowCost;

  RTC_CHECK(false) << "Unexpected CandidateNetworkPolicy enum_name "
                   << enum_name;
  return PeerConnectionInterface::kCandidateNetworkPolicyAll;
}

rtc::KeyType JavaToNativeKeyType(JNIEnv* jni, jobject j_key_type) {
  std::string enum_name =
      GetJavaEnumName(jni, "org/webrtc/PeerConnection$KeyType", j_key_type);

  if (enum_name == "RSA")
    return rtc::KT_RSA;
  if (enum_name == "ECDSA")
    return rtc::KT_ECDSA;

  RTC_CHECK(false) << "Unexpected KeyType enum_name " << enum_name;
  return rtc::KT_ECDSA;
}

PeerConnectionInterface::ContinualGatheringPolicy
JavaToNativeContinualGatheringPolicy(JNIEnv* jni, jobject j_gathering_policy) {
  std::string enum_name =
      GetJavaEnumName(jni, "org/webrtc/PeerConnection$ContinualGatheringPolicy",
                      j_gathering_policy);
  if (enum_name == "GATHER_ONCE")
    return PeerConnectionInterface::GATHER_ONCE;

  if (enum_name == "GATHER_CONTINUALLY")
    return PeerConnectionInterface::GATHER_CONTINUALLY;

  RTC_CHECK(false) << "Unexpected ContinualGatheringPolicy enum name "
                   << enum_name;
  return PeerConnectionInterface::GATHER_ONCE;
}

PeerConnectionInterface::TlsCertPolicy JavaToNativeTlsCertPolicy(
    JNIEnv* jni,
    jobject j_ice_server_tls_cert_policy) {
  std::string enum_name =
      GetJavaEnumName(jni, "org/webrtc/PeerConnection$TlsCertPolicy",
                      j_ice_server_tls_cert_policy);

  if (enum_name == "TLS_CERT_POLICY_SECURE")
    return PeerConnectionInterface::kTlsCertPolicySecure;

  if (enum_name == "TLS_CERT_POLICY_INSECURE_NO_CHECK")
    return PeerConnectionInterface::kTlsCertPolicyInsecureNoCheck;

  RTC_CHECK(false) << "Unexpected TlsCertPolicy enum_name " << enum_name;
  return PeerConnectionInterface::kTlsCertPolicySecure;
}

void JavaToNativeIceServers(JNIEnv* jni,
                            jobject j_ice_servers,
                            PeerConnectionInterface::IceServers* ice_servers) {
  for (jobject j_ice_server : Iterable(jni, j_ice_servers)) {
    jclass j_ice_server_class = GetObjectClass(jni, j_ice_server);
    jfieldID j_ice_server_uri_id =
        GetFieldID(jni, j_ice_server_class, "uri", "Ljava/lang/String;");
    jfieldID j_ice_server_username_id =
        GetFieldID(jni, j_ice_server_class, "username", "Ljava/lang/String;");
    jfieldID j_ice_server_password_id =
        GetFieldID(jni, j_ice_server_class, "password", "Ljava/lang/String;");
    jfieldID j_ice_server_tls_cert_policy_id =
        GetFieldID(jni, j_ice_server_class, "tlsCertPolicy",
                   "Lorg/webrtc/PeerConnection$TlsCertPolicy;");
    jobject j_ice_server_tls_cert_policy =
        GetObjectField(jni, j_ice_server, j_ice_server_tls_cert_policy_id);
    jfieldID j_ice_server_hostname_id =
        GetFieldID(jni, j_ice_server_class, "hostname", "Ljava/lang/String;");
    jfieldID j_ice_server_tls_alpn_protocols_id = GetFieldID(
        jni, j_ice_server_class, "tlsAlpnProtocols", "Ljava/util/List;");
    jfieldID j_ice_server_tls_elliptic_curves_id = GetFieldID(
        jni, j_ice_server_class, "tlsEllipticCurves", "Ljava/util/List;");
    jstring uri = reinterpret_cast<jstring>(
        GetObjectField(jni, j_ice_server, j_ice_server_uri_id));
    jstring username = reinterpret_cast<jstring>(
        GetObjectField(jni, j_ice_server, j_ice_server_username_id));
    jstring password = reinterpret_cast<jstring>(
        GetObjectField(jni, j_ice_server, j_ice_server_password_id));
    PeerConnectionInterface::TlsCertPolicy tls_cert_policy =
        JavaToNativeTlsCertPolicy(jni, j_ice_server_tls_cert_policy);
    jstring hostname = reinterpret_cast<jstring>(
        GetObjectField(jni, j_ice_server, j_ice_server_hostname_id));
    jobject tls_alpn_protocols = GetNullableObjectField(
        jni, j_ice_server, j_ice_server_tls_alpn_protocols_id);
    jobject tls_elliptic_curves = GetNullableObjectField(
        jni, j_ice_server, j_ice_server_tls_elliptic_curves_id);
    PeerConnectionInterface::IceServer server;
    server.uri = JavaToStdString(jni, uri);
    server.username = JavaToStdString(jni, username);
    server.password = JavaToStdString(jni, password);
    server.tls_cert_policy = tls_cert_policy;
    server.hostname = JavaToStdString(jni, hostname);
    server.tls_alpn_protocols = JavaToStdVectorStrings(jni, tls_alpn_protocols);
    server.tls_elliptic_curves =
        JavaToStdVectorStrings(jni, tls_elliptic_curves);
    ice_servers->push_back(server);
  }
}

void JavaToNativeRTCConfiguration(
    JNIEnv* jni,
    jobject j_rtc_config,
    PeerConnectionInterface::RTCConfiguration* rtc_config) {
  jclass j_rtc_config_class = GetObjectClass(jni, j_rtc_config);

  jfieldID j_ice_transports_type_id =
      GetFieldID(jni, j_rtc_config_class, "iceTransportsType",
                 "Lorg/webrtc/PeerConnection$IceTransportsType;");
  jobject j_ice_transports_type =
      GetObjectField(jni, j_rtc_config, j_ice_transports_type_id);

  jfieldID j_bundle_policy_id =
      GetFieldID(jni, j_rtc_config_class, "bundlePolicy",
                 "Lorg/webrtc/PeerConnection$BundlePolicy;");
  jobject j_bundle_policy =
      GetObjectField(jni, j_rtc_config, j_bundle_policy_id);

  jfieldID j_rtcp_mux_policy_id =
      GetFieldID(jni, j_rtc_config_class, "rtcpMuxPolicy",
                 "Lorg/webrtc/PeerConnection$RtcpMuxPolicy;");
  jobject j_rtcp_mux_policy =
      GetObjectField(jni, j_rtc_config, j_rtcp_mux_policy_id);

  jfieldID j_tcp_candidate_policy_id =
      GetFieldID(jni, j_rtc_config_class, "tcpCandidatePolicy",
                 "Lorg/webrtc/PeerConnection$TcpCandidatePolicy;");
  jobject j_tcp_candidate_policy =
      GetObjectField(jni, j_rtc_config, j_tcp_candidate_policy_id);

  jfieldID j_candidate_network_policy_id =
      GetFieldID(jni, j_rtc_config_class, "candidateNetworkPolicy",
                 "Lorg/webrtc/PeerConnection$CandidateNetworkPolicy;");
  jobject j_candidate_network_policy =
      GetObjectField(jni, j_rtc_config, j_candidate_network_policy_id);

  jfieldID j_ice_servers_id =
      GetFieldID(jni, j_rtc_config_class, "iceServers", "Ljava/util/List;");
  jobject j_ice_servers = GetObjectField(jni, j_rtc_config, j_ice_servers_id);

  jfieldID j_audio_jitter_buffer_max_packets_id =
      GetFieldID(jni, j_rtc_config_class, "audioJitterBufferMaxPackets", "I");
  jfieldID j_audio_jitter_buffer_fast_accelerate_id = GetFieldID(
      jni, j_rtc_config_class, "audioJitterBufferFastAccelerate", "Z");

  jfieldID j_ice_connection_receiving_timeout_id =
      GetFieldID(jni, j_rtc_config_class, "iceConnectionReceivingTimeout", "I");

  jfieldID j_ice_backup_candidate_pair_ping_interval_id = GetFieldID(
      jni, j_rtc_config_class, "iceBackupCandidatePairPingInterval", "I");

  jfieldID j_continual_gathering_policy_id =
      GetFieldID(jni, j_rtc_config_class, "continualGatheringPolicy",
                 "Lorg/webrtc/PeerConnection$ContinualGatheringPolicy;");
  jobject j_continual_gathering_policy =
      GetObjectField(jni, j_rtc_config, j_continual_gathering_policy_id);

  jfieldID j_ice_candidate_pool_size_id =
      GetFieldID(jni, j_rtc_config_class, "iceCandidatePoolSize", "I");
  jfieldID j_presume_writable_when_fully_relayed_id = GetFieldID(
      jni, j_rtc_config_class, "presumeWritableWhenFullyRelayed", "Z");

  jfieldID j_prune_turn_ports_id =
      GetFieldID(jni, j_rtc_config_class, "pruneTurnPorts", "Z");

  jfieldID j_ice_check_min_interval_id = GetFieldID(
      jni, j_rtc_config_class, "iceCheckMinInterval", "Ljava/lang/Integer;");
  jclass j_integer_class = jni->FindClass("java/lang/Integer");
  jmethodID int_value_id = GetMethodID(jni, j_integer_class, "intValue", "()I");

  jfieldID j_disable_ipv6_on_wifi_id =
      GetFieldID(jni, j_rtc_config_class, "disableIPv6OnWifi", "Z");

  jfieldID j_max_ipv6_networks_id =
      GetFieldID(jni, j_rtc_config_class, "maxIPv6Networks", "I");

  jfieldID j_ice_regather_interval_range_id =
      GetFieldID(jni, j_rtc_config_class, "iceRegatherIntervalRange",
                 "Lorg/webrtc/PeerConnection$IntervalRange;");
  jclass j_interval_range_class =
      jni->FindClass("org/webrtc/PeerConnection$IntervalRange");
  jmethodID get_min_id =
      GetMethodID(jni, j_interval_range_class, "getMin", "()I");
  jmethodID get_max_id =
      GetMethodID(jni, j_interval_range_class, "getMax", "()I");

  rtc_config->type = JavaToNativeIceTransportsType(jni, j_ice_transports_type);
  rtc_config->bundle_policy = JavaToNativeBundlePolicy(jni, j_bundle_policy);
  rtc_config->rtcp_mux_policy =
      JavaToNativeRtcpMuxPolicy(jni, j_rtcp_mux_policy);
  rtc_config->tcp_candidate_policy =
      JavaToNativeTcpCandidatePolicy(jni, j_tcp_candidate_policy);
  rtc_config->candidate_network_policy =
      JavaToNativeCandidateNetworkPolicy(jni, j_candidate_network_policy);
  JavaToNativeIceServers(jni, j_ice_servers, &rtc_config->servers);
  rtc_config->audio_jitter_buffer_max_packets =
      GetIntField(jni, j_rtc_config, j_audio_jitter_buffer_max_packets_id);
  rtc_config->audio_jitter_buffer_fast_accelerate = GetBooleanField(
      jni, j_rtc_config, j_audio_jitter_buffer_fast_accelerate_id);
  rtc_config->ice_connection_receiving_timeout =
      GetIntField(jni, j_rtc_config, j_ice_connection_receiving_timeout_id);
  rtc_config->ice_backup_candidate_pair_ping_interval = GetIntField(
      jni, j_rtc_config, j_ice_backup_candidate_pair_ping_interval_id);
  rtc_config->continual_gathering_policy =
      JavaToNativeContinualGatheringPolicy(jni, j_continual_gathering_policy);
  rtc_config->ice_candidate_pool_size =
      GetIntField(jni, j_rtc_config, j_ice_candidate_pool_size_id);
  rtc_config->prune_turn_ports =
      GetBooleanField(jni, j_rtc_config, j_prune_turn_ports_id);
  rtc_config->presume_writable_when_fully_relayed = GetBooleanField(
      jni, j_rtc_config, j_presume_writable_when_fully_relayed_id);
  jobject j_ice_check_min_interval =
      GetNullableObjectField(jni, j_rtc_config, j_ice_check_min_interval_id);
  if (!IsNull(jni, j_ice_check_min_interval)) {
    int ice_check_min_interval_value =
        jni->CallIntMethod(j_ice_check_min_interval, int_value_id);
    rtc_config->ice_check_min_interval =
        rtc::Optional<int>(ice_check_min_interval_value);
  }
  rtc_config->disable_ipv6_on_wifi =
      GetBooleanField(jni, j_rtc_config, j_disable_ipv6_on_wifi_id);
  rtc_config->max_ipv6_networks =
      GetIntField(jni, j_rtc_config, j_max_ipv6_networks_id);
  jobject j_ice_regather_interval_range = GetNullableObjectField(
      jni, j_rtc_config, j_ice_regather_interval_range_id);
  if (!IsNull(jni, j_ice_regather_interval_range)) {
    int min = jni->CallIntMethod(j_ice_regather_interval_range, get_min_id);
    int max = jni->CallIntMethod(j_ice_regather_interval_range, get_max_id);
    rtc_config->ice_regather_interval_range.emplace(min, max);
  }
}

void JavaToNativeRtpParameters(JNIEnv* jni,
                               jobject j_parameters,
                               RtpParameters* parameters) {
  RTC_CHECK(parameters != nullptr);
  jclass parameters_class = jni->FindClass("org/webrtc/RtpParameters");
  jfieldID encodings_id =
      GetFieldID(jni, parameters_class, "encodings", "Ljava/util/LinkedList;");
  jfieldID codecs_id =
      GetFieldID(jni, parameters_class, "codecs", "Ljava/util/LinkedList;");

  // Convert encodings.
  jobject j_encodings = GetObjectField(jni, j_parameters, encodings_id);
  jclass j_encoding_parameters_class =
      jni->FindClass("org/webrtc/RtpParameters$Encoding");
  jfieldID active_id =
      GetFieldID(jni, j_encoding_parameters_class, "active", "Z");
  jfieldID bitrate_id = GetFieldID(jni, j_encoding_parameters_class,
                                   "maxBitrateBps", "Ljava/lang/Integer;");
  jfieldID ssrc_id =
      GetFieldID(jni, j_encoding_parameters_class, "ssrc", "Ljava/lang/Long;");
  jclass j_integer_class = jni->FindClass("java/lang/Integer");
  jclass j_long_class = jni->FindClass("java/lang/Long");
  jmethodID int_value_id = GetMethodID(jni, j_integer_class, "intValue", "()I");
  jmethodID long_value_id = GetMethodID(jni, j_long_class, "longValue", "()J");

  for (jobject j_encoding_parameters : Iterable(jni, j_encodings)) {
    RtpEncodingParameters encoding;
    encoding.active = GetBooleanField(jni, j_encoding_parameters, active_id);
    jobject j_bitrate =
        GetNullableObjectField(jni, j_encoding_parameters, bitrate_id);
    if (!IsNull(jni, j_bitrate)) {
      int bitrate_value = jni->CallIntMethod(j_bitrate, int_value_id);
      CHECK_EXCEPTION(jni) << "error during CallIntMethod";
      encoding.max_bitrate_bps = rtc::Optional<int>(bitrate_value);
    }
    jobject j_ssrc =
        GetNullableObjectField(jni, j_encoding_parameters, ssrc_id);
    if (!IsNull(jni, j_ssrc)) {
      jlong ssrc_value = jni->CallLongMethod(j_ssrc, long_value_id);
      CHECK_EXCEPTION(jni) << "error during CallLongMethod";
      encoding.ssrc = rtc::Optional<uint32_t>(ssrc_value);
    }
    parameters->encodings.push_back(encoding);
  }

  // Convert codecs.
  jobject j_codecs = GetObjectField(jni, j_parameters, codecs_id);
  jclass codec_class = jni->FindClass("org/webrtc/RtpParameters$Codec");
  jfieldID payload_type_id = GetFieldID(jni, codec_class, "payloadType", "I");
  jfieldID name_id = GetFieldID(jni, codec_class, "name", "Ljava/lang/String;");
  jfieldID kind_id = GetFieldID(jni, codec_class, "kind",
                                "Lorg/webrtc/MediaStreamTrack$MediaType;");
  jfieldID clock_rate_id =
      GetFieldID(jni, codec_class, "clockRate", "Ljava/lang/Integer;");
  jfieldID num_channels_id =
      GetFieldID(jni, codec_class, "numChannels", "Ljava/lang/Integer;");

  for (jobject j_codec : Iterable(jni, j_codecs)) {
    RtpCodecParameters codec;
    codec.payload_type = GetIntField(jni, j_codec, payload_type_id);
    codec.name = JavaToStdString(jni, GetStringField(jni, j_codec, name_id));
    codec.kind =
        JavaToNativeMediaType(jni, GetObjectField(jni, j_codec, kind_id));
    jobject j_clock_rate = GetNullableObjectField(jni, j_codec, clock_rate_id);
    if (!IsNull(jni, j_clock_rate)) {
      int clock_rate_value = jni->CallIntMethod(j_clock_rate, int_value_id);
      CHECK_EXCEPTION(jni) << "error during CallIntMethod";
      codec.clock_rate = rtc::Optional<int>(clock_rate_value);
    }
    jobject j_num_channels =
        GetNullableObjectField(jni, j_codec, num_channels_id);
    if (!IsNull(jni, j_num_channels)) {
      int num_channels_value = jni->CallIntMethod(j_num_channels, int_value_id);
      CHECK_EXCEPTION(jni) << "error during CallIntMethod";
      codec.num_channels = rtc::Optional<int>(num_channels_value);
    }
    parameters->codecs.push_back(codec);
  }
}

jobject NativeToJavaRtpParameters(JNIEnv* jni,
                                  const RtpParameters& parameters) {
  jclass parameters_class = jni->FindClass("org/webrtc/RtpParameters");
  jmethodID parameters_ctor =
      GetMethodID(jni, parameters_class, "<init>", "()V");
  jobject j_parameters = jni->NewObject(parameters_class, parameters_ctor);
  CHECK_EXCEPTION(jni) << "error during NewObject";

  // Add encodings.
  jclass encoding_class = jni->FindClass("org/webrtc/RtpParameters$Encoding");
  jmethodID encoding_ctor = GetMethodID(jni, encoding_class, "<init>", "()V");
  jfieldID encodings_id =
      GetFieldID(jni, parameters_class, "encodings", "Ljava/util/LinkedList;");
  jobject j_encodings = GetObjectField(jni, j_parameters, encodings_id);
  jmethodID encodings_add = GetMethodID(jni, GetObjectClass(jni, j_encodings),
                                        "add", "(Ljava/lang/Object;)Z");
  jfieldID active_id = GetFieldID(jni, encoding_class, "active", "Z");
  jfieldID bitrate_id =
      GetFieldID(jni, encoding_class, "maxBitrateBps", "Ljava/lang/Integer;");
  jfieldID ssrc_id =
      GetFieldID(jni, encoding_class, "ssrc", "Ljava/lang/Long;");

  jclass integer_class = jni->FindClass("java/lang/Integer");
  jclass long_class = jni->FindClass("java/lang/Long");
  jmethodID integer_ctor = GetMethodID(jni, integer_class, "<init>", "(I)V");
  jmethodID long_ctor = GetMethodID(jni, long_class, "<init>", "(J)V");

  for (const RtpEncodingParameters& encoding : parameters.encodings) {
    jobject j_encoding_parameters =
        jni->NewObject(encoding_class, encoding_ctor);
    CHECK_EXCEPTION(jni) << "error during NewObject";
    jni->SetBooleanField(j_encoding_parameters, active_id, encoding.active);
    CHECK_EXCEPTION(jni) << "error during SetBooleanField";
    if (encoding.max_bitrate_bps) {
      jobject j_bitrate_value = jni->NewObject(integer_class, integer_ctor,
                                               *(encoding.max_bitrate_bps));
      CHECK_EXCEPTION(jni) << "error during NewObject";
      jni->SetObjectField(j_encoding_parameters, bitrate_id, j_bitrate_value);
      CHECK_EXCEPTION(jni) << "error during SetObjectField";
    }
    if (encoding.ssrc) {
      jobject j_ssrc_value = jni->NewObject(long_class, long_ctor,
                                            static_cast<jlong>(*encoding.ssrc));
      CHECK_EXCEPTION(jni) << "error during NewObject";
      jni->SetObjectField(j_encoding_parameters, ssrc_id, j_ssrc_value);
      CHECK_EXCEPTION(jni) << "error during SetObjectField";
    }
    jboolean added = jni->CallBooleanMethod(j_encodings, encodings_add,
                                            j_encoding_parameters);
    CHECK_EXCEPTION(jni) << "error during CallBooleanMethod";
    RTC_CHECK(added);
  }

  // Add codecs.
  jclass codec_class = jni->FindClass("org/webrtc/RtpParameters$Codec");
  jmethodID codec_ctor = GetMethodID(jni, codec_class, "<init>", "()V");
  jfieldID codecs_id =
      GetFieldID(jni, parameters_class, "codecs", "Ljava/util/LinkedList;");
  jobject j_codecs = GetObjectField(jni, j_parameters, codecs_id);
  jmethodID codecs_add = GetMethodID(jni, GetObjectClass(jni, j_codecs), "add",
                                     "(Ljava/lang/Object;)Z");
  jfieldID payload_type_id = GetFieldID(jni, codec_class, "payloadType", "I");
  jfieldID name_id = GetFieldID(jni, codec_class, "name", "Ljava/lang/String;");
  jfieldID kind_id = GetFieldID(jni, codec_class, "kind",
                                "Lorg/webrtc/MediaStreamTrack$MediaType;");
  jfieldID clock_rate_id =
      GetFieldID(jni, codec_class, "clockRate", "Ljava/lang/Integer;");
  jfieldID num_channels_id =
      GetFieldID(jni, codec_class, "numChannels", "Ljava/lang/Integer;");

  for (const RtpCodecParameters& codec : parameters.codecs) {
    jobject j_codec = jni->NewObject(codec_class, codec_ctor);
    CHECK_EXCEPTION(jni) << "error during NewObject";
    jni->SetIntField(j_codec, payload_type_id, codec.payload_type);
    CHECK_EXCEPTION(jni) << "error during SetIntField";
    jni->SetObjectField(j_codec, name_id,
                        JavaStringFromStdString(jni, codec.name));
    CHECK_EXCEPTION(jni) << "error during SetObjectField";
    jni->SetObjectField(j_codec, kind_id,
                        NativeToJavaMediaType(jni, codec.kind));
    CHECK_EXCEPTION(jni) << "error during SetObjectField";
    if (codec.clock_rate) {
      jobject j_clock_rate_value =
          jni->NewObject(integer_class, integer_ctor, *(codec.clock_rate));
      CHECK_EXCEPTION(jni) << "error during NewObject";
      jni->SetObjectField(j_codec, clock_rate_id, j_clock_rate_value);
      CHECK_EXCEPTION(jni) << "error during SetObjectField";
    }
    if (codec.num_channels) {
      jobject j_num_channels_value =
          jni->NewObject(integer_class, integer_ctor, *(codec.num_channels));
      CHECK_EXCEPTION(jni) << "error during NewObject";
      jni->SetObjectField(j_codec, num_channels_id, j_num_channels_value);
      CHECK_EXCEPTION(jni) << "error during SetObjectField";
    }
    jboolean added = jni->CallBooleanMethod(j_codecs, codecs_add, j_codec);
    CHECK_EXCEPTION(jni) << "error during CallBooleanMethod";
    RTC_CHECK(added);
  }

  return j_parameters;
}

}  // namespace jni
}  // namespace webrtc
