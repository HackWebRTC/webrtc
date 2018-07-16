/*
 *  Copyright (c) 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "call/payload_router.h"

#include "modules/rtp_rtcp/include/rtp_rtcp.h"
#include "modules/rtp_rtcp/include/rtp_rtcp_defines.h"
#include "modules/video_coding/include/video_codec_interface.h"
#include "rtc_base/checks.h"

namespace webrtc {

namespace {
absl::optional<size_t> GetSimulcastIdx(const CodecSpecificInfo* info) {
  if (!info)
    return absl::nullopt;
  switch (info->codecType) {
    case kVideoCodecVP8:
      return absl::optional<size_t>(info->codecSpecific.VP8.simulcastIdx);
    case kVideoCodecH264:
      return absl::optional<size_t>(info->codecSpecific.H264.simulcast_idx);
    case kVideoCodecMultiplex:
    case kVideoCodecGeneric:
      return absl::optional<size_t>(info->codecSpecific.generic.simulcast_idx);
    default:
      return absl::nullopt;
  }
}
}  // namespace

PayloadRouter::PayloadRouter(const std::vector<RtpRtcp*>& rtp_modules,
                             const std::vector<uint32_t>& ssrcs,
                             int payload_type,
                             const std::map<uint32_t, RtpPayloadState>& states)
    : active_(false), rtp_modules_(rtp_modules), payload_type_(payload_type) {
  RTC_DCHECK_EQ(ssrcs.size(), rtp_modules.size());
  // SSRCs are assumed to be sorted in the same order as |rtp_modules|.
  for (uint32_t ssrc : ssrcs) {
    // Restore state if it previously existed.
    const RtpPayloadState* state = nullptr;
    auto it = states.find(ssrc);
    if (it != states.end()) {
      state = &it->second;
    }
    params_.push_back(RtpPayloadParams(ssrc, state));
  }
}

PayloadRouter::~PayloadRouter() {}

void PayloadRouter::SetActive(bool active) {
  rtc::CritScope lock(&crit_);
  if (active_ == active)
    return;
  const std::vector<bool> active_modules(rtp_modules_.size(), active);
  SetActiveModules(active_modules);
}

void PayloadRouter::SetActiveModules(const std::vector<bool> active_modules) {
  rtc::CritScope lock(&crit_);
  RTC_DCHECK_EQ(rtp_modules_.size(), active_modules.size());
  active_ = false;
  for (size_t i = 0; i < active_modules.size(); ++i) {
    if (active_modules[i]) {
      active_ = true;
    }
    // Sends a kRtcpByeCode when going from true to false.
    rtp_modules_[i]->SetSendingStatus(active_modules[i]);
    // If set to false this module won't send media.
    rtp_modules_[i]->SetSendingMediaStatus(active_modules[i]);
  }
}

bool PayloadRouter::IsActive() {
  rtc::CritScope lock(&crit_);
  return active_ && !rtp_modules_.empty();
}

std::map<uint32_t, RtpPayloadState> PayloadRouter::GetRtpPayloadStates() const {
  rtc::CritScope lock(&crit_);
  std::map<uint32_t, RtpPayloadState> payload_states;
  for (const auto& param : params_) {
    payload_states[param.ssrc()] = param.state();
  }
  return payload_states;
}

EncodedImageCallback::Result PayloadRouter::OnEncodedImage(
    const EncodedImage& encoded_image,
    const CodecSpecificInfo* codec_specific_info,
    const RTPFragmentationHeader* fragmentation) {
  rtc::CritScope lock(&crit_);
  RTC_DCHECK(!rtp_modules_.empty());
  if (!active_)
    return Result(Result::ERROR_SEND_FAILED);

  size_t stream_index = GetSimulcastIdx(codec_specific_info).value_or(0);
  RTC_DCHECK_LT(stream_index, rtp_modules_.size());
  RTPVideoHeader rtp_video_header = params_[stream_index].GetRtpVideoHeader(
      encoded_image, codec_specific_info);

  uint32_t frame_id;
  if (!rtp_modules_[stream_index]->Sending()) {
    // The payload router could be active but this module isn't sending.
    return Result(Result::ERROR_SEND_FAILED);
  }
  bool send_result = rtp_modules_[stream_index]->SendOutgoingData(
      encoded_image._frameType, payload_type_, encoded_image._timeStamp,
      encoded_image.capture_time_ms_, encoded_image._buffer,
      encoded_image._length, fragmentation, &rtp_video_header, &frame_id);
  if (!send_result)
    return Result(Result::ERROR_SEND_FAILED);

  return Result(Result::OK, frame_id);
}

void PayloadRouter::OnBitrateAllocationUpdated(
    const VideoBitrateAllocation& bitrate) {
  rtc::CritScope lock(&crit_);
  if (IsActive()) {
    if (rtp_modules_.size() == 1) {
      // If spatial scalability is enabled, it is covered by a single stream.
      rtp_modules_[0]->SetVideoBitrateAllocation(bitrate);
    } else {
      std::vector<absl::optional<VideoBitrateAllocation>> layer_bitrates =
          bitrate.GetSimulcastAllocations();
      // Simulcast is in use, split the VideoBitrateAllocation into one struct
      // per rtp stream, moving over the temporal layer allocation.
      for (size_t i = 0; i < rtp_modules_.size(); ++i) {
        // The next spatial layer could be used if the current one is
        // inactive.
        if (layer_bitrates[i]) {
          rtp_modules_[i]->SetVideoBitrateAllocation(*layer_bitrates[i]);
        }
      }
    }
  }
}

}  // namespace webrtc
