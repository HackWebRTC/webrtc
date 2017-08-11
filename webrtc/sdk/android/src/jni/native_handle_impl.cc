/*
 *  Copyright 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/sdk/android/src/jni/native_handle_impl.h"

#include <memory>

#include "webrtc/common_video/include/video_frame_buffer.h"
#include "webrtc/rtc_base/bind.h"
#include "webrtc/rtc_base/checks.h"
#include "webrtc/rtc_base/keep_ref_until_done.h"
#include "webrtc/rtc_base/logging.h"
#include "webrtc/rtc_base/scoped_ref_ptr.h"
#include "webrtc/rtc_base/timeutils.h"
#include "webrtc/sdk/android/src/jni/classreferenceholder.h"
#include "webrtc/sdk/android/src/jni/jni_helpers.h"
#include "webrtc/system_wrappers/include/aligned_malloc.h"

namespace webrtc_jni {

namespace {

class AndroidVideoI420Buffer : public webrtc::I420BufferInterface {
 public:
  // Wraps an existing reference to a Java VideoBuffer. Retain will not be
  // called but release will be called when the C++ object is destroyed.
  static rtc::scoped_refptr<AndroidVideoI420Buffer> WrapReference(
      JNIEnv* jni,
      jmethodID j_release_id,
      int width,
      int height,
      jobject j_video_frame_buffer);

 protected:
  AndroidVideoI420Buffer(JNIEnv* jni,
                         jmethodID j_retain_id,
                         jmethodID j_release_id,
                         int width,
                         int height,
                         jobject j_video_frame_buffer);
  // Should not be called directly. Wraps a reference. Use
  // AndroidVideoI420Buffer::WrapReference instead for clarity.
  AndroidVideoI420Buffer(JNIEnv* jni,
                         jmethodID j_release_id,
                         int width,
                         int height,
                         jobject j_video_frame_buffer);
  ~AndroidVideoI420Buffer();

 private:
  const uint8_t* DataY() const override { return data_y_; }
  const uint8_t* DataU() const override { return data_u_; }
  const uint8_t* DataV() const override { return data_v_; }

  int StrideY() const override { return stride_y_; }
  int StrideU() const override { return stride_u_; }
  int StrideV() const override { return stride_v_; }

  int width() const override { return width_; }
  int height() const override { return height_; }

  const jmethodID j_release_id_;
  const int width_;
  const int height_;
  // Holds a VideoFrame.I420Buffer.
  const ScopedGlobalRef<jobject> j_video_frame_buffer_;

  const uint8_t* data_y_;
  const uint8_t* data_u_;
  const uint8_t* data_v_;
  int stride_y_;
  int stride_u_;
  int stride_v_;
};

rtc::scoped_refptr<AndroidVideoI420Buffer>
AndroidVideoI420Buffer::WrapReference(JNIEnv* jni,
                                      jmethodID j_release_id,
                                      int width,
                                      int height,
                                      jobject j_video_frame_buffer) {
  return new rtc::RefCountedObject<AndroidVideoI420Buffer>(
      jni, j_release_id, width, height, j_video_frame_buffer);
}

AndroidVideoI420Buffer::AndroidVideoI420Buffer(JNIEnv* jni,
                                               jmethodID j_retain_id,
                                               jmethodID j_release_id,
                                               int width,
                                               int height,
                                               jobject j_video_frame_buffer)
    : AndroidVideoI420Buffer(jni,
                             j_release_id,
                             width,
                             height,
                             j_video_frame_buffer) {
  jni->CallVoidMethod(j_video_frame_buffer, j_retain_id);
}

AndroidVideoI420Buffer::AndroidVideoI420Buffer(JNIEnv* jni,
                                               jmethodID j_release_id,
                                               int width,
                                               int height,
                                               jobject j_video_frame_buffer)
    : j_release_id_(j_release_id),
      width_(width),
      height_(height),
      j_video_frame_buffer_(jni, j_video_frame_buffer) {
  jclass j_video_frame_i420_buffer_class =
      FindClass(jni, "org/webrtc/VideoFrame$I420Buffer");
  jmethodID j_get_data_y_id = jni->GetMethodID(
      j_video_frame_i420_buffer_class, "getDataY", "()Ljava/nio/ByteBuffer;");
  jmethodID j_get_data_u_id = jni->GetMethodID(
      j_video_frame_i420_buffer_class, "getDataU", "()Ljava/nio/ByteBuffer;");
  jmethodID j_get_data_v_id = jni->GetMethodID(
      j_video_frame_i420_buffer_class, "getDataV", "()Ljava/nio/ByteBuffer;");
  jmethodID j_get_stride_y_id =
      jni->GetMethodID(j_video_frame_i420_buffer_class, "getStrideY", "()I");
  jmethodID j_get_stride_u_id =
      jni->GetMethodID(j_video_frame_i420_buffer_class, "getStrideU", "()I");
  jmethodID j_get_stride_v_id =
      jni->GetMethodID(j_video_frame_i420_buffer_class, "getStrideV", "()I");

  jobject j_data_y =
      jni->CallObjectMethod(j_video_frame_buffer, j_get_data_y_id);
  jobject j_data_u =
      jni->CallObjectMethod(j_video_frame_buffer, j_get_data_u_id);
  jobject j_data_v =
      jni->CallObjectMethod(j_video_frame_buffer, j_get_data_v_id);

  data_y_ = static_cast<const uint8_t*>(jni->GetDirectBufferAddress(j_data_y));
  data_u_ = static_cast<const uint8_t*>(jni->GetDirectBufferAddress(j_data_u));
  data_v_ = static_cast<const uint8_t*>(jni->GetDirectBufferAddress(j_data_v));

  stride_y_ = jni->CallIntMethod(j_video_frame_buffer, j_get_stride_y_id);
  stride_u_ = jni->CallIntMethod(j_video_frame_buffer, j_get_stride_u_id);
  stride_v_ = jni->CallIntMethod(j_video_frame_buffer, j_get_stride_v_id);
}

AndroidVideoI420Buffer::~AndroidVideoI420Buffer() {
  JNIEnv* jni = AttachCurrentThreadIfNeeded();
  jni->CallVoidMethod(*j_video_frame_buffer_, j_release_id_);
}

}  // namespace

Matrix::Matrix(JNIEnv* jni, jfloatArray a) {
  RTC_CHECK_EQ(16, jni->GetArrayLength(a));
  jfloat* ptr = jni->GetFloatArrayElements(a, nullptr);
  for (int i = 0; i < 16; ++i) {
    elem_[i] = ptr[i];
  }
  jni->ReleaseFloatArrayElements(a, ptr, 0);
}

jfloatArray Matrix::ToJava(JNIEnv* jni) const {
  jfloatArray matrix = jni->NewFloatArray(16);
  jni->SetFloatArrayRegion(matrix, 0, 16, elem_);
  return matrix;
}

void Matrix::Rotate(webrtc::VideoRotation rotation) {
  // Texture coordinates are in the range 0 to 1. The transformation of the last
  // row in each rotation matrix is needed for proper translation, e.g, to
  // mirror x, we don't replace x by -x, but by 1-x.
  switch (rotation) {
    case webrtc::kVideoRotation_0:
      break;
    case webrtc::kVideoRotation_90: {
      const float ROTATE_90[16] =
          { elem_[4], elem_[5], elem_[6], elem_[7],
            -elem_[0], -elem_[1], -elem_[2], -elem_[3],
            elem_[8], elem_[9], elem_[10], elem_[11],
            elem_[0] + elem_[12], elem_[1] + elem_[13],
            elem_[2] + elem_[14], elem_[3] + elem_[15]};
      memcpy(elem_, ROTATE_90, sizeof(elem_));
    } break;
    case webrtc::kVideoRotation_180: {
      const float ROTATE_180[16] =
          { -elem_[0], -elem_[1], -elem_[2], -elem_[3],
            -elem_[4], -elem_[5], -elem_[6], -elem_[7],
            elem_[8], elem_[9], elem_[10], elem_[11],
            elem_[0] + elem_[4] + elem_[12], elem_[1] + elem_[5] + elem_[13],
            elem_[2] + elem_[6] + elem_[14], elem_[3] + elem_[11]+ elem_[15]};
        memcpy(elem_, ROTATE_180, sizeof(elem_));
    } break;
    case webrtc::kVideoRotation_270: {
      const float ROTATE_270[16] =
          { -elem_[4], -elem_[5], -elem_[6], -elem_[7],
            elem_[0], elem_[1], elem_[2], elem_[3],
            elem_[8], elem_[9], elem_[10], elem_[11],
            elem_[4] + elem_[12], elem_[5] + elem_[13],
            elem_[6] + elem_[14], elem_[7] + elem_[15]};
      memcpy(elem_, ROTATE_270, sizeof(elem_));
    } break;
  }
}

// Calculates result = a * b, in column-major order.
void Matrix::Multiply(const float a[16], const float b[16], float result[16]) {
  for (int i = 0; i < 4; ++i) {
    for (int j = 0; j < 4; ++j) {
      float sum = 0;
      for (int k = 0; k < 4; ++k) {
        sum += a[k * 4 + j] * b[i * 4 + k];
      }
      result[i * 4 + j] = sum;
    }
  }
}

// Center crop by keeping xFraction of the width and yFraction of the height,
// so e.g. cropping from 640x480 to 640x360 would use
// xFraction=1, yFraction=360/480.
void Matrix::Crop(float xFraction,
                  float yFraction,
                  float xOffset,
                  float yOffset) {
  const float crop_matrix[16] =
      {xFraction, 0, 0, 0,
       0, yFraction, 0, 0,
       0, 0, 1, 0,
       xOffset, yOffset, 0, 1};
  const Matrix old = *this;
  Multiply(crop_matrix, old.elem_, this->elem_);
}

// Aligning pointer to 64 bytes for improved performance, e.g. use SIMD.
static const int kBufferAlignment = 64;

NativeHandleImpl::NativeHandleImpl(int id, const Matrix& matrix)
    : oes_texture_id(id), sampling_matrix(matrix) {}

NativeHandleImpl::NativeHandleImpl(JNIEnv* jni,
                                   jint j_oes_texture_id,
                                   jfloatArray j_transform_matrix)
    : oes_texture_id(j_oes_texture_id),
      sampling_matrix(jni, j_transform_matrix) {}

AndroidTextureBuffer::AndroidTextureBuffer(
    int width,
    int height,
    const NativeHandleImpl& native_handle,
    jobject surface_texture_helper,
    const rtc::Callback0<void>& no_longer_used)
    : width_(width),
      height_(height),
      native_handle_(native_handle),
      surface_texture_helper_(surface_texture_helper),
      no_longer_used_cb_(no_longer_used) {}

AndroidTextureBuffer::~AndroidTextureBuffer() {
  no_longer_used_cb_();
}

webrtc::VideoFrameBuffer::Type AndroidTextureBuffer::type() const {
  return Type::kNative;
}

NativeHandleImpl AndroidTextureBuffer::native_handle_impl() const {
  return native_handle_;
}

int AndroidTextureBuffer::width() const {
  return width_;
}

int AndroidTextureBuffer::height() const {
  return height_;
}

rtc::scoped_refptr<webrtc::I420BufferInterface> AndroidTextureBuffer::ToI420() {
  int uv_width = (width() + 7) / 8;
  int stride = 8 * uv_width;
  int uv_height = (height() + 1) / 2;
  size_t size = stride * (height() + uv_height);
  // The data is owned by the frame, and the normal case is that the
  // data is deleted by the frame's destructor callback.
  //
  // TODO(nisse): Use an I420BufferPool. We then need to extend that
  // class, and I420Buffer, to support our memory layout.
  // TODO(nisse): Depending on
  // system_wrappers/include/aligned_malloc.h violate current DEPS
  // rules. We get away for now only because it is indirectly included
  // by i420_buffer.h
  std::unique_ptr<uint8_t, webrtc::AlignedFreeDeleter> yuv_data(
      static_cast<uint8_t*>(webrtc::AlignedMalloc(size, kBufferAlignment)));
  // See YuvConverter.java for the required layout.
  uint8_t* y_data = yuv_data.get();
  uint8_t* u_data = y_data + height() * stride;
  uint8_t* v_data = u_data + stride/2;

  rtc::scoped_refptr<webrtc::I420BufferInterface> copy = webrtc::WrapI420Buffer(
      width(), height(), y_data, stride, u_data, stride, v_data, stride,
      rtc::Bind(&webrtc::AlignedFree, yuv_data.release()));

  JNIEnv* jni = AttachCurrentThreadIfNeeded();
  ScopedLocalRefFrame local_ref_frame(jni);

  jmethodID transform_mid = GetMethodID(
      jni,
      GetObjectClass(jni, surface_texture_helper_),
      "textureToYUV",
      "(Ljava/nio/ByteBuffer;IIII[F)V");

  jobject byte_buffer = jni->NewDirectByteBuffer(y_data, size);

  jfloatArray sampling_matrix = native_handle_.sampling_matrix.ToJava(jni);
  jni->CallVoidMethod(surface_texture_helper_,
                      transform_mid,
                      byte_buffer, width(), height(), stride,
                      native_handle_.oes_texture_id, sampling_matrix);
  CHECK_EXCEPTION(jni) << "textureToYUV throwed an exception";

  return copy;
}

rtc::scoped_refptr<AndroidVideoBuffer> AndroidVideoBuffer::WrapReference(
    JNIEnv* jni,
    jmethodID j_release_id,
    int width,
    int height,
    jobject j_video_frame_buffer) {
  return new rtc::RefCountedObject<AndroidVideoBuffer>(
      jni, j_release_id, width, height, j_video_frame_buffer);
}

AndroidVideoBuffer::AndroidVideoBuffer(JNIEnv* jni,
                                       jmethodID j_retain_id,
                                       jmethodID j_release_id,
                                       int width,
                                       int height,
                                       jobject j_video_frame_buffer)
    : AndroidVideoBuffer(jni,
                         j_release_id,
                         width,
                         height,
                         j_video_frame_buffer) {
  jni->CallVoidMethod(j_video_frame_buffer, j_retain_id);
}

AndroidVideoBuffer::AndroidVideoBuffer(JNIEnv* jni,
                                       jmethodID j_release_id,
                                       int width,
                                       int height,
                                       jobject j_video_frame_buffer)
    : j_release_id_(j_release_id),
      width_(width),
      height_(height),
      j_video_frame_buffer_(jni, j_video_frame_buffer) {}

AndroidVideoBuffer::~AndroidVideoBuffer() {
  JNIEnv* jni = AttachCurrentThreadIfNeeded();
  jni->CallVoidMethod(*j_video_frame_buffer_, j_release_id_);
}

jobject AndroidVideoBuffer::video_frame_buffer() const {
  return *j_video_frame_buffer_;
}

webrtc::VideoFrameBuffer::Type AndroidVideoBuffer::type() const {
  return Type::kNative;
}

int AndroidVideoBuffer::width() const {
  return width_;
}

int AndroidVideoBuffer::height() const {
  return height_;
}

rtc::scoped_refptr<webrtc::I420BufferInterface> AndroidVideoBuffer::ToI420() {
  JNIEnv* jni = AttachCurrentThreadIfNeeded();
  ScopedLocalRefFrame local_ref_frame(jni);

  jclass j_video_frame_buffer_class =
      FindClass(jni, "org/webrtc/VideoFrame$Buffer");
  jmethodID j_to_i420_id =
      jni->GetMethodID(j_video_frame_buffer_class, "toI420",
                       "()Lorg/webrtc/VideoFrame$I420Buffer;");

  jobject j_i420_buffer =
      jni->CallObjectMethod(*j_video_frame_buffer_, j_to_i420_id);

  // We don't need to retain the buffer because toI420 returns a new object that
  // we are assumed to take the ownership of.
  return AndroidVideoI420Buffer::WrapReference(jni, j_release_id_, width_,
                                               height_, j_i420_buffer);
}

jobject AndroidVideoBuffer::ToJavaI420Frame(JNIEnv* jni, int rotation) {
  jclass j_byte_buffer_class = jni->FindClass("java/nio/ByteBuffer");
  jclass j_i420_frame_class =
      FindClass(jni, "org/webrtc/VideoRenderer$I420Frame");
  jmethodID j_i420_frame_ctor_id = GetMethodID(
      jni, j_i420_frame_class, "<init>", "(ILorg/webrtc/VideoFrame$Buffer;J)V");
  // Java code just uses the native frame to hold a reference to the buffer so
  // this is okay.
  webrtc::VideoFrame* native_frame = new webrtc::VideoFrame(
      this, 0 /* timestamp */, 0 /* render_time_ms */,
      webrtc::VideoRotation::kVideoRotation_0 /* rotation */);
  return jni->NewObject(j_i420_frame_class, j_i420_frame_ctor_id, rotation,
                        *j_video_frame_buffer_, jlongFromPointer(native_frame));
}

