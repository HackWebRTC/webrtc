/*
 *  Copyright (c) 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/modules/audio_device/android/ensure_initialized.h"

#include <pthread.h>

// Note: this dependency is dangerous since it reaches into Chromium's
// base. You can't include anything in this file that includes WebRTC's
// base/checks.h, for instance, since it will clash with Chromium's
// logging.h. Therefore, the CHECKs in this file will actually use
// Chromium's checks rather than the WebRTC ones.
#include "base/android/jni_android.h"
#include "webrtc/modules/audio_device/android/audio_record_jni.h"
#include "webrtc/modules/audio_device/android/audio_track_jni.h"
#include "webrtc/modules/utility/interface/jvm_android.h"

namespace webrtc {
namespace audiodevicemodule {

static pthread_once_t g_initialize_once = PTHREAD_ONCE_INIT;

void EnsureInitializedOnce() {
  CHECK(::base::android::IsVMInitialized());
  JNIEnv* jni = ::base::android::AttachCurrentThread();
  JavaVM* jvm = NULL;
  CHECK_EQ(0, jni->GetJavaVM(&jvm));
  jobject context = ::base::android::GetApplicationContext();

  // Initialize the Java environment (currently only used by the audio manager).
  webrtc::JVM::Initialize(jvm, context);
}

void EnsureInitialized() {
  CHECK_EQ(0, pthread_once(&g_initialize_once, &EnsureInitializedOnce));
}

}  // namespace audiodevicemodule
}  // namespace webrtc
