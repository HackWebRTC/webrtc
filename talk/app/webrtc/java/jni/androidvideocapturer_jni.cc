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
rtc::scoped_ptr<AndroidVideoCapturerJni>
AndroidVideoCapturerJni::Create(JNIEnv* jni,
                                jobject j_video_capture,
                                jstring device_name) {
  rtc::scoped_ptr<AndroidVideoCapturerJni> capturer(
      new AndroidVideoCapturerJni(jni,
                                  j_video_capture));

  if (capturer->Init(device_name))
    return capturer.Pass();
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
      capturer_(nullptr) {
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
}

void AndroidVideoCapturerJni::Start(int width, int height, int framerate,
                                    webrtc::AndroidVideoCapturer* capturer) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(capturer_ == nullptr);
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
  DCHECK(thread_checker_.CalledOnValidThread());
  capturer_ = nullptr;
  jmethodID m = GetMethodID(jni(), *j_video_capturer_class_,
                            "stopCapture", "()V");
  jni()->CallVoidMethod(*j_capturer_global_, m);
  CHECK_EXCEPTION(jni()) << "error during VideoCapturerAndroid.stopCapture";
  DeleteGlobalRef(jni(), j_frame_observer_);
  // Do not process frames in flight after stop have returned since
  // the memory buffers they point to have been deleted.
  rtc::MessageQueueManager::Clear(&invoker_);
}

void AndroidVideoCapturerJni::ReturnBuffer(int64 time_stamp) {
  jmethodID m = GetMethodID(jni(), *j_video_capturer_class_,
                            "returnBuffer", "(J)V");
  jni()->CallVoidMethod(*j_capturer_global_, m, time_stamp);
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
  invoker_.AsyncInvoke<void>(
      thread_,
      rtc::Bind(&AndroidVideoCapturerJni::OnCapturerStarted_w, this, success));
}

void AndroidVideoCapturerJni::OnIncomingFrame(void* video_frame,
                                              int length,
                                              int rotation,
                                              int64 time_stamp) {
  invoker_.AsyncInvoke<void>(
      thread_,
      rtc::Bind(&AndroidVideoCapturerJni::OnIncomingFrame_w,
                this, video_frame, length, rotation, time_stamp));
}

void AndroidVideoCapturerJni::OnCapturerStarted_w(bool success) {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (capturer_)
    capturer_->OnCapturerStarted(success);
}

void AndroidVideoCapturerJni::OnIncomingFrame_w(void* video_frame,
                                                int length,
                                                int rotation,
                                                int64 time_stamp) {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (capturer_)
    capturer_->OnIncomingFrame(video_frame, length, rotation, time_stamp);
}

JNIEnv* AndroidVideoCapturerJni::jni() { return AttachCurrentThreadIfNeeded(); }

JOW(void, VideoCapturerAndroid_00024NativeObserver_nativeOnFrameCaptured)
    (JNIEnv* jni, jclass, jlong j_capturer, jbyteArray j_frame, jint length,
        jint rotation, jlong ts) {
  jboolean is_copy = true;
  jbyte* bytes = jni->GetByteArrayElements(j_frame, &is_copy);
  if (!is_copy) {
    reinterpret_cast<AndroidVideoCapturerJni*>(
        j_capturer)->OnIncomingFrame(bytes, length, rotation, ts);
  }  else {
    // If this is a copy of the original frame, it means that the memory
    // is not direct memory and thus VideoCapturerAndroid does not guarantee
    // that the memory is valid when we have released |j_frame|.
    DCHECK(false) << "j_frame is a copy.";
  }
  jni->ReleaseByteArrayElements(j_frame, bytes, JNI_ABORT);
}

JOW(void, VideoCapturerAndroid_00024NativeObserver_nativeCapturerStarted)
    (JNIEnv* jni, jclass, jlong j_capturer, jboolean j_success) {
  reinterpret_cast<AndroidVideoCapturerJni*>(j_capturer)->OnCapturerStarted(
      j_success);
}

}  // namespace webrtc_jni

