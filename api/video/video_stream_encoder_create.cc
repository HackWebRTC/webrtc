/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "api/video/video_stream_encoder_create.h"

#include "rtc_base/ptr_util.h"
#include "video/video_stream_encoder.h"

namespace webrtc {
std::unique_ptr<VideoStreamEncoderInterface> CreateVideoStreamEncoder(
    uint32_t number_of_cores,
    VideoStreamEncoderObserver* encoder_stats_observer,
    const VideoStreamEncoderSettings& settings,
    // Deprecated, used for tests only.
    rtc::VideoSinkInterface<VideoFrame>* pre_encode_callback) {
  return rtc::MakeUnique<VideoStreamEncoder>(
      number_of_cores, encoder_stats_observer, settings, pre_encode_callback,
      rtc::MakeUnique<OveruseFrameDetector>(encoder_stats_observer));
}
}  // namespace webrtc
