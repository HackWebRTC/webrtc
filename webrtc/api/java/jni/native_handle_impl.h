/*
 *  Copyright 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_API_JAVA_JNI_NATIVE_HANDLE_IMPL_H_
#define WEBRTC_API_JAVA_JNI_NATIVE_HANDLE_IMPL_H_

#include <jni.h>

#include "webrtc/common_video/include/video_frame_buffer.h"
#include "webrtc/common_video/rotation.h"

namespace webrtc_jni {

// Wrapper for texture object.
struct NativeHandleImpl {
  NativeHandleImpl(JNIEnv* jni,
                   jint j_oes_texture_id,
                   jfloatArray j_transform_matrix);

  const int oes_texture_id;
  float sampling_matrix[16];
};

class AndroidTextureBuffer : public webrtc::NativeHandleBuffer {
 public:
  AndroidTextureBuffer(int width,
                       int height,
                       const NativeHandleImpl& native_handle,
                       jobject surface_texture_helper,
                       const rtc::Callback0<void>& no_longer_used);
  ~AndroidTextureBuffer();
  rtc::scoped_refptr<VideoFrameBuffer> NativeToI420Buffer() override;

  rtc::scoped_refptr<AndroidTextureBuffer> ScaleAndRotate(
      int dst_widht,
      int dst_height,
      webrtc::VideoRotation rotation);

 private:
  NativeHandleImpl native_handle_;
  // Raw object pointer, relying on the caller, i.e.,
  // AndroidVideoCapturerJni or the C++ SurfaceTextureHelper, to keep
  // a global reference. TODO(nisse): Make this a reference to the C++
  // SurfaceTextureHelper instead, but that requires some refactoring
  // of AndroidVideoCapturerJni.
  jobject surface_texture_helper_;
  rtc::Callback0<void> no_longer_used_cb_;
};

}  // namespace webrtc_jni

#endif  // WEBRTC_API_JAVA_JNI_NATIVE_HANDLE_IMPL_H_
