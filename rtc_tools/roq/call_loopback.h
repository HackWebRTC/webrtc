/*
 *  Copyright (c) 2026 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS. All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef RTC_TOOLS_ROQ_CALL_LOOPBACK_H_
#define RTC_TOOLS_ROQ_CALL_LOOPBACK_H_

#include <cstddef>
#include <string>

namespace webrtc::roq {

struct CallLoopbackResult {
  size_t sent_video_rtp = 0;
  size_t sent_audio_rtp = 0;
  size_t received_audio_rtp = 0;
  int audio_decoding_normal = 0;
  size_t forward_rtcp = 0;
  size_t reverse_rtcp = 0;
  size_t decoded_video_frames = 0;
  size_t decoded_audio_samples_per_channel = 0;
  bool clean_shutdown = false;
  std::string error;
};

CallLoopbackResult RunCallLoopback(double duration_seconds);

}  // namespace webrtc::roq

#endif  // RTC_TOOLS_ROQ_CALL_LOOPBACK_H_
