/*
 *  Copyright 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "sdk/android/src/jni/pc/java_native_conversion.h"

#include <string>

#include "pc/webrtcsdp.h"
#include "sdk/android/generated_peerconnection_jni/jni/IceCandidate_jni.h"
#include "sdk/android/generated_peerconnection_jni/jni/MediaStreamTrack_jni.h"
#include "sdk/android/src/jni/classreferenceholder.h"

namespace webrtc {
namespace jni {

namespace {

jobject CreateJavaIceCandidate(JNIEnv* env,
                               const std::string& sdp_mid,
                               int sdp_mline_index,
                               const std::string& sdp,
                               const std::string server_url) {
  return Java_IceCandidate_Constructor(
      env, NativeToJavaString(env, sdp_mid), sdp_mline_index,
      NativeToJavaString(env, sdp), NativeToJavaString(env, server_url));
}

}  // namespace

jobject NativeToJavaMediaType(JNIEnv* jni, cricket::MediaType media_type) {
  return Java_MediaType_fromNativeIndex(jni, media_type);
}

cricket::MediaType JavaToNativeMediaType(JNIEnv* jni, jobject j_media_type) {
  return static_cast<cricket::MediaType>(
      Java_MediaType_getNative(jni, j_media_type));
}

cricket::Candidate JavaToNativeCandidate(JNIEnv* jni, jobject j_candidate) {
  std::string sdp_mid =
      JavaToStdString(jni, Java_IceCandidate_getSdpMid(jni, j_candidate));
  std::string sdp =
      JavaToStdString(jni, Java_IceCandidate_getSdp(jni, j_candidate));
  cricket::Candidate candidate;
  if (!SdpDeserializeCandidate(sdp_mid, sdp, &candidate, NULL)) {
    RTC_LOG(LS_ERROR) << "SdpDescrializeCandidate failed with sdp " << sdp;
  }
  return candidate;
}

jobject NativeToJavaCandidate(JNIEnv* env,
                              const cricket::Candidate& candidate) {
  std::string sdp = SdpSerializeCandidate(candidate);
  RTC_CHECK(!sdp.empty()) << "got an empty ICE candidate";
  // sdp_mline_index is not used, pass an invalid value -1.
  return CreateJavaIceCandidate(env, candidate.transport_name(),
                                -1 /* sdp_mline_index */, sdp,
                                "" /* server_url */);
}

jobject NativeToJavaIceCandidate(JNIEnv* env,
                                 const IceCandidateInterface& candidate) {
  std::string sdp;
  RTC_CHECK(candidate.ToString(&sdp)) << "got so far: " << sdp;
  return CreateJavaIceCandidate(env, candidate.sdp_mid(),
                                candidate.sdp_mline_index(), sdp,
                                candidate.candidate().url());
}

jobjectArray NativeToJavaCandidateArray(
    JNIEnv* jni,
    const std::vector<cricket::Candidate>& candidates) {
  return NativeToJavaObjectArray(jni, candidates,
                                 org_webrtc_IceCandidate_clazz(jni),
                                 &NativeToJavaCandidate);
}

std::unique_ptr<SessionDescriptionInterface> JavaToNativeSessionDescription(
    JNIEnv* jni,
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
  rtc::Optional<SdpType> sdp_type_maybe = SdpTypeFromString(std_type);
  if (!sdp_type_maybe) {
    RTC_LOG(LS_ERROR) << "Unexpected SDP type: " << std_type;
    return nullptr;
  }

  jfieldID j_description_id = GetFieldID(jni, GetObjectClass(jni, j_sdp),
                                         "description", "Ljava/lang/String;");
  jstring j_description = (jstring)GetObjectField(jni, j_sdp, j_description_id);
  std::string std_description = JavaToStdString(jni, j_description);

  return CreateSessionDescription(*sdp_type_maybe, std_description);
}

jobject NativeToJavaSessionDescription(
    JNIEnv* jni,
    const SessionDescriptionInterface* desc) {
  std::string sdp;
  RTC_CHECK(desc->ToString(&sdp)) << "got so far: " << sdp;
  jstring j_description = NativeToJavaString(jni, sdp);

  jclass j_type_class = FindClass(jni, "org/webrtc/SessionDescription$Type");
  jmethodID j_type_from_canonical = GetStaticMethodID(
      jni, j_type_class, "fromCanonicalForm",
      "(Ljava/lang/String;)Lorg/webrtc/SessionDescription$Type;");
  jstring j_type_string = NativeToJavaString(jni, desc->type());
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
  std::string enum_name = GetJavaEnumName(jni, j_ice_transports_type);

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
  std::string enum_name = GetJavaEnumName(jni, j_bundle_policy);

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
  std::string enum_name = GetJavaEnumName(jni, j_rtcp_mux_policy);

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
  std::string enum_name = GetJavaEnumName(jni, j_tcp_candidate_policy);

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
  std::string enum_name = GetJavaEnumName(jni, j_candidate_network_policy);

  if (enum_name == "ALL")
    return PeerConnectionInterface::kCandidateNetworkPolicyAll;

  if (enum_name == "LOW_COST")
    return PeerConnectionInterface::kCandidateNetworkPolicyLowCost;

  RTC_CHECK(false) << "Unexpected CandidateNetworkPolicy enum_name "
                   << enum_name;
  return PeerConnectionInterface::kCandidateNetworkPolicyAll;
}

