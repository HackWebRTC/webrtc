/*
 *  Copyright 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_SDK_ANDROID_SRC_JNI_PC_RTPRECEIVEROBSERVER_JNI_H_
#define WEBRTC_SDK_ANDROID_SRC_JNI_PC_RTPRECEIVEROBSERVER_JNI_H_

#include "webrtc/api/rtpreceiverinterface.h"
#include "webrtc/sdk/android/src/jni/jni_helpers.h"

namespace webrtc_jni {

// Adapter between the C++ RtpReceiverObserverInterface and the Java
// RtpReceiver.Observer interface. Wraps an instance of the Java interface and
// dispatches C++ callbacks to Java.
class RtpReceiverObserverJni : public webrtc::RtpReceiverObserverInterface {
 public:
  RtpReceiverObserverJni(JNIEnv* jni, jobject j_observer)
      : j_observer_global_(jni, j_observer) {}

  ~RtpReceiverObserverJni() override {}

  void OnFirstPacketReceived(cricket::MediaType media_type) override;

 private:
  const ScopedGlobalRef<jobject> j_observer_global_;
};

}  // namespace webrtc_jni

#endif  // WEBRTC_SDK_ANDROID_SRC_JNI_PC_RTPRECEIVEROBSERVER_JNI_H_
