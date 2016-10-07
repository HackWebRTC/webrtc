/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

// TODO(nisse): Deprecated, replace cricket::WebRtcVideoFrame with
// webrtc::VideoFrame everywhere, then delete this file. See
// https://bugs.chromium.org/p/webrtc/issues/detail?id=5682.

#ifndef WEBRTC_MEDIA_ENGINE_WEBRTCVIDEOFRAME_H_
#define WEBRTC_MEDIA_ENGINE_WEBRTCVIDEOFRAME_H_

#include <memory>

#include "webrtc/base/buffer.h"
#include "webrtc/base/refcount.h"
#include "webrtc/base/scoped_ref_ptr.h"
#include "webrtc/common_types.h"
#include "webrtc/common_video/include/video_frame_buffer.h"
#include "webrtc/media/base/videoframe.h"

namespace cricket {

class WebRtcVideoFrame : public VideoFrame {
 public:
  WebRtcVideoFrame() : VideoFrame() {}
  WebRtcVideoFrame(const rtc::scoped_refptr<webrtc::VideoFrameBuffer>& buffer,
                   webrtc::VideoRotation rotation,
                   int64_t timestamp_us)
      : VideoFrame(buffer, rotation, timestamp_us) {}
  WebRtcVideoFrame(const rtc::scoped_refptr<webrtc::VideoFrameBuffer>& buffer,
                   webrtc::VideoRotation rotation,
                   int64_t timestamp_us,
                   uint32_t transport_frame_id)
      : VideoFrame(buffer, rotation, timestamp_us) {
    // For now, transport_frame_id and rtp timestamp are the same.
    // TODO(nisse): Must be handled differently for QUIC.
    set_timestamp(transport_frame_id);
  }
};

}  // namespace cricket

#endif  // WEBRTC_MEDIA_ENGINE_WEBRTCVIDEOFRAME_H_
