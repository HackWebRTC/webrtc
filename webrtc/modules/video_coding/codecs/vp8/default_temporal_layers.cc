/* Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
*
*  Use of this source code is governed by a BSD-style license
*  that can be found in the LICENSE file in the root of the source
*  tree. An additional intellectual property rights grant can be found
*  in the file PATENTS.  All contributing project authors may
*  be found in the AUTHORS file in the root of the source tree.
*/

#include "webrtc/modules/video_coding/codecs/vp8/default_temporal_layers.h"

#include <stdlib.h>
#include <string.h>

#include <algorithm>
#include <vector>

#include "webrtc/base/checks.h"
#include "webrtc/modules/include/module_common_types.h"
#include "webrtc/modules/video_coding/include/video_codec_interface.h"
#include "webrtc/modules/video_coding/codecs/vp8/include/vp8_common_types.h"

#include "vpx/vpx_encoder.h"
#include "vpx/vp8cx.h"

namespace webrtc {

TemporalReferences::TemporalReferences(TemporalBufferFlags last,
                                       TemporalBufferFlags golden,
                                       TemporalBufferFlags arf)
    : TemporalReferences(last, golden, arf, false, false) {}

TemporalReferences::TemporalReferences(TemporalBufferFlags last,
                                       TemporalBufferFlags golden,
                                       TemporalBufferFlags arf,
                                       int extra_flags)
    : TemporalReferences(last,
                         golden,
                         arf,
                         (extra_flags & kLayerSync) != 0,
                         (extra_flags & kFreezeEntropy) != 0) {}

TemporalReferences::TemporalReferences(TemporalBufferFlags last,
                                       TemporalBufferFlags golden,
                                       TemporalBufferFlags arf,
                                       bool layer_sync,
                                       bool freeze_entropy)
    : drop_frame(last == kNone && golden == kNone && arf == kNone),
      last_buffer_flags(last),
      golden_buffer_flags(golden),
      arf_buffer_flags(arf),
      layer_sync(layer_sync),
      freeze_entropy(freeze_entropy) {}

namespace {

std::vector<unsigned int> GetTemporalIds(size_t num_layers) {
  switch (num_layers) {
    case 1:
      // Temporal layer structure (single layer):
      // 0 0 0 0 ...
      return {0};
    case 2:
      // Temporal layer structure:
      //   1   1 ...
      // 0   0   ...
      return {0, 1};
    case 3:
      // Temporal layer structure:
      //   2   2   2   2 ...
      //     1       1   ...
      // 0       0       ...
      return {0, 2, 1, 2};
    case 4:
      // Temporal layer structure:
      //   3   3   3   3   3   3   3   3 ...
      //     2       2       2       2   ...
      //         1               1       ...
      // 0               0               ...
      return {0, 3, 2, 3, 1, 3, 2, 3};
    default:
      RTC_NOTREACHED();
      break;
  }
  RTC_NOTREACHED();
  return {0};
}

std::vector<TemporalReferences> GetTemporalPattern(
    size_t num_layers) {
  // For indexing in the patterns described below (which temporal layers they
  // belong to), see the diagram above.
  // Layer sync is done similarly for all patterns (except single stream) and
  // happens every 8 frames:
  // TL1 layer syncs by periodically by only referencing TL0 ('last'), but still
  // updating 'golden', so it can be used as a reference by future TL1 frames.
  // TL2 layer syncs just before TL1 by only depending on TL0 (and not depending
  // on TL1's buffer before TL1 has layer synced).
  // TODO(pbos): Consider cyclically updating 'arf' (and 'golden' for 1TL) for
  // the base layer in 1-3TL instead of 'last' periodically on long intervals,
  // so that if scene changes occur (user walks between rooms or rotates webcam)
  // the 'arf' (or 'golden' respectively) is not stuck on a no-longer relevant
  // keyframe.
  switch (num_layers) {
    case 1:
      // All frames reference all buffers and the 'last' buffer is updated.
      return {TemporalReferences(kReferenceAndUpdate, kReference, kReference)};
    case 2:
      // All layers can reference but not update the 'alt' buffer, this means
      // that the 'alt' buffer reference is effectively the last keyframe.
      // TL0 also references and updates the 'last' buffer.
      // TL1 also references 'last' and references and updates 'golden'.
      return {TemporalReferences(kReferenceAndUpdate, kUpdate, kReference),
              TemporalReferences(kReference, kUpdate, kReference, kLayerSync),
              TemporalReferences(kReferenceAndUpdate, kNone, kReference),
              TemporalReferences(kReference, kReferenceAndUpdate, kReference),
              TemporalReferences(kReferenceAndUpdate, kNone, kReference),
              TemporalReferences(kReference, kReferenceAndUpdate, kReference),
              TemporalReferences(kReferenceAndUpdate, kNone, kReference),
              TemporalReferences(kReference, kReference, kReference,
                                 kFreezeEntropy)};
    case 3:
      // All layers can reference but not update the 'alt' buffer, this means
      // that the 'alt' buffer reference is effectively the last keyframe.
      // TL0 also references and updates the 'last' buffer.
      // TL1 also references 'last' and references and updates 'golden'.
      // TL2 references both 'last' and 'golden' but updates no buffer.
      return {TemporalReferences(kReferenceAndUpdate, kUpdate, kReference),
              TemporalReferences(kReference, kNone, kReference,
                                 kLayerSync | kFreezeEntropy),
              TemporalReferences(kReference, kUpdate, kReference, kLayerSync),
              TemporalReferences(kReference, kReference, kReference,
                                 kFreezeEntropy),
              TemporalReferences(kReferenceAndUpdate, kNone, kReference),
              TemporalReferences(kReference, kReference, kReference,
                                 kFreezeEntropy),
              TemporalReferences(kReference, kReferenceAndUpdate, kReference),
              TemporalReferences(kReference, kReference, kReference,
                                 kFreezeEntropy)};
    case 4:
      // TL0 references and updates only the 'last' buffer.
      // TL1 references 'last' and updates and references 'golden'.
      // TL2 references 'last' and 'golden', and references and updates 'arf'.
      // TL3 references all buffers but update none of them.
      return {TemporalReferences(kReferenceAndUpdate, kNone, kNone),
              TemporalReferences(kReference, kReference, kReference,
                                 kLayerSync | kFreezeEntropy),
              TemporalReferences(kReference, kNone, kUpdate, kLayerSync),
              TemporalReferences(kReference, kReference, kReference,
                                 kLayerSync | kFreezeEntropy),
              TemporalReferences(kReference, kUpdate, kNone, kLayerSync),
              TemporalReferences(kReference, kReference, kReference,
                                 kLayerSync | kFreezeEntropy),
              TemporalReferences(kReference, kReference, kReferenceAndUpdate),
              TemporalReferences(kReference, kReference, kReference,
                                 kLayerSync | kFreezeEntropy),
              TemporalReferences(kReferenceAndUpdate, kNone, kNone),
              TemporalReferences(kReference, kReference, kReference,
                                 kLayerSync | kFreezeEntropy),
              TemporalReferences(kReference, kReference, kReferenceAndUpdate),
              TemporalReferences(kReference, kReference, kReference,
                                 kLayerSync | kFreezeEntropy),
              TemporalReferences(kReference, kReferenceAndUpdate, kNone),
              TemporalReferences(kReference, kReference, kReference,
                                 kLayerSync | kFreezeEntropy),
              TemporalReferences(kReference, kReference, kReferenceAndUpdate),
              TemporalReferences(kReference, kReference, kReference,
                                 kLayerSync | kFreezeEntropy)};
    default:
      RTC_NOTREACHED();
      break;
  }
  RTC_NOTREACHED();
  return {TemporalReferences(kNone, kNone, kNone)};
}

}  // namespace

DefaultTemporalLayers::DefaultTemporalLayers(int number_of_temporal_layers,
                                             uint8_t initial_tl0_pic_idx)
    : num_layers_(std::max(1, number_of_temporal_layers)),
      temporal_ids_(GetTemporalIds(num_layers_)),
      temporal_pattern_(GetTemporalPattern(num_layers_)),
      tl0_pic_idx_(initial_tl0_pic_idx),
      pattern_idx_(255),
      timestamp_(0),
      last_base_layer_sync_(false) {
  RTC_CHECK_GE(kMaxTemporalStreams, number_of_temporal_layers);
  RTC_CHECK_GE(number_of_temporal_layers, 0);
  RTC_CHECK_LE(number_of_temporal_layers, 4);
}

int DefaultTemporalLayers::CurrentLayerId() const {
  return temporal_ids_[pattern_idx_ % temporal_ids_.size()];
}

std::vector<uint32_t> DefaultTemporalLayers::OnRatesUpdated(
    int bitrate_kbps,
    int max_bitrate_kbps,
    int framerate) {
  std::vector<uint32_t> bitrates;
  for (size_t i = 0; i < num_layers_; ++i) {
    float layer_bitrate =
        bitrate_kbps * kVp8LayerRateAlloction[num_layers_ - 1][i];
    bitrates.push_back(static_cast<uint32_t>(layer_bitrate + 0.5));
  }
  new_bitrates_kbps_ = rtc::Optional<std::vector<uint32_t>>(bitrates);

  // Allocation table is of aggregates, transform to individual rates.
  uint32_t sum = 0;
  for (size_t i = 0; i < num_layers_; ++i) {
    uint32_t layer_bitrate = bitrates[i];
    RTC_DCHECK_LE(sum, bitrates[i]);
    bitrates[i] -= sum;
    sum = layer_bitrate;

    if (sum >= static_cast<uint32_t>(bitrate_kbps)) {
      // Sum adds up; any subsequent layers will be 0.
      bitrates.resize(i + 1);
      break;
    }
  }

  return bitrates;
}

bool DefaultTemporalLayers::UpdateConfiguration(vpx_codec_enc_cfg_t* cfg) {
  if (!new_bitrates_kbps_)
    return false;

  for (size_t i = 0; i < num_layers_; ++i) {
    cfg->ts_target_bitrate[i] = (*new_bitrates_kbps_)[i];
    // ..., 4, 2, 1
    cfg->ts_rate_decimator[i] = 1 << (num_layers_ - i - 1);
  }

  cfg->ts_number_layers = num_layers_;
  cfg->ts_periodicity = temporal_ids_.size();
  memcpy(cfg->ts_layer_id, &temporal_ids_[0],
         sizeof(unsigned int) * temporal_ids_.size());

  new_bitrates_kbps_ = rtc::Optional<std::vector<uint32_t>>();

  return true;
}

// TODO(pbos): Name method so that it's obvious that it updates state.
TemporalReferences DefaultTemporalLayers::UpdateLayerConfig(
    uint32_t timestamp) {
  RTC_DCHECK_GT(num_layers_, 0);
  RTC_DCHECK_LT(0, temporal_pattern_.size());
  return temporal_pattern_[++pattern_idx_ % temporal_pattern_.size()];
}

int TemporalLayers::EncodeFlags(uint32_t timestamp) {
  TemporalReferences references = UpdateLayerConfig(timestamp);
  if (references.drop_frame)
    return -1;

  int flags = 0;

  if ((references.last_buffer_flags & kReference) == 0)
    flags |= VP8_EFLAG_NO_REF_LAST;
  if ((references.last_buffer_flags & kUpdate) == 0)
    flags |= VP8_EFLAG_NO_UPD_LAST;
  if ((references.golden_buffer_flags & kReference) == 0)
    flags |= VP8_EFLAG_NO_REF_GF;
  if ((references.golden_buffer_flags & kUpdate) == 0)
    flags |= VP8_EFLAG_NO_UPD_GF;
  if ((references.arf_buffer_flags & kReference) == 0)
    flags |= VP8_EFLAG_NO_REF_ARF;
  if ((references.arf_buffer_flags & kUpdate) == 0)
    flags |= VP8_EFLAG_NO_UPD_ARF;
  if (references.freeze_entropy)
    flags |= VP8_EFLAG_NO_UPD_ENTROPY;

  return flags;
}

void DefaultTemporalLayers::PopulateCodecSpecific(
    bool frame_is_keyframe,
    CodecSpecificInfoVP8* vp8_info,
    uint32_t timestamp) {
  RTC_DCHECK_GT(num_layers_, 0);

  if (num_layers_ == 1) {
    vp8_info->temporalIdx = kNoTemporalIdx;
    vp8_info->layerSync = false;
    vp8_info->tl0PicIdx = kNoTl0PicIdx;
  } else {
    if (frame_is_keyframe) {
      vp8_info->temporalIdx = 0;
      vp8_info->layerSync = true;
    } else {
      vp8_info->temporalIdx = CurrentLayerId();
      TemporalReferences temporal_reference =
          temporal_pattern_[pattern_idx_ % temporal_pattern_.size()];

      vp8_info->layerSync = temporal_reference.layer_sync;
    }
    if (last_base_layer_sync_ && vp8_info->temporalIdx != 0) {
      // Regardless of pattern the frame after a base layer sync will always
      // be a layer sync.
      vp8_info->layerSync = true;
    }
    if (vp8_info->temporalIdx == 0 && timestamp != timestamp_) {
      timestamp_ = timestamp;
      tl0_pic_idx_++;
    }
    last_base_layer_sync_ = frame_is_keyframe;
    vp8_info->tl0PicIdx = tl0_pic_idx_;
  }
}

TemporalLayers* TemporalLayersFactory::Create(
    int simulcast_id,
    int temporal_layers,
    uint8_t initial_tl0_pic_idx) const {
  TemporalLayers* tl =
      new DefaultTemporalLayers(temporal_layers, initial_tl0_pic_idx);
  if (listener_)
    listener_->OnTemporalLayersCreated(simulcast_id, tl);
  return tl;
}

void TemporalLayersFactory::SetListener(TemporalLayersListener* listener) {
  listener_ = listener;
}

}  // namespace webrtc
