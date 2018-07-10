/*
 *  Copyright 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef SDK_ANDROID_SRC_JNI_ANDROIDMEDIAENCODER_H_
#define SDK_ANDROID_SRC_JNI_ANDROIDMEDIAENCODER_H_

#include <vector>

#include "api/video_codecs/video_encoder_factory.h"
#include "sdk/android/src/jni/jni_helpers.h"

namespace webrtc {
namespace jni {

// Implementation of Android MediaCodec based encoder factory.
class MediaCodecVideoEncoderFactory : public VideoEncoderFactory {
 public:
  MediaCodecVideoEncoderFactory();
  ~MediaCodecVideoEncoderFactory() override;

  void SetEGLContext(JNIEnv* jni, jobject egl_context);

  // VideoEncoderFactory implementation.
  std::vector<SdpVideoFormat> GetSupportedFormats() const override;
  CodecInfo QueryVideoEncoder(const SdpVideoFormat& format) const override;
  std::unique_ptr<VideoEncoder> CreateVideoEncoder(
      const SdpVideoFormat& format) override;

 private:
  jobject egl_context_;

  // Empty if platform support is lacking, const after ctor returns.
  std::vector<SdpVideoFormat> supported_formats_;
  std::vector<SdpVideoFormat> supported_formats_with_h264_hp_;
};

}  // namespace jni
}  // namespace webrtc

#endif  // SDK_ANDROID_SRC_JNI_ANDROIDMEDIAENCODER_H_
