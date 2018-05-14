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

#include "media/base/h264_profile_level_id.h"
#include "media/base/mediaconstants.h"
#include "media/engine/simulcast.h"
#include "modules/video_coding/codecs/vp9/svc_config.h"
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

void ConfigureSimulcast(VideoCodec* codec_settings) {
  const std::vector<webrtc::VideoStream> streams = cricket::GetSimulcastConfig(
      codec_settings->numberOfSimulcastStreams, codec_settings->width,
      codec_settings->height, kMaxBitrateBps, kMaxQp, kMaxFramerateFps, false);

  for (size_t i = 0; i < streams.size(); ++i) {
    SimulcastStream* ss = &codec_settings->simulcastStream[i];
    ss->width = static_cast<uint16_t>(streams[i].width);
    ss->height = static_cast<uint16_t>(streams[i].height);
    ss->numberOfTemporalLayers =
        static_cast<unsigned char>(*streams[i].num_temporal_layers);
    ss->maxBitrate = streams[i].max_bitrate_bps / 1000;
    ss->targetBitrate = streams[i].target_bitrate_bps / 1000;
    ss->minBitrate = streams[i].min_bitrate_bps / 1000;
    ss->qpMax = streams[i].max_qp;
    ss->active = true;
  }
}

void ConfigureSvc(VideoCodec* codec_settings) {
  RTC_CHECK_EQ(kVideoCodecVP9, codec_settings->codecType);

  const std::vector<SpatialLayer> layers =
      GetSvcConfig(codec_settings->width, codec_settings->height,
                   codec_settings->VP9()->numberOfSpatialLayers,
                   codec_settings->VP9()->numberOfTemporalLayers, false);

  for (size_t i = 0; i < layers.size(); ++i) {
    codec_settings->spatialLayers[i] = layers[i];
  }
}

std::string CodecSpecificToString(const VideoCodec& codec) {
  std::stringstream ss;
  switch (codec.codecType) {
    case kVideoCodecVP8:
      ss << "complexity: " << codec.VP8().complexity;
      ss << "\nnum_temporal_layers: "
         << static_cast<int>(codec.VP8().numberOfTemporalLayers);
      ss << "\ndenoising: " << codec.VP8().denoisingOn;
      ss << "\nautomatic_resize: " << codec.VP8().automaticResizeOn;
      ss << "\nframe_dropping: " << codec.VP8().frameDroppingOn;
      ss << "\nkey_frame_interval: " << codec.VP8().keyFrameInterval;
      break;
    case kVideoCodecVP9:
      ss << "complexity: " << codec.VP9().complexity;
      ss << "\nnum_temporal_layers: "
         << static_cast<int>(codec.VP9().numberOfTemporalLayers);
      ss << "\nnum_spatial_layers: "
         << static_cast<int>(codec.VP9().numberOfSpatialLayers);
      ss << "\ndenoising: " << codec.VP9().denoisingOn;
      ss << "\nframe_dropping: " << codec.VP9().frameDroppingOn;
      ss << "\nkey_frame_interval: " << codec.VP9().keyFrameInterval;
      ss << "\nadaptive_qp_mode: " << codec.VP9().adaptiveQpMode;
      ss << "\nautomatic_resize: " << codec.VP9().automaticResizeOn;
      ss << "\nflexible_mode: " << codec.VP9().flexibleMode;
      break;
    case kVideoCodecH264:
      ss << "frame_dropping: " << codec.H264().frameDroppingOn;
      ss << "\nkey_frame_interval: " << codec.H264().keyFrameInterval;
      ss << "\nprofile: " << codec.H264().profile;
      break;
    default:
      break;
  }
  ss << "\n";
  return ss.str();
}

}  // namespace

