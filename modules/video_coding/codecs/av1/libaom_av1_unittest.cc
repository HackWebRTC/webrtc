/*
 *  Copyright (c) 2020 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <stddef.h>
#include <stdint.h>

#include <memory>
#include <vector>

#include "absl/types/optional.h"
#include "api/test/create_frame_generator.h"
#include "api/test/frame_generator_interface.h"
#include "api/video_codecs/video_codec.h"
#include "api/video_codecs/video_encoder.h"
#include "modules/video_coding/codecs/av1/libaom_av1_decoder.h"
#include "modules/video_coding/codecs/av1/libaom_av1_encoder.h"
#include "modules/video_coding/include/video_codec_interface.h"
#include "modules/video_coding/include/video_error_codes.h"
#include "test/gmock.h"
#include "test/gtest.h"

namespace webrtc {
namespace {

using ::testing::ElementsAreArray;
using ::testing::IsEmpty;
using ::testing::Not;
using ::testing::NotNull;

// Use small resolution for this test to make it faster.
constexpr int kWidth = 320;
constexpr int kHeight = 180;
constexpr int kFramerate = 30;
constexpr int kRtpTicksPerSecond = 90000;

class TestAv1Encoder {
 public:
  struct Encoded {
    EncodedImage encoded_image;
    CodecSpecificInfo codec_specific_info;
  };

  TestAv1Encoder() : encoder_(CreateLibaomAv1Encoder()) {
    RTC_CHECK(encoder_);
    VideoCodec codec_settings;
    codec_settings.width = kWidth;
    codec_settings.height = kHeight;
    codec_settings.maxFramerate = kFramerate;
    VideoEncoder::Settings encoder_settings(
        VideoEncoder::Capabilities(/*loss_notification=*/false),
        /*number_of_cores=*/1, /*max_payload_size=*/1200);
    EXPECT_EQ(encoder_->InitEncode(&codec_settings, encoder_settings),
              WEBRTC_VIDEO_CODEC_OK);
    EXPECT_EQ(encoder_->RegisterEncodeCompleteCallback(&callback_),
              WEBRTC_VIDEO_CODEC_OK);
  }
  // This class requires pointer stability and thus not copyable nor movable.
  TestAv1Encoder(const TestAv1Encoder&) = delete;
  TestAv1Encoder& operator=(const TestAv1Encoder&) = delete;

  void EncodeAndAppend(const VideoFrame& frame, std::vector<Encoded>* encoded) {
    callback_.SetEncodeStorage(encoded);
    std::vector<VideoFrameType> frame_types = {
        VideoFrameType::kVideoFrameDelta};
    EXPECT_EQ(encoder_->Encode(frame, &frame_types), WEBRTC_VIDEO_CODEC_OK);
    // Prefer to crash checking nullptr rather than writing to random memory.
    callback_.SetEncodeStorage(nullptr);
  }

 private:
  class EncoderCallback : public EncodedImageCallback {
   public:
    void SetEncodeStorage(std::vector<Encoded>* storage) { storage_ = storage; }

   private:
    Result OnEncodedImage(
        const EncodedImage& encoded_image,
        const CodecSpecificInfo* codec_specific_info,
        const RTPFragmentationHeader* /*fragmentation*/) override {
      RTC_CHECK(storage_);
      storage_->push_back({encoded_image, *codec_specific_info});
      return Result(Result::Error::OK);
    }

    std::vector<Encoded>* storage_ = nullptr;
  };

  EncoderCallback callback_;
  std::unique_ptr<VideoEncoder> encoder_;
};