AndroidVideoBufferFactory::AndroidVideoBufferFactory(JNIEnv* jni)
    : j_video_frame_class_(jni, FindClass(jni, "org/webrtc/VideoFrame")),
      j_get_buffer_id_(GetMethodID(jni,
                                   *j_video_frame_class_,
                                   "getBuffer",
                                   "()Lorg/webrtc/VideoFrame$Buffer;")),
      j_get_rotation_id_(
          GetMethodID(jni, *j_video_frame_class_, "getRotation", "()I")),
      j_get_timestamp_ns_id_(
          GetMethodID(jni, *j_video_frame_class_, "getTimestampNs", "()J")),
      j_video_frame_buffer_class_(
          jni,
          FindClass(jni, "org/webrtc/VideoFrame$Buffer")),
      j_retain_id_(
          GetMethodID(jni, *j_video_frame_buffer_class_, "retain", "()V")),
      j_release_id_(
          GetMethodID(jni, *j_video_frame_buffer_class_, "release", "()V")),
      j_get_width_id_(
          GetMethodID(jni, *j_video_frame_buffer_class_, "getWidth", "()I")),
      j_get_height_id_(
          GetMethodID(jni, *j_video_frame_buffer_class_, "getHeight", "()I")) {}

webrtc::VideoFrame AndroidVideoBufferFactory::CreateFrame(
    JNIEnv* jni,
    jobject j_video_frame,
    uint32_t timestamp_rtp) const {
  jobject j_video_frame_buffer =
      jni->CallObjectMethod(j_video_frame, j_get_buffer_id_);
  int rotation = jni->CallIntMethod(j_video_frame, j_get_rotation_id_);
  uint32_t timestamp_ns =
      jni->CallLongMethod(j_video_frame, j_get_timestamp_ns_id_);
  rtc::scoped_refptr<AndroidVideoBuffer> buffer =
      CreateBuffer(jni, j_video_frame_buffer);
  return webrtc::VideoFrame(buffer, timestamp_rtp,
                            timestamp_ns / rtc::kNumNanosecsPerMillisec,
                            static_cast<webrtc::VideoRotation>(rotation));
}

