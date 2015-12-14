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
 */

#include "talk/app/webrtc/test/androidtestinitializer.h"

#include <pthread.h>

// Note: this dependency is dangerous since it reaches into Chromium's base.
// There's a risk of e.g. macro clashes. This file may only be used in tests.
// Since we use Chromes build system for creating the gtest binary, this should
// be fine.
#include "base/android/context_utils.h"
#include "base/android/jni_android.h"

#include "talk/app/webrtc/java/jni/classreferenceholder.h"
#include "talk/app/webrtc/java/jni/jni_helpers.h"
#include "webrtc/base/checks.h"
#include "webrtc/base/ssladapter.h"
#include "webrtc/voice_engine/include/voe_base.h"

namespace webrtc {

namespace {

static pthread_once_t g_initialize_once = PTHREAD_ONCE_INIT;

// There can only be one JNI_OnLoad in each binary. So since this is a GTEST
// C++ runner binary, we want to initialize the same global objects we normally
// do if this had been a Java binary.
void EnsureInitializedOnce() {
  RTC_CHECK(::base::android::IsVMInitialized());
  JNIEnv* jni = ::base::android::AttachCurrentThread();
  JavaVM* jvm = NULL;
  RTC_CHECK_EQ(0, jni->GetJavaVM(&jvm));
  jobject context = ::base::android::GetApplicationContext();

  RTC_CHECK_GE(webrtc_jni::InitGlobalJniVariables(jvm), 0);
  RTC_CHECK(rtc::InitializeSSL()) << "Failed to InitializeSSL()";
  webrtc_jni::LoadGlobalClassReferenceHolder();

  webrtc::VoiceEngine::SetAndroidObjects(jvm, context);
}

}  // anonymous namespace

void InitializeAndroidObjects() {
  RTC_CHECK_EQ(0, pthread_once(&g_initialize_once, &EnsureInitializedOnce));
}

}  // namespace webrtc
