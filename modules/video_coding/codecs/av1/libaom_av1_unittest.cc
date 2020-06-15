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
#include "api/video_codecs/video_codec.h"
#include "api/video_codecs/video_encoder.h"
#include "modules/video_coding/codecs/av1/libaom_av1_decoder.h"
#include "modules/video_coding/codecs/av1/libaom_av1_encoder.h"
#include "modules/video_coding/codecs/av1/scalability_structure_l1t2.h"
#include "modules/video_coding/codecs/av1/scalability_structure_l1t3.h"
#include "modules/video_coding/codecs/av1/scalability_structure_l2t1.h"
#include "modules/video_coding/codecs/av1/scalability_structure_l2t1_key.h"
#include "modules/video_coding/codecs/av1/scalability_structure_l2t2.h"
#include "modules/video_coding/codecs/av1/scalability_structure_l2t2_key.h"
#include "modules/video_coding/codecs/av1/scalability_structure_l2t2_key_shift.h"
#include "modules/video_coding/codecs/av1/scalability_structure_l3t1.h"
#include "modules/video_coding/codecs/av1/scalability_structure_l3t3.h"
#include "modules/video_coding/codecs/av1/scalability_structure_s2t1.h"
#include "modules/video_coding/codecs/av1/scalable_video_controller.h"
#include "modules/video_coding/codecs/av1/scalable_video_controller_no_layering.h"
#include "modules/video_coding/codecs/test/encoded_video_frame_producer.h"
#include "modules/video_coding/include/video_codec_interface.h"
#include "modules/video_coding/include/video_error_codes.h"
#include "test/gmock.h"
#include "test/gtest.h"

