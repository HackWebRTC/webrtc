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
VideoSendStream::Config::EncoderSettings CreateEncoderSettings(
    VideoEncoder* encoder,
    const char* payload_name,
    int payload_type,
    size_t num_streams) {
  assert(num_streams > 0);

  // Add more streams to the settings above with reasonable values if required.
  static const size_t kNumSettings = 3;
  assert(num_streams <= kNumSettings);

  VideoStream stream_settings[kNumSettings];

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

  VideoSendStream::Config::EncoderSettings settings;

  for (size_t i = 0; i < num_streams; ++i)
    settings.streams.push_back(stream_settings[i]);

  settings.encoder = encoder;
  settings.payload_name = payload_name;
  settings.payload_type = payload_type;
  return settings;
}

VideoCodec CreateDecoderVideoCodec(
    const VideoSendStream::Config::EncoderSettings& settings) {
  assert(settings.streams.size() > 0);
  VideoCodec codec;
  memset(&codec, 0, sizeof(codec));

  codec.plType = settings.payload_type;
  strcpy(codec.plName, settings.payload_name.c_str());
  codec.codecType =
      (settings.payload_name == "VP8" ? kVideoCodecVP8 : kVideoCodecGeneric);

  if (codec.codecType == kVideoCodecVP8) {
    codec.codecSpecific.VP8.resilience = kResilientStream;
    codec.codecSpecific.VP8.numberOfTemporalLayers = 1;
    codec.codecSpecific.VP8.denoisingOn = true;
    codec.codecSpecific.VP8.errorConcealmentOn = false;
    codec.codecSpecific.VP8.automaticResizeOn = false;
    codec.codecSpecific.VP8.frameDroppingOn = true;
    codec.codecSpecific.VP8.keyFrameInterval = 3000;
  }

  codec.minBitrate = settings.streams[0].min_bitrate_bps / 1000;
  for (size_t i = 0; i < settings.streams.size(); ++i) {
    const VideoStream& stream = settings.streams[i];
    if (stream.width > codec.width)
      codec.width = static_cast<unsigned short>(stream.width);
    if (stream.height > codec.height)
      codec.height = static_cast<unsigned short>(stream.height);
    if (static_cast<unsigned int>(stream.min_bitrate_bps / 1000) <
        codec.minBitrate)
      codec.minBitrate =
          static_cast<unsigned int>(stream.min_bitrate_bps / 1000);
    codec.maxBitrate += stream.max_bitrate_bps / 1000;
    if (static_cast<unsigned int>(stream.max_qp) > codec.qpMax)
      codec.qpMax = static_cast<unsigned int>(stream.max_qp);
  }

  if (codec.minBitrate < kViEMinCodecBitrate)
    codec.minBitrate = kViEMinCodecBitrate;
  if (codec.maxBitrate < kViEMinCodecBitrate)
    codec.maxBitrate = kViEMinCodecBitrate;

  codec.startBitrate = 300;

  if (codec.startBitrate < codec.minBitrate)
    codec.startBitrate = codec.minBitrate;
  if (codec.startBitrate > codec.maxBitrate)
    codec.startBitrate = codec.maxBitrate;

  return codec;
}

}  // namespace test
}  // namespace webrtc
