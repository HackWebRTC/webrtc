/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <jni.h>

#include "webrtc/base/scoped_ref_ptr.h"
#include "webrtc/sdk/android/src/jni/classreferenceholder.h"

namespace cricket {
class WebRtcVideoEncoderFactory;
class WebRtcVideoDecoderFactory;
}  // namespace cricket

namespace webrtc_jni {

class MediaCodecVideoEncoderFactory;
class MediaCodecVideoDecoderFactory;
class SurfaceTextureHelper;

cricket::WebRtcVideoEncoderFactory* CreateVideoEncoderFactory() {
  return nullptr;
}

cricket::WebRtcVideoDecoderFactory* CreateVideoDecoderFactory() {
  return nullptr;
}

jobject GetJavaSurfaceTextureHelper(
    rtc::scoped_refptr<SurfaceTextureHelper> surface_texture_helper) {
  return nullptr;
}

}  // namespace webrtc_jni