namespace webrtc {
namespace {

using ::testing::ContainerEq;
using ::testing::Each;
using ::testing::ElementsAreArray;
using ::testing::Ge;
using ::testing::IsEmpty;
using ::testing::Not;
using ::testing::NotNull;
using ::testing::SizeIs;
using ::testing::Truly;
using ::testing::Values;

// Use small resolution for this test to make it faster.
constexpr int kWidth = 320;
constexpr int kHeight = 180;
constexpr int kFramerate = 30;

VideoCodec DefaultCodecSettings() {
  VideoCodec codec_settings;
  codec_settings.width = kWidth;
  codec_settings.height = kHeight;
  codec_settings.maxFramerate = kFramerate;
  codec_settings.maxBitrate = 1000;
  codec_settings.qpMax = 63;
  return codec_settings;
}
VideoEncoder::Settings DefaultEncoderSettings() {
  return VideoEncoder::Settings(
      VideoEncoder::Capabilities(/*loss_notification=*/false),
      /*number_of_cores=*/1, /*max_payload_size=*/1200);
}

class TestAv1Decoder {
 public:
  explicit TestAv1Decoder(int decoder_id)
      : decoder_id_(decoder_id), decoder_(CreateLibaomAv1Decoder()) {
    if (decoder_ == nullptr) {
      ADD_FAILURE() << "Failed to create a decoder#" << decoder_id_;
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
    int32_t error = decoder_->Decode(image, /*missing_frames=*/false,
                                     /*render_time_ms=*/image.capture_time_ms_);
    if (error != WEBRTC_VIDEO_CODEC_OK) {
      ADD_FAILURE() << "Failed to decode frame id " << frame_id
                    << " with error code " << error << " by decoder#"
                    << decoder_id_;
      return;
    }
    decoded_ids_.push_back(frame_id);
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

  const int decoder_id_;
  std::vector<int64_t> decoded_ids_;
  DecoderCallback callback_;
  const std::unique_ptr<VideoDecoder> decoder_;
};

TEST(LibaomAv1Test, EncodeDecode) {
  TestAv1Decoder decoder(0);
  std::unique_ptr<VideoEncoder> encoder = CreateLibaomAv1Encoder();
  VideoCodec codec_settings = DefaultCodecSettings();
  ASSERT_EQ(encoder->InitEncode(&codec_settings, DefaultEncoderSettings()),
            WEBRTC_VIDEO_CODEC_OK);

  std::vector<EncodedVideoFrameProducer::EncodedFrame> encoded_frames =
      EncodedVideoFrameProducer(*encoder).SetNumInputFrames(4).Encode();
  for (size_t frame_id = 0; frame_id < encoded_frames.size(); ++frame_id) {
    decoder.Decode(static_cast<int64_t>(frame_id),
                   encoded_frames[frame_id].encoded_image);
  }

  // Check encoder produced some frames for decoder to decode.
  ASSERT_THAT(encoded_frames, Not(IsEmpty()));
  // Check decoder found all of them valid.
  EXPECT_THAT(decoder.decoded_frame_ids(), SizeIs(encoded_frames.size()));
  // Check each of them produced an output frame.
  EXPECT_EQ(decoder.num_output_frames(), decoder.decoded_frame_ids().size());
}

struct SvcTestParam {
  std::function<std::unique_ptr<ScalableVideoController>()> svc_factory;
  int num_frames_to_generate;
};

class LibaomAv1SvcTest : public ::testing::TestWithParam<SvcTestParam> {};

TEST_P(LibaomAv1SvcTest, EncodeAndDecodeAllDecodeTargets) {
  std::unique_ptr<ScalableVideoController> svc_controller =
      GetParam().svc_factory();
  size_t num_decode_targets =
      svc_controller->DependencyStructure().num_decode_targets;

  std::unique_ptr<VideoEncoder> encoder =
      CreateLibaomAv1Encoder(std::move(svc_controller));
  VideoCodec codec_settings = DefaultCodecSettings();
  ASSERT_EQ(encoder->InitEncode(&codec_settings, DefaultEncoderSettings()),
            WEBRTC_VIDEO_CODEC_OK);
  std::vector<EncodedVideoFrameProducer::EncodedFrame> encoded_frames =
      EncodedVideoFrameProducer(*encoder)
          .SetNumInputFrames(GetParam().num_frames_to_generate)
          .SetResolution({kWidth, kHeight})
          .Encode();

  ASSERT_THAT(
      encoded_frames,
      Each(Truly([&](const EncodedVideoFrameProducer::EncodedFrame& frame) {
        return frame.codec_specific_info.generic_frame_info &&
               frame.codec_specific_info.generic_frame_info
                       ->decode_target_indications.size() == num_decode_targets;
      })));

  for (size_t dt = 0; dt < num_decode_targets; ++dt) {
    TestAv1Decoder decoder(dt);
    std::vector<int64_t> requested_ids;
    for (int64_t frame_id = 0;
         frame_id < static_cast<int64_t>(encoded_frames.size()); ++frame_id) {
      const EncodedVideoFrameProducer::EncodedFrame& frame =
          encoded_frames[frame_id];
      if (frame.codec_specific_info.generic_frame_info
              ->decode_target_indications[dt] !=
          DecodeTargetIndication::kNotPresent) {
        requested_ids.push_back(frame_id);
        decoder.Decode(frame_id, frame.encoded_image);
      }
    }

    ASSERT_THAT(requested_ids, SizeIs(Ge(2u)));
    // Check decoder found all of them valid.
    EXPECT_THAT(decoder.decoded_frame_ids(), ContainerEq(requested_ids))
        << "Decoder#" << dt;
    // Check each of them produced an output frame.
    EXPECT_EQ(decoder.num_output_frames(), decoder.decoded_frame_ids().size())
        << "Decoder#" << dt;
  }
}

INSTANTIATE_TEST_SUITE_P(
    Svc,
    LibaomAv1SvcTest,
    Values(SvcTestParam{std::make_unique<ScalableVideoControllerNoLayering>,
                        /*num_frames_to_generate=*/4},
           SvcTestParam{std::make_unique<ScalabilityStructureL1T2>,
                        /*num_frames_to_generate=*/4},
           SvcTestParam{std::make_unique<ScalabilityStructureL1T3>,
                        /*num_frames_to_generate=*/8},
           SvcTestParam{std::make_unique<ScalabilityStructureL2T1>,
                        /*num_frames_to_generate=*/3},
           SvcTestParam{std::make_unique<ScalabilityStructureL2T1Key>,
                        /*num_frames_to_generate=*/3},
           SvcTestParam{std::make_unique<ScalabilityStructureL3T1>,
                        /*num_frames_to_generate=*/3},
           SvcTestParam{std::make_unique<ScalabilityStructureL3T3>,
                        /*num_frames_to_generate=*/8},
           SvcTestParam{std::make_unique<ScalabilityStructureS2T1>,
                        /*num_frames_to_generate=*/3},
           SvcTestParam{std::make_unique<ScalabilityStructureL2T2>,
                        /*num_frames_to_generate=*/4},
           SvcTestParam{std::make_unique<ScalabilityStructureL2T2Key>,
                        /*num_frames_to_generate=*/4},
           SvcTestParam{std::make_unique<ScalabilityStructureL2T2KeyShift>,
                        /*num_frames_to_generate=*/4}));

}  // namespace
}  // namespace webrtc
