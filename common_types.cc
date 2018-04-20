/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "common_types.h"  // NOLINT(build/include)

#include <string.h>
#include <algorithm>
#include <limits>
#include <type_traits>

#include "rtc_base/checks.h"
#include "rtc_base/strings/string_builder.h"
#include "rtc_base/stringutils.h"

namespace webrtc {

bool VideoCodecVP8::operator==(const VideoCodecVP8& other) const {
  return (complexity == other.complexity &&
          resilienceOn == other.resilienceOn &&
          numberOfTemporalLayers == other.numberOfTemporalLayers &&
          denoisingOn == other.denoisingOn &&
          automaticResizeOn == other.automaticResizeOn &&
          frameDroppingOn == other.frameDroppingOn &&
          keyFrameInterval == other.keyFrameInterval);
}

bool VideoCodecVP9::operator==(const VideoCodecVP9& other) const {
  return (complexity == other.complexity &&
          resilienceOn == other.resilienceOn &&
          numberOfTemporalLayers == other.numberOfTemporalLayers &&
          denoisingOn == other.denoisingOn &&
          frameDroppingOn == other.frameDroppingOn &&
          keyFrameInterval == other.keyFrameInterval &&
          adaptiveQpMode == other.adaptiveQpMode &&
          automaticResizeOn == other.automaticResizeOn &&
          numberOfSpatialLayers == other.numberOfSpatialLayers &&
          flexibleMode == other.flexibleMode);
}

bool VideoCodecH264::operator==(const VideoCodecH264& other) const {
  return (frameDroppingOn == other.frameDroppingOn &&
          keyFrameInterval == other.keyFrameInterval &&
          spsLen == other.spsLen &&
          ppsLen == other.ppsLen &&
          profile == other.profile &&
          (spsLen == 0 || memcmp(spsData, other.spsData, spsLen) == 0) &&
          (ppsLen == 0 || memcmp(ppsData, other.ppsData, ppsLen) == 0));
}

bool SpatialLayer::operator==(const SpatialLayer& other) const {
  return (width == other.width &&
          height == other.height &&
          numberOfTemporalLayers == other.numberOfTemporalLayers &&
          maxBitrate == other.maxBitrate &&
          targetBitrate == other.targetBitrate &&
          minBitrate == other.minBitrate &&
          qpMax == other.qpMax &&
          active == other.active);
}

VideoCodec::VideoCodec()
    : codecType(kVideoCodecUnknown),
      plType(0),
      width(0),
      height(0),
      startBitrate(0),
      maxBitrate(0),
      minBitrate(0),
      targetBitrate(0),
      maxFramerate(0),
      active(true),
      qpMax(0),
      numberOfSimulcastStreams(0),
      simulcastStream(),
      spatialLayers(),
      mode(kRealtimeVideo),
      expect_encode_from_texture(false),
      timing_frame_thresholds({0, 0}),
      codec_specific_() {}

VideoCodecVP8* VideoCodec::VP8() {
  RTC_DCHECK_EQ(codecType, kVideoCodecVP8);
  return &codec_specific_.VP8;
}

const VideoCodecVP8& VideoCodec::VP8() const {
  RTC_DCHECK_EQ(codecType, kVideoCodecVP8);
  return codec_specific_.VP8;
}

VideoCodecVP9* VideoCodec::VP9() {
  RTC_DCHECK_EQ(codecType, kVideoCodecVP9);
  return &codec_specific_.VP9;
}

const VideoCodecVP9& VideoCodec::VP9() const {
  RTC_DCHECK_EQ(codecType, kVideoCodecVP9);
  return codec_specific_.VP9;
}

VideoCodecH264* VideoCodec::H264() {
  RTC_DCHECK_EQ(codecType, kVideoCodecH264);
  return &codec_specific_.H264;
}

const VideoCodecH264& VideoCodec::H264() const {
  RTC_DCHECK_EQ(codecType, kVideoCodecH264);
  return codec_specific_.H264;
}

static const char* kPayloadNameVp8 = "VP8";
static const char* kPayloadNameVp9 = "VP9";
static const char* kPayloadNameH264 = "H264";
static const char* kPayloadNameI420 = "I420";
static const char* kPayloadNameRED = "RED";
static const char* kPayloadNameULPFEC = "ULPFEC";
static const char* kPayloadNameFlexfec = "flexfec-03";
static const char* kPayloadNameGeneric = "Generic";
static const char* kPayloadNameMultiplex = "Multiplex";

static bool CodecNamesEq(const char* name1, const char* name2) {
  return _stricmp(name1, name2) == 0;
}

const char* CodecTypeToPayloadString(VideoCodecType type) {
  switch (type) {
    case kVideoCodecVP8:
      return kPayloadNameVp8;
    case kVideoCodecVP9:
      return kPayloadNameVp9;
    case kVideoCodecH264:
      return kPayloadNameH264;
    case kVideoCodecI420:
      return kPayloadNameI420;
    case kVideoCodecRED:
      return kPayloadNameRED;
    case kVideoCodecULPFEC:
      return kPayloadNameULPFEC;
    case kVideoCodecFlexfec:
      return kPayloadNameFlexfec;
    // Other codecs default to generic.
    case kVideoCodecMultiplex:
    case kVideoCodecGeneric:
    case kVideoCodecUnknown:
      return kPayloadNameGeneric;
  }
  return kPayloadNameGeneric;
}

VideoCodecType PayloadStringToCodecType(const std::string& name) {
  if (CodecNamesEq(name.c_str(), kPayloadNameVp8))
    return kVideoCodecVP8;
  if (CodecNamesEq(name.c_str(), kPayloadNameVp9))
    return kVideoCodecVP9;
  if (CodecNamesEq(name.c_str(), kPayloadNameH264))
    return kVideoCodecH264;
  if (CodecNamesEq(name.c_str(), kPayloadNameI420))
    return kVideoCodecI420;
  if (CodecNamesEq(name.c_str(), kPayloadNameRED))
    return kVideoCodecRED;
  if (CodecNamesEq(name.c_str(), kPayloadNameULPFEC))
    return kVideoCodecULPFEC;
  if (CodecNamesEq(name.c_str(), kPayloadNameFlexfec))
    return kVideoCodecFlexfec;
  if (CodecNamesEq(name.c_str(), kPayloadNameMultiplex))
    return kVideoCodecMultiplex;
  return kVideoCodecGeneric;
}

const uint32_t BitrateAllocation::kMaxBitrateBps =
    std::numeric_limits<uint32_t>::max();

BitrateAllocation::BitrateAllocation() : sum_(0), bitrates_{}, has_bitrate_{} {}

bool BitrateAllocation::SetBitrate(size_t spatial_index,
                                   size_t temporal_index,
                                   uint32_t bitrate_bps) {
  RTC_CHECK_LT(spatial_index, kMaxSpatialLayers);
  RTC_CHECK_LT(temporal_index, kMaxTemporalStreams);
  RTC_CHECK_LE(bitrates_[spatial_index][temporal_index], sum_);
  uint64_t new_bitrate_sum_bps = sum_;
  new_bitrate_sum_bps -= bitrates_[spatial_index][temporal_index];
  new_bitrate_sum_bps += bitrate_bps;
  if (new_bitrate_sum_bps > kMaxBitrateBps)
    return false;

  bitrates_[spatial_index][temporal_index] = bitrate_bps;
  has_bitrate_[spatial_index][temporal_index] = true;
  sum_ = static_cast<uint32_t>(new_bitrate_sum_bps);
  return true;
}

bool BitrateAllocation::HasBitrate(size_t spatial_index,
                                   size_t temporal_index) const {
  RTC_CHECK_LT(spatial_index, kMaxSpatialLayers);
  RTC_CHECK_LT(temporal_index, kMaxTemporalStreams);
  return has_bitrate_[spatial_index][temporal_index];
}

uint32_t BitrateAllocation::GetBitrate(size_t spatial_index,
                                       size_t temporal_index) const {
  RTC_CHECK_LT(spatial_index, kMaxSpatialLayers);
  RTC_CHECK_LT(temporal_index, kMaxTemporalStreams);
  return bitrates_[spatial_index][temporal_index];
}

// Whether the specific spatial layers has the bitrate set in any of its
// temporal layers.
bool BitrateAllocation::IsSpatialLayerUsed(size_t spatial_index) const {
  RTC_CHECK_LT(spatial_index, kMaxSpatialLayers);
  for (int i = 0; i < kMaxTemporalStreams; ++i) {
    if (has_bitrate_[spatial_index][i])
      return true;
  }
  return false;
}

// Get the sum of all the temporal layer for a specific spatial layer.
uint32_t BitrateAllocation::GetSpatialLayerSum(size_t spatial_index) const {
  RTC_CHECK_LT(spatial_index, kMaxSpatialLayers);
  return GetTemporalLayerSum(spatial_index, kMaxTemporalStreams - 1);
}

uint32_t BitrateAllocation::GetTemporalLayerSum(size_t spatial_index,
                                                size_t temporal_index) const {
  RTC_CHECK_LT(spatial_index, kMaxSpatialLayers);
  RTC_CHECK_LT(temporal_index, kMaxTemporalStreams);
  uint32_t sum = 0;
  for (size_t i = 0; i <= temporal_index; ++i) {
    sum += bitrates_[spatial_index][i];
  }
  return sum;
}

std::vector<uint32_t> BitrateAllocation::GetTemporalLayerAllocation(
    size_t spatial_index) const {
  RTC_CHECK_LT(spatial_index, kMaxSpatialLayers);
  std::vector<uint32_t> temporal_rates;

  // Find the highest temporal layer with a defined bitrate in order to
  // determine the size of the temporal layer allocation.
  for (size_t i = kMaxTemporalStreams; i > 0; --i) {
    if (has_bitrate_[spatial_index][i - 1]) {
      temporal_rates.resize(i);
      break;
    }
  }

  for (size_t i = 0; i < temporal_rates.size(); ++i) {
    temporal_rates[i] = bitrates_[spatial_index][i];
  }

  return temporal_rates;
}

std::string BitrateAllocation::ToString() const {
  if (sum_ == 0)
    return "BitrateAllocation [ [] ]";

  // Max string length in practice is 260, but let's have some overhead and
  // round up to nearest power of two.
  char string_buf[512];
  rtc::SimpleStringBuilder ssb(string_buf);

  ssb << "BitrateAllocation [";
  uint32_t spatial_cumulator = 0;
  for (int si = 0; si < kMaxSpatialLayers; ++si) {
    RTC_DCHECK_LE(spatial_cumulator, sum_);
    if (spatial_cumulator == sum_)
      break;

    const uint32_t layer_sum = GetSpatialLayerSum(si);
    if (layer_sum == sum_) {
      ssb << " [";
    } else {
      if (si > 0)
        ssb << ",";
      ssb << '\n' << "  [";
    }
    spatial_cumulator += layer_sum;

    uint32_t temporal_cumulator = 0;
    for (int ti = 0; ti < kMaxTemporalStreams; ++ti) {
      RTC_DCHECK_LE(temporal_cumulator, layer_sum);
      if (temporal_cumulator == layer_sum)
        break;

      if (ti > 0)
        ssb << ", ";

      uint32_t bitrate = bitrates_[si][ti];
      ssb << bitrate;
      temporal_cumulator += bitrate;
    }
    ssb << "]";
  }

  RTC_DCHECK_EQ(spatial_cumulator, sum_);
  ssb << " ]";
  return ssb.str();
}

}  // namespace webrtc
