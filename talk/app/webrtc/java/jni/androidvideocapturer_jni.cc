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

#include "talk/app/webrtc/java/jni/androidvideocapturer_jni.h"
#include "talk/app/webrtc/java/jni/classreferenceholder.h"
#include "talk/app/webrtc/java/jni/native_handle_impl.h"
#include "webrtc/base/bind.h"
#include "webrtc/common_video/libyuv/include/webrtc_libyuv.h"

namespace webrtc_jni {

jobject AndroidVideoCapturerJni::application_context_ = nullptr;

// static
int AndroidVideoCapturerJni::SetAndroidObjects(JNIEnv* jni,
                                               jobject appliction_context) {
  if (application_context_) {
    jni->DeleteGlobalRef(application_context_);
  }
  application_context_ = NewGlobalRef(jni, appliction_context);

  return 0;
}

AndroidVideoCapturerJni::AndroidVideoCapturerJni(JNIEnv* jni,
                                                 jobject j_video_capturer)
    : j_capturer_global_(jni, j_video_capturer),
      j_video_capturer_class_(
          jni, FindClass(jni, "org/webrtc/VideoCapturerAndroid")),
      j_observer_class_(
          jni,
          FindClass(jni,
                    "org/webrtc/VideoCapturerAndroid$NativeObserver")),
      capturer_(nullptr) {
  LOG(LS_INFO) << "AndroidVideoCapturerJni ctor";
  thread_checker_.DetachFromThread();
}

AndroidVideoCapturerJni::~AndroidVideoCapturerJni() {
  LOG(LS_INFO) << "AndroidVideoCapturerJni dtor";
  jni()->CallVoidMethod(
      *j_capturer_global_,
      GetMethodID(jni(), *j_video_capturer_class_, "release", "()V"));
  CHECK_EXCEPTION(jni()) << "error during VideoCapturerAndroid.release()";
}

void AndroidVideoCapturerJni::Start(int width, int height, int framerate,
                                    webrtc::AndroidVideoCapturer* capturer) {
  LOG(LS_INFO) << "AndroidVideoCapturerJni start";
  RTC_DCHECK(thread_checker_.CalledOnValidThread());
  {
    rtc::CritScope cs(&capturer_lock_);
    RTC_CHECK(capturer_ == nullptr);
    RTC_CHECK(invoker_.get() == nullptr);
    capturer_ = capturer;
    invoker_.reset(new rtc::GuardedAsyncInvoker());
  }
  jobject j_frame_observer =
      jni()->NewObject(*j_observer_class_,
                       GetMethodID(jni(), *j_observer_class_, "<init>", "(J)V"),
                       jlongFromPointer(this));
  CHECK_EXCEPTION(jni()) << "error during NewObject";

  jmethodID m = GetMethodID(
      jni(), *j_video_capturer_class_, "startCapture",
      "(IIILandroid/content/Context;"
      "Lorg/webrtc/VideoCapturerAndroid$CapturerObserver;)V");
  jni()->CallVoidMethod(*j_capturer_global_,
                        m, width, height,
                        framerate,
                        application_context_,
                        j_frame_observer);
  CHECK_EXCEPTION(jni()) << "error during VideoCapturerAndroid.startCapture";
}

void AndroidVideoCapturerJni::Stop() {
  LOG(LS_INFO) << "AndroidVideoCapturerJni stop";
  RTC_DCHECK(thread_checker_.CalledOnValidThread());
  {
    rtc::CritScope cs(&capturer_lock_);
    // Destroying |invoker_| will cancel all pending calls to |capturer_|.
    invoker_ = nullptr;
    capturer_ = nullptr;
  }
  jmethodID m = GetMethodID(jni(), *j_video_capturer_class_,
                            "stopCapture", "()V");
  jni()->CallVoidMethod(*j_capturer_global_, m);
  CHECK_EXCEPTION(jni()) << "error during VideoCapturerAndroid.stopCapture";
  LOG(LS_INFO) << "AndroidVideoCapturerJni stop done";
}

template <typename... Args>
void AndroidVideoCapturerJni::AsyncCapturerInvoke(
    const char* method_name,
    void (webrtc::AndroidVideoCapturer::*method)(Args...),
    typename Identity<Args>::type... args) {
  rtc::CritScope cs(&capturer_lock_);
  if (!invoker_) {
    LOG(LS_WARNING) << method_name << "() called for closed capturer.";
    return;
  }
  invoker_->AsyncInvoke<void>(rtc::Bind(method, capturer_, args...));
}

void AndroidVideoCapturerJni::ReturnBuffer(int64_t time_stamp) {
  jmethodID m = GetMethodID(jni(), *j_video_capturer_class_,
                            "returnBuffer", "(J)V");
  jni()->CallVoidMethod(*j_capturer_global_, m, time_stamp);
  CHECK_EXCEPTION(jni()) << "error during VideoCapturerAndroid.returnBuffer";
}

std::string AndroidVideoCapturerJni::GetSupportedFormats() {
  jmethodID m =
      GetMethodID(jni(), *j_video_capturer_class_,
                  "getSupportedFormatsAsJson", "()Ljava/lang/String;");
  jstring j_json_caps =
      (jstring) jni()->CallObjectMethod(*j_capturer_global_, m);
  CHECK_EXCEPTION(jni()) << "error during supportedFormatsAsJson";
  return JavaToStdString(jni(), j_json_caps);
}

void AndroidVideoCapturerJni::OnCapturerStarted(bool success) {
  LOG(LS_INFO) << "AndroidVideoCapturerJni capture started: " << success;
  AsyncCapturerInvoke("OnCapturerStarted",
                      &webrtc::AndroidVideoCapturer::OnCapturerStarted,
                      success);
}

void AndroidVideoCapturerJni::OnMemoryBufferFrame(void* video_frame,
                                                  int length,
                                                  int width,
                                                  int height,
                                                  int rotation,
                                                  int64_t timestamp_ns) {
  const uint8_t* y_plane = static_cast<uint8_t*>(video_frame);
  // Android guarantees that the stride is a multiple of 16.
  // http://developer.android.com/reference/android/hardware/Camera.Parameters.html#setPreviewFormat%28int%29
  int y_stride;
  int uv_stride;
  webrtc::Calc16ByteAlignedStride(width, &y_stride, &uv_stride);
  const uint8_t* v_plane = y_plane + y_stride * height;
  const uint8_t* u_plane =
      v_plane + uv_stride * webrtc::AlignInt(height, 2) / 2;

  // Wrap the Java buffer, and call ReturnBuffer() in the wrapped
  // VideoFrameBuffer destructor.
  rtc::scoped_refptr<webrtc::VideoFrameBuffer> buffer(
      new rtc::RefCountedObject<webrtc::WrappedI420Buffer>(
          width, height, y_plane, y_stride, u_plane, uv_stride, v_plane,
          uv_stride,
          rtc::Bind(&AndroidVideoCapturerJni::ReturnBuffer, this,
                    timestamp_ns)));
  AsyncCapturerInvoke("OnIncomingFrame",
                      &webrtc::AndroidVideoCapturer::OnIncomingFrame,
                      buffer, rotation, timestamp_ns);
}

void AndroidVideoCapturerJni::OnTextureFrame(
    int width,
    int height,
    int64_t timestamp_ns,
    const NativeTextureHandleImpl& handle) {
  // TODO(magjed): Fix this. See bug webrtc:4993.
  RTC_NOTREACHED()
      << "The rest of the stack for Android expects the native "
         "handle to be a NativeHandleImpl with a SurfaceTexture, not a "
         "NativeTextureHandleImpl";
  rtc::scoped_refptr<webrtc::VideoFrameBuffer> buffer(
      new rtc::RefCountedObject<AndroidTextureBuffer>(
          width, height, handle,
          rtc::Bind(&AndroidVideoCapturerJni::ReturnBuffer, this,
                    timestamp_ns)));
  AsyncCapturerInvoke("OnIncomingFrame",
                      &webrtc::AndroidVideoCapturer::OnIncomingFrame,
                      buffer, 0, timestamp_ns);
}

void AndroidVideoCapturerJni::OnOutputFormatRequest(int width,
                                                    int height,
                                                    int fps) {
  AsyncCapturerInvoke("OnOutputFormatRequest",
                      &webrtc::AndroidVideoCapturer::OnOutputFormatRequest,
                      width, height, fps);
}

JNIEnv* AndroidVideoCapturerJni::jni() { return AttachCurrentThreadIfNeeded(); }

JOW(void,
    VideoCapturerAndroid_00024NativeObserver_nativeOnByteBufferFrameCaptured)
    (JNIEnv* jni, jclass, jlong j_capturer, jbyteArray j_frame, jint length,
        jint width, jint height, jint rotation, jlong timestamp) {
  jboolean is_copy = true;
  jbyte* bytes = jni->GetByteArrayElements(j_frame, &is_copy);
  // If this is a copy of the original frame, it means that the memory
  // is not direct memory and thus VideoCapturerAndroid does not guarantee
  // that the memory is valid when we have released |j_frame|.
  // TODO(magjed): Move ReleaseByteArrayElements() into ReturnBuffer() and
  // remove this check.
  RTC_CHECK(!is_copy)
      << "NativeObserver_nativeOnFrameCaptured: frame is a copy";
  reinterpret_cast<AndroidVideoCapturerJni*>(j_capturer)
      ->OnMemoryBufferFrame(bytes, length, width, height, rotation, timestamp);
  jni->ReleaseByteArrayElements(j_frame, bytes, JNI_ABORT);
}

JOW(void, VideoCapturerAndroid_00024NativeObserver_nativeOnTextureFrameCaptured)
    (JNIEnv* jni, jclass, jlong j_capturer, jint j_width, jint j_height,
        jint j_oes_texture_id, jfloatArray j_transform_matrix,
        jlong j_timestamp) {
   reinterpret_cast<AndroidVideoCapturerJni*>(j_capturer)
         ->OnTextureFrame(j_width, j_height, j_timestamp,
                          NativeTextureHandleImpl(jni, j_oes_texture_id,
                                                  j_transform_matrix));
}

JOW(void, VideoCapturerAndroid_00024NativeObserver_nativeCapturerStarted)
    (JNIEnv* jni, jclass, jlong j_capturer, jboolean j_success) {
  LOG(LS_INFO) << "NativeObserver_nativeCapturerStarted";
  reinterpret_cast<AndroidVideoCapturerJni*>(j_capturer)->OnCapturerStarted(
      j_success);
}

JOW(void, VideoCapturerAndroid_00024NativeObserver_nativeOnOutputFormatRequest)
    (JNIEnv* jni, jclass, jlong j_capturer, jint j_width, jint j_height,
        jint j_fps) {
  LOG(LS_INFO) << "NativeObserver_nativeOnOutputFormatRequest";
  reinterpret_cast<AndroidVideoCapturerJni*>(j_capturer)->OnOutputFormatRequest(
      j_width, j_height, j_fps);
}

JOW(jlong, VideoCapturerAndroid_nativeCreateVideoCapturer)
    (JNIEnv* jni, jclass, jobject j_video_capturer) {
  rtc::scoped_refptr<webrtc::AndroidVideoCapturerDelegate> delegate =
      new rtc::RefCountedObject<AndroidVideoCapturerJni>(jni, j_video_capturer);
  rtc::scoped_ptr<cricket::VideoCapturer> capturer(
      new webrtc::AndroidVideoCapturer(delegate));
  // Caller takes ownership of the cricket::VideoCapturer* pointer.
  return jlongFromPointer(capturer.release());
}

}  // namespace webrtc_jni
