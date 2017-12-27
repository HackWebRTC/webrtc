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

#include "modules/audio_processing/include/audio_processing.h"
#include "rtc_base/scoped_ref_ptr.h"
#include "sdk/android/generated_audio_jni/jni/DefaultAudioProcessingFactory_jni.h"
#include "sdk/android/src/jni/jni_helpers.h"

namespace webrtc {
namespace jni {

static jlong JNI_DefaultAudioProcessingFactory_CreateAudioProcessing(
    JNIEnv*,
    const JavaParamRef<jclass>&,
    jlong native_post_processor) {
  std::unique_ptr<CustomProcessing> post_processor(
      reinterpret_cast<CustomProcessing*>(native_post_processor));
  rtc::scoped_refptr<AudioProcessing> audio_processing =
      AudioProcessing::Create(webrtc::Config(), std::move(post_processor),
                              nullptr /* render_pre_processing */,
                              nullptr /* echo_control_factory */,
                              nullptr /* beamformer */);
  return jlongFromPointer(audio_processing.release());
}

}  // namespace jni
}  // namespace webrtc
