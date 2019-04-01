/*
 *  Copyright (c) 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "video/encoder_bitrate_adjuster.h"

#include <algorithm>

#include "absl/memory/memory.h"
#include "rtc_base/logging.h"
#include "rtc_base/time_utils.h"

namespace webrtc {

constexpr int64_t EncoderBitrateAdjuster::kWindowSizeMs;
constexpr size_t EncoderBitrateAdjuster::kMinFramesSinceLayoutChange;
constexpr double EncoderBitrateAdjuster::kDefaultUtilizationFactor;

EncoderBitrateAdjuster::EncoderBitrateAdjuster(const VideoCodec& codec_settings)
    : current_total_framerate_fps_(0),
      frames_since_layout_change_(0),
      min_bitrates_bps_{} {
  if (codec_settings.codecType == VideoCodecType::kVideoCodecVP9) {
    for (size_t si = 0; si < codec_settings.VP9().numberOfSpatialLayers; ++si) {
      if (codec_settings.spatialLayers[si].active) {
        min_bitrates_bps_[si] =
            std::max(codec_settings.minBitrate * 1000,
                     codec_settings.spatialLayers[si].minBitrate * 1000);
      }
    }
  } else {
    for (size_t si = 0; si < codec_settings.numberOfSimulcastStreams; ++si) {
      if (codec_settings.simulcastStream[si].active) {
        min_bitrates_bps_[si] =
            std::max(codec_settings.minBitrate * 1000,
                     codec_settings.simulcastStream[si].minBitrate * 1000);
      }
    }
  }
}

EncoderBitrateAdjuster::~EncoderBitrateAdjuster() = default;

VideoBitrateAllocation EncoderBitrateAdjuster::AdjustRateAllocation(
    const VideoBitrateAllocation& bitrate_allocation,
    int framerate_fps) {
  current_bitrate_allocation_ = bitrate_allocation;
  current_total_framerate_fps_ = framerate_fps;

  // First check that overshoot detectors exist, and store per spatial layer
  // how many active temporal layers we have.
  size_t active_tls_[kMaxSpatialLayers] = {};
  for (size_t si = 0; si < kMaxSpatialLayers; ++si) {
    active_tls_[si] = 0;
    for (size_t ti = 0; ti < kMaxTemporalStreams; ++ti) {
      // Layer is enabled iff it has both positive bitrate and framerate target.
      if (bitrate_allocation.GetBitrate(si, ti) > 0 &&
          current_fps_allocation_[si].size() > ti &&
          current_fps_allocation_[si][ti] > 0) {
        ++active_tls_[si];
        if (!overshoot_detectors_[si][ti]) {
          overshoot_detectors_[si][ti] =
              absl::make_unique<EncoderOvershootDetector>(kWindowSizeMs);
          frames_since_layout_change_ = 0;
        }
      } else if (overshoot_detectors_[si][ti]) {
        // Layer removed, destroy overshoot detector.
        overshoot_detectors_[si][ti].reset();
        frames_since_layout_change_ = 0;
      }
    }
  }

  // Next poll the overshoot detectors and populate the adjusted allocation.
  const int64_t now_ms = rtc::TimeMillis();
  VideoBitrateAllocation adjusted_allocation;
  for (size_t si = 0; si < kMaxSpatialLayers; ++si) {
    const uint32_t spatial_layer_bitrate_bps =
        bitrate_allocation.GetSpatialLayerSum(si);

    // Adjustment is done per spatial layer only (not per temporal layer).
    double utilization_factor;
    if (frames_since_layout_change_ < kMinFramesSinceLayoutChange) {
      utilization_factor = kDefaultUtilizationFactor;
    } else if (active_tls_[si] == 0 || spatial_layer_bitrate_bps == 0) {
      // No signaled temporal layers, or no bitrate set. Could either be unused
      // spatial layer or bitrate dynamic mode; pass bitrate through without any
      // change.
      utilization_factor = 1.0;
    } else if (active_tls_[si] == 1) {
      // A single active temporal layer, this might mean single layer or that
      // encoder does not support temporal layers. Merge target bitrates for
      // this spatial layer.
      RTC_DCHECK(overshoot_detectors_[si][0]);
      utilization_factor = overshoot_detectors_[si][0]
                               ->GetNetworkRateUtilizationFactor(now_ms)
                               .value_or(kDefaultUtilizationFactor);
    } else if (spatial_layer_bitrate_bps > 0) {
      // Multiple temporal layers enabled for this spatial layer. Update rate
      // for each of them and make a weighted average of utilization factors,
      // with bitrate fraction used as weight.
      // If any layer is missing a utilization factor, fall back to default.
      utilization_factor = 0.0;
      for (size_t ti = 0; ti < active_tls_[si]; ++ti) {
        RTC_DCHECK(overshoot_detectors_[si][ti]);
        const absl::optional<double> ti_utilization_factor =
            overshoot_detectors_[si][ti]->GetNetworkRateUtilizationFactor(
                now_ms);
        if (!ti_utilization_factor) {
          utilization_factor = kDefaultUtilizationFactor;
          break;
        }
        const double weight =
            static_cast<double>(bitrate_allocation.GetBitrate(si, ti)) /
            spatial_layer_bitrate_bps;
        utilization_factor += weight * ti_utilization_factor.value();
      }
    } else {
      RTC_NOTREACHED();
    }

    // Don't boost target bitrate if encoder is under-using.
    utilization_factor = std::max(utilization_factor, 1.0);

    // Don't reduce encoder target below 50%, in which case the frame dropper
    // should kick in instead.
    utilization_factor = std::min(utilization_factor, 2.0);

    if (min_bitrates_bps_[si] > 0 && spatial_layer_bitrate_bps > 0 &&
        min_bitrates_bps_[si] < spatial_layer_bitrate_bps) {
      // Make sure rate adjuster doesn't push target bitrate below minimum.
      utilization_factor = std::min(
          utilization_factor, static_cast<double>(spatial_layer_bitrate_bps) /
                                  min_bitrates_bps_[si]);
    }

    if (spatial_layer_bitrate_bps > 0) {
      RTC_LOG(LS_VERBOSE) << "Utilization factor for spatial index " << si
                          << ": " << utilization_factor;
    }

    // Populate the adjusted allocation with determined utilization factor.
    if (active_tls_[si] == 1 &&
        spatial_layer_bitrate_bps > bitrate_allocation.GetBitrate(si, 0)) {
      // Bitrate allocation indicates temporal layer usage, but encoder
      // does not seem to support it. Pipe all bitrate into a single
      // overshoot detector.
      uint32_t adjusted_layer_bitrate_bps = static_cast<uint32_t>(
          spatial_layer_bitrate_bps / utilization_factor + 0.5);
      adjusted_allocation.SetBitrate(si, 0, adjusted_layer_bitrate_bps);
    } else {
      for (size_t ti = 0; ti < kMaxTemporalStreams; ++ti) {
        if (bitrate_allocation.HasBitrate(si, ti)) {
          uint32_t adjusted_layer_bitrate_bps = static_cast<uint32_t>(
              bitrate_allocation.GetBitrate(si, ti) / utilization_factor + 0.5);
          adjusted_allocation.SetBitrate(si, ti, adjusted_layer_bitrate_bps);
        }
      }
    }

    // In case of rounding errors, add bitrate to TL0 until min bitrate
    // constraint has been met.
    const uint32_t adjusted_spatial_layer_sum =
        adjusted_allocation.GetSpatialLayerSum(si);
    if (spatial_layer_bitrate_bps > 0 &&
        adjusted_spatial_layer_sum < min_bitrates_bps_[si]) {
      adjusted_allocation.SetBitrate(si, 0,
                                     adjusted_allocation.GetBitrate(si, 0) +
                                         min_bitrates_bps_[si] -
                                         adjusted_spatial_layer_sum);
    }

    // Update all detectors with the new adjusted bitrate targets.
    for (size_t ti = 0; ti < kMaxTemporalStreams; ++ti) {
      const uint32_t layer_bitrate_bps = adjusted_allocation.GetBitrate(si, ti);
      // Overshoot detector may not exist, eg for ScreenshareLayers case.
      if (layer_bitrate_bps > 0 && overshoot_detectors_[si][ti]) {
        // Number of frames in this layer alone is not cumulative, so
        // subtract fps from any low temporal layer.
        const double fps_fraction =
            static_cast<double>(
                current_fps_allocation_[si][ti] -
                (ti == 0 ? 0 : current_fps_allocation_[si][ti - 1])) /
            VideoEncoder::EncoderInfo::kMaxFramerateFraction;

        overshoot_detectors_[si][ti]->SetTargetRate(
            DataRate::bps(layer_bitrate_bps),
            fps_fraction * current_total_framerate_fps_, now_ms);
      }
    }
  }

  return adjusted_allocation;
}

void EncoderBitrateAdjuster::OnEncoderInfo(
    const VideoEncoder::EncoderInfo& encoder_info) {
  // Copy allocation into current state and re-allocate.
  for (size_t si = 0; si < kMaxSpatialLayers; ++si) {
    current_fps_allocation_[si] = encoder_info.fps_allocation[si];
  }

  // Trigger re-allocation so that overshoot detectors have correct targets.
  AdjustRateAllocation(current_bitrate_allocation_,
                       current_total_framerate_fps_);
}

void EncoderBitrateAdjuster::OnEncodedFrame(const EncodedImage& encoded_image,
                                            int temporal_index) {
  ++frames_since_layout_change_;
  // Detectors may not exist, for instance if ScreenshareLayers is used.
  auto& detector =
      overshoot_detectors_[encoded_image.SpatialIndex().value_or(0)]
                          [temporal_index];
  if (detector) {
    detector->OnEncodedFrame(encoded_image.size(), rtc::TimeMillis());
  }
}

void EncoderBitrateAdjuster::Reset() {
  for (size_t si = 0; si < kMaxSpatialLayers; ++si) {
    for (size_t ti = 0; ti < kMaxTemporalStreams; ++ti) {
      overshoot_detectors_[si][ti].reset();
    }
  }
  // Call AdjustRateAllocation() with the last know bitrate allocation, so that
  // the appropriate overuse detectors are immediately re-created.
  AdjustRateAllocation(current_bitrate_allocation_,
                       current_total_framerate_fps_);
}

}  // namespace webrtc
