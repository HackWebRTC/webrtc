/*
 *  Copyright (c) 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "pc/video_rtp_track_source.h"

namespace webrtc {

VideoRtpTrackSource::VideoRtpTrackSource()
    : VideoTrackSource(true /* remote */) {}

rtc::VideoSourceInterface<VideoFrame>* VideoRtpTrackSource::source() {
  return &broadcaster_;
}
rtc::VideoSinkInterface<VideoFrame>* VideoRtpTrackSource::sink() {
  return &broadcaster_;
}

}  // namespace webrtc
