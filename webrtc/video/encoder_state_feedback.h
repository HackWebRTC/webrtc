/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

// TODO(mflodman) ViEEncoder has a time check to not send key frames too often,
// move the logic to this class.

#ifndef WEBRTC_VIDEO_ENCODER_STATE_FEEDBACK_H_
#define WEBRTC_VIDEO_ENCODER_STATE_FEEDBACK_H_

#include <vector>

#include "webrtc/base/criticalsection.h"
#include "webrtc/modules/rtp_rtcp/include/rtp_rtcp_defines.h"
#include "webrtc/typedefs.h"

namespace webrtc {

class ViEEncoder;

class EncoderStateFeedback : public RtcpIntraFrameObserver {
 public:
  EncoderStateFeedback();

  // Adds an encoder to receive feedback for a set of SSRCs.
  void Init(const std::vector<uint32_t>& ssrc, ViEEncoder* encoder);

  void OnReceivedIntraFrameRequest(uint32_t ssrc) override;
  void OnReceivedSLI(uint32_t ssrc, uint8_t picture_id) override;
  void OnReceivedRPSI(uint32_t ssrc, uint64_t picture_id) override;
  void OnLocalSsrcChanged(uint32_t old_ssrc, uint32_t new_ssrc) override;

 private:
  bool HasSsrc(uint32_t ssrc) EXCLUSIVE_LOCKS_REQUIRED(crit_);
  rtc::CriticalSection crit_;

  std::vector<uint32_t> ssrcs_ GUARDED_BY(crit_);
  ViEEncoder* vie_encoder_ GUARDED_BY(crit_);
};

}  // namespace webrtc

#endif  // WEBRTC_VIDEO_ENCODER_STATE_FEEDBACK_H_