void TestConfig::SetCodecSettings(std::string codec_name,
                                  size_t num_simulcast_streams,
                                  size_t num_spatial_layers,
                                  size_t num_temporal_layers,
                                  bool denoising_on,
                                  bool frame_dropper_on,
                                  bool spatial_resize_on,
                                  size_t width,
                                  size_t height) {
  this->codec_name = codec_name;
  VideoCodecType codec_type = PayloadStringToCodecType(codec_name);
  webrtc::test::CodecSettings(codec_type, &codec_settings);

  // TODO(brandtr): Move the setting of |width| and |height| to the tests, and
  // DCHECK that they are set before initializing the codec instead.
  codec_settings.width = static_cast<uint16_t>(width);
  codec_settings.height = static_cast<uint16_t>(height);

  RTC_CHECK(num_simulcast_streams >= 1 &&
            num_simulcast_streams <= kMaxSimulcastStreams);
  RTC_CHECK(num_spatial_layers >= 1 && num_spatial_layers <= kMaxSpatialLayers);
  RTC_CHECK(num_temporal_layers >= 1 &&
            num_temporal_layers <= kMaxTemporalStreams);

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
      codec_settings.VP8()->numberOfTemporalLayers =
          static_cast<uint8_t>(num_temporal_layers);
      codec_settings.VP8()->denoisingOn = denoising_on;
      codec_settings.VP8()->automaticResizeOn = spatial_resize_on;
      codec_settings.VP8()->frameDroppingOn = frame_dropper_on;
      codec_settings.VP8()->keyFrameInterval = kBaseKeyFrameInterval;
      break;
    case kVideoCodecVP9:
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
      break;
  }

  if (codec_settings.numberOfSimulcastStreams > 1) {
    ConfigureSimulcast(&codec_settings);
  } else if (codec_settings.codecType == kVideoCodecVP9 &&
             codec_settings.VP9()->numberOfSpatialLayers > 1) {
    ConfigureSvc(&codec_settings);
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

std::vector<FrameType> TestConfig::FrameTypeForFrame(size_t frame_idx) const {
  if (keyframe_interval > 0 && (frame_idx % keyframe_interval == 0)) {
    return {kVideoFrameKey};
  }
  return {kVideoFrameDelta};
}

std::string TestConfig::ToString() const {
  std::string codec_type = CodecTypeToPayloadString(codec_settings.codecType);
  std::stringstream ss;
  ss << "filename: " << filename;
  ss << "\nnum_frames: " << num_frames;
  ss << "\nmax_payload_size_bytes: " << max_payload_size_bytes;
  ss << "\ndecode: " << decode;
  ss << "\nuse_single_core: " << use_single_core;
  ss << "\nmeasure_cpu: " << measure_cpu;
  ss << "\nnum_cores: " << NumberOfCores();
  ss << "\nkeyframe_interval: " << keyframe_interval;
  ss << "\ncodec_type: " << codec_type;
  ss << "\n--> codec_settings";
  ss << "\nwidth: " << codec_settings.width;
  ss << "\nheight: " << codec_settings.height;
  ss << "\nmax_framerate_fps: " << codec_settings.maxFramerate;
  ss << "\nstart_bitrate_kbps: " << codec_settings.startBitrate;
  ss << "\nmax_bitrate_kbps: " << codec_settings.maxBitrate;
  ss << "\nmin_bitrate_kbps: " << codec_settings.minBitrate;
  ss << "\nmax_qp: " << codec_settings.qpMax;
  ss << "\nnum_simulcast_streams : "
     << static_cast<int>(codec_settings.numberOfSimulcastStreams);
  ss << "\n"
     << "--> codec_settings." << codec_type << "\n";
  ss << CodecSpecificToString(codec_settings);
  return ss.str();
}

SdpVideoFormat TestConfig::ToSdpVideoFormat() const {
  if (codec_settings.codecType == kVideoCodecH264) {
    const char* packetization_mode =
        h264_codec_settings.packetization_mode ==
                H264PacketizationMode::NonInterleaved
            ? "1"
            : "0";
    return SdpVideoFormat(
        codec_name,
        {{cricket::kH264FmtpProfileLevelId,
          *H264::ProfileLevelIdToString(H264::ProfileLevelId(
              h264_codec_settings.profile, H264::kLevel3_1))},
         {cricket::kH264FmtpPacketizationMode, packetization_mode}});
  }
  return SdpVideoFormat(codec_name);
}

std::string TestConfig::CodecName() const {
  std::string name = codec_name;
  if (name.empty()) {
    name = CodecTypeToPayloadString(codec_settings.codecType);
  }
  if (codec_settings.codecType == kVideoCodecH264) {
    if (h264_codec_settings.profile == H264::kProfileConstrainedHigh) {
      return name + "-CHP";
    } else {
      RTC_DCHECK_EQ(h264_codec_settings.profile,
                    H264::kProfileConstrainedBaseline);
      return name + "-CBP";
    }
  }
  return name;
}

std::string TestConfig::FilenameWithParams() const {
  std::string implementation_type = hw_encoder ? "hw" : "sw";
  return filename + "_" + CodecName() + "_" + implementation_type + "_" +
         std::to_string(codec_settings.startBitrate);
}

bool TestConfig::IsAsyncCodec() const {
  return hw_encoder || hw_decoder;
}

}  // namespace test
}  // namespace webrtc
