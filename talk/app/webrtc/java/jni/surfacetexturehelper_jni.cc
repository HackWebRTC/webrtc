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
#include "webrtc/base/checks.h"

namespace webrtc_jni {

class SurfaceTextureHelper::TextureBuffer : public webrtc::NativeHandleBuffer {
 public:
  TextureBuffer(int width,
                int height,
                const rtc::scoped_refptr<SurfaceTextureHelper>& pool,
                const NativeHandleImpl& native_handle)
      : webrtc::NativeHandleBuffer(&native_handle_, width, height),
        native_handle_(native_handle),
        pool_(pool) {}

  ~TextureBuffer() {
    pool_->ReturnTextureFrame();
  }

  rtc::scoped_refptr<VideoFrameBuffer> NativeToI420Buffer() override {
    RTC_NOTREACHED()
        << "SurfaceTextureHelper::NativeToI420Buffer not implemented.";
    return nullptr;
  }

 private:
  NativeHandleImpl native_handle_;
  const rtc::scoped_refptr<SurfaceTextureHelper> pool_;
};

SurfaceTextureHelper::SurfaceTextureHelper(JNIEnv* jni,
                                           jobject egl_shared_context)
    : j_surface_texture_helper_class_(
          jni,
          FindClass(jni, "org/webrtc/SurfaceTextureHelper")),
      j_surface_texture_helper_(
          jni,
          jni->NewObject(*j_surface_texture_helper_class_,
                         GetMethodID(jni,
                                     *j_surface_texture_helper_class_,
                                     "<init>",
                                     "(Landroid/opengl/EGLContext;)V"),
                         egl_shared_context)),
      j_return_texture_method_(
          GetMethodID(jni,
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
    const NativeHandleImpl& native_handle) {
  return new rtc::RefCountedObject<TextureBuffer>(
      width, height, this, native_handle);
}

}  // namespace webrtc_jni
