/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/video_coding/codecs/test/test_config.h"

#include <sstream>

#include "media/engine/simulcast.h"
#include "modules/video_coding/include/video_codec_interface.h"
#include "rtc_base/checks.h"
#include "system_wrappers/include/cpu_info.h"
#include "test/video_codec_settings.h"

namespace webrtc {
namespace test {

namespace {
const int kBaseKeyFrameInterval = 3000;
const int kMaxBitrateBps = 5000 * 1000;  // From kSimulcastFormats.
const int kMaxFramerateFps = 30;
const int kMaxQp = 56;

std::string CodecSpecificToString(const webrtc::VideoCodec& codec) {
  std::stringstream ss;
  switch (codec.codecType) {
    case kVideoCodecVP8:
      ss << "\n  Complexity          : " << codec.VP8().complexity;
      ss << "\n  Resilience          : " << codec.VP8().resilience;
      ss << "\n  # temporal layers   : "
         << static_cast<int>(codec.VP8().numberOfTemporalLayers);
      ss << "\n  Denoising           : " << codec.VP8().denoisingOn;
      ss << "\n  Error concealment   : " << codec.VP8().errorConcealmentOn;
      ss << "\n  Automatic resize    : " << codec.VP8().automaticResizeOn;
      ss << "\n  Frame dropping      : " << codec.VP8().frameDroppingOn;
      ss << "\n  Key frame interval  : " << codec.VP8().keyFrameInterval;
      break;
    case kVideoCodecVP9:
      ss << "\n  Complexity          : " << codec.VP9().complexity;
      ss << "\n  Resilience          : " << codec.VP9().resilienceOn;
      ss << "\n  # temporal layers   : "
         << static_cast<int>(codec.VP9().numberOfTemporalLayers);
      ss << "\n  # spatial layers    : "
         << static_cast<int>(codec.VP9().numberOfSpatialLayers);
      ss << "\n  Denoising           : " << codec.VP9().denoisingOn;
      ss << "\n  Frame dropping      : " << codec.VP9().frameDroppingOn;
      ss << "\n  Key frame interval  : " << codec.VP9().keyFrameInterval;
      ss << "\n  Adaptive QP mode    : " << codec.VP9().adaptiveQpMode;
      ss << "\n  Automatic resize    : " << codec.VP9().automaticResizeOn;
      ss << "\n  # spatial layers    : "
         << static_cast<int>(codec.VP9().numberOfSpatialLayers);
      ss << "\n  Flexible mode       : " << codec.VP9().flexibleMode;
      break;
    case kVideoCodecH264:
      ss << "\n  Frame dropping      : " << codec.H264().frameDroppingOn;
      ss << "\n  Key frame interval  : " << codec.H264().keyFrameInterval;
      ss << "\n  Profile             : " << codec.H264().profile;
      break;
    default:
      break;
  }
  ss << "\n";
  return ss.str();
}
}  // namespace

void TestConfig::SetCodecSettings(VideoCodecType codec_type,
                                  size_t num_simulcast_streams,
                                  size_t num_spatial_layers,
                                  size_t num_temporal_layers,
                                  bool error_concealment_on,
                                  bool denoising_on,
                                  bool frame_dropper_on,
                                  bool spatial_resize_on,
                                  bool resilience_on,
                                  size_t width,
                                  size_t height) {
  webrtc::test::CodecSettings(codec_type, &codec_settings);

  // TODO(brandtr): Move the setting of |width| and |height| to the tests, and
  // DCHECK that they are set before initializing the codec instead.
  codec_settings.width = static_cast<uint16_t>(width);
  codec_settings.height = static_cast<uint16_t>(height);

  RTC_CHECK(num_simulcast_streams >= 1 &&
            num_simulcast_streams <= kMaxSimulcastStreams);
  RTC_CHECK(num_spatial_layers >= 1 && num_spatial_layers <= kMaxSpatialLayers);

  // Simulcast is only available with VP8.
  RTC_CHECK(num_simulcast_streams < 2 || codec_type == kVideoCodecVP8);

  // Spatial scalability is only available with VP9.
  RTC_CHECK(num_spatial_layers < 2 || codec_type == kVideoCodecVP9);

  // Some base code requires numberOfSimulcastStreams to be set to zero
  // when simulcast is not used.
  codec_settings.numberOfSimulcastStreams =
      num_simulcast_streams <= 1 ? 0
                                 : static_cast<uint8_t>(num_simulcast_streams);

  switch (codec_settings.codecType) {
    case kVideoCodecVP8:
      codec_settings.VP8()->resilience =
          resilience_on ? kResilientStream : kResilienceOff;
      codec_settings.VP8()->numberOfTemporalLayers =
          static_cast<uint8_t>(num_temporal_layers);
      codec_settings.VP8()->denoisingOn = denoising_on;
      codec_settings.VP8()->errorConcealmentOn = error_concealment_on;
      codec_settings.VP8()->automaticResizeOn = spatial_resize_on;
      codec_settings.VP8()->frameDroppingOn = frame_dropper_on;
      codec_settings.VP8()->keyFrameInterval = kBaseKeyFrameInterval;
      break;
    case kVideoCodecVP9:
      codec_settings.VP9()->resilienceOn = resilience_on;
      codec_settings.VP9()->numberOfTemporalLayers =
          static_cast<uint8_t>(num_temporal_layers);
      codec_settings.VP9()->denoisingOn = denoising_on;
      codec_settings.VP9()->frameDroppingOn = frame_dropper_on;
      codec_settings.VP9()->keyFrameInterval = kBaseKeyFrameInterval;
      codec_settings.VP9()->automaticResizeOn = spatial_resize_on;
      codec_settings.VP9()->numberOfSpatialLayers =
          static_cast<uint8_t>(num_spatial_layers);
      break;
    case kVideoCodecH264:
      codec_settings.H264()->frameDroppingOn = frame_dropper_on;
      codec_settings.H264()->keyFrameInterval = kBaseKeyFrameInterval;
      break;
    default:
      RTC_NOTREACHED();
      break;
  }

  if (codec_settings.numberOfSimulcastStreams > 1) {
    ConfigureSimulcast();
  }
}

void TestConfig::ConfigureSimulcast() {
  std::vector<webrtc::VideoStream> stream = cricket::GetSimulcastConfig(
      codec_settings.numberOfSimulcastStreams, codec_settings.width,
      codec_settings.height, kMaxBitrateBps, kMaxQp, kMaxFramerateFps, false);

  for (size_t i = 0; i < stream.size(); ++i) {
    SimulcastStream* ss = &codec_settings.simulcastStream[i];
    ss->width = static_cast<uint16_t>(stream[i].width);
    ss->height = static_cast<uint16_t>(stream[i].height);
    ss->numberOfTemporalLayers = static_cast<unsigned char>(
        stream[i].temporal_layer_thresholds_bps.size() + 1);
    ss->maxBitrate = stream[i].max_bitrate_bps / 1000;
    ss->targetBitrate = stream[i].target_bitrate_bps / 1000;
    ss->minBitrate = stream[i].min_bitrate_bps / 1000;
    ss->qpMax = stream[i].max_qp;
    ss->active = true;
  }
}

size_t TestConfig::NumberOfCores() const {
  return use_single_core ? 1 : CpuInfo::DetectNumberOfCores();
}

size_t TestConfig::NumberOfTemporalLayers() const {
  if (codec_settings.codecType == kVideoCodecVP8) {
    return codec_settings.VP8().numberOfTemporalLayers;
  } else if (codec_settings.codecType == kVideoCodecVP9) {
    return codec_settings.VP9().numberOfTemporalLayers;
  } else {
    return 1;
  }
}

size_t TestConfig::NumberOfSpatialLayers() const {
  if (codec_settings.codecType == kVideoCodecVP9) {
    return codec_settings.VP9().numberOfSpatialLayers;
  } else {
    return 1;
  }
}

size_t TestConfig::NumberOfSimulcastStreams() const {
  return codec_settings.numberOfSimulcastStreams;
}

size_t TestConfig::TemporalLayerForFrame(size_t frame_idx) const {
  size_t tl = 0;
  switch (NumberOfTemporalLayers()) {
    case 1:
      tl = 0;
      break;
    case 2:
      // temporal layer 1:     1     3
      // temporal layer 0:  0     2     4 ...
      tl = (frame_idx % 2 == 0) ? 0 : 1;
      break;
    case 3:
      // temporal layer 2:     1     3     5     7
      // temporal layer 1:        2           6
      // temporal layer 0:  0           4           8 ...
      if (frame_idx % 4 == 0) {
        tl = 0;
      } else if ((frame_idx + 2) % 4 == 0) {
        tl = 1;
      } else if ((frame_idx + 1) % 2 == 0) {
        tl = 2;
      }
      break;
    default:
      RTC_NOTREACHED();
      break;
  }
  return tl;
}

std::vector<FrameType> TestConfig::FrameTypeForFrame(size_t frame_idx) const {
  if (keyframe_interval > 0 && (frame_idx % keyframe_interval == 0)) {
    return {kVideoFrameKey};
  }
  return {kVideoFrameDelta};
}

std::string TestConfig::ToString() const {
  std::string codec_type = CodecTypeToPayloadString(codec_settings.codecType);
  std::stringstream ss;
  ss << "\n Filename             : " << filename;
  ss << "\n # CPU cores used     : " << NumberOfCores();
  ss << "\n General:";
  ss << "\n  Codec type          : " << codec_type;
  ss << "\n  Start bitrate       : " << codec_settings.startBitrate << " kbps";
  ss << "\n  Max bitrate         : " << codec_settings.maxBitrate << " kbps";
  ss << "\n  Min bitrate         : " << codec_settings.minBitrate << " kbps";
  ss << "\n  Width               : " << codec_settings.width;
  ss << "\n  Height              : " << codec_settings.height;
  ss << "\n  Max frame rate      : " << codec_settings.maxFramerate;
  ss << "\n  QPmax               : " << codec_settings.qpMax;
  ss << "\n  # simulcast streams : "
     << static_cast<int>(codec_settings.numberOfSimulcastStreams);
  ss << "\n " << codec_type << " specific: ";
  ss << CodecSpecificToString(codec_settings);
  return ss.str();
}

std::string TestConfig::CodecName() const {
  std::string codec_name = CodecTypeToPayloadString(codec_settings.codecType);
  if (codec_settings.codecType == kVideoCodecH264) {
    if (h264_codec_settings.profile == H264::kProfileConstrainedHigh) {
      codec_name += "-CHP";
    } else {
      RTC_DCHECK_EQ(h264_codec_settings.profile,
                    H264::kProfileConstrainedBaseline);
      codec_name += "-CBP";
    }
  }
  return codec_name;
}

std::string TestConfig::FilenameWithParams() const {
  std::string implementation_type = hw_encoder ? "hw" : "sw";
  return filename + "_" + CodecName() + "_" + implementation_type + "_" +
         std::to_string(codec_settings.startBitrate);
}

}  // namespace test
}  // namespace webrtc
