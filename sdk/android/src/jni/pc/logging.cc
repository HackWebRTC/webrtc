/*
 *  Copyright 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <memory>

#include "rtc_base/logging.h"
#include "sdk/android/native_api/jni/java_types.h"
#include "sdk/android/src/jni/jni_helpers.h"

namespace webrtc {
namespace jni {

JNI_FUNCTION_DECLARATION(void,
                         Logging_nativeEnableLogThreads,
                         JNIEnv* jni,
                         jclass) {
  rtc::LogMessage::LogThreads(true);
}

JNI_FUNCTION_DECLARATION(void,
                         Logging_nativeEnableLogTimeStamps,
                         JNIEnv* jni,
                         jclass) {
  rtc::LogMessage::LogTimestamps(true);
}

}  // namespace jni
}  // namespace webrtc