class TestAv1Decoder {
 public:
  TestAv1Decoder() {
    decoder_ = CreateLibaomAv1Decoder();
    if (decoder_ == nullptr) {
      ADD_FAILURE() << "Failed to create a decoder";
      return;
    }
    EXPECT_EQ(decoder_->InitDecode(/*codec_settings=*/nullptr,
                                   /*number_of_cores=*/1),
              WEBRTC_VIDEO_CODEC_OK);
    EXPECT_EQ(decoder_->RegisterDecodeCompleteCallback(&callback_),
              WEBRTC_VIDEO_CODEC_OK);
  }
  // This class requires pointer stability and thus not copyable nor movable.
  TestAv1Decoder(const TestAv1Decoder&) = delete;
  TestAv1Decoder& operator=(const TestAv1Decoder&) = delete;

  void Decode(int64_t frame_id, const EncodedImage& image) {
    ASSERT_THAT(decoder_, NotNull());
    requested_ids_.push_back(frame_id);
    int32_t error = decoder_->Decode(image, /*missing_frames=*/false,
                                     /*render_time_ms=*/image.capture_time_ms_);
    if (error != WEBRTC_VIDEO_CODEC_OK) {
      ADD_FAILURE() << "Failed to decode frame id " << frame_id
                    << " with error code " << error;
      return;
    }
    decoded_ids_.push_back(frame_id);
  }

  const std::vector<int64_t>& requested_frame_ids() const {
    return requested_ids_;
  }
  const std::vector<int64_t>& decoded_frame_ids() const { return decoded_ids_; }
  size_t num_output_frames() const { return callback_.num_called(); }

 private:
  // Decoder callback that only counts how many times it was called.
  // While it is tempting to replace it with a simple mock, that one requires
  // to set expectation on number of calls in advance. Tests below unsure about
  // expected number of calls until after calls are done.
  class DecoderCallback : public DecodedImageCallback {
   public:
    size_t num_called() const { return num_called_; }

   private:
    int32_t Decoded(VideoFrame& /*decoded_image*/) override {
      ++num_called_;
      return 0;
    }
    void Decoded(VideoFrame& /*decoded_image*/,
                 absl::optional<int32_t> /*decode_time_ms*/,
                 absl::optional<uint8_t> /*qp*/) override {
      ++num_called_;
    }

    int num_called_ = 0;
  };

  std::vector<int64_t> requested_ids_;
  std::vector<int64_t> decoded_ids_;
  DecoderCallback callback_;
  std::unique_ptr<VideoDecoder> decoder_;
};

std::vector<VideoFrame> GenerateFrames(size_t num_frames) {
  std::vector<VideoFrame> frames;
  frames.reserve(num_frames);

  auto input_frame_generator = test::CreateSquareFrameGenerator(
      kWidth, kHeight, test::FrameGeneratorInterface::OutputType::kI420,
      absl::nullopt);
  uint32_t timestamp = 1000;
  for (size_t i = 0; i < num_frames; ++i) {
    frames.push_back(
        VideoFrame::Builder()
            .set_video_frame_buffer(input_frame_generator->NextFrame().buffer)
            .set_timestamp_rtp(timestamp += kRtpTicksPerSecond / kFramerate)
            .build());
  }
  return frames;
}

TEST(LibaomAv1Test, EncodeDecode) {
  TestAv1Decoder decoder;
  TestAv1Encoder encoder;

  std::vector<TestAv1Encoder::Encoded> encoded_frames;
  for (const VideoFrame& frame : GenerateFrames(/*num_frames=*/4)) {
    encoder.EncodeAndAppend(frame, &encoded_frames);
  }
  for (size_t frame_idx = 0; frame_idx < encoded_frames.size(); ++frame_idx) {
    decoder.Decode(static_cast<int64_t>(frame_idx),
                   encoded_frames[frame_idx].encoded_image);
  }

  // Check encoder produced some frames for decoder to decode.
  ASSERT_THAT(encoded_frames, Not(IsEmpty()));
  // Check decoder found all of them valid.
  EXPECT_THAT(decoder.decoded_frame_ids(),
              ElementsAreArray(decoder.requested_frame_ids()));
  // Check each of them produced an output frame.
  EXPECT_EQ(decoder.num_output_frames(), decoder.decoded_frame_ids().size());
}

}  // namespace
}  // namespace webrtc
