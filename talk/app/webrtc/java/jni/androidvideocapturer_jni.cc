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
      j_frame_observer_class_(
          jni,
          FindClass(jni,
                    "org/webrtc/VideoCapturerAndroid$NativeFrameObserver")) {
  }

AndroidVideoCapturerJni::~AndroidVideoCapturerJni() {}

void AndroidVideoCapturerJni::Start(int width, int height, int framerate,
                                    webrtc::AndroidVideoCapturer* capturer) {
  j_frame_observer_ = NewGlobalRef(
      jni(),
      jni()->NewObject(*j_frame_observer_class_,
                       GetMethodID(jni(),
                                   *j_frame_observer_class_,
                                   "<init>",
                                   "(J)V"),
                                   jlongFromPointer(capturer)));
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

bool AndroidVideoCapturerJni::Stop() {
  jmethodID m = GetMethodID(jni(), *j_video_capturer_class_,
                            "stopCapture", "()Z");
  jboolean result = jni()->CallBooleanMethod(*j_capturer_global_, m);
  CHECK_EXCEPTION(jni()) << "error during VideoCapturerAndroid.stopCapture";
  DeleteGlobalRef(jni(), j_frame_observer_);
  return result;
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

JOW(void, VideoCapturerAndroid_00024NativeFrameObserver_nativeOnFrameCaptured)
    (JNIEnv* jni, jclass, jlong j_capturer, jbyteArray j_frame,
        jint rotation, jlong ts) {
  jbyte* bytes = jni->GetByteArrayElements(j_frame, NULL);
  reinterpret_cast<webrtc::AndroidVideoCapturer*>(
      j_capturer)->OnIncomingFrame(bytes, jni->GetArrayLength(j_frame),
                                   rotation, ts);
  jni->ReleaseByteArrayElements(j_frame, bytes, JNI_ABORT);
}

JOW(void, VideoCapturerAndroid_00024NativeFrameObserver_nativeCapturerStarted)
    (JNIEnv* jni, jclass, jlong j_capturer, jboolean j_success) {
  reinterpret_cast<webrtc::AndroidVideoCapturer*>(
      j_capturer)->OnCapturerStarted(j_success);
}

}  // namespace webrtc_jni

