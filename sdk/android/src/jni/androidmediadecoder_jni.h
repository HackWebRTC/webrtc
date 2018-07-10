/*
 *  Copyright 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef SDK_ANDROID_SRC_JNI_ANDROIDMEDIADECODER_H_
#define SDK_ANDROID_SRC_JNI_ANDROIDMEDIADECODER_H_

#include <vector>

#include "api/video_codecs/video_decoder_factory.h"
#include "sdk/android/src/jni/jni_helpers.h"

namespace webrtc {
namespace jni {

// Implementation of Android MediaCodec based decoder factory.
class MediaCodecVideoDecoderFactory : public VideoDecoderFactory {
 public:
  MediaCodecVideoDecoderFactory();
  ~MediaCodecVideoDecoderFactory() override;

  void SetEGLContext(JNIEnv* jni, jobject render_egl_context);

  // VideoDecoderFactory implementation.
  std::vector<SdpVideoFormat> GetSupportedFormats() const override;
  std::unique_ptr<VideoDecoder> CreateVideoDecoder(
      const SdpVideoFormat& format) override;

  static bool IsH264HighProfileSupported(JNIEnv* env);

 private:
  jobject egl_context_;
  std::vector<SdpVideoFormat> supported_formats_;
};

}  // namespace jni
}  // namespace webrtc

#endif  // SDK_ANDROID_SRC_JNI_ANDROIDMEDIADECODER_H_
