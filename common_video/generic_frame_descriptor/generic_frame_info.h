/*
 *  Copyright (c) 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef COMMON_VIDEO_GENERIC_FRAME_DESCRIPTOR_GENERIC_FRAME_INFO_H_
#define COMMON_VIDEO_GENERIC_FRAME_DESCRIPTOR_GENERIC_FRAME_INFO_H_

#include <initializer_list>
#include <vector>

#include "absl/container/inlined_vector.h"
#include "absl/strings/string_view.h"
#include "api/array_view.h"
#include "api/video/video_codec_constants.h"

namespace webrtc {

// Describes how a certain encoder buffer was used when encoding a frame.
struct CodecBufferUsage {
  CodecBufferUsage(int id, bool referenced, bool updated)
      : id(id), referenced(referenced), updated(updated) {}

  int id = 0;
  bool referenced = false;
  bool updated = false;
};

struct GenericFrameInfo {
  enum class DecodeTargetIndication {
    kNotPresent,   // DecodeTargetInfo symbol '-'
    kDiscardable,  // DecodeTargetInfo symbol 'D'
    kSwitch,       // DecodeTargetInfo symbol 'S'
    kRequired      // DecodeTargetInfo symbol 'R'
  };

  static absl::InlinedVector<DecodeTargetIndication, 10> DecodeTargetInfo(
      absl::string_view indication_symbols);

  class Builder;

  GenericFrameInfo();
  GenericFrameInfo(const GenericFrameInfo&);
  ~GenericFrameInfo();

  int64_t frame_id = 0;
  int temporal_id = 0;
  int spatial_id = 0;
  absl::InlinedVector<int, 10> frame_diffs;
  absl::InlinedVector<DecodeTargetIndication, 10> decode_target_indications;
  absl::InlinedVector<CodecBufferUsage, kMaxEncoderBuffers> encoder_buffers;
};

class GenericFrameInfo::Builder {
 public:
  Builder();
  ~Builder();

  GenericFrameInfo Build() const;
  Builder& T(int temporal_id);
  Builder& S(int spatial_id);
  Builder& Dtis(absl::string_view indication_symbols);
  Builder& Fdiffs(std::initializer_list<int> frame_diffs);

 private:
  GenericFrameInfo info_;
};

struct TemplateStructure {
  TemplateStructure();
  TemplateStructure(const TemplateStructure&);
  TemplateStructure(TemplateStructure&&);
  TemplateStructure& operator=(const TemplateStructure&);
  ~TemplateStructure();

  int num_decode_targets = 0;
  std::vector<GenericFrameInfo> templates;
};
}  // namespace webrtc

#endif  // COMMON_VIDEO_GENERIC_FRAME_DESCRIPTOR_GENERIC_FRAME_INFO_H_
