/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <jni.h>

#include "api/video/video_frame.h"
#include "media/base/videosinkinterface.h"
#include "sdk/android/generated_video_jni/jni/VideoRenderer_jni.h"
#include "sdk/android/src/jni/jni_helpers.h"
#include "sdk/android/src/jni/videoframe.h"

namespace webrtc {
namespace jni {

// Wrapper dispatching rtc::VideoSinkInterface to a Java VideoRenderer
// instance.
class JavaVideoRendererWrapper : public rtc::VideoSinkInterface<VideoFrame> {
 public:
  JavaVideoRendererWrapper(JNIEnv* jni, jobject j_callbacks)
      : j_callbacks_(jni, j_callbacks) {}

  virtual ~JavaVideoRendererWrapper() {}

  void OnFrame(const VideoFrame& video_frame) override {
    JNIEnv* env = AttachCurrentThreadIfNeeded();
    ScopedLocalRefFrame local_ref_frame(env);

    jobject j_frame;
    if (video_frame.video_frame_buffer()->type() ==
        VideoFrameBuffer::Type::kNative) {
      AndroidVideoFrameBuffer* android_buffer =
          static_cast<AndroidVideoFrameBuffer*>(
              video_frame.video_frame_buffer().get());
      switch (android_buffer->android_type()) {
        case AndroidVideoFrameBuffer::AndroidType::kTextureBuffer:
          j_frame = ToJavaTextureFrame(env, video_frame);
          break;
        case AndroidVideoFrameBuffer::AndroidType::kJavaBuffer:
          j_frame = FromWrappedJavaBuffer(env, video_frame);
          break;
        default:
          RTC_NOTREACHED();
      }
    } else {
      j_frame = ToJavaI420Frame(env, video_frame);
    }
    // |j_callbacks_| is responsible for releasing |j_frame| with
    // VideoRenderer.renderFrameDone().
    Java_Callbacks_renderFrame(env, *j_callbacks_, j_frame);
  }

 private:
  // Make a shallow copy of |frame| to be used with Java. The callee has
  // ownership of the frame, and the frame should be released with
  // VideoRenderer.releaseNativeFrame().
  static jlong javaShallowCopy(const VideoFrame& frame) {
    return jlongFromPointer(new VideoFrame(frame));
  }

  // Return a VideoRenderer.I420Frame referring to the data in |frame|.
  jobject FromWrappedJavaBuffer(JNIEnv* env, const VideoFrame& frame) {
    return Java_I420Frame_Constructor(
        env, frame.rotation(),
        static_cast<AndroidVideoBuffer*>(frame.video_frame_buffer().get())
            ->video_frame_buffer(),
        javaShallowCopy(frame));
  }

  // Return a VideoRenderer.I420Frame referring to the data in |frame|.
  jobject ToJavaI420Frame(JNIEnv* env, const VideoFrame& frame) {
    rtc::scoped_refptr<I420BufferInterface> i420_buffer =
        frame.video_frame_buffer()->ToI420();
    jobject y_buffer = env->NewDirectByteBuffer(
        const_cast<uint8_t*>(i420_buffer->DataY()),
        i420_buffer->StrideY() * i420_buffer->height());
    size_t chroma_height = i420_buffer->ChromaHeight();
    jobject u_buffer =
        env->NewDirectByteBuffer(const_cast<uint8_t*>(i420_buffer->DataU()),
                                 i420_buffer->StrideU() * chroma_height);
    jobject v_buffer =
        env->NewDirectByteBuffer(const_cast<uint8_t*>(i420_buffer->DataV()),
                                 i420_buffer->StrideV() * chroma_height);
    return Java_I420Frame_createI420Frame(
        env, frame.width(), frame.height(), static_cast<int>(frame.rotation()),
        i420_buffer->StrideY(), y_buffer, i420_buffer->StrideU(), u_buffer,
        i420_buffer->StrideV(), v_buffer, javaShallowCopy(frame));
  }

  // Return a VideoRenderer.I420Frame referring texture object in |frame|.
  jobject ToJavaTextureFrame(JNIEnv* env, const VideoFrame& frame) {
    NativeHandleImpl handle =
        static_cast<AndroidTextureBuffer*>(frame.video_frame_buffer().get())
            ->native_handle_impl();
    jfloatArray sampling_matrix = handle.sampling_matrix.ToJava(env);

    return Java_I420Frame_createTextureFrame(
        env, frame.width(), frame.height(), static_cast<int>(frame.rotation()),
        handle.oes_texture_id, sampling_matrix, javaShallowCopy(frame));
  }

  ScopedGlobalRef<jobject> j_callbacks_;
};

JNI_FUNCTION_DECLARATION(void,
                         VideoRenderer_freeWrappedVideoRenderer,
                         JNIEnv*,
                         jclass,
                         jlong j_p) {
  delete reinterpret_cast<JavaVideoRendererWrapper*>(j_p);
}

JNI_FUNCTION_DECLARATION(void,
                         VideoRenderer_releaseNativeFrame,
                         JNIEnv* jni,
                         jclass,
                         jlong j_frame_ptr) {
  delete reinterpret_cast<const VideoFrame*>(j_frame_ptr);
}

JNI_FUNCTION_DECLARATION(jlong,
                         VideoRenderer_createNativeVideoRenderer,
                         JNIEnv* jni,
                         jclass,
                         jobject j_callbacks) {
  std::unique_ptr<JavaVideoRendererWrapper> renderer(
      new JavaVideoRendererWrapper(jni, j_callbacks));
  return jlongFromPointer(renderer.release());
}

JNI_FUNCTION_DECLARATION(void,
                         VideoRenderer_copyPlaneNative,
                         JNIEnv* jni,
                         jclass,
                         jobject j_src_buffer,
                         jint width,
                         jint height,
                         jint src_stride,
                         jobject j_dst_buffer,
                         jint dst_stride) {
  size_t src_size = jni->GetDirectBufferCapacity(j_src_buffer);
  size_t dst_size = jni->GetDirectBufferCapacity(j_dst_buffer);
  RTC_CHECK(src_stride >= width) << "Wrong source stride " << src_stride;
  RTC_CHECK(dst_stride >= width) << "Wrong destination stride " << dst_stride;
  RTC_CHECK(src_size >= src_stride * height)
      << "Insufficient source buffer capacity " << src_size;
  RTC_CHECK(dst_size >= dst_stride * height)
      << "Insufficient destination buffer capacity " << dst_size;
  uint8_t* src =
      reinterpret_cast<uint8_t*>(jni->GetDirectBufferAddress(j_src_buffer));
  uint8_t* dst =
      reinterpret_cast<uint8_t*>(jni->GetDirectBufferAddress(j_dst_buffer));
  if (src_stride == dst_stride) {
    memcpy(dst, src, src_stride * height);
  } else {
    for (int i = 0; i < height; i++) {
      memcpy(dst, src, width);
      src += src_stride;
      dst += dst_stride;
    }
  }
}

}  // namespace jni
}  // namespace webrtc
