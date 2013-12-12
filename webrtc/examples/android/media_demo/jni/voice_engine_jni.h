/*
 *  Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_EXAMPLES_ANDROID_MEDIA_DEMO_JNI_VOICE_ENGINE_H_
#define WEBRTC_EXAMPLES_ANDROID_MEDIA_DEMO_JNI_VOICE_ENGINE_H_

#include <jni.h>

namespace webrtc {

class VoiceEngine;

}  // namespace webrtc

namespace webrtc_examples {

void SetVoeDeviceObjects(JavaVM* vm);
void ClearVoeDeviceObjects();

}  // namespace webrtc_examples

webrtc::VoiceEngine* GetVoiceEngine(JNIEnv* jni, jobject j_voe);

#endif  // WEBRTC_EXAMPLES_ANDROID_MEDIA_DEMO_JNI_VOICE_ENGINE_H_
