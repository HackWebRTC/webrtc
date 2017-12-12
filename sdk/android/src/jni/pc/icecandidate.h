/*
 *  Copyright 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef SDK_ANDROID_SRC_JNI_PC_ICECANDIDATE_H_
#define SDK_ANDROID_SRC_JNI_PC_ICECANDIDATE_H_

#include <vector>

#include "api/datachannelinterface.h"
#include "api/jsep.h"
#include "api/jsepicecandidate.h"
#include "api/peerconnectioninterface.h"
#include "api/rtpparameters.h"
#include "rtc_base/sslidentity.h"
#include "sdk/android/src/jni/jni_helpers.h"

namespace webrtc {
namespace jni {

cricket::Candidate JavaToNativeCandidate(JNIEnv* jni, jobject j_candidate);

jobject NativeToJavaCandidate(JNIEnv* env, const cricket::Candidate& candidate);

jobject NativeToJavaIceCandidate(JNIEnv* env,
                                 const IceCandidateInterface& candidate);

jobjectArray NativeToJavaCandidateArray(
    JNIEnv* jni,
    const std::vector<cricket::Candidate>& candidates);

/*****************************************************
 * Below are all things that go into RTCConfiguration.
 *****************************************************/
PeerConnectionInterface::IceTransportsType JavaToNativeIceTransportsType(
    JNIEnv* jni,
    jobject j_ice_transports_type);

PeerConnectionInterface::BundlePolicy JavaToNativeBundlePolicy(
    JNIEnv* jni,
    jobject j_bundle_policy);

PeerConnectionInterface::RtcpMuxPolicy JavaToNativeRtcpMuxPolicy(
    JNIEnv* jni,
    jobject j_rtcp_mux_policy);

PeerConnectionInterface::TcpCandidatePolicy JavaToNativeTcpCandidatePolicy(
    JNIEnv* jni,
    jobject j_tcp_candidate_policy);

PeerConnectionInterface::CandidateNetworkPolicy
JavaToNativeCandidateNetworkPolicy(JNIEnv* jni,
                                   jobject j_candidate_network_policy);

rtc::KeyType JavaToNativeKeyType(JNIEnv* jni, jobject j_key_type);

PeerConnectionInterface::ContinualGatheringPolicy
JavaToNativeContinualGatheringPolicy(JNIEnv* jni, jobject j_gathering_policy);

PeerConnectionInterface::TlsCertPolicy JavaToNativeTlsCertPolicy(
    JNIEnv* jni,
    jobject j_ice_server_tls_cert_policy);

}  // namespace jni
}  // namespace webrtc

#endif  // SDK_ANDROID_SRC_JNI_PC_ICECANDIDATE_H_
