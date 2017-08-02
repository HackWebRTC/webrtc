/*
 *  Copyright 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/sdk/android/src/jni/pc/datachannelobserver_jni.h"

#include "webrtc/sdk/android/src/jni/classreferenceholder.h"

namespace webrtc_jni {

// Convenience, used since callbacks occur on the signaling thread, which may
// be a non-Java thread.
static JNIEnv* jni() {
  return AttachCurrentThreadIfNeeded();
}

DataChannelObserverJni::DataChannelObserverJni(JNIEnv* jni, jobject j_observer)
    : j_observer_global_(jni, j_observer),
      j_observer_class_(jni, GetObjectClass(jni, j_observer)),
      j_buffer_class_(jni, FindClass(jni, "org/webrtc/DataChannel$Buffer")),
      j_on_buffered_amount_change_mid_(GetMethodID(jni,
                                                   *j_observer_class_,
                                                   "onBufferedAmountChange",
                                                   "(J)V")),
      j_on_state_change_mid_(
          GetMethodID(jni, *j_observer_class_, "onStateChange", "()V")),
      j_on_message_mid_(GetMethodID(jni,
                                    *j_observer_class_,
                                    "onMessage",
                                    "(Lorg/webrtc/DataChannel$Buffer;)V")),
      j_buffer_ctor_(GetMethodID(jni,
                                 *j_buffer_class_,
                                 "<init>",
                                 "(Ljava/nio/ByteBuffer;Z)V")) {}

void DataChannelObserverJni::OnBufferedAmountChange(uint64_t previous_amount) {
  ScopedLocalRefFrame local_ref_frame(jni());
  jni()->CallVoidMethod(*j_observer_global_, j_on_buffered_amount_change_mid_,
                        previous_amount);
  CHECK_EXCEPTION(jni()) << "error during CallVoidMethod";
}

void DataChannelObserverJni::OnStateChange() {
  ScopedLocalRefFrame local_ref_frame(jni());
  jni()->CallVoidMethod(*j_observer_global_, j_on_state_change_mid_);
  CHECK_EXCEPTION(jni()) << "error during CallVoidMethod";
}

void DataChannelObserverJni::OnMessage(const webrtc::DataBuffer& buffer) {
  ScopedLocalRefFrame local_ref_frame(jni());
  jobject byte_buffer = jni()->NewDirectByteBuffer(
      const_cast<char*>(buffer.data.data<char>()), buffer.data.size());
  jobject j_buffer = jni()->NewObject(*j_buffer_class_, j_buffer_ctor_,
                                      byte_buffer, buffer.binary);
  jni()->CallVoidMethod(*j_observer_global_, j_on_message_mid_, j_buffer);
  CHECK_EXCEPTION(jni()) << "error during CallVoidMethod";
}

}  // namespace webrtc_jni
