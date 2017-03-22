/* Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
*
*  Use of this source code is governed by a BSD-style license
*  that can be found in the LICENSE file in the root of the source
*  tree. An additional intellectual property rights grant can be found
*  in the file PATENTS.  All contributing project authors may
*  be found in the AUTHORS file in the root of the source tree.
*/
/*
* This file defines classes for doing temporal layers with VP8.
*/
#ifndef WEBRTC_MODULES_VIDEO_CODING_CODECS_VP8_DEFAULT_TEMPORAL_LAYERS_H_
#define WEBRTC_MODULES_VIDEO_CODING_CODECS_VP8_DEFAULT_TEMPORAL_LAYERS_H_

#include <vector>

#include "webrtc/modules/video_coding/codecs/vp8/temporal_layers.h"

#include "webrtc/base/optional.h"

namespace webrtc {

enum TemporalBufferUsage {
  kNone = 0,
  kReference = 1,
  kUpdate = 2,
  kReferenceAndUpdate = kReference | kUpdate,
};
enum TemporalFlags { kLayerSync = 1, kFreezeEntropy = 2 };

struct TemporalReferences {
  TemporalReferences(TemporalBufferUsage last,
                     TemporalBufferUsage golden,
                     TemporalBufferUsage arf);
  TemporalReferences(TemporalBufferUsage last,
                     TemporalBufferUsage golden,
                     TemporalBufferUsage arf,
                     int extra_flags);

  const bool reference_last;
  const bool update_last;
  const bool reference_golden;
  const bool update_golden;
  const bool reference_arf;
  const bool update_arf;

  // TODO(pbos): Consider breaking these out of here and returning only a
  // pattern index that needs to be returned to fill CodecSpecificInfoVP8 or
  // EncodeFlags.
  const bool layer_sync;
  const bool freeze_entropy;

 private:
  TemporalReferences(TemporalBufferUsage last,
                     TemporalBufferUsage golden,
                     TemporalBufferUsage arf,
                     bool layer_sync,
                     bool freeze_entropy);
};

class DefaultTemporalLayers : public TemporalLayers {
 public:
  DefaultTemporalLayers(int number_of_temporal_layers,
                        uint8_t initial_tl0_pic_idx);
  virtual ~DefaultTemporalLayers() {}

  // Returns the recommended VP8 encode flags needed. May refresh the decoder
  // and/or update the reference buffers.
  int EncodeFlags(uint32_t timestamp) override;

  // Update state based on new bitrate target and incoming framerate.
  // Returns the bitrate allocation for the active temporal layers.
  std::vector<uint32_t> OnRatesUpdated(int bitrate_kbps,
                                       int max_bitrate_kbps,
                                       int framerate) override;

  bool UpdateConfiguration(vpx_codec_enc_cfg_t* cfg) override;

  void PopulateCodecSpecific(bool base_layer_sync,
                             CodecSpecificInfoVP8* vp8_info,
                             uint32_t timestamp) override;

  void FrameEncoded(unsigned int size, uint32_t timestamp, int qp) override {}

  int CurrentLayerId() const override;

 private:
  const size_t num_layers_;
  const std::vector<unsigned int> temporal_ids_;
  const std::vector<TemporalReferences> temporal_pattern_;

  uint8_t tl0_pic_idx_;
  uint8_t pattern_idx_;
  uint32_t timestamp_;
  bool last_base_layer_sync_;
  rtc::Optional<std::vector<uint32_t>> new_bitrates_kbps_;
};

}  // namespace webrtc
#endif  // WEBRTC_MODULES_VIDEO_CODING_CODECS_VP8_DEFAULT_TEMPORAL_LAYERS_H_
