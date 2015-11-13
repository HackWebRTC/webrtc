/*
 * libjingle
 * Copyright 2015 Google Inc.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 *  1. Redistributions of source code must retain the above copyright notice,
 *     this list of conditions and the following disclaimer.
 *  2. Redistributions in binary form must reproduce the above copyright notice,
 *     this list of conditions and the following disclaimer in the documentation
 *     and/or other materials provided with the distribution.
 *  3. The name of the author may not be used to endorse or promote products
 *     derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO
 * EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */


#include "talk/app/webrtc/java/jni/surfacetexturehelper_jni.h"

#include "talk/app/webrtc/java/jni/classreferenceholder.h"
#include "webrtc/base/bind.h"
#include "webrtc/base/checks.h"

namespace webrtc_jni {

SurfaceTextureHelper::SurfaceTextureHelper(JNIEnv* jni,
                                           jobject egl_shared_context)
    : j_surface_texture_helper_class_(
          jni,
          FindClass(jni, "org/webrtc/SurfaceTextureHelper")),
      j_surface_texture_helper_(
          jni,
          jni->CallStaticObjectMethod(
              *j_surface_texture_helper_class_,
              GetStaticMethodID(jni,
                                *j_surface_texture_helper_class_,
                                "create",
                                "(Ljavax/microedition/khronos/egl/EGLContext;)"
                                "Lorg/webrtc/SurfaceTextureHelper;"),
              egl_shared_context)),
      j_return_texture_method_(GetMethodID(jni,
                                           *j_surface_texture_helper_class_,
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
    const NativeTextureHandleImpl& native_handle) {
  return new rtc::RefCountedObject<AndroidTextureBuffer>(
      width, height, native_handle,
      rtc::Bind(&SurfaceTextureHelper::ReturnTextureFrame, this));
}

}  // namespace webrtc_jni