rtc::scoped_refptr<AndroidVideoBuffer> AndroidVideoBufferFactory::WrapBuffer(
    JNIEnv* jni,
    jobject j_video_frame_buffer) const {
  int width = jni->CallIntMethod(j_video_frame_buffer, j_get_width_id_);
  int height = jni->CallIntMethod(j_video_frame_buffer, j_get_height_id_);
  return AndroidVideoBuffer::WrapReference(jni, j_release_id_, width, height,
                                           j_video_frame_buffer);
}

rtc::scoped_refptr<AndroidVideoBuffer> AndroidVideoBufferFactory::CreateBuffer(
    JNIEnv* jni,
    jobject j_video_frame_buffer) const {
  int width = jni->CallIntMethod(j_video_frame_buffer, j_get_width_id_);
  int height = jni->CallIntMethod(j_video_frame_buffer, j_get_height_id_);
  return new rtc::RefCountedObject<AndroidVideoBuffer>(
      jni, j_retain_id_, j_release_id_, width, height, j_video_frame_buffer);
}

JavaVideoFrameFactory::JavaVideoFrameFactory(JNIEnv* jni)
    : j_video_frame_class_(jni, FindClass(jni, "org/webrtc/VideoFrame")) {
  j_video_frame_constructor_id_ =
      GetMethodID(jni, *j_video_frame_class_, "<init>",
                  "(Lorg/webrtc/VideoFrame$Buffer;IJ)V");
}

jobject JavaVideoFrameFactory::ToJavaFrame(
    JNIEnv* jni,
    const webrtc::VideoFrame& frame) const {
  RTC_DCHECK(frame.video_frame_buffer()->type() ==
             webrtc::VideoFrameBuffer::Type::kNative);
  AndroidVideoFrameBuffer* android_buffer =
      static_cast<AndroidVideoFrameBuffer*>(frame.video_frame_buffer().get());
  RTC_DCHECK(android_buffer->android_type() ==
             AndroidVideoFrameBuffer::AndroidType::kJavaBuffer);
  AndroidVideoBuffer* android_video_buffer =
      static_cast<AndroidVideoBuffer*>(android_buffer);
  jobject buffer = android_video_buffer->video_frame_buffer();
  return jni->NewObject(
      *j_video_frame_class_, j_video_frame_constructor_id_, buffer,
      static_cast<jint>(frame.rotation()),
      static_cast<jlong>(frame.timestamp_us() * rtc::kNumNanosecsPerMicrosec));
}

}  // namespace webrtc_jni
