/*
 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 *
 */

#ifndef COMMON_VIDEO_H264_SPS_VUI_REWRITER_H_
#define COMMON_VIDEO_H264_SPS_VUI_REWRITER_H_

#include <stddef.h>
#include <stdint.h>

#include "absl/types/optional.h"
#include "common_video/h264/sps_parser.h"
#include "rtc_base/buffer.h"

namespace webrtc {

// A class that can parse an SPS block of a NAL unit and if necessary
// creates a copy with updated settings to allow for faster decoding for streams
// that use picture order count type 0. Streams in that format incur additional
// delay because it allows decode order to differ from render order.
// The mechanism used is to rewrite (edit or add) the SPS's VUI to contain
// restrictions on the maximum number of reordered pictures. This reduces
// latency significantly, though it still adds about a frame of latency to
// decoding.
class SpsVuiRewriter : private SpsParser {
 public:
  enum class ParseResult { kFailure, kVuiOk, kVuiRewritten };
  enum class Direction { kIncoming, kOutgoing };

  // Parses an SPS block and if necessary copies it and rewrites the VUI.
  // Returns kFailure on failure, kParseOk if parsing succeeded and no update
  // was necessary and kParsedAndModified if an updated copy of buffer was
  // written to destination. destination may be populated with some data even if
  // no rewrite was necessary, but the end offset should remain unchanged.
  // Unless parsing fails, the sps parameter will be populated with the parsed
  // SPS state. This function assumes that any previous headers
  // (NALU start, type, Stap-A, etc) have already been parsed and that RBSP
  // decoding has been performed.
  static ParseResult ParseAndRewriteSps(
      const uint8_t* buffer,
      size_t length,
      absl::optional<SpsParser::SpsState>* sps,
      rtc::Buffer* destination,
      Direction Direction);

  // Parses NAL units from |buffer| based on |nalu_offsets| and |nalu_lengths|
  // and rewrites VUI in SPS blocks if necessary.
  // The result is written to |output_buffer| and modified NAL unit offsets
  // and lenghts are written to |output_nalu_offsets| and |output_nalu_lenghts|
  // to account for any added data.
  static void ParseOutgoingBitstreamAndRewriteSps(
      rtc::ArrayView<const uint8_t> buffer,
      size_t num_nalus,
      const size_t* nalu_offsets,
      const size_t* nalu_lengths,
      rtc::Buffer* output_buffer,
      size_t* output_nalu_offsets,
      size_t* output_nalu_lengths);

 private:
  static ParseResult ParseAndRewriteSps(
      const uint8_t* buffer,
      size_t length,
      absl::optional<SpsParser::SpsState>* sps,
      rtc::Buffer* destination);

  static void UpdateStats(ParseResult result, Direction direction);
};

}  // namespace webrtc

#endif  // COMMON_VIDEO_H264_SPS_VUI_REWRITER_H_
