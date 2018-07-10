/*
 *  Copyright 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef SDK_ANDROID_SRC_JNI_PC_VIDEO_H_
#define SDK_ANDROID_SRC_JNI_PC_VIDEO_H_

#include <jni.h>

#include "rtc_base/scoped_ref_ptr.h"
#include "rtc_base/thread.h"
#include "sdk/android/native_api/jni/scoped_java_ref.h"

namespace webrtc {
class VideoEncoderFactory;
class VideoDecoderFactory;
}  // namespace webrtc

namespace webrtc {
namespace jni {

VideoEncoderFactory* CreateVideoEncoderFactory(
    JNIEnv* jni,
    const JavaRef<jobject>& j_encoder_factory);

VideoDecoderFactory* CreateVideoDecoderFactory(
    JNIEnv* jni,
    const JavaRef<jobject>& j_decoder_factory);

void SetEglContext(JNIEnv* env,
                   VideoEncoderFactory* encoder_factory,
                   const JavaRef<jobject>& egl_context);
void SetEglContext(JNIEnv* env,
                   VideoDecoderFactory* decoder_factory,
                   const JavaRef<jobject>& egl_context);

void* CreateVideoSource(JNIEnv* env,
                        rtc::Thread* signaling_thread,
                        rtc::Thread* worker_thread,
                        jboolean is_screencast);

std::unique_ptr<VideoEncoderFactory> CreateLegacyVideoEncoderFactory();
std::unique_ptr<VideoDecoderFactory> CreateLegacyVideoDecoderFactory();

std::unique_ptr<VideoEncoderFactory> WrapLegacyVideoEncoderFactory(
    std::unique_ptr<VideoEncoderFactory> legacy_encoder_factory);
std::unique_ptr<VideoDecoderFactory> WrapLegacyVideoDecoderFactory(
    std::unique_ptr<VideoDecoderFactory> legacy_decoder_factory);

}  // namespace jni
}  // namespace webrtc

#endif  // SDK_ANDROID_SRC_JNI_PC_VIDEO_H_
