/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_VIDEO_CODING_CODECS_TEST_TEST_CONFIG_H_
#define MODULES_VIDEO_CODING_CODECS_TEST_TEST_CONFIG_H_

#include <string>
#include <vector>

#include "api/video_codecs/sdp_video_format.h"
#include "common_types.h"  // NOLINT(build/include)
#include "modules/video_coding/codecs/h264/include/h264_globals.h"
#include "modules/video_coding/include/video_codec_interface.h"

namespace webrtc {
namespace test {

struct TestConfig {
  class EncodedFrameChecker {
   public:
    virtual ~EncodedFrameChecker() = default;

    virtual void CheckEncodedFrame(webrtc::VideoCodecType codec,
                                   const EncodedImage& encoded_frame) const = 0;
  };

  void SetCodecSettings(VideoCodecType codec_type,
                        size_t num_simulcast_streams,
                        size_t num_spatial_layers,
                        size_t num_temporal_layers,
                        bool denoising_on,
                        bool frame_dropper_on,
                        bool spatial_resize_on,
                        bool resilience_on,
                        size_t width,
                        size_t height);

  size_t NumberOfCores() const;
  size_t NumberOfTemporalLayers() const;
  size_t NumberOfSpatialLayers() const;
  size_t NumberOfSimulcastStreams() const;

  std::vector<FrameType> FrameTypeForFrame(size_t frame_idx) const;
  std::string ToString() const;
  SdpVideoFormat ToSdpVideoFormat() const;
  std::string CodecName() const;
  std::string FilenameWithParams() const;
  bool IsAsyncCodec() const;

  // Plain name of YUV file to process without file extension.
  std::string filename;

  // File to process. This must be a video file in the YUV format.
  std::string filepath;

  // Number of frames to process.
  size_t num_frames = 0;

  // Bitstream constraints.
  size_t max_payload_size_bytes = 1440;

  // Force the encoder and decoder to use a single core for processing.
  bool use_single_core = false;

  // Should cpu usage be measured?
  // If set to true, the encoding will run in real-time.
  bool measure_cpu = false;

  // If > 0: forces the encoder to create a keyframe every Nth frame.
  size_t keyframe_interval = 0;

  // Codec settings to use.
  webrtc::VideoCodec codec_settings;

  // H.264 specific settings.
  struct H264CodecSettings {
    H264::Profile profile = H264::kProfileConstrainedBaseline;
    H264PacketizationMode packetization_mode =
        webrtc::H264PacketizationMode::NonInterleaved;
  } h264_codec_settings;

  // Should hardware accelerated codecs be used?
  bool hw_encoder = false;
  bool hw_decoder = false;

  // Should the encoder be wrapped in a SimulcastEncoderAdapter?
  bool simulcast_adapted_encoder = false;

  // Should the hardware codecs be wrapped in software fallbacks?
  bool sw_fallback_encoder = false;
  bool sw_fallback_decoder = false;

  // Custom checker that will be called for each frame.
  const EncodedFrameChecker* encoded_frame_checker = nullptr;

  // Print out frame level stats.
  bool print_frame_level_stats = false;
};

}  // namespace test
}  // namespace webrtc

#endif  // MODULES_VIDEO_CODING_CODECS_TEST_TEST_CONFIG_H_
