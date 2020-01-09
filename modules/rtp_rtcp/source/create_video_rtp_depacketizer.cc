/*
 *  Copyright (c) 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/rtp_rtcp/source/create_video_rtp_depacketizer.h"

#include <memory>

#include "absl/memory/memory.h"
#include "absl/types/optional.h"
#include "modules/rtp_rtcp/source/rtp_depacketizer_av1.h"
#include "modules/rtp_rtcp/source/video_rtp_depacketizer.h"
#include "modules/rtp_rtcp/source/video_rtp_depacketizer_generic.h"
#include "modules/rtp_rtcp/source/video_rtp_depacketizer_h264.h"
#include "modules/rtp_rtcp/source/video_rtp_depacketizer_vp8.h"
#include "modules/rtp_rtcp/source/video_rtp_depacketizer_vp9.h"
#include "rtc_base/checks.h"
#include "rtc_base/copy_on_write_buffer.h"

namespace webrtc {
namespace {

// Wrapper over legacy RtpDepacketizer interface.
// TODO(bugs.webrtc.org/11152): Delete when all RtpDepacketizers updated to
// the VideoRtpDepacketizer interface.
template <typename Depacketizer>
class Legacy : public VideoRtpDepacketizer {
 public:
  absl::optional<ParsedRtpPayload> Parse(
      rtc::CopyOnWriteBuffer rtp_payload) override {
    Depacketizer depacketizer;
    RtpDepacketizer::ParsedPayload parsed_payload;
    if (!depacketizer.Parse(&parsed_payload, rtp_payload.cdata(),
                            rtp_payload.size())) {
      return absl::nullopt;
    }
    absl::optional<ParsedRtpPayload> result(absl::in_place);
    result->video_header = parsed_payload.video;
    result->video_payload.SetData(parsed_payload.payload,
                                  parsed_payload.payload_length);
    return result;
  }
};

}  // namespace

std::unique_ptr<VideoRtpDepacketizer> CreateVideoRtpDepacketizer(
    VideoCodecType codec) {
  switch (codec) {
    case kVideoCodecH264:
      return std::make_unique<Legacy<RtpDepacketizerH264>>();
    case kVideoCodecVP8:
      return std::make_unique<VideoRtpDepacketizerVp8>();
    case kVideoCodecVP9:
      return std::make_unique<VideoRtpDepacketizerVp9>();
    case kVideoCodecAV1:
      return std::make_unique<Legacy<RtpDepacketizerAv1>>();
    case kVideoCodecGeneric:
    case kVideoCodecMultiplex:
      return std::make_unique<VideoRtpDepacketizerGeneric>();
  }
}

}  // namespace webrtc
