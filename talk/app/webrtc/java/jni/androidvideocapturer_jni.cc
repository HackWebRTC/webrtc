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

// JavaCaptureProxy is responsible for marshaling calls from the
// Java VideoCapturerAndroid to the C++ class AndroidVideoCapturer.
// Calls from Java occur on a Java thread and are marshaled to
// AndroidVideoCapturer on the thread that creates an instance of this object.
//
// An instance is created when AndroidVideoCapturerJni::Start is called and
// ownership is passed to an instance of the Java class NativeObserver.
// JavaCaptureProxy is destroyed when NativeObserver has reported that the
// capturer has stopped, see
// VideoCapturerAndroid_00024NativeObserver_nativeCapturerStopped.
// Marshaling is done as long as JavaCaptureProxy has a pointer to the
// AndroidVideoCapturer.
class JavaCaptureProxy {
 public:
  JavaCaptureProxy() : thread_(rtc::Thread::Current()), capturer_(nullptr) {
  }

  ~JavaCaptureProxy() {
  }

  void SetAndroidCapturer(webrtc::AndroidVideoCapturer* capturer) {
    DCHECK(thread_->IsCurrent());
    capturer_ = capturer;
  }

  void OnCapturerStarted(bool success) {
    thread_->Invoke<void>(
        rtc::Bind(&JavaCaptureProxy::OnCapturerStarted_w, this, success));
  }

  void OnIncomingFrame(signed char* video_frame,
                       int length,
                       int rotation,
                       int64 time_stamp) {
    thread_->Invoke<void>(
        rtc::Bind(&JavaCaptureProxy::OnIncomingFrame_w, this, video_frame,
                  length, rotation, time_stamp));
  }

 private:
  void OnCapturerStarted_w(bool success) {
    DCHECK(thread_->IsCurrent());
    if (capturer_)
      capturer_->OnCapturerStarted(success);
  }
  void OnIncomingFrame_w(signed char* video_frame,
                         int length,
                         int rotation,
                         int64 time_stamp) {
    DCHECK(thread_->IsCurrent());
    if (capturer_)
      capturer_->OnIncomingFrame(video_frame, length, rotation, time_stamp);
  }

  rtc::Thread* thread_;
  webrtc::AndroidVideoCapturer* capturer_;
};

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
      proxy_(nullptr) {
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
  DeInit();
}

void AndroidVideoCapturerJni::DeInit() {
  DCHECK(proxy_ == nullptr);
  jmethodID m = GetMethodID(jni(), *j_video_capturer_class_, "deInit", "()V");
  jni()->CallVoidMethod(*j_capturer_global_, m);
  CHECK_EXCEPTION(jni()) << "error during VideoCapturerAndroid.DeInit";
}

void AndroidVideoCapturerJni::Start(int width, int height, int framerate,
                                    webrtc::AndroidVideoCapturer* capturer) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(proxy_ == nullptr);
  proxy_ = new JavaCaptureProxy();
  proxy_->SetAndroidCapturer(capturer);

  j_frame_observer_ = NewGlobalRef(
      jni(),
      jni()->NewObject(*j_observer_class_,
                       GetMethodID(jni(),
                                   *j_observer_class_,
                                   "<init>",
                                   "(J)V"),
                                   jlongFromPointer(proxy_)));
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
  DCHECK(thread_checker_.CalledOnValidThread());
  proxy_->SetAndroidCapturer(nullptr);
  proxy_ = nullptr;
  jmethodID m = GetMethodID(jni(), *j_video_capturer_class_,
                            "stopCapture", "()V");
  jni()->CallVoidMethod(*j_capturer_global_, m);
  CHECK_EXCEPTION(jni()) << "error during VideoCapturerAndroid.stopCapture";
  DeleteGlobalRef(jni(), j_frame_observer_);
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

JNIEnv* AndroidVideoCapturerJni::jni() { return AttachCurrentThreadIfNeeded(); }

JOW(void, VideoCapturerAndroid_00024NativeObserver_nativeOnFrameCaptured)
    (JNIEnv* jni, jclass, jlong j_proxy, jbyteArray j_frame,
        jint rotation, jlong ts) {
  jbyte* bytes = jni->GetByteArrayElements(j_frame, NULL);
  reinterpret_cast<JavaCaptureProxy*>(
      j_proxy)->OnIncomingFrame(bytes, jni->GetArrayLength(j_frame), rotation,
                                ts);
  jni->ReleaseByteArrayElements(j_frame, bytes, JNI_ABORT);
}

JOW(void, VideoCapturerAndroid_00024NativeObserver_nativeCapturerStarted)
    (JNIEnv* jni, jclass, jlong j_proxy, jboolean j_success) {
  JavaCaptureProxy* proxy = reinterpret_cast<JavaCaptureProxy*>(j_proxy);
  proxy->OnCapturerStarted(j_success);
  if (!j_success)
    delete proxy;
}

JOW(void, VideoCapturerAndroid_00024NativeObserver_nativeCapturerStopped)
    (JNIEnv* jni, jclass, jlong j_proxy) {
  delete reinterpret_cast<JavaCaptureProxy*>(j_proxy);
}

}  // namespace webrtc_jni

