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
 */

#include "talk/app/webrtc/java/jni/native_handle_impl.h"

#include "webrtc/base/checks.h"

namespace webrtc_jni {

NativeTextureHandleImpl::NativeTextureHandleImpl(JNIEnv* jni,
                                                 jint j_oes_texture_id,
                                                 jfloatArray j_transform_matrix)
    : oes_texture_id(j_oes_texture_id) {
  RTC_CHECK_EQ(16, jni->GetArrayLength(j_transform_matrix));
  jfloat* transform_matrix_ptr =
      jni->GetFloatArrayElements(j_transform_matrix, nullptr);
  for (int i = 0; i < 16; ++i) {
    sampling_matrix[i] = transform_matrix_ptr[i];
  }
  jni->ReleaseFloatArrayElements(j_transform_matrix, transform_matrix_ptr, 0);
}

NativeHandleImpl::NativeHandleImpl() : texture_object_(NULL), texture_id_(-1) {}

void* NativeHandleImpl::GetHandle() {
  return texture_object_;
}

int NativeHandleImpl::GetTextureId() {
  return texture_id_;
}

void NativeHandleImpl::SetTextureObject(void* texture_object, int texture_id) {
  texture_object_ = reinterpret_cast<jobject>(texture_object);
  texture_id_ = texture_id;
}

JniNativeHandleBuffer::JniNativeHandleBuffer(void* native_handle,
                                             int width,
                                             int height)
    : NativeHandleBuffer(native_handle, width, height) {}

rtc::scoped_refptr<webrtc::VideoFrameBuffer>
JniNativeHandleBuffer::NativeToI420Buffer() {
  // TODO(pbos): Implement before using this in the encoder pipeline (or
  // remove the RTC_CHECK() in VideoCapture).
  RTC_NOTREACHED();
  return nullptr;
}

AndroidTextureBuffer::AndroidTextureBuffer(
    int width,
    int height,
    const NativeTextureHandleImpl& native_handle,
    const rtc::Callback0<void>& no_longer_used)
    : webrtc::NativeHandleBuffer(&native_handle_, width, height),
      native_handle_(native_handle),
      no_longer_used_cb_(no_longer_used) {}

AndroidTextureBuffer::~AndroidTextureBuffer() {
  no_longer_used_cb_();
}

rtc::scoped_refptr<webrtc::VideoFrameBuffer>
AndroidTextureBuffer::NativeToI420Buffer() {
  RTC_NOTREACHED()
      << "AndroidTextureBuffer::NativeToI420Buffer not implemented.";
  return nullptr;
}

}  // namespace webrtc_jni
