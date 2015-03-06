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

#include "webrtc/common_video/interface/native_handle.h"

namespace webrtc_jni {

// Wrapper for texture object in TextureBuffer.
class NativeHandleImpl : public webrtc::NativeHandle {
 public:
  NativeHandleImpl() :
    ref_count_(0), texture_object_(NULL), texture_id_(-1) {}
  virtual ~NativeHandleImpl() {}
  virtual int32_t AddRef() {
    return ++ref_count_;
  }
  virtual int32_t Release() {
    return --ref_count_;
  }
  virtual void* GetHandle() {
    return texture_object_;
  }
  int GetTextureId() {
    return texture_id_;
  }
  void SetTextureObject(void *texture_object, int texture_id) {
    texture_object_ = reinterpret_cast<jobject>(texture_object);
    texture_id_ = texture_id;
  }
  int32_t ref_count() {
    return ref_count_;
  }

 private:
  int32_t ref_count_;
  jobject texture_object_;
  int32_t texture_id_;
};

}  // namespace webrtc_jni

#endif  // TALK_APP_WEBRTC_JAVA_JNI_NATIVE_HANDLE_IMPL_H_
