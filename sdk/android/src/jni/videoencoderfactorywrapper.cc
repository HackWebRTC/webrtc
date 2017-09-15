/*
 *  Copyright 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "sdk/android/src/jni/videoencoderfactorywrapper.h"

#include "api/video_codecs/video_encoder.h"
#include "common_types.h"
#include "rtc_base/logging.h"
#include "sdk/android/src/jni/classreferenceholder.h"
#include "sdk/android/src/jni/videoencoderwrapper.h"

namespace webrtc {
namespace jni {

VideoEncoderFactoryWrapper::VideoEncoderFactoryWrapper(JNIEnv* jni,
                                                       jobject encoder_factory)
    : video_codec_info_class_(jni, FindClass(jni, "org/webrtc/VideoCodecInfo")),
      hash_map_class_(jni, jni->FindClass("java/util/HashMap")),
      encoder_factory_(jni, encoder_factory) {
  jclass encoder_factory_class = jni->GetObjectClass(*encoder_factory_);
  create_encoder_method_ = jni->GetMethodID(
      encoder_factory_class, "createEncoder",
      "(Lorg/webrtc/VideoCodecInfo;)Lorg/webrtc/VideoEncoder;");
  get_supported_codecs_method_ =
      jni->GetMethodID(encoder_factory_class, "getSupportedCodecs",
                       "()[Lorg/webrtc/VideoCodecInfo;");

  video_codec_info_constructor_ =
      jni->GetMethodID(*video_codec_info_class_, "<init>",
                       "(ILjava/lang/String;Ljava/util/Map;)V");
  payload_field_ = jni->GetFieldID(*video_codec_info_class_, "payload", "I");
  name_field_ =
      jni->GetFieldID(*video_codec_info_class_, "name", "Ljava/lang/String;");
  params_field_ =
      jni->GetFieldID(*video_codec_info_class_, "params", "Ljava/util/Map;");

  hash_map_constructor_ = jni->GetMethodID(*hash_map_class_, "<init>", "()V");
  put_method_ = jni->GetMethodID(
      *hash_map_class_, "put",
      "(Ljava/lang/Object;Ljava/lang/Object;)Ljava/lang/Object;");

  supported_codecs_ = GetSupportedCodecs(jni);
}

VideoEncoder* VideoEncoderFactoryWrapper::CreateVideoEncoder(
    const cricket::VideoCodec& codec) {
  JNIEnv* jni = AttachCurrentThreadIfNeeded();
  ScopedLocalRefFrame local_ref_frame(jni);
  jobject j_codec_info = ToJavaCodecInfo(jni, codec);
  jobject encoder = jni->CallObjectMethod(*encoder_factory_,
                                          create_encoder_method_, j_codec_info);
  return encoder != nullptr ? new VideoEncoderWrapper(jni, encoder) : nullptr;
}

jobject VideoEncoderFactoryWrapper::ToJavaCodecInfo(
    JNIEnv* jni,
    const cricket::VideoCodec& codec) {
  jobject j_params = jni->NewObject(*hash_map_class_, hash_map_constructor_);
  for (auto const& param : codec.params) {
    jni->CallObjectMethod(j_params, put_method_,
                          JavaStringFromStdString(jni, param.first),
                          JavaStringFromStdString(jni, param.second));
  }
  return jni->NewObject(*video_codec_info_class_, video_codec_info_constructor_,
                        codec.id, JavaStringFromStdString(jni, codec.name),
                        j_params);
}

std::vector<cricket::VideoCodec> VideoEncoderFactoryWrapper::GetSupportedCodecs(
    JNIEnv* jni) const {
  const jobjectArray j_supported_codecs = static_cast<jobjectArray>(
      jni->CallObjectMethod(*encoder_factory_, get_supported_codecs_method_));
  const jsize supported_codecs_count = jni->GetArrayLength(j_supported_codecs);

  std::vector<cricket::VideoCodec> supported_codecs;
  supported_codecs.resize(supported_codecs_count);
  for (jsize i = 0; i < supported_codecs_count; i++) {
    jobject j_supported_codec =
        jni->GetObjectArrayElement(j_supported_codecs, i);
    int payload = jni->GetIntField(j_supported_codec, payload_field_);
    jobject j_params = jni->GetObjectField(j_supported_codec, params_field_);
    jstring j_name = static_cast<jstring>(
        jni->GetObjectField(j_supported_codec, name_field_));
    supported_codecs[i] =
        cricket::VideoCodec(payload, JavaToStdString(jni, j_name));
    supported_codecs[i].params = JavaToStdMapStrings(jni, j_params);
  }
  return supported_codecs;
}

void VideoEncoderFactoryWrapper::DestroyVideoEncoder(VideoEncoder* encoder) {
  delete encoder;
}

}  // namespace jni
}  // namespace webrtc
