/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/video/encoder_state_feedback.h"

#include "webrtc/base/checks.h"
#include "webrtc/video/vie_encoder.h"

namespace webrtc {

EncoderStateFeedback::EncoderStateFeedback() : vie_encoder_(nullptr) {}

void EncoderStateFeedback::Init(const std::vector<uint32_t>& ssrcs,
                                ViEEncoder* encoder) {
  RTC_DCHECK(!ssrcs.empty());
  rtc::CritScope lock(&crit_);
  ssrcs_ = ssrcs;
  vie_encoder_ = encoder;
}

bool EncoderStateFeedback::HasSsrc(uint32_t ssrc) {
  for (uint32_t registered_ssrc : ssrcs_) {
    if (registered_ssrc == ssrc)
      return true;
  }
  return false;
}

void EncoderStateFeedback::OnReceivedIntraFrameRequest(uint32_t ssrc) {
  rtc::CritScope lock(&crit_);
  if (!HasSsrc(ssrc))
    return;
  RTC_DCHECK(vie_encoder_);

  vie_encoder_->OnReceivedIntraFrameRequest(ssrc);
}

void EncoderStateFeedback::OnReceivedSLI(uint32_t ssrc, uint8_t picture_id) {
  rtc::CritScope lock(&crit_);
  if (!HasSsrc(ssrc))
    return;
  RTC_DCHECK(vie_encoder_);

  vie_encoder_->OnReceivedSLI(ssrc, picture_id);
}

void EncoderStateFeedback::OnReceivedRPSI(uint32_t ssrc, uint64_t picture_id) {
  rtc::CritScope lock(&crit_);
  if (!HasSsrc(ssrc))
    return;
  RTC_DCHECK(vie_encoder_);

  vie_encoder_->OnReceivedRPSI(ssrc, picture_id);
}

// Sending SSRCs for this encoder should never change since they are configured
// once and not reconfigured.
void EncoderStateFeedback::OnLocalSsrcChanged(uint32_t old_ssrc,
                                              uint32_t new_ssrc) {
  if (!RTC_DCHECK_IS_ON)
    return;
  rtc::CritScope lock(&crit_);
  if (ssrcs_.empty())  // Encoder not yet attached (or detached for teardown).
    return;
  // SSRC shouldn't change to something we haven't already registered with the
  // encoder.
  RTC_DCHECK(HasSsrc(new_ssrc));
}

}  // namespace webrtc
