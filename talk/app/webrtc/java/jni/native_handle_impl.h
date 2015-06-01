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

#include "webrtc/base/checks.h"
#include "webrtc/common_video/interface/video_frame_buffer.h"

namespace webrtc_jni {

// Wrapper for texture object.
class NativeHandleImpl {
 public:
  NativeHandleImpl() : texture_object_(NULL), texture_id_(-1) {}

  void* GetHandle() {
    return texture_object_;
  }
  int GetTextureId() {
    return texture_id_;
  }
  void SetTextureObject(void *texture_object, int texture_id) {
    texture_object_ = reinterpret_cast<jobject>(texture_object);
    texture_id_ = texture_id;
  }

 private:
  jobject texture_object_;
  int32_t texture_id_;
};

class JniNativeHandleBuffer : public webrtc::NativeHandleBuffer {
 public:
  JniNativeHandleBuffer(void* native_handle, int width, int height)
      : NativeHandleBuffer(native_handle, width, height) {}

  // TODO(pbos): Override destructor to release native handle, at the moment the
  // native handle is not released based on refcount.

 private:
  rtc::scoped_refptr<VideoFrameBuffer> NativeToI420Buffer() override {
    // TODO(pbos): Implement before using this in the encoder pipeline (or
    // remove the CHECK() in VideoCapture).
    RTC_NOTREACHED();
    return nullptr;
  }
};

}  // namespace webrtc_jni

#endif  // TALK_APP_WEBRTC_JAVA_JNI_NATIVE_HANDLE_IMPL_H_
