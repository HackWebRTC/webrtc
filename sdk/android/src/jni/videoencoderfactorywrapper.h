/*
 *  Copyright 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef SDK_ANDROID_SRC_JNI_VIDEOENCODERFACTORYWRAPPER_H_
#define SDK_ANDROID_SRC_JNI_VIDEOENCODERFACTORYWRAPPER_H_

#include <jni.h>
#include <vector>

#include "media/engine/webrtcvideoencoderfactory.h"
#include "sdk/android/src/jni/jni_helpers.h"

namespace webrtc {
namespace jni {

// Wrapper for Java VideoEncoderFactory class. Delegates method calls through
// JNI and wraps the encoder inside VideoEncoderWrapper.
class VideoEncoderFactoryWrapper : public cricket::WebRtcVideoEncoderFactory {
 public:
  VideoEncoderFactoryWrapper(JNIEnv* jni, jobject encoder_factory);

  // Caller takes the ownership of the returned object and it should be released
  // by calling DestroyVideoEncoder().
  VideoEncoder* CreateVideoEncoder(const cricket::VideoCodec& codec) override;

  // Returns a list of supported codecs in order of preference.
  const std::vector<cricket::VideoCodec>& supported_codecs() const override {
    return supported_codecs_;
  }

  void DestroyVideoEncoder(VideoEncoder* encoder) override;

 private:
  std::vector<cricket::VideoCodec> GetSupportedCodecs(JNIEnv* jni) const;
  jobject ToJavaCodecInfo(JNIEnv* jni, const cricket::VideoCodec& codec);

  const ScopedGlobalRef<jclass> video_codec_info_class_;
  const ScopedGlobalRef<jclass> hash_map_class_;
  const ScopedGlobalRef<jobject> encoder_factory_;

  jmethodID create_encoder_method_;
  jmethodID get_supported_codecs_method_;

  jmethodID video_codec_info_constructor_;
  jfieldID payload_field_;
  jfieldID name_field_;
  jfieldID params_field_;

  jmethodID hash_map_constructor_;
  jmethodID put_method_;

  std::vector<cricket::VideoCodec> supported_codecs_;
};

}  // namespace jni
}  // namespace webrtc

#endif  // SDK_ANDROID_SRC_JNI_VIDEOENCODERFACTORYWRAPPER_H_
