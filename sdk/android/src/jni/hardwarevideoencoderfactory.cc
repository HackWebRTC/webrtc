/*
 *  Copyright 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <jni.h>

#include "media/base/codec.h"
#include "media/base/h264_profile_level_id.h"
#include "media/base/mediaconstants.h"
#include "sdk/android/src/jni/jni_helpers.h"

namespace webrtc {
namespace jni {

static bool IsSameH264Profile(const cricket::CodecParameterMap& params1,
                              const cricket::CodecParameterMap& params2) {
  const rtc::Optional<H264::ProfileLevelId> profile_level_id =
      H264::ParseSdpProfileLevelId(params1);
  const rtc::Optional<H264::ProfileLevelId> other_profile_level_id =
      H264::ParseSdpProfileLevelId(params2);
  // Compare H264 profiles, but not levels.
  return profile_level_id && other_profile_level_id &&
         profile_level_id->profile == other_profile_level_id->profile;
}

JNI_FUNCTION_DECLARATION(jboolean,
                         HardwareVideoEncoderFactory_isSameH264Profile,
                         JNIEnv* jni,
                         jclass,
                         jobject params1,
                         jobject params2) {
  return IsSameH264Profile(JavaToStdMapStrings(jni, params1),
                           JavaToStdMapStrings(jni, params2));
}

}  // namespace jni
}  // namespace webrtc
