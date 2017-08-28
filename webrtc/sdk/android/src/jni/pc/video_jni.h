/*
 *  Copyright 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_SDK_ANDROID_SRC_JNI_PC_VIDEO_JNI_H_
#define WEBRTC_SDK_ANDROID_SRC_JNI_PC_VIDEO_JNI_H_

#include <jni.h>

#include "webrtc/rtc_base/scoped_ref_ptr.h"

namespace cricket {
class WebRtcVideoEncoderFactory;
class WebRtcVideoDecoderFactory;
}  // namespace cricket

namespace webrtc {
namespace jni {

class SurfaceTextureHelper;

cricket::WebRtcVideoEncoderFactory* CreateVideoEncoderFactory(
    JNIEnv* jni,
    jobject j_encoder_factory);

cricket::WebRtcVideoDecoderFactory* CreateVideoDecoderFactory(
    JNIEnv* jni,
    jobject j_decoder_factory);

jobject GetJavaSurfaceTextureHelper(
    const rtc::scoped_refptr<SurfaceTextureHelper>& surface_texture_helper);

}  // namespace jni
}  // namespace webrtc

#endif  // WEBRTC_SDK_ANDROID_SRC_JNI_PC_VIDEO_JNI_H_
