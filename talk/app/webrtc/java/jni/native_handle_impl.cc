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

#include "talk/app/webrtc/java/jni/jni_helpers.h"
#include "webrtc/base/bind.h"
#include "webrtc/base/checks.h"
#include "webrtc/base/keep_ref_until_done.h"
#include "webrtc/base/scoped_ptr.h"
#include "webrtc/base/scoped_ref_ptr.h"
#include "webrtc/base/logging.h"

using webrtc::NativeHandleBuffer;

namespace {

void RotateMatrix(float a[16], webrtc::VideoRotation rotation) {
  // Texture coordinates are in the range 0 to 1. The transformation of the last
  // row in each rotation matrix is needed for proper translation, e.g, to
  // mirror x, we don't replace x by -x, but by 1-x.
  switch (rotation) {
    case webrtc::kVideoRotation_0:
      break;
    case webrtc::kVideoRotation_90: {
      const float ROTATE_90[16] =
          { a[4], a[5], a[6], a[7],
            -a[0], -a[1], -a[2], -a[3],
            a[8], a[9], a[10], a[11],
            a[0] + a[12], a[1] + a[13], a[2] + a[14], a[3] + a[15]};
      memcpy(a, ROTATE_90, sizeof(ROTATE_90));
    } break;
    case webrtc::kVideoRotation_180: {
      const float ROTATE_180[16] =
          { -a[0], -a[1], -a[2], -a[3],
            -a[4], -a[5], -a[6], -a[7],
            a[8], a[9], a[10], a[11],
            a[0] + a[4] + a[12], a[1] +a[5] + a[13], a[2] + a[6] + a[14],
            a[3] + a[11]+ a[15]};
        memcpy(a, ROTATE_180, sizeof(ROTATE_180));
      }
      break;
    case webrtc::kVideoRotation_270: {
      const float ROTATE_270[16] =
          { -a[4], -a[5], -a[6], -a[7],
            a[0], a[1], a[2], a[3],
            a[8], a[9], a[10], a[11],
            a[4] + a[12], a[5] + a[13], a[6] + a[14], a[7] + a[15]};
        memcpy(a, ROTATE_270, sizeof(ROTATE_270));
    } break;
  }
}

}  // anonymouse namespace

namespace webrtc_jni {

// Aligning pointer to 64 bytes for improved performance, e.g. use SIMD.
static const int kBufferAlignment = 64;

NativeHandleImpl::NativeHandleImpl(JNIEnv* jni,
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

AndroidTextureBuffer::AndroidTextureBuffer(
    int width,
    int height,
    const NativeHandleImpl& native_handle,
    jobject surface_texture_helper,
    const rtc::Callback0<void>& no_longer_used)
    : webrtc::NativeHandleBuffer(&native_handle_, width, height),
      native_handle_(native_handle),
      surface_texture_helper_(surface_texture_helper),
      no_longer_used_cb_(no_longer_used) {}

AndroidTextureBuffer::~AndroidTextureBuffer() {
  no_longer_used_cb_();
}

rtc::scoped_refptr<webrtc::VideoFrameBuffer>
AndroidTextureBuffer::NativeToI420Buffer() {
  int uv_width = (width()+7) / 8;
  int stride = 8 * uv_width;
  int uv_height = (height()+1)/2;
  size_t size = stride * (height() + uv_height);
  // The data is owned by the frame, and the normal case is that the
  // data is deleted by the frame's destructor callback.
  //
  // TODO(nisse): Use an I420BufferPool. We then need to extend that
  // class, and I420Buffer, to support our memory layout.
  rtc::scoped_ptr<uint8_t, webrtc::AlignedFreeDeleter> yuv_data(
      static_cast<uint8_t*>(webrtc::AlignedMalloc(size, kBufferAlignment)));
  // See SurfaceTextureHelper.java for the required layout.
  uint8_t* y_data = yuv_data.get();
  uint8_t* u_data = y_data + height() * stride;
  uint8_t* v_data = u_data + stride/2;

  rtc::scoped_refptr<webrtc::VideoFrameBuffer> copy =
    new rtc::RefCountedObject<webrtc::WrappedI420Buffer>(
        width(), height(),
        y_data, stride,
        u_data, stride,
        v_data, stride,
        rtc::Bind(&webrtc::AlignedFree, yuv_data.release()));

  JNIEnv* jni = AttachCurrentThreadIfNeeded();
  ScopedLocalRefFrame local_ref_frame(jni);

  jmethodID transform_mid = GetMethodID(
      jni,
      GetObjectClass(jni, surface_texture_helper_),
      "textureToYUV",
      "(Ljava/nio/ByteBuffer;IIII[F)V");

  jobject byte_buffer = jni->NewDirectByteBuffer(y_data, size);

  // TODO(nisse): Keep java transform matrix around.
  jfloatArray sampling_matrix = jni->NewFloatArray(16);
  jni->SetFloatArrayRegion(sampling_matrix, 0, 16,
                           native_handle_.sampling_matrix);

  jni->CallVoidMethod(surface_texture_helper_,
                      transform_mid,
                      byte_buffer, width(), height(), stride,
                      native_handle_.oes_texture_id, sampling_matrix);
  CHECK_EXCEPTION(jni) << "textureToYUV throwed an exception";

  return copy;
}

rtc::scoped_refptr<AndroidTextureBuffer>
AndroidTextureBuffer::ScaleAndRotate(int dst_widht,
                                     int dst_height,
                                     webrtc::VideoRotation rotation) {
  if (width() == dst_widht && height() == dst_height &&
      rotation == webrtc::kVideoRotation_0) {
    return this;
  }
  int rotated_width = (rotation % 180 == 0) ? dst_widht : dst_height;
  int rotated_height = (rotation % 180 == 0) ? dst_height : dst_widht;

  // Here we use Bind magic to add a reference count to |this| until the newly
  // created AndroidTextureBuffer is destructed
  rtc::scoped_refptr<AndroidTextureBuffer> buffer(
      new rtc::RefCountedObject<AndroidTextureBuffer>(
          rotated_width, rotated_height, native_handle_,
          surface_texture_helper_, rtc::KeepRefUntilDone(this)));

  RotateMatrix(buffer->native_handle_.sampling_matrix, rotation);
  return buffer;
}

}  // namespace webrtc_jni
