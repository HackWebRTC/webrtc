/*
 *  Copyright (c) 2014 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

// Platform-specific initialization bits, if any, go here.

#ifndef ANDROID

namespace webrtc {
namespace videocapturemodule {
void EnsureInitialized() {}
}  // namespace videocapturemodule
}  // namespace webrtc

#else

#include <pthread.h>

// Note: this dependency is dangerous since it reaches into Chromium's
// base. You can't include anything in this file that includes WebRTC's
// base/checks.h, for instance, since it will clash with Chromium's
// logging.h. Therefore, the CHECKs in this file will actually use
// Chromium's checks rather than the WebRTC ones.
#include "base/android/jni_android.h"
#include "webrtc/modules/video_capture/video_capture_internal.h"

namespace webrtc {
namespace videocapturemodule {

static pthread_once_t g_initialize_once = PTHREAD_ONCE_INIT;

void EnsureInitializedOnce() {
  JNIEnv* jni = ::base::android::AttachCurrentThread();
  jobject context = ::base::android::GetApplicationContext();
  JavaVM* jvm = NULL;
  CHECK_EQ(0, jni->GetJavaVM(&jvm));
  CHECK_EQ(0, webrtc::SetCaptureAndroidVM(jvm, context));
}

void EnsureInitialized() {
  CHECK_EQ(0, pthread_once(&g_initialize_once, &EnsureInitializedOnce));
}

}  // namespace videocapturemodule
}  // namespace webrtc

#endif  // !ANDROID
