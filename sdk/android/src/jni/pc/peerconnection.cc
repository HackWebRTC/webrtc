/*
 *  Copyright 2013 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

// Lifecycle notes: objects are owned where they will be called; in other words
// FooObservers are owned by C++-land, and user-callable objects (e.g.
// PeerConnection and VideoTrack) are owned by Java-land.
// When this file (or other files in this directory) allocates C++
// RefCountInterfaces it AddRef()s an artificial ref simulating the jlong held
// in Java-land, and then Release()s the ref in the respective free call.
// Sometimes this AddRef is implicit in the construction of a scoped_refptr<>
// which is then .release()d. Any persistent (non-local) references from C++ to
// Java must be global or weak (in which case they must be checked before use)!
//
// Exception notes: pretty much all JNI calls can throw Java exceptions, so each
// call through a JNIEnv* pointer needs to be followed by an ExceptionCheck()
// call. In this file this is done in CHECK_EXCEPTION, making for much easier
// debugging in case of failure (the alternative is to wait for control to
// return to the Java frame that called code in this file, at which point it's
// impossible to tell which JNI call broke).

#include "sdk/android/src/jni/pc/peerconnection.h"

#include <limits>
#include <memory>
#include <string>
#include <utility>

#include "api/mediaconstraintsinterface.h"
#include "api/peerconnectioninterface.h"
#include "api/rtpreceiverinterface.h"
#include "api/rtpsenderinterface.h"
#include "rtc_base/checks.h"
#include "rtc_base/logging.h"
#include "rtc_base/ptr_util.h"
#include "sdk/android/generated_peerconnection_jni/jni/MediaStream_jni.h"
#include "sdk/android/generated_peerconnection_jni/jni/PeerConnection_jni.h"
#include "sdk/android/generated_peerconnection_jni/jni/RtpSender_jni.h"
#include "sdk/android/src/jni/classreferenceholder.h"
#include "sdk/android/src/jni/jni_helpers.h"
#include "sdk/android/src/jni/pc/datachannel.h"
#include "sdk/android/src/jni/pc/java_native_conversion.h"
#include "sdk/android/src/jni/pc/mediaconstraints_jni.h"
#include "sdk/android/src/jni/pc/rtcstatscollectorcallbackwrapper.h"
#include "sdk/android/src/jni/pc/sdpobserver_jni.h"
#include "sdk/android/src/jni/pc/statsobserver_jni.h"

namespace webrtc {
namespace jni {

namespace {

rtc::scoped_refptr<PeerConnectionInterface> ExtractNativePC(JNIEnv* jni,
                                                            jobject j_pc) {
  jfieldID native_pc_id =
      GetFieldID(jni, GetObjectClass(jni, j_pc), "nativePeerConnection", "J");
  jlong j_p = GetLongField(jni, j_pc, native_pc_id);
  return rtc::scoped_refptr<PeerConnectionInterface>(
      reinterpret_cast<PeerConnectionInterface*>(j_p));
}

jobject NativeToJavaRtpSender(JNIEnv* env,
                              rtc::scoped_refptr<RtpSenderInterface> sender) {
  if (!sender)
    return nullptr;
  // Sender is now owned by the Java object, and will be freed from
  // RtpSender.dispose(), called by PeerConnection.dispose() or getSenders().
  return Java_RtpSender_Constructor(env, jlongFromPointer(sender.release()));
}

// Convenience, used since callbacks occur on the signaling thread, which may
// be a non-Java thread.
JNIEnv* jni() {
  return AttachCurrentThreadIfNeeded();
}

}  // namespace

void JavaToNativeIceServers(JNIEnv* jni,
                            jobject j_ice_servers,
                            PeerConnectionInterface::IceServers* ice_servers) {
  for (jobject j_ice_server : Iterable(jni, j_ice_servers)) {
    jclass j_ice_server_class = GetObjectClass(jni, j_ice_server);
    jfieldID j_ice_server_urls_id =
        GetFieldID(jni, j_ice_server_class, "urls", "Ljava/util/List;");
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
    jobject urls = GetObjectField(jni, j_ice_server, j_ice_server_urls_id);
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
    server.urls = JavaToStdVectorStrings(jni, urls);
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

  jfieldID j_turn_customizer_type_id = GetFieldID(
      jni, j_rtc_config_class, "turnCustomizer", "Lorg/webrtc/TurnCustomizer;");
  jobject j_turn_customizer =
      GetNullableObjectField(jni, j_rtc_config, j_turn_customizer_type_id);

  jclass j_turn_customizer_class = jni->FindClass("org/webrtc/TurnCustomizer");
  jfieldID j_native_turn_customizer_id =
      GetFieldID(jni, j_turn_customizer_class, "nativeTurnCustomizer", "J");

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
  rtc_config->ice_check_min_interval =
      JavaToNativeOptionalInt(jni, j_ice_check_min_interval);
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

  if (!IsNull(jni, j_turn_customizer)) {
    rtc_config->turn_customizer = reinterpret_cast<webrtc::TurnCustomizer*>(
        GetLongField(jni, j_turn_customizer, j_native_turn_customizer_id));
  }
}

PeerConnectionObserverJni::PeerConnectionObserverJni(JNIEnv* jni,
                                                     jobject j_observer)
    : j_observer_global_(jni, j_observer) {}

PeerConnectionObserverJni::~PeerConnectionObserverJni() {
  ScopedLocalRefFrame local_ref_frame(jni());
  while (!remote_streams_.empty())
    DisposeRemoteStream(remote_streams_.begin());
}

void PeerConnectionObserverJni::OnIceCandidate(
    const IceCandidateInterface* candidate) {
  JNIEnv* env = AttachCurrentThreadIfNeeded();
  ScopedLocalRefFrame local_ref_frame(env);
  Java_Observer_onIceCandidate(env, *j_observer_global_,
                               NativeToJavaIceCandidate(env, *candidate));
}

void PeerConnectionObserverJni::OnIceCandidatesRemoved(
    const std::vector<cricket::Candidate>& candidates) {
  JNIEnv* env = AttachCurrentThreadIfNeeded();
  ScopedLocalRefFrame local_ref_frame(env);
  Java_Observer_onIceCandidatesRemoved(
      env, *j_observer_global_, NativeToJavaCandidateArray(env, candidates));
}

void PeerConnectionObserverJni::OnSignalingChange(
    PeerConnectionInterface::SignalingState new_state) {
  JNIEnv* env = AttachCurrentThreadIfNeeded();
  ScopedLocalRefFrame local_ref_frame(env);
  Java_Observer_onSignalingChange(
      env, *j_observer_global_,
      Java_SignalingState_fromNativeIndex(env, new_state));
}

void PeerConnectionObserverJni::OnIceConnectionChange(
    PeerConnectionInterface::IceConnectionState new_state) {
  JNIEnv* env = AttachCurrentThreadIfNeeded();
  ScopedLocalRefFrame local_ref_frame(env);
  Java_Observer_onIceConnectionChange(
      env, *j_observer_global_,
      Java_IceConnectionState_fromNativeIndex(env, new_state));
}

void PeerConnectionObserverJni::OnIceConnectionReceivingChange(bool receiving) {
  JNIEnv* env = AttachCurrentThreadIfNeeded();
  ScopedLocalRefFrame local_ref_frame(env);
  Java_Observer_onIceConnectionReceivingChange(env, *j_observer_global_,
                                               receiving);
}

void PeerConnectionObserverJni::OnIceGatheringChange(
    PeerConnectionInterface::IceGatheringState new_state) {
  JNIEnv* env = AttachCurrentThreadIfNeeded();
  ScopedLocalRefFrame local_ref_frame(env);
  Java_Observer_onIceGatheringChange(
      env, *j_observer_global_,
      Java_IceGatheringState_fromNativeIndex(env, new_state));
}

void PeerConnectionObserverJni::OnAddStream(
    rtc::scoped_refptr<MediaStreamInterface> stream) {
  JNIEnv* env = AttachCurrentThreadIfNeeded();
  ScopedLocalRefFrame local_ref_frame(env);
  // The stream could be added into the remote_streams_ map when calling
  // OnAddTrack.
  jobject j_stream = GetOrCreateJavaStream(stream);

  for (const auto& track : stream->GetAudioTracks()) {
    AddNativeAudioTrackToJavaStream(track, j_stream);
  }
  for (const auto& track : stream->GetVideoTracks()) {
    AddNativeVideoTrackToJavaStream(track, j_stream);
  }

  Java_Observer_onAddStream(env, *j_observer_global_, j_stream);

  // Create an observer to update the Java stream when the native stream's set
  // of tracks changes.
  auto observer = rtc::MakeUnique<MediaStreamObserver>(stream);
  observer->SignalAudioTrackRemoved.connect(
      this, &PeerConnectionObserverJni::OnAudioTrackRemovedFromStream);
  observer->SignalVideoTrackRemoved.connect(
      this, &PeerConnectionObserverJni::OnVideoTrackRemovedFromStream);
  observer->SignalAudioTrackAdded.connect(
      this, &PeerConnectionObserverJni::OnAudioTrackAddedToStream);
  observer->SignalVideoTrackAdded.connect(
      this, &PeerConnectionObserverJni::OnVideoTrackAddedToStream);
  stream_observers_.push_back(std::move(observer));
}

void PeerConnectionObserverJni::AddNativeAudioTrackToJavaStream(
    rtc::scoped_refptr<AudioTrackInterface> track,
    jobject j_stream) {
  JNIEnv* env = AttachCurrentThreadIfNeeded();
  Java_MediaStream_addNativeAudioTrack(env, j_stream,
                                       jlongFromPointer(track.release()));
}

void PeerConnectionObserverJni::AddNativeVideoTrackToJavaStream(
    rtc::scoped_refptr<VideoTrackInterface> track,
    jobject j_stream) {
  JNIEnv* env = AttachCurrentThreadIfNeeded();
  Java_MediaStream_addNativeVideoTrack(env, j_stream,
                                       jlongFromPointer(track.release()));
}

void PeerConnectionObserverJni::OnAudioTrackAddedToStream(
    AudioTrackInterface* track,
    MediaStreamInterface* stream) {
  ScopedLocalRefFrame local_ref_frame(jni());
  jobject j_stream = GetOrCreateJavaStream(stream);
  AddNativeAudioTrackToJavaStream(track, j_stream);
}

void PeerConnectionObserverJni::OnVideoTrackAddedToStream(
    VideoTrackInterface* track,
    MediaStreamInterface* stream) {
  ScopedLocalRefFrame local_ref_frame(jni());
  jobject j_stream = GetOrCreateJavaStream(stream);
  AddNativeVideoTrackToJavaStream(track, j_stream);
}

void PeerConnectionObserverJni::OnAudioTrackRemovedFromStream(
    AudioTrackInterface* track,
    MediaStreamInterface* stream) {
  JNIEnv* env = AttachCurrentThreadIfNeeded();
  ScopedLocalRefFrame local_ref_frame(env);
  jobject j_stream = GetOrCreateJavaStream(stream);
  Java_MediaStream_removeAudioTrack(env, j_stream, jlongFromPointer(track));
}

void PeerConnectionObserverJni::OnVideoTrackRemovedFromStream(
    VideoTrackInterface* track,
    MediaStreamInterface* stream) {
  JNIEnv* env = AttachCurrentThreadIfNeeded();
  ScopedLocalRefFrame local_ref_frame(env);
  jobject j_stream = GetOrCreateJavaStream(stream);
  Java_MediaStream_removeVideoTrack(env, j_stream, jlongFromPointer(track));
}

void PeerConnectionObserverJni::OnRemoveStream(
    rtc::scoped_refptr<MediaStreamInterface> stream) {
  JNIEnv* env = AttachCurrentThreadIfNeeded();
  ScopedLocalRefFrame local_ref_frame(env);
  NativeToJavaStreamsMap::iterator it = remote_streams_.find(stream);
  RTC_CHECK(it != remote_streams_.end())
      << "unexpected stream: " << std::hex << stream;
  Java_Observer_onRemoveStream(env, *j_observer_global_, it->second);

  // Release the refptr reference so that DisposeRemoteStream can assert
  // it removes the final reference.
  stream = nullptr;
  DisposeRemoteStream(it);
}

void PeerConnectionObserverJni::OnDataChannel(
    rtc::scoped_refptr<DataChannelInterface> channel) {
  JNIEnv* env = AttachCurrentThreadIfNeeded();
  ScopedLocalRefFrame local_ref_frame(env);
  Java_Observer_onDataChannel(env, *j_observer_global_,
                              WrapNativeDataChannel(env, channel));
}

void PeerConnectionObserverJni::OnRenegotiationNeeded() {
  JNIEnv* env = AttachCurrentThreadIfNeeded();
  ScopedLocalRefFrame local_ref_frame(env);
  Java_Observer_onRenegotiationNeeded(env, *j_observer_global_);
}

void PeerConnectionObserverJni::OnAddTrack(
    rtc::scoped_refptr<RtpReceiverInterface> receiver,
    const std::vector<rtc::scoped_refptr<MediaStreamInterface>>& streams) {
  JNIEnv* env = AttachCurrentThreadIfNeeded();
  ScopedLocalRefFrame local_ref_frame(env);
  jobject j_rtp_receiver = NativeToJavaRtpReceiver(env, receiver);
  rtp_receivers_.emplace_back(env, j_rtp_receiver);

  Java_Observer_onAddTrack(env, *j_observer_global_, j_rtp_receiver,
                           NativeToJavaMediaStreamArray(env, streams));
}

void PeerConnectionObserverJni::SetConstraints(
    std::unique_ptr<MediaConstraintsInterface> constraints) {
  RTC_CHECK(!constraints_.get()) << "constraints already set!";
  constraints_ = std::move(constraints);
}

void PeerConnectionObserverJni::DisposeRemoteStream(
    const NativeToJavaStreamsMap::iterator& it) {
  MediaStreamInterface* stream = it->first;
  jobject j_stream = it->second;

  // Remove the observer first, so it doesn't react to events during deletion.
  stream_observers_.erase(
      std::remove_if(
          stream_observers_.begin(), stream_observers_.end(),
          [stream](const std::unique_ptr<MediaStreamObserver>& observer) {
            return observer->stream() == stream;
          }),
      stream_observers_.end());

  remote_streams_.erase(it);
  JNIEnv* env = AttachCurrentThreadIfNeeded();
  Java_MediaStream_dispose(env, j_stream);
  DeleteGlobalRef(env, j_stream);
}

// If the NativeToJavaStreamsMap contains the stream, return it.
// Otherwise, create a new Java MediaStream.
jobject PeerConnectionObserverJni::GetOrCreateJavaStream(
    const rtc::scoped_refptr<MediaStreamInterface>& stream) {
  NativeToJavaStreamsMap::iterator it = remote_streams_.find(stream);
  if (it != remote_streams_.end()) {
    return it->second;
  }
  // Java MediaStream holds one reference. Corresponding Release() is in
  // MediaStream_free, triggered by MediaStream.dispose().
  stream->AddRef();
  jobject j_stream =
      Java_MediaStream_Constructor(jni(), jlongFromPointer(stream.get()));

  remote_streams_[stream] = NewGlobalRef(jni(), j_stream);
  return j_stream;
}

jobjectArray PeerConnectionObserverJni::NativeToJavaMediaStreamArray(
    JNIEnv* jni,
    const std::vector<rtc::scoped_refptr<MediaStreamInterface>>& streams) {
  jobjectArray java_streams = jni->NewObjectArray(
      streams.size(), org_webrtc_MediaStream_clazz(jni), nullptr);
  CHECK_EXCEPTION(jni) << "error during NewObjectArray";
  for (size_t i = 0; i < streams.size(); ++i) {
    jobject j_stream = GetOrCreateJavaStream(streams[i]);
    jni->SetObjectArrayElement(java_streams, i, j_stream);
  }
  return java_streams;
}

JNI_FUNCTION_DECLARATION(void,
                         PeerConnection_freeObserver,
                         JNIEnv*,
                         jclass,
                         jlong j_p) {
  PeerConnectionObserverJni* p =
      reinterpret_cast<PeerConnectionObserverJni*>(j_p);
  delete p;
}

JNI_FUNCTION_DECLARATION(jobject,
                         PeerConnection_getLocalDescription,
                         JNIEnv* jni,
                         jobject j_pc) {
  const SessionDescriptionInterface* sdp =
      ExtractNativePC(jni, j_pc)->local_description();
  return sdp ? NativeToJavaSessionDescription(jni, sdp) : NULL;
}

JNI_FUNCTION_DECLARATION(jobject,
                         PeerConnection_getRemoteDescription,
                         JNIEnv* jni,
                         jobject j_pc) {
  const SessionDescriptionInterface* sdp =
      ExtractNativePC(jni, j_pc)->remote_description();
  return sdp ? NativeToJavaSessionDescription(jni, sdp) : NULL;
}

JNI_FUNCTION_DECLARATION(jobject,
                         PeerConnection_createDataChannel,
                         JNIEnv* jni,
                         jobject j_pc,
                         jstring j_label,
                         jobject j_init) {
  DataChannelInit init = JavaToNativeDataChannelInit(jni, j_init);
  rtc::scoped_refptr<DataChannelInterface> channel(
      ExtractNativePC(jni, j_pc)->CreateDataChannel(
          JavaToStdString(jni, j_label), &init));
  return WrapNativeDataChannel(jni, channel);
}

JNI_FUNCTION_DECLARATION(void,
                         PeerConnection_createOffer,
                         JNIEnv* jni,
                         jobject j_pc,
                         jobject j_observer,
                         jobject j_constraints) {
  std::unique_ptr<MediaConstraintsInterface> constraints =
      JavaToNativeMediaConstraints(jni, j_constraints);
  rtc::scoped_refptr<CreateSdpObserverJni> observer(
      new rtc::RefCountedObject<CreateSdpObserverJni>(jni, j_observer,
                                                      std::move(constraints)));
  ExtractNativePC(jni, j_pc)->CreateOffer(observer, observer->constraints());
}

JNI_FUNCTION_DECLARATION(void,
                         PeerConnection_createAnswer,
                         JNIEnv* jni,
                         jobject j_pc,
                         jobject j_observer,
                         jobject j_constraints) {
  std::unique_ptr<MediaConstraintsInterface> constraints =
      JavaToNativeMediaConstraints(jni, j_constraints);
  rtc::scoped_refptr<CreateSdpObserverJni> observer(
      new rtc::RefCountedObject<CreateSdpObserverJni>(jni, j_observer,
                                                      std::move(constraints)));
  ExtractNativePC(jni, j_pc)->CreateAnswer(observer, observer->constraints());
}

JNI_FUNCTION_DECLARATION(void,
                         PeerConnection_setLocalDescription,
                         JNIEnv* jni,
                         jobject j_pc,
                         jobject j_observer,
                         jobject j_sdp) {
  rtc::scoped_refptr<SetSdpObserverJni> observer(
      new rtc::RefCountedObject<SetSdpObserverJni>(jni, j_observer, nullptr));
  ExtractNativePC(jni, j_pc)->SetLocalDescription(
      observer, JavaToNativeSessionDescription(jni, j_sdp).release());
}

JNI_FUNCTION_DECLARATION(void,
                         PeerConnection_setRemoteDescription,
                         JNIEnv* jni,
                         jobject j_pc,
                         jobject j_observer,
                         jobject j_sdp) {
  rtc::scoped_refptr<SetSdpObserverJni> observer(
      new rtc::RefCountedObject<SetSdpObserverJni>(jni, j_observer, nullptr));
  ExtractNativePC(jni, j_pc)->SetRemoteDescription(
      observer, JavaToNativeSessionDescription(jni, j_sdp).release());
}

JNI_FUNCTION_DECLARATION(void,
                         PeerConnection_setAudioPlayout,
                         JNIEnv* jni,
                         jobject j_pc,
                         jboolean playout) {
  ExtractNativePC(jni, j_pc)->SetAudioPlayout(playout);
}

JNI_FUNCTION_DECLARATION(void,
                         PeerConnection_setAudioRecording,
                         JNIEnv* jni,
                         jobject j_pc,
                         jboolean recording) {
  ExtractNativePC(jni, j_pc)->SetAudioRecording(recording);
}

JNI_FUNCTION_DECLARATION(jboolean,
                         PeerConnection_setNativeConfiguration,
                         JNIEnv* jni,
                         jobject j_pc,
                         jobject j_rtc_config,
                         jlong native_observer) {
  // Need to merge constraints into RTCConfiguration again, which are stored
  // in the observer object.
  PeerConnectionObserverJni* observer =
      reinterpret_cast<PeerConnectionObserverJni*>(native_observer);
  PeerConnectionInterface::RTCConfiguration rtc_config(
      PeerConnectionInterface::RTCConfigurationType::kAggressive);
  JavaToNativeRTCConfiguration(jni, j_rtc_config, &rtc_config);
  CopyConstraintsIntoRtcConfiguration(observer->constraints(), &rtc_config);
  return ExtractNativePC(jni, j_pc)->SetConfiguration(rtc_config);
}

JNI_FUNCTION_DECLARATION(jboolean,
                         PeerConnection_addNativeIceCandidate,
                         JNIEnv* jni,
                         jobject j_pc,
                         jstring j_sdp_mid,
                         jint j_sdp_mline_index,
                         jstring j_candidate_sdp) {
  std::string sdp_mid = JavaToStdString(jni, j_sdp_mid);
  std::string sdp = JavaToStdString(jni, j_candidate_sdp);
  std::unique_ptr<IceCandidateInterface> candidate(
      CreateIceCandidate(sdp_mid, j_sdp_mline_index, sdp, nullptr));
  return ExtractNativePC(jni, j_pc)->AddIceCandidate(candidate.get());
}

JNI_FUNCTION_DECLARATION(jboolean,
                         PeerConnection_removeNativeIceCandidates,
                         JNIEnv* jni,
                         jobject j_pc,
                         jobjectArray j_candidates) {
  std::vector<cricket::Candidate> candidates =
      JavaToNativeVector<cricket::Candidate>(jni, j_candidates,
                                             &JavaToNativeCandidate);
  return ExtractNativePC(jni, j_pc)->RemoveIceCandidates(candidates);
}

JNI_FUNCTION_DECLARATION(jboolean,
                         PeerConnection_addNativeLocalStream,
                         JNIEnv* jni,
                         jobject j_pc,
                         jlong native_stream) {
  return ExtractNativePC(jni, j_pc)->AddStream(
      reinterpret_cast<MediaStreamInterface*>(native_stream));
}

JNI_FUNCTION_DECLARATION(void,
                         PeerConnection_removeNativeLocalStream,
                         JNIEnv* jni,
                         jobject j_pc,
                         jlong native_stream) {
  ExtractNativePC(jni, j_pc)->RemoveStream(
      reinterpret_cast<MediaStreamInterface*>(native_stream));
}

JNI_FUNCTION_DECLARATION(jobject,
                         PeerConnection_createNativeSender,
                         JNIEnv* jni,
                         jobject j_pc,
                         jstring j_kind,
                         jstring j_stream_id) {
  std::string kind = JavaToStdString(jni, j_kind);
  std::string stream_id = JavaToStdString(jni, j_stream_id);
  rtc::scoped_refptr<RtpSenderInterface> sender =
      ExtractNativePC(jni, j_pc)->CreateSender(kind, stream_id);
  return NativeToJavaRtpSender(jni, sender);
}

JNI_FUNCTION_DECLARATION(jobject,
                         PeerConnection_getNativeSenders,
                         JNIEnv* jni,
                         jobject j_pc) {
  return NativeToJavaList(jni, ExtractNativePC(jni, j_pc)->GetSenders(),
                          &NativeToJavaRtpSender);
}

JNI_FUNCTION_DECLARATION(jobject,
                         PeerConnection_getNativeReceivers,
                         JNIEnv* jni,
                         jobject j_pc) {
  return NativeToJavaList(jni, ExtractNativePC(jni, j_pc)->GetReceivers(),
                          &NativeToJavaRtpReceiver);
}

JNI_FUNCTION_DECLARATION(bool,
                         PeerConnection_oldGetNativeStats,
                         JNIEnv* jni,
                         jobject j_pc,
                         jobject j_observer,
                         jlong native_track) {
  rtc::scoped_refptr<StatsObserverJni> observer(
      new rtc::RefCountedObject<StatsObserverJni>(jni, j_observer));
  return ExtractNativePC(jni, j_pc)->GetStats(
      observer, reinterpret_cast<MediaStreamTrackInterface*>(native_track),
      PeerConnectionInterface::kStatsOutputLevelStandard);
}

JNI_FUNCTION_DECLARATION(void,
                         PeerConnection_newGetNativeStats,
                         JNIEnv* jni,
                         jobject j_pc,
                         jobject j_callback) {
  rtc::scoped_refptr<RTCStatsCollectorCallbackWrapper> callback(
      new rtc::RefCountedObject<RTCStatsCollectorCallbackWrapper>(jni,
                                                                  j_callback));
  ExtractNativePC(jni, j_pc)->GetStats(callback);
}

JNI_FUNCTION_DECLARATION(jboolean,
                         PeerConnection_setBitrate,
                         JNIEnv* jni,
                         jobject j_pc,
                         jobject j_min,
                         jobject j_current,
                         jobject j_max) {
  PeerConnectionInterface::BitrateParameters params;
  params.min_bitrate_bps = JavaToNativeOptionalInt(jni, j_min);
  params.current_bitrate_bps = JavaToNativeOptionalInt(jni, j_current);
  params.max_bitrate_bps = JavaToNativeOptionalInt(jni, j_max);
  return ExtractNativePC(jni, j_pc)->SetBitrate(params).ok();
}

JNI_FUNCTION_DECLARATION(bool,
                         PeerConnection_startNativeRtcEventLog,
                         JNIEnv* jni,
                         jobject j_pc,
                         int file_descriptor,
                         int max_size_bytes) {
  return ExtractNativePC(jni, j_pc)->StartRtcEventLog(file_descriptor,
                                                      max_size_bytes);
}

JNI_FUNCTION_DECLARATION(void,
                         PeerConnection_stopNativeRtcEventLog,
                         JNIEnv* jni,
                         jobject j_pc) {
  ExtractNativePC(jni, j_pc)->StopRtcEventLog();
}

JNI_FUNCTION_DECLARATION(jobject,
                         PeerConnection_signalingState,
                         JNIEnv* jni,
                         jobject j_pc) {
  PeerConnectionInterface::SignalingState state =
      ExtractNativePC(jni, j_pc)->signaling_state();
  return JavaEnumFromIndexAndClassName(jni, "PeerConnection$SignalingState",
                                       state);
}

JNI_FUNCTION_DECLARATION(jobject,
                         PeerConnection_iceConnectionState,
                         JNIEnv* jni,
                         jobject j_pc) {
  PeerConnectionInterface::IceConnectionState state =
      ExtractNativePC(jni, j_pc)->ice_connection_state();
  return JavaEnumFromIndexAndClassName(jni, "PeerConnection$IceConnectionState",
                                       state);
}

JNI_FUNCTION_DECLARATION(jobject,
                         PeerConnection_iceGatheringState,
                         JNIEnv* jni,
                         jobject j_pc) {
  PeerConnectionInterface::IceGatheringState state =
      ExtractNativePC(jni, j_pc)->ice_gathering_state();
  return JavaEnumFromIndexAndClassName(jni, "PeerConnection$IceGatheringState",
                                       state);
}

JNI_FUNCTION_DECLARATION(void,
                         PeerConnection_close,
                         JNIEnv* jni,
                         jobject j_pc) {
  ExtractNativePC(jni, j_pc)->Close();
  return;
}

}  // namespace jni
}  // namespace webrtc
