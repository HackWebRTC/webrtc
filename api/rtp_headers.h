/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef API_RTP_HEADERS_H_
#define API_RTP_HEADERS_H_

#include <stddef.h>
#include <stdint.h>
#include <string>

#include "absl/types/optional.h"
#include "api/array_view.h"
#include "api/video/color_space.h"
#include "api/video/video_content_type.h"
#include "api/video/video_frame_marking.h"
#include "api/video/video_rotation.h"
#include "api/video/video_timing.h"
#include "common_types.h"  // NOLINT(build/include)

namespace webrtc {

struct FeedbackRequest {
  // Determines whether the recv delta as specified in
  // https://tools.ietf.org/html/draft-holmer-rmcat-transport-wide-cc-extensions-01
  // should be included.
  bool include_timestamps;
  // Include feedback of received packets in the range [sequence_number -
  // sequence_count + 1, sequence_number]. That is, no feedback will be sent if
  // sequence_count is zero.
  int sequence_count;
};

struct RTPHeaderExtension {
  RTPHeaderExtension();
  RTPHeaderExtension(const RTPHeaderExtension& other);
  RTPHeaderExtension& operator=(const RTPHeaderExtension& other);

  bool hasTransmissionTimeOffset;
  int32_t transmissionTimeOffset;
  bool hasAbsoluteSendTime;
  uint32_t absoluteSendTime;
  bool hasTransportSequenceNumber;
  uint16_t transportSequenceNumber;
  absl::optional<FeedbackRequest> feedback_request;

  // Audio Level includes both level in dBov and voiced/unvoiced bit. See:
  // https://datatracker.ietf.org/doc/draft-lennox-avt-rtp-audio-level-exthdr/
  bool hasAudioLevel;
  bool voiceActivity;
  uint8_t audioLevel;

  // For Coordination of Video Orientation. See
  // http://www.etsi.org/deliver/etsi_ts/126100_126199/126114/12.07.00_60/
  // ts_126114v120700p.pdf
  bool hasVideoRotation;
  VideoRotation videoRotation;

  // TODO(ilnik): Refactor this and one above to be absl::optional() and remove
  // a corresponding bool flag.
  bool hasVideoContentType;
  VideoContentType videoContentType;

  bool has_video_timing;
  VideoSendTiming video_timing;

  bool has_frame_marking;
  FrameMarking frame_marking;

  PlayoutDelay playout_delay = {-1, -1};

  // For identification of a stream when ssrc is not signaled. See
  // https://tools.ietf.org/html/draft-ietf-avtext-rid-09
  // TODO(danilchap): Update url from draft to release version.
  std::string stream_id;
  std::string repaired_stream_id;

  // For identifying the media section used to interpret this RTP packet. See
  // https://tools.ietf.org/html/draft-ietf-mmusic-sdp-bundle-negotiation-38
  std::string mid;

  absl::optional<ColorSpace> color_space;
};

struct RTPHeader {
  RTPHeader();
  RTPHeader(const RTPHeader& other);
  RTPHeader& operator=(const RTPHeader& other);

  bool markerBit;
  uint8_t payloadType;
  uint16_t sequenceNumber;
  uint32_t timestamp;
  uint32_t ssrc;
  uint8_t numCSRCs;
  uint32_t arrOfCSRCs[kRtpCsrcSize];
  size_t paddingLength;
  size_t headerLength;
  int payload_type_frequency;
  RTPHeaderExtension extension;
};

// RTCP mode to use. Compound mode is described by RFC 4585 and reduced-size
// RTCP mode is described by RFC 5506.
enum class RtcpMode { kOff, kCompound, kReducedSize };

enum NetworkState {
  kNetworkUp,
  kNetworkDown,
};

}  // namespace webrtc

#endif  // API_RTP_HEADERS_H_
