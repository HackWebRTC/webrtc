/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "sdk/android/src/jni/pc/video.h"

#include "api/video_codecs/video_decoder_factory.h"
#include "api/video_codecs/video_encoder_factory.h"

namespace webrtc {
namespace jni {

VideoEncoderFactory* CreateVideoEncoderFactory(
    JNIEnv* jni,
    const JavaRef<jobject>& j_encoder_factory) {
  return nullptr;
}

VideoDecoderFactory* CreateVideoDecoderFactory(
    JNIEnv* jni,
    const JavaRef<jobject>& j_decoder_factory) {
  return nullptr;
}

void SetEglContext(JNIEnv* env,
                   VideoEncoderFactory* encoder_factory,
                   const JavaRef<jobject>& egl_context) {}
void SetEglContext(JNIEnv* env,
                   VideoDecoderFactory* decoder_factory,
                   const JavaRef<jobject>& egl_context) {}

void* CreateVideoSource(JNIEnv* env,
                        rtc::Thread* signaling_thread,
                        rtc::Thread* worker_thread,
                        jboolean is_screencast) {
  return nullptr;
}

std::unique_ptr<VideoEncoderFactory> CreateLegacyVideoEncoderFactory() {
  return nullptr;
}

std::unique_ptr<VideoDecoderFactory> CreateLegacyVideoDecoderFactory() {
  return nullptr;
}

std::unique_ptr<VideoEncoderFactory> WrapLegacyVideoEncoderFactory(
    std::unique_ptr<VideoEncoderFactory> legacy_encoder_factory) {
  return nullptr;
}

std::unique_ptr<VideoDecoderFactory> WrapLegacyVideoDecoderFactory(
    std::unique_ptr<VideoDecoderFactory> legacy_decoder_factory) {
  return nullptr;
}

}  // namespace jni
}  // namespace webrtc
