/*
 *  Copyright 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */


#include "webrtc/api/java/jni/surfacetexturehelper_jni.h"

#include "webrtc/api/java/jni/classreferenceholder.h"
#include "webrtc/base/bind.h"
#include "webrtc/base/checks.h"

namespace webrtc_jni {

SurfaceTextureHelper::SurfaceTextureHelper(
    JNIEnv* jni, jobject surface_texture_helper)
  : j_surface_texture_helper_(jni, surface_texture_helper),
    j_return_texture_method_(
        GetMethodID(jni,
                    FindClass(jni, "org/webrtc/SurfaceTextureHelper"),
                    "returnTextureFrame",
                    "()V")) {
  CHECK_EXCEPTION(jni) << "error during initialization of SurfaceTextureHelper";
}

SurfaceTextureHelper::~SurfaceTextureHelper() {
}

void SurfaceTextureHelper::ReturnTextureFrame() const {
  JNIEnv* jni = AttachCurrentThreadIfNeeded();
  jni->CallVoidMethod(*j_surface_texture_helper_, j_return_texture_method_);

  CHECK_EXCEPTION(
      jni) << "error during SurfaceTextureHelper.returnTextureFrame";
}

rtc::scoped_refptr<webrtc::VideoFrameBuffer>
SurfaceTextureHelper::CreateTextureFrame(int width, int height,
    const NativeHandleImpl& native_handle) {
  return new rtc::RefCountedObject<AndroidTextureBuffer>(
      width, height, native_handle, *j_surface_texture_helper_,
      rtc::Bind(&SurfaceTextureHelper::ReturnTextureFrame, this));
}

}  // namespace webrtc_jni
