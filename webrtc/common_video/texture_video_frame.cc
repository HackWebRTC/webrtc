/*
 *  Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/common_video/interface/texture_video_frame.h"

#include "webrtc/base/refcount.h"

namespace webrtc {

TextureVideoFrame::TextureVideoFrame(NativeHandle* handle,
                                     int width,
                                     int height,
                                     uint32_t timestamp,
                                     int64_t render_time_ms)
    : I420VideoFrame(
          new rtc::RefCountedObject<TextureBuffer>(handle, width, height),
          timestamp,
          render_time_ms,
          kVideoRotation_0) {
}

}  // namespace webrtc
