/*
 *  Copyright 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/api/java/jni/androidvideocapturer_jni.h"
#include "webrtc/api/java/jni/classreferenceholder.h"
#include "webrtc/api/java/jni/native_handle_impl.h"
#include "webrtc/api/java/jni/surfacetexturehelper_jni.h"
#include "third_party/libyuv/include/libyuv/convert.h"
#include "webrtc/base/bind.h"

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

AndroidVideoCapturerJni::AndroidVideoCapturerJni(
    JNIEnv* jni,
    jobject j_video_capturer,
    jobject j_egl_context)
    : j_video_capturer_(jni, j_video_capturer),
      j_video_capturer_class_(
          jni, FindClass(jni, "org/webrtc/VideoCapturer")),
      j_observer_class_(
          jni,
          FindClass(jni,
                    "org/webrtc/VideoCapturer$NativeObserver")),
      surface_texture_helper_(new rtc::RefCountedObject<SurfaceTextureHelper>(
          jni, "Camera SurfaceTextureHelper", j_egl_context)),
      capturer_(nullptr) {
  LOG(LS_INFO) << "AndroidVideoCapturerJni ctor";
  thread_checker_.DetachFromThread();
}

AndroidVideoCapturerJni::~AndroidVideoCapturerJni() {
  LOG(LS_INFO) << "AndroidVideoCapturerJni dtor";
  jni()->CallVoidMethod(
      *j_video_capturer_,
      GetMethodID(jni(), *j_video_capturer_class_, "dispose", "()V"));
  CHECK_EXCEPTION(jni()) << "error during VideoCapturer.dispose()";
  jni()->CallVoidMethod(
      surface_texture_helper_->GetJavaSurfaceTextureHelper(),
      GetMethodID(jni(), FindClass(jni(), "org/webrtc/SurfaceTextureHelper"),
                  "dispose", "()V"));
  CHECK_EXCEPTION(jni()) << "error during SurfaceTextureHelper.dispose()";
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
      "(IIILorg/webrtc/SurfaceTextureHelper;Landroid/content/Context;"
      "Lorg/webrtc/VideoCapturer$CapturerObserver;)V");
  jni()->CallVoidMethod(*j_video_capturer_,
                        m, width, height,
                        framerate,
                        surface_texture_helper_->GetJavaSurfaceTextureHelper(),
                        application_context_,
                        j_frame_observer);
  CHECK_EXCEPTION(jni()) << "error during VideoCapturer.startCapture";
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
  jni()->CallVoidMethod(*j_video_capturer_, m);
  CHECK_EXCEPTION(jni()) << "error during VideoCapturer.stopCapture";
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

std::vector<cricket::VideoFormat>
AndroidVideoCapturerJni::GetSupportedFormats() {
  JNIEnv* jni = AttachCurrentThreadIfNeeded();
  jobject j_list_of_formats = jni->CallObjectMethod(
      *j_video_capturer_,
      GetMethodID(jni, *j_video_capturer_class_, "getSupportedFormats",
                  "()Ljava/util/List;"));
  CHECK_EXCEPTION(jni) << "error during getSupportedFormats";

  // Extract Java List<CaptureFormat> to std::vector<cricket::VideoFormat>.
  jclass j_list_class = jni->FindClass("java/util/List");
  jclass j_format_class =
      jni->FindClass("org/webrtc/CameraEnumerationAndroid$CaptureFormat");
  const int size = jni->CallIntMethod(
      j_list_of_formats, GetMethodID(jni, j_list_class, "size", "()I"));
  jmethodID j_get =
      GetMethodID(jni, j_list_class, "get", "(I)Ljava/lang/Object;");
  jfieldID j_width_field = GetFieldID(jni, j_format_class, "width", "I");
  jfieldID j_height_field = GetFieldID(jni, j_format_class, "height", "I");
  jfieldID j_max_framerate_field =
      GetFieldID(jni, j_format_class, "maxFramerate", "I");

  std::vector<cricket::VideoFormat> formats;
  formats.reserve(size);
  for (int i = 0; i < size; ++i) {
    jobject j_format = jni->CallObjectMethod(j_list_of_formats, j_get, i);
    const int frame_interval = cricket::VideoFormat::FpsToInterval(
        (GetIntField(jni, j_format, j_max_framerate_field) + 999) / 1000);
    formats.emplace_back(GetIntField(jni, j_format, j_width_field),
                         GetIntField(jni, j_format, j_height_field),
                         frame_interval, cricket::FOURCC_NV21);
  }
  CHECK_EXCEPTION(jni) << "error while extracting formats";
  return formats;
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
  const uint8_t* vu_plane = y_plane + width * height;

  rtc::scoped_refptr<webrtc::VideoFrameBuffer> buffer =
      buffer_pool_.CreateBuffer(width, height);
  libyuv::NV21ToI420(
      y_plane, width,
      vu_plane, width,
      buffer->MutableData(webrtc::kYPlane), buffer->stride(webrtc::kYPlane),
      buffer->MutableData(webrtc::kUPlane), buffer->stride(webrtc::kUPlane),
      buffer->MutableData(webrtc::kVPlane), buffer->stride(webrtc::kVPlane),
      width, height);
  AsyncCapturerInvoke("OnIncomingFrame",
                      &webrtc::AndroidVideoCapturer::OnIncomingFrame,
                      buffer, rotation, timestamp_ns);
}

void AndroidVideoCapturerJni::OnTextureFrame(int width,
                                             int height,
                                             int rotation,
                                             int64_t timestamp_ns,
                                             const NativeHandleImpl& handle) {
  rtc::scoped_refptr<webrtc::VideoFrameBuffer> buffer(
      surface_texture_helper_->CreateTextureFrame(width, height, handle));

  AsyncCapturerInvoke("OnIncomingFrame",
                      &webrtc::AndroidVideoCapturer::OnIncomingFrame,
                      buffer, rotation, timestamp_ns);
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
    VideoCapturer_00024NativeObserver_nativeOnByteBufferFrameCaptured)
    (JNIEnv* jni, jclass, jlong j_capturer, jbyteArray j_frame, jint length,
        jint width, jint height, jint rotation, jlong timestamp) {
  jboolean is_copy = true;
  jbyte* bytes = jni->GetByteArrayElements(j_frame, &is_copy);
  reinterpret_cast<AndroidVideoCapturerJni*>(j_capturer)
      ->OnMemoryBufferFrame(bytes, length, width, height, rotation, timestamp);
  jni->ReleaseByteArrayElements(j_frame, bytes, JNI_ABORT);
}

JOW(void, VideoCapturer_00024NativeObserver_nativeOnTextureFrameCaptured)
    (JNIEnv* jni, jclass, jlong j_capturer, jint j_width, jint j_height,
        jint j_oes_texture_id, jfloatArray j_transform_matrix,
        jint j_rotation, jlong j_timestamp) {
   reinterpret_cast<AndroidVideoCapturerJni*>(j_capturer)
         ->OnTextureFrame(j_width, j_height, j_rotation, j_timestamp,
                          NativeHandleImpl(jni, j_oes_texture_id,
                                           j_transform_matrix));
}

JOW(void, VideoCapturer_00024NativeObserver_nativeCapturerStarted)
    (JNIEnv* jni, jclass, jlong j_capturer, jboolean j_success) {
  LOG(LS_INFO) << "NativeObserver_nativeCapturerStarted";
  reinterpret_cast<AndroidVideoCapturerJni*>(j_capturer)->OnCapturerStarted(
      j_success);
}

JOW(void, VideoCapturer_00024NativeObserver_nativeOnOutputFormatRequest)
    (JNIEnv* jni, jclass, jlong j_capturer, jint j_width, jint j_height,
        jint j_fps) {
  LOG(LS_INFO) << "NativeObserver_nativeOnOutputFormatRequest";
  reinterpret_cast<AndroidVideoCapturerJni*>(j_capturer)->OnOutputFormatRequest(
      j_width, j_height, j_fps);
}

}  // namespace webrtc_jni
