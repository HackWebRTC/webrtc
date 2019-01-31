/*
 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "rtc_base/logging.h"
#include "sdk/android/generated_video_jni/jni/VideoSource_jni.h"
#include "sdk/android/native_api/jni/java_types.h"
#include "sdk/android/src/jni/android_video_track_source.h"

namespace webrtc {
namespace jni {

static void JNI_VideoSource_AdaptOutputFormat(JNIEnv* jni,
                                              jlong j_source,
                                              jint j_landscape_width,
                                              jint j_landscape_height,
                                              jint j_portrait_width,
                                              jint j_portrait_height,
                                              jint j_fps) {
  RTC_LOG(LS_INFO) << "VideoSource_nativeAdaptOutputFormat";
  reinterpret_cast<AndroidVideoTrackSource*>(j_source)->OnOutputFormatRequest(
      j_landscape_width, j_landscape_height, j_portrait_width,
      j_portrait_height, j_fps);
}

}  // namespace jni
}  // namespace webrtc
