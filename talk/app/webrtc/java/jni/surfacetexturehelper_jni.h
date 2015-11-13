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

#ifndef TALK_APP_WEBRTC_JAVA_JNI_SURFACETEXTUREHELPER_JNI_H_
#define TALK_APP_WEBRTC_JAVA_JNI_SURFACETEXTUREHELPER_JNI_H_

#include <jni.h>

#include "talk/app/webrtc/java/jni/jni_helpers.h"
#include "talk/app/webrtc/java/jni/native_handle_impl.h"
#include "webrtc/base/refcount.h"
#include "webrtc/base/scoped_ref_ptr.h"
#include "webrtc/common_video/interface/video_frame_buffer.h"

namespace webrtc_jni {

// Helper class to create and synchronize access to an Android SurfaceTexture.
// It is used for creating webrtc::VideoFrameBuffers from a SurfaceTexture when
// the SurfaceTexture has been updated.
// When the VideoFrameBuffer is released, this class returns the buffer to the
// java SurfaceTextureHelper so it can be updated safely. The VideoFrameBuffer
// can be released on an arbitrary thread.
// SurfaceTextureHelper is reference counted to make sure that it is not
// destroyed while a VideoFrameBuffer is in use.
// This class is the C++ counterpart of the java class SurfaceTextureHelper.
// Usage:
// 1. Create an instance of this class.
// 2. Call GetJavaSurfaceTextureHelper to get the Java SurfaceTextureHelper.
// 3. Register a listener to the Java SurfaceListener and start producing
// new buffers.
// 3. Call CreateTextureFrame to wrap the Java texture in a VideoFrameBuffer.
class SurfaceTextureHelper : public rtc::RefCountInterface {
 public:
  SurfaceTextureHelper(JNIEnv* jni, jobject shared_egl_context);

  // Returns the Java SurfaceTextureHelper.
  jobject GetJavaSurfaceTextureHelper() const {
    return *j_surface_texture_helper_;
  }

  rtc::scoped_refptr<webrtc::VideoFrameBuffer> CreateTextureFrame(
      int width,
      int height,
      const NativeTextureHandleImpl& native_handle);

 protected:
  ~SurfaceTextureHelper();

 private:
  //  May be called on arbitrary thread.
  void ReturnTextureFrame() const;

  const ScopedGlobalRef<jclass> j_surface_texture_helper_class_;
  const ScopedGlobalRef<jobject> j_surface_texture_helper_;
  const jmethodID j_return_texture_method_;
};

}  // namespace webrtc_jni

#endif  // TALK_APP_WEBRTC_JAVA_JNI_SURFACETEXTUREHELPER_JNI_H_
