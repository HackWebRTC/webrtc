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

#ifndef TALK_APP_WEBRTC_JAVA_JNI_NATIVE_HANDLE_IMPL_H_
#define TALK_APP_WEBRTC_JAVA_JNI_NATIVE_HANDLE_IMPL_H_

#include <jni.h>

#include "webrtc/common_video/interface/video_frame_buffer.h"

namespace webrtc_jni {

// Wrapper for texture object.
struct NativeTextureHandleImpl {
  NativeTextureHandleImpl(JNIEnv* jni,
                          jint j_oes_texture_id,
                          jfloatArray j_transform_matrix);

  const int oes_texture_id;
  float sampling_matrix[16];
};

// Native handle for SurfaceTexture + texture id.
class NativeHandleImpl {
 public:
  NativeHandleImpl();

  void* GetHandle();
  int GetTextureId();
  void SetTextureObject(void* texture_object, int texture_id);

 private:
  jobject texture_object_;
  int32_t texture_id_;
};

class JniNativeHandleBuffer : public webrtc::NativeHandleBuffer {
 public:
  JniNativeHandleBuffer(void* native_handle, int width, int height);

  // TODO(pbos): Override destructor to release native handle, at the moment the
  // native handle is not released based on refcount.

 private:
  rtc::scoped_refptr<webrtc::VideoFrameBuffer> NativeToI420Buffer() override;
};

class AndroidTextureBuffer : public webrtc::NativeHandleBuffer {
 public:
  AndroidTextureBuffer(int width,
                       int height,
                       const NativeTextureHandleImpl& native_handle,
                       const rtc::Callback0<void>& no_longer_used);
  ~AndroidTextureBuffer();
  rtc::scoped_refptr<VideoFrameBuffer> NativeToI420Buffer() override;

 private:
  NativeTextureHandleImpl native_handle_;
  rtc::Callback0<void> no_longer_used_cb_;
};

}  // namespace webrtc_jni

#endif  // TALK_APP_WEBRTC_JAVA_JNI_NATIVE_HANDLE_IMPL_H_
