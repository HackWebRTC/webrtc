/*
 *  Copyright 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "api/video_codecs/h264_profile_level_id.h"
#include "media/base/sdp_video_format_utils.h"

#include "sdk/android/generated_peerconnection_jni/VideoCodecUtils_jni.h"
#include "sdk/android/native_api/jni/java_types.h"

namespace webrtc {
namespace jni {

static jboolean JNI_VideoCodecUtils_IsSameH264Profile(
    JNIEnv* env,
    const jni_zero::JavaParamRef<jobject>& aParams,
    const jni_zero::JavaParamRef<jobject>& bParams) {
    std::map<std::string, std::string> aMap = webrtc::JavaToNativeStringMap(env, aParams);
    std::map<std::string, std::string> bMap = webrtc::JavaToNativeStringMap(env, bParams);
    return webrtc::H264IsSameProfile(aMap, bMap);
}

static jni_zero::ScopedJavaLocalRef<jstring> JNI_VideoCodecUtils_H264GenerateProfileLevelIdForAnswer(
    JNIEnv* env,
    const jni_zero::JavaParamRef<jobject>& aParams,
    const jni_zero::JavaParamRef<jobject>& bParams) {
    std::map<std::string, std::string> aMap = webrtc::JavaToNativeStringMap(env, aParams);
    std::map<std::string, std::string> bMap = webrtc::JavaToNativeStringMap(env, bParams);
    std::string profile_level_id;
    webrtc::CodecParameterMap newParams;
    webrtc::H264GenerateProfileLevelIdForAnswer(aMap, bMap, &newParams);
    auto profile_level_id_it = newParams.find("profile-level-id");
    if (profile_level_id_it != newParams.end()) {
        profile_level_id = profile_level_id_it->second;
    }
    return NativeToJavaString(env, profile_level_id);
}

}  // namespace jni
}  // namespace webrtc
 