/*
 *  Copyright (c) 2014 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#include "webrtc/test/encoder_settings.h"

#include <assert.h>
#include <string.h>

#include "webrtc/video_engine/vie_defines.h"

namespace webrtc {
namespace test {
std::vector<VideoStream> CreateVideoStreams(size_t num_streams) {
  assert(num_streams > 0);

  // Add more streams to the settings above with reasonable values if required.
  static const size_t kNumSettings = 3;
  assert(num_streams <= kNumSettings);

  std::vector<VideoStream> stream_settings(kNumSettings);

  stream_settings[0].width = 320;
  stream_settings[0].height = 180;
  stream_settings[0].max_framerate = 30;
  stream_settings[0].min_bitrate_bps = 50000;
  stream_settings[0].target_bitrate_bps = stream_settings[0].max_bitrate_bps =
      150000;
  stream_settings[0].max_qp = 56;

  stream_settings[1].width = 640;
  stream_settings[1].height = 360;
  stream_settings[1].max_framerate = 30;
  stream_settings[1].min_bitrate_bps = 200000;
  stream_settings[1].target_bitrate_bps = stream_settings[1].max_bitrate_bps =
      450000;
  stream_settings[1].max_qp = 56;

  stream_settings[2].width = 1280;
  stream_settings[2].height = 720;
  stream_settings[2].max_framerate = 30;
  stream_settings[2].min_bitrate_bps = 700000;
  stream_settings[2].target_bitrate_bps = stream_settings[2].max_bitrate_bps =
      1500000;
  stream_settings[2].max_qp = 56;
  stream_settings.resize(num_streams);
  return stream_settings;
}

VideoCodec CreateDecoderVideoCodec(
    const VideoSendStream::Config::EncoderSettings& encoder_settings) {
  VideoCodec codec;
  memset(&codec, 0, sizeof(codec));

  codec.plType = encoder_settings.payload_type;
  strcpy(codec.plName, encoder_settings.payload_name.c_str());
  if (encoder_settings.payload_name == "VP8") {
    codec.codecType = kVideoCodecVP8;
  } else if (encoder_settings.payload_name == "H264") {
    codec.codecType = kVideoCodecH264;
  } else {
    codec.codecType = kVideoCodecGeneric;
  }

  if (codec.codecType == kVideoCodecVP8) {
    codec.codecSpecific.VP8.resilience = kResilientStream;
    codec.codecSpecific.VP8.numberOfTemporalLayers = 1;
    codec.codecSpecific.VP8.denoisingOn = true;
    codec.codecSpecific.VP8.errorConcealmentOn = false;
    codec.codecSpecific.VP8.automaticResizeOn = false;
    codec.codecSpecific.VP8.frameDroppingOn = true;
    codec.codecSpecific.VP8.keyFrameInterval = 3000;
  }

  if (codec.codecType == kVideoCodecH264) {
    codec.codecSpecific.H264.profile = kProfileBase;
    codec.codecSpecific.H264.frameDroppingOn = true;
    codec.codecSpecific.H264.keyFrameInterval = 3000;
  }

  codec.width = 320;
  codec.height = 180;
  codec.startBitrate = codec.minBitrate = codec.maxBitrate = 300;

  return codec;
}

}  // namespace test
}  // namespace webrtc
