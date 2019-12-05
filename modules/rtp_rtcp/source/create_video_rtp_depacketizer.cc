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
#include "modules/rtp_rtcp/source/rtp_format.h"
#include "modules/rtp_rtcp/source/video_rtp_depacketizer.h"
#include "rtc_base/checks.h"
#include "rtc_base/copy_on_write_buffer.h"

namespace webrtc {
namespace {

// Wrapper over legacy RtpDepacketizer interface.
// TODO(bugs.webrtc.org/11152): Delete when all RtpDepacketizers updated to
// the VideoRtpDepacketizer interface.
class LegacyRtpDepacketizer : public VideoRtpDepacketizer {
 public:
  explicit LegacyRtpDepacketizer(VideoCodecType codec) : codec_(codec) {}
  ~LegacyRtpDepacketizer() override = default;

  absl::optional<ParsedRtpPayload> Parse(
      rtc::CopyOnWriteBuffer rtp_payload) override {
    auto depacketizer = absl::WrapUnique(RtpDepacketizer::Create(codec_));
    RTC_CHECK(depacketizer);
    RtpDepacketizer::ParsedPayload parsed_payload;
    if (!depacketizer->Parse(&parsed_payload, rtp_payload.cdata(),
                             rtp_payload.size())) {
      return absl::nullopt;
    }
    absl::optional<ParsedRtpPayload> result(absl::in_place);
    result->video_header = parsed_payload.video;
    result->video_payload.SetData(parsed_payload.payload,
                                  parsed_payload.payload_length);
    return result;
  }

 private:
  const VideoCodecType codec_;
};

}  // namespace

std::unique_ptr<VideoRtpDepacketizer> CreateVideoRtpDepacketizer(
    VideoCodecType codec) {
  // TODO(bugs.webrtc.org/11152): switch on codec and create specialized
  // VideoRtpDepacketizers when they are migrated to new interface.
  return std::make_unique<LegacyRtpDepacketizer>(codec);
}

}  // namespace webrtc
