/*
 *  Copyright 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "sdk/android/src/jni/pc/rtpreceiver.h"

#include "sdk/android/generated_peerconnection_jni/jni/RtpReceiver_jni.h"
#include "sdk/android/src/jni/jni_helpers.h"
#include "sdk/android/src/jni/pc/java_native_conversion.h"

namespace webrtc {
namespace jni {

namespace {

// Adapter between the C++ RtpReceiverObserverInterface and the Java
// RtpReceiver.Observer interface. Wraps an instance of the Java interface and
// dispatches C++ callbacks to Java.
class RtpReceiverObserverJni : public RtpReceiverObserverInterface {
 public:
  RtpReceiverObserverJni(JNIEnv* env, jobject j_observer)
      : j_observer_global_(env, j_observer) {}

  ~RtpReceiverObserverJni() override = default;

  void OnFirstPacketReceived(cricket::MediaType media_type) override {
    JNIEnv* const env = AttachCurrentThreadIfNeeded();
    Java_Observer_onFirstPacketReceived(env, *j_observer_global_,
                                        NativeToJavaMediaType(env, media_type));
  }

 private:
  const ScopedGlobalRef<jobject> j_observer_global_;
};

}  // namespace

jobject NativeToJavaRtpReceiver(
    JNIEnv* env,
    rtc::scoped_refptr<RtpReceiverInterface> receiver) {
  // Receiver is now owned by Java object, and will be freed from there.
  return Java_RtpReceiver_Constructor(env,
                                      jlongFromPointer(receiver.release()));
}

JavaRtpReceiverGlobalOwner::JavaRtpReceiverGlobalOwner(JNIEnv* env,
                                                       jobject j_receiver)
    : j_receiver_(env, j_receiver) {}

JavaRtpReceiverGlobalOwner::JavaRtpReceiverGlobalOwner(
    JavaRtpReceiverGlobalOwner&& other) = default;

JavaRtpReceiverGlobalOwner::~JavaRtpReceiverGlobalOwner() {
  if (*j_receiver_)
    Java_RtpReceiver_dispose(AttachCurrentThreadIfNeeded(), *j_receiver_);
}

JNI_FUNCTION_DECLARATION(jlong,
                         RtpReceiver_getNativeTrack,
                         JNIEnv* jni,
                         jclass,
                         jlong j_rtp_receiver_pointer,
                         jlong j_track_pointer) {
  return jlongFromPointer(
      reinterpret_cast<RtpReceiverInterface*>(j_rtp_receiver_pointer)
          ->track()
          .release());
}

JNI_FUNCTION_DECLARATION(jboolean,
                         RtpReceiver_setNativeParameters,
                         JNIEnv* jni,
                         jclass,
                         jlong j_rtp_receiver_pointer,
                         jobject j_parameters) {
  RtpParameters parameters = JavaToNativeRtpParameters(jni, j_parameters);
  return reinterpret_cast<RtpReceiverInterface*>(j_rtp_receiver_pointer)
      ->SetParameters(parameters);
}

JNI_FUNCTION_DECLARATION(jobject,
                         RtpReceiver_getNativeParameters,
                         JNIEnv* jni,
                         jclass,
                         jlong j_rtp_receiver_pointer) {
  RtpParameters parameters =
      reinterpret_cast<RtpReceiverInterface*>(j_rtp_receiver_pointer)
          ->GetParameters();
  return NativeToJavaRtpParameters(jni, parameters);
}

JNI_FUNCTION_DECLARATION(jstring,
                         RtpReceiver_getNativeId,
                         JNIEnv* jni,
                         jclass,
                         jlong j_rtp_receiver_pointer) {
  return NativeToJavaString(
      jni,
      reinterpret_cast<RtpReceiverInterface*>(j_rtp_receiver_pointer)->id());
}

JNI_FUNCTION_DECLARATION(jlong,
                         RtpReceiver_setNativeObserver,
                         JNIEnv* jni,
                         jclass,
                         jlong j_rtp_receiver_pointer,
                         jobject j_observer) {
  RtpReceiverObserverJni* rtpReceiverObserver =
      new RtpReceiverObserverJni(jni, j_observer);
  reinterpret_cast<RtpReceiverInterface*>(j_rtp_receiver_pointer)
      ->SetObserver(rtpReceiverObserver);
  return jlongFromPointer(rtpReceiverObserver);
}

JNI_FUNCTION_DECLARATION(void,
                         RtpReceiver_unsetNativeObserver,
                         JNIEnv* jni,
                         jclass,
                         jlong j_rtp_receiver_pointer,
                         jlong j_observer_pointer) {
  reinterpret_cast<RtpReceiverInterface*>(j_rtp_receiver_pointer)
      ->SetObserver(nullptr);
  RtpReceiverObserverJni* observer =
      reinterpret_cast<RtpReceiverObserverJni*>(j_observer_pointer);
  if (observer) {
    delete observer;
  }
}

}  // namespace jni
}  // namespace webrtc
