/*
 *  Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/video_engine/test/common/fake_encoder.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace webrtc {

FakeEncoder::FakeEncoder(Clock* clock)
    : clock_(clock),
      callback_(NULL),
      target_bitrate_kbps_(0),
      last_encode_time_ms_(0) {
  memset(encoded_buffer_, 0, sizeof(encoded_buffer_));
}

FakeEncoder::~FakeEncoder() {}

int32_t FakeEncoder::InitEncode(const VideoCodec* config,
                                int32_t number_of_cores,
                                uint32_t max_payload_size) {
  config_ = *config;
  target_bitrate_kbps_ = config_.startBitrate;
  return 0;
}

int32_t FakeEncoder::Encode(
    const I420VideoFrame& input_image,
    const CodecSpecificInfo* codec_specific_info,
    const std::vector<VideoFrameType>* frame_types) {
  assert(config_.maxFramerate > 0);
  int delta_since_last_encode = 1000 / config_.maxFramerate;
  int64_t time_now_ms = clock_->TimeInMilliseconds();
  if (last_encode_time_ms_ > 0) {
    // For all frames but the first we can estimate the display time by looking
    // at the display time of the previous frame.
    delta_since_last_encode = time_now_ms - last_encode_time_ms_;
  }

  int bits_available = target_bitrate_kbps_ * delta_since_last_encode;
  last_encode_time_ms_ = time_now_ms;

  for (int i = 0; i < config_.numberOfSimulcastStreams; ++i) {
    CodecSpecificInfo specifics;
    memset(&specifics, 0, sizeof(specifics));
    specifics.codecType = kVideoCodecVP8;
    specifics.codecSpecific.VP8.simulcastIdx = i;
    int min_stream_bits = config_.simulcastStream[i].minBitrate *
        delta_since_last_encode;
    int max_stream_bits = config_.simulcastStream[i].maxBitrate *
        delta_since_last_encode;
    int stream_bits = (bits_available > max_stream_bits) ? max_stream_bits :
        bits_available;
    int stream_bytes = (stream_bits + 7) / 8;
    EXPECT_LT(stream_bytes, kMaxFrameSizeBytes);
    if (stream_bytes > kMaxFrameSizeBytes)
      return -1;

    EncodedImage encoded(encoded_buffer_, stream_bytes, kMaxFrameSizeBytes);
    encoded._timeStamp = input_image.timestamp();
    encoded.capture_time_ms_ = input_image.render_time_ms();
    if (min_stream_bits > bits_available) {
      encoded._length = 0;
      encoded._frameType = kSkipFrame;
    }
    if (callback_->Encoded(encoded, &specifics, NULL) != 0)
      return -1;

    bits_available -= encoded._length * 8;
  }
  return 0;
}

int32_t FakeEncoder::RegisterEncodeCompleteCallback(
    EncodedImageCallback* callback) {
  callback_ = callback;
  return 0;
}

int32_t FakeEncoder::Release() { return 0; }

int32_t FakeEncoder::SetChannelParameters(uint32_t packet_loss, int rtt) {
  return 0;
}

int32_t FakeEncoder::SetRates(uint32_t new_target_bitrate, uint32_t framerate) {
  target_bitrate_kbps_ = new_target_bitrate;
  return 0;
}
}  // namespace webrtc
