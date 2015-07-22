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

// static
rtc::scoped_refptr<AndroidVideoCapturerJni>
AndroidVideoCapturerJni::Create(JNIEnv* jni,
                                jobject j_video_capture,
                                jstring device_name) {
  LOG(LS_INFO) << "AndroidVideoCapturerJni::Create";
  rtc::scoped_refptr<AndroidVideoCapturerJni> capturer(
      new rtc::RefCountedObject<AndroidVideoCapturerJni>(jni, j_video_capture));

  if (capturer->Init(device_name)) {
    return capturer;
  }
  LOG(LS_ERROR) << "AndroidVideoCapturerJni init fails";
  return nullptr;
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
      capturer_(nullptr),
      thread_(nullptr),
      valid_global_refs_(true) {
  LOG(LS_INFO) << "AndroidVideoCapturerJni ctor";
  thread_checker_.DetachFromThread();
}

bool AndroidVideoCapturerJni::Init(jstring device_name) {
  const jmethodID m(GetMethodID(
        jni(), *j_video_capturer_class_, "init", "(Ljava/lang/String;)Z"));
  if (!jni()->CallBooleanMethod(*j_capturer_global_, m, device_name)) {
    return false;
  }
  CHECK_EXCEPTION(jni()) << "error during CallVoidMethod";
  return true;
}

AndroidVideoCapturerJni::~AndroidVideoCapturerJni() {
  valid_global_refs_ = false;
  if (thread_ != nullptr) {
    LOG(LS_INFO) << "AndroidVideoCapturerJni dtor - flush invoker";
    invoker_.Flush(thread_);
  }
  LOG(LS_INFO) << "AndroidVideoCapturerJni dtor done";
}

void AndroidVideoCapturerJni::Start(int width, int height, int framerate,
                                    webrtc::AndroidVideoCapturer* capturer) {
  LOG(LS_INFO) << "AndroidVideoCapturerJni start";
  CHECK(thread_checker_.CalledOnValidThread());
  CHECK(capturer_ == nullptr);
  thread_ = rtc::Thread::Current();
  capturer_ = capturer;

  j_frame_observer_ = NewGlobalRef(
      jni(),
      jni()->NewObject(*j_observer_class_,
                       GetMethodID(jni(),
                                   *j_observer_class_,
                                   "<init>",
                                   "(J)V"),
                                   jlongFromPointer(this)));
  CHECK_EXCEPTION(jni()) << "error during NewObject";

  jmethodID m = GetMethodID(
      jni(), *j_video_capturer_class_, "startCapture",
      "(IIILandroid/content/Context;"
      "Lorg/webrtc/VideoCapturerAndroid$CapturerObserver;)V");
  jni()->CallVoidMethod(*j_capturer_global_,
                        m, width, height,
                        framerate,
                        application_context_,
                        j_frame_observer_);
  CHECK_EXCEPTION(jni()) << "error during VideoCapturerAndroid.startCapture";
}

void AndroidVideoCapturerJni::Stop() {
  LOG(LS_INFO) << "AndroidVideoCapturerJni stop";
  CHECK(thread_checker_.CalledOnValidThread());
  capturer_ = nullptr;
  jmethodID m = GetMethodID(jni(), *j_video_capturer_class_,
                            "stopCapture", "()V");
  jni()->CallVoidMethod(*j_capturer_global_, m);
  CHECK_EXCEPTION(jni()) << "error during VideoCapturerAndroid.stopCapture";
  DeleteGlobalRef(jni(), j_frame_observer_);
  LOG(LS_INFO) << "AndroidVideoCapturerJni stop done";
}

void AndroidVideoCapturerJni::ReturnBuffer(int64 time_stamp) {
  invoker_.AsyncInvoke<void>(
      thread_,
      rtc::Bind(&AndroidVideoCapturerJni::ReturnBuffer_w, this, time_stamp));
}

void AndroidVideoCapturerJni::ReturnBuffer_w(int64 time_stamp) {
  if (!valid_global_refs_) {
    LOG(LS_ERROR) << "ReturnBuffer_w is called for invalid global refs.";
    return;
  }
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
  LOG(LS_INFO) <<  "AndroidVideoCapturerJni capture started: " << success;
  invoker_.AsyncInvoke<void>(
      thread_,
      rtc::Bind(&AndroidVideoCapturerJni::OnCapturerStarted_w, this, success));
}

void AndroidVideoCapturerJni::OnIncomingFrame(void* video_frame,
                                              int length,
                                              int width,
                                              int height,
                                              int rotation,
                                              int64 time_stamp) {
  invoker_.AsyncInvoke<void>(
      thread_,
      rtc::Bind(&AndroidVideoCapturerJni::OnIncomingFrame_w, this, video_frame,
                length, width, height, rotation, time_stamp));
}

void AndroidVideoCapturerJni::OnOutputFormatRequest(int width,
                                                    int height,
                                                    int fps) {
  invoker_.AsyncInvoke<void>(
      thread_,
      rtc::Bind(&AndroidVideoCapturerJni::OnOutputFormatRequest_w,
                this, width, height, fps));
}

void AndroidVideoCapturerJni::OnCapturerStarted_w(bool success) {
  CHECK(thread_checker_.CalledOnValidThread());
  if (capturer_) {
    capturer_->OnCapturerStarted(success);
  } else {
    LOG(LS_WARNING) << "OnCapturerStarted_w is called for closed capturer.";
  }
}

void AndroidVideoCapturerJni::OnIncomingFrame_w(void* video_frame,
                                                int length,
                                                int width,
                                                int height,
                                                int rotation,
                                                int64 time_stamp) {
  CHECK(thread_checker_.CalledOnValidThread());
  if (capturer_) {
    capturer_->OnIncomingFrame(video_frame, length, width, height, rotation,
                               time_stamp);
  } else {
    LOG(LS_INFO) <<
        "Frame arrived after camera has been stopped: " << time_stamp <<
        ". Valid global refs: " << valid_global_refs_;
    ReturnBuffer_w(time_stamp);
  }
}

void AndroidVideoCapturerJni::OnOutputFormatRequest_w(int width,
                                                      int height,
                                                      int fps) {
  CHECK(thread_checker_.CalledOnValidThread());
  if (capturer_) {
    capturer_->OnOutputFormatRequest(width, height, fps);
  } else {
    LOG(LS_WARNING) << "OnOutputFormatRequest_w is called for closed capturer.";
  }
}

JNIEnv* AndroidVideoCapturerJni::jni() { return AttachCurrentThreadIfNeeded(); }

JOW(void, VideoCapturerAndroid_00024NativeObserver_nativeOnFrameCaptured)
    (JNIEnv* jni, jclass, jlong j_capturer, jbyteArray j_frame, jint length,
        jint width, jint height, jint rotation, jlong ts) {
  jboolean is_copy = true;
  jbyte* bytes = jni->GetByteArrayElements(j_frame, &is_copy);
  if (!is_copy) {
    reinterpret_cast<AndroidVideoCapturerJni*>(j_capturer)
        ->OnIncomingFrame(bytes, length, width, height, rotation, ts);
  } else {
    // If this is a copy of the original frame, it means that the memory
    // is not direct memory and thus VideoCapturerAndroid does not guarantee
    // that the memory is valid when we have released |j_frame|.
    LOG(LS_ERROR) << "NativeObserver_nativeOnFrameCaptured: frame is a copy";
    CHECK(false) << "j_frame is a copy.";
  }
  jni->ReleaseByteArrayElements(j_frame, bytes, JNI_ABORT);
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

}  // namespace webrtc_jni

