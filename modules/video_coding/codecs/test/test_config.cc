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

#include <string.h>

#include "modules/video_coding/include/video_codec_interface.h"
#include "rtc_base/checks.h"
#include "system_wrappers/include/cpu_info.h"
#include "test/video_codec_settings.h"

namespace webrtc {
namespace test {

namespace {
const int kBaseKeyFrameInterval = 3000;
}  // namespace

void TestConfig::SetCodecSettings(VideoCodecType codec_type,
                                  int num_temporal_layers,
                                  bool error_concealment_on,
                                  bool denoising_on,
                                  bool frame_dropper_on,
                                  bool spatial_resize_on,
                                  bool resilience_on,
                                  int width,
                                  int height) {
  webrtc::test::CodecSettings(codec_type, &codec_settings);

  // TODO(brandtr): Move the setting of |width| and |height| to the tests, and
  // DCHECK that they are set before initializing the codec instead.
  codec_settings.width = width;
  codec_settings.height = height;

  switch (codec_settings.codecType) {
    case kVideoCodecVP8:
      codec_settings.VP8()->resilience =
          resilience_on ? kResilientStream : kResilienceOff;
      codec_settings.VP8()->numberOfTemporalLayers = num_temporal_layers;
      codec_settings.VP8()->denoisingOn = denoising_on;
      codec_settings.VP8()->errorConcealmentOn = error_concealment_on;
      codec_settings.VP8()->automaticResizeOn = spatial_resize_on;
      codec_settings.VP8()->frameDroppingOn = frame_dropper_on;
      codec_settings.VP8()->keyFrameInterval = kBaseKeyFrameInterval;
      break;
    case kVideoCodecVP9:
      codec_settings.VP9()->resilienceOn = resilience_on;
      codec_settings.VP9()->numberOfTemporalLayers = num_temporal_layers;
      codec_settings.VP9()->denoisingOn = denoising_on;
      codec_settings.VP9()->frameDroppingOn = frame_dropper_on;
      codec_settings.VP9()->keyFrameInterval = kBaseKeyFrameInterval;
      codec_settings.VP9()->automaticResizeOn = spatial_resize_on;
      break;
    case kVideoCodecH264:
      codec_settings.H264()->frameDroppingOn = frame_dropper_on;
      codec_settings.H264()->keyFrameInterval = kBaseKeyFrameInterval;
      break;
    default:
      RTC_NOTREACHED();
      break;
  }
}

int TestConfig::NumberOfCores() const {
  return use_single_core ? 1 : CpuInfo::DetectNumberOfCores();
}

int TestConfig::NumberOfTemporalLayers() const {
  if (codec_settings.codecType == kVideoCodecVP8) {
    return codec_settings.VP8().numberOfTemporalLayers;
  } else if (codec_settings.codecType == kVideoCodecVP9) {
    return codec_settings.VP9().numberOfTemporalLayers;
  } else {
    return 1;
  }
}

void TestConfig::Print() const {
  printf("Video config:\n");
  printf(" Filename         : %s\n", filename.c_str());
  printf(" # CPU cores used : %u\n", NumberOfCores());
  PrintCodecSettings();
  printf("\n");
}

void TestConfig::PrintCodecSettings() const {
  printf(" Codec settings:\n");
  printf("  Codec type        : %s\n",
         CodecTypeToPayloadString(codec_settings.codecType));
  printf("  Start bitrate     : %d kbps\n", codec_settings.startBitrate);
  printf("  Max bitrate       : %d kbps\n", codec_settings.maxBitrate);
  printf("  Min bitrate       : %d kbps\n", codec_settings.minBitrate);
  printf("  Width             : %d\n", codec_settings.width);
  printf("  Height            : %d\n", codec_settings.height);
  printf("  Max frame rate    : %d\n", codec_settings.maxFramerate);
  printf("  QPmax             : %d\n", codec_settings.qpMax);
  if (codec_settings.codecType == kVideoCodecVP8) {
    printf("  Complexity        : %d\n", codec_settings.VP8().complexity);
    printf("  Resilience        : %d\n", codec_settings.VP8().resilience);
    printf("  # temporal layers : %d\n",
           codec_settings.VP8().numberOfTemporalLayers);
    printf("  Denoising         : %d\n", codec_settings.VP8().denoisingOn);
    printf("  Error concealment : %d\n",
           codec_settings.VP8().errorConcealmentOn);
    printf("  Automatic resize  : %d\n",
           codec_settings.VP8().automaticResizeOn);
    printf("  Frame dropping    : %d\n", codec_settings.VP8().frameDroppingOn);
    printf("  Key frame interval: %d\n", codec_settings.VP8().keyFrameInterval);
  } else if (codec_settings.codecType == kVideoCodecVP9) {
    printf("  Complexity        : %d\n", codec_settings.VP9().complexity);
    printf("  Resilience        : %d\n", codec_settings.VP9().resilienceOn);
    printf("  # temporal layers : %d\n",
           codec_settings.VP9().numberOfTemporalLayers);
    printf("  Denoising         : %d\n", codec_settings.VP9().denoisingOn);
    printf("  Frame dropping    : %d\n", codec_settings.VP9().frameDroppingOn);
    printf("  Key frame interval: %d\n", codec_settings.VP9().keyFrameInterval);
    printf("  Adaptive QP mode  : %d\n", codec_settings.VP9().adaptiveQpMode);
    printf("  Automatic resize  : %d\n",
           codec_settings.VP9().automaticResizeOn);
    printf("  # spatial layers  : %d\n",
           codec_settings.VP9().numberOfSpatialLayers);
    printf("  Flexible mode     : %d\n", codec_settings.VP9().flexibleMode);
  } else if (codec_settings.codecType == kVideoCodecH264) {
    printf("  Frame dropping    : %d\n", codec_settings.H264().frameDroppingOn);
    printf("  Key frame interval: %d\n",
           codec_settings.H264().keyFrameInterval);
    printf("  Profile           : %d\n", codec_settings.H264().profile);
  }
}

}  // namespace test
}  // namespace webrtc
