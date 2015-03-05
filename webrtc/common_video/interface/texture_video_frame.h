/*
 *  Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef COMMON_VIDEO_INTERFACE_TEXTURE_VIDEO_FRAME_H
#define COMMON_VIDEO_INTERFACE_TEXTURE_VIDEO_FRAME_H

// TextureVideoFrame class
//
// Storing and handling of video frames backed by textures.

#include "webrtc/common_video/interface/i420_video_frame.h"
#include "webrtc/common_video/interface/native_handle.h"
#include "webrtc/typedefs.h"

namespace webrtc {

class TextureVideoFrame : public I420VideoFrame {
 public:
  TextureVideoFrame(NativeHandle* handle,
                    int width,
                    int height,
                    uint32_t timestamp,
                    int64_t render_time_ms);
};

}  // namespace webrtc

#endif  // COMMON_VIDEO_INTERFACE_TEXTURE_VIDEO_FRAME_H
