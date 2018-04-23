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

#include "sdk/android/generated_video_jni/jni/VideoFrameDrawer_jni.h"
#include "sdk/android/native_api/jni/scoped_java_ref.h"

namespace webrtc {
namespace jni {

static void JNI_VideoFrameDrawer_CopyPlane(
    JNIEnv* jni,
    const JavaParamRef<jclass>&,
    const JavaParamRef<jobject>& j_src_buffer,
    jint width,
    jint height,
    jint src_stride,
    const JavaParamRef<jobject>& j_dst_buffer,
    jint dst_stride) {
  size_t src_size = jni->GetDirectBufferCapacity(j_src_buffer.obj());
  size_t dst_size = jni->GetDirectBufferCapacity(j_dst_buffer.obj());
  RTC_CHECK(src_stride >= width) << "Wrong source stride " << src_stride;
  RTC_CHECK(dst_stride >= width) << "Wrong destination stride " << dst_stride;
  RTC_CHECK(src_size >= src_stride * height)
      << "Insufficient source buffer capacity " << src_size;
  RTC_CHECK(dst_size >= dst_stride * height)
      << "Insufficient destination buffer capacity " << dst_size;
  uint8_t* src = reinterpret_cast<uint8_t*>(
      jni->GetDirectBufferAddress(j_src_buffer.obj()));
  uint8_t* dst = reinterpret_cast<uint8_t*>(
      jni->GetDirectBufferAddress(j_dst_buffer.obj()));
  if (src_stride == dst_stride) {
    memcpy(dst, src, src_stride * height);
  } else {
    for (int i = 0; i < height; i++) {
      memcpy(dst, src, width);
      src += src_stride;
      dst += dst_stride;
    }
  }
}

}  // namespace jni
}  // namespace webrtc
