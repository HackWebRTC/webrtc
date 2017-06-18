/*
 *  Copyright 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_SDK_ANDROID_SRC_JNI_VIDEO_JNI_H_
#define WEBRTC_SDK_ANDROID_SRC_JNI_VIDEO_JNI_H_

#include <jni.h>

#include "webrtc/base/scoped_ref_ptr.h"
// Adding 'nogncheck' to disable the gn include headers check.
// We don't want this target depend on video related targets
#include "webrtc/sdk/android/src/jni/surfacetexturehelper_jni.h"  // nogncheck

namespace webrtc_jni {

WebRtcVideoEncoderFactory* CreateVideoEncoderFactory();

WebRtcVideoDecoderFactory* CreateVideoDecoderFactory();

jobject GetJavaSurfaceTextureHelper(
    rtc::scoped_refptr<SurfaceTextureHelper> surface_texture_helper);

}  // namespace webrtc_jni

#endif  // WEBRTC_SDK_ANDROID_SRC_JNI_VIDEO_JNI_H_
