/*
 *  Copyright 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_SDK_ANDROID_SRC_JNI_PC_DATACHANNELOBSERVER_JNI_H_
#define WEBRTC_SDK_ANDROID_SRC_JNI_PC_DATACHANNELOBSERVER_JNI_H_

#include "webrtc/api/datachannelinterface.h"
#include "webrtc/sdk/android/src/jni/jni_helpers.h"

namespace webrtc_jni {

// Adapter for a Java DataChannel$Observer presenting a C++ DataChannelObserver
// and dispatching the callback from C++ back to Java.
class DataChannelObserverJni : public webrtc::DataChannelObserver {
 public:
  DataChannelObserverJni(JNIEnv* jni, jobject j_observer);
  virtual ~DataChannelObserverJni() {}

  void OnBufferedAmountChange(uint64_t previous_amount) override;
  void OnStateChange() override;
  void OnMessage(const webrtc::DataBuffer& buffer) override;

 private:
  const ScopedGlobalRef<jobject> j_observer_global_;
  const ScopedGlobalRef<jclass> j_observer_class_;
  const ScopedGlobalRef<jclass> j_buffer_class_;
  const jmethodID j_on_buffered_amount_change_mid_;
  const jmethodID j_on_state_change_mid_;
  const jmethodID j_on_message_mid_;
  const jmethodID j_buffer_ctor_;
};

}  // namespace webrtc_jni

#endif  // WEBRTC_SDK_ANDROID_SRC_JNI_PC_DATACHANNELOBSERVER_JNI_H_
