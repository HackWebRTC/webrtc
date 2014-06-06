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

#if !defined(ANDROID) || !defined(WEBRTC_CHROMIUM_BUILD)

namespace webrtc {
namespace videocapturemodule {
void EnsureInitialized() {}
}  // namespace videocapturemodule
}  // namespace webrtc

#else  // !defined(ANDROID) || !defined(WEBRTC_CHROMIUM_BUILD)

#include <assert.h>
#include <pthread.h>

#include "base/android/jni_android.h"

// Handy alternative to assert() which suppresses unused-variable warnings when
// assert() is a no-op (i.e. in Release builds).
#ifdef NDEBUG
#define ASSERT(x) if (false && (x)); else
#else
#define ASSERT(x) assert(x)
#endif

namespace webrtc {

// Declared in webrtc/modules/video_capture/include/video_capture.h.
int32_t SetCaptureAndroidVM(JavaVM* javaVM, jobject g_context);

namespace videocapturemodule {

static pthread_once_t g_initialize_once = PTHREAD_ONCE_INIT;

void EnsureInitializedOnce() {
  JNIEnv* jni = ::base::android::AttachCurrentThread();
  jobject context = ::base::android::GetApplicationContext();
  JavaVM* jvm = NULL;
  int status = jni->GetJavaVM(&jvm);
  ASSERT(status == 0);
  status = webrtc::SetCaptureAndroidVM(jvm, context) == 0;
  ASSERT(status);
}

void EnsureInitialized() {
  int ret = pthread_once(&g_initialize_once, &EnsureInitializedOnce);
  ASSERT(ret == 0);
}

}  // namespace videocapturemodule
}  // namespace webrtc

#endif  // ANDROID & WEBRTC_CHROMIUM_BUILD