rtc::KeyType JavaToNativeKeyType(JNIEnv* jni, jobject j_key_type) {
  std::string enum_name = GetJavaEnumName(jni, j_key_type);

  if (enum_name == "RSA")
    return rtc::KT_RSA;
  if (enum_name == "ECDSA")
    return rtc::KT_ECDSA;

  RTC_CHECK(false) << "Unexpected KeyType enum_name " << enum_name;
  return rtc::KT_ECDSA;
}

PeerConnectionInterface::ContinualGatheringPolicy
JavaToNativeContinualGatheringPolicy(JNIEnv* jni, jobject j_gathering_policy) {
  std::string enum_name = GetJavaEnumName(jni, j_gathering_policy);
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
  std::string enum_name = GetJavaEnumName(jni, j_ice_server_tls_cert_policy);

  if (enum_name == "TLS_CERT_POLICY_SECURE")
    return PeerConnectionInterface::kTlsCertPolicySecure;

  if (enum_name == "TLS_CERT_POLICY_INSECURE_NO_CHECK")
    return PeerConnectionInterface::kTlsCertPolicyInsecureNoCheck;

  RTC_CHECK(false) << "Unexpected TlsCertPolicy enum_name " << enum_name;
  return PeerConnectionInterface::kTlsCertPolicySecure;
}

RtpParameters JavaToNativeRtpParameters(JNIEnv* jni, jobject j_parameters) {
  RtpParameters parameters;
  jclass parameters_class = jni->FindClass("org/webrtc/RtpParameters");
  jfieldID encodings_id =
      GetFieldID(jni, parameters_class, "encodings", "Ljava/util/List;");
  jfieldID codecs_id =
      GetFieldID(jni, parameters_class, "codecs", "Ljava/util/List;");

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
  jclass j_long_class = jni->FindClass("java/lang/Long");
  jmethodID long_value_id = GetMethodID(jni, j_long_class, "longValue", "()J");

  for (jobject j_encoding_parameters : Iterable(jni, j_encodings)) {
    RtpEncodingParameters encoding;
    encoding.active = GetBooleanField(jni, j_encoding_parameters, active_id);
    jobject j_bitrate =
        GetNullableObjectField(jni, j_encoding_parameters, bitrate_id);
    encoding.max_bitrate_bps = JavaToNativeOptionalInt(jni, j_bitrate);
    jobject j_ssrc =
        GetNullableObjectField(jni, j_encoding_parameters, ssrc_id);
    if (!IsNull(jni, j_ssrc)) {
      jlong ssrc_value = jni->CallLongMethod(j_ssrc, long_value_id);
      CHECK_EXCEPTION(jni) << "error during CallLongMethod";
      encoding.ssrc = ssrc_value;
    }
    parameters.encodings.push_back(encoding);
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
    codec.clock_rate = JavaToNativeOptionalInt(jni, j_clock_rate);
    jobject j_num_channels =
        GetNullableObjectField(jni, j_codec, num_channels_id);
    codec.num_channels = JavaToNativeOptionalInt(jni, j_num_channels);
    parameters.codecs.push_back(codec);
  }
  return parameters;
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
      GetFieldID(jni, parameters_class, "encodings", "Ljava/util/List;");
  jobject j_encodings = GetObjectField(jni, j_parameters, encodings_id);
  jmethodID encodings_add = GetMethodID(jni, GetObjectClass(jni, j_encodings),
                                        "add", "(Ljava/lang/Object;)Z");
  jfieldID active_id = GetFieldID(jni, encoding_class, "active", "Z");
  jfieldID bitrate_id =
      GetFieldID(jni, encoding_class, "maxBitrateBps", "Ljava/lang/Integer;");
  jfieldID ssrc_id =
      GetFieldID(jni, encoding_class, "ssrc", "Ljava/lang/Long;");

  jclass long_class = jni->FindClass("java/lang/Long");
  jmethodID long_ctor = GetMethodID(jni, long_class, "<init>", "(J)V");

  for (const RtpEncodingParameters& encoding : parameters.encodings) {
    jobject j_encoding_parameters =
        jni->NewObject(encoding_class, encoding_ctor);
    CHECK_EXCEPTION(jni) << "error during NewObject";
    jni->SetBooleanField(j_encoding_parameters, active_id, encoding.active);
    CHECK_EXCEPTION(jni) << "error during SetBooleanField";
    jni->SetObjectField(j_encoding_parameters, bitrate_id,
                        NativeToJavaInteger(jni, encoding.max_bitrate_bps));
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
      GetFieldID(jni, parameters_class, "codecs", "Ljava/util/List;");
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
    jni->SetObjectField(j_codec, name_id, NativeToJavaString(jni, codec.name));
    CHECK_EXCEPTION(jni) << "error during SetObjectField";
    jni->SetObjectField(j_codec, kind_id,
                        NativeToJavaMediaType(jni, codec.kind));
    CHECK_EXCEPTION(jni) << "error during SetObjectField";
    jni->SetObjectField(j_codec, clock_rate_id,
                        NativeToJavaInteger(jni, codec.clock_rate));
    jni->SetObjectField(j_codec, num_channels_id,
                        NativeToJavaInteger(jni, codec.num_channels));
    jboolean added = jni->CallBooleanMethod(j_codecs, codecs_add, j_codec);
    CHECK_EXCEPTION(jni) << "error during CallBooleanMethod";
    RTC_CHECK(added);
  }

  return j_parameters;
}

}  // namespace jni
}  // namespace webrtc
