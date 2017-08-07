/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <memory>

#include "webrtc/api/video/i420_buffer.h"
#include "webrtc/modules/video_coding/codecs/test/mock/mock_packet_manipulator.h"
#include "webrtc/modules/video_coding/codecs/test/videoprocessor.h"
#include "webrtc/modules/video_coding/include/mock/mock_video_codec_interface.h"
#include "webrtc/modules/video_coding/include/video_coding.h"
#include "webrtc/rtc_base/ptr_util.h"
#include "webrtc/test/gmock.h"
#include "webrtc/test/gtest.h"
#include "webrtc/test/testsupport/mock/mock_frame_reader.h"
#include "webrtc/test/testsupport/mock/mock_frame_writer.h"
#include "webrtc/test/testsupport/packet_reader.h"
#include "webrtc/test/testsupport/unittest_utils.h"
#include "webrtc/typedefs.h"

using ::testing::_;
using ::testing::AtLeast;
using ::testing::Return;

namespace webrtc {
namespace test {

namespace {

const int kWidth = 352;
const int kHeight = 288;
const int kFrameSize = kWidth * kHeight * 3 / 2;  // I420.
const int kFramerate = 30;
const int kNumFrames = 2;

}  // namespace

class VideoProcessorTest : public testing::Test {
 protected:
  VideoProcessorTest() {
    // Get a codec configuration struct and configure it.
    VideoCodingModule::Codec(kVideoCodecVP8, &codec_settings_);
    config_.codec_settings = &codec_settings_;
    config_.codec_settings->startBitrate = 100;
    config_.codec_settings->width = kWidth;
    config_.codec_settings->height = kHeight;
    config_.codec_settings->maxFramerate = kFramerate;

    EXPECT_CALL(frame_reader_mock_, NumberOfFrames())
        .WillRepeatedly(Return(kNumFrames));
    EXPECT_CALL(frame_reader_mock_, FrameLength())
        .WillRepeatedly(Return(kFrameSize));
    video_processor_ = rtc::MakeUnique<VideoProcessor>(
        &encoder_mock_, &decoder_mock_, &frame_reader_mock_,
        &frame_writer_mock_, &packet_manipulator_mock_, config_, &stats_,
        nullptr /* encoded_frame_writer */, nullptr /* decoded_frame_writer */);
  }

  void ExpectInit() {
    EXPECT_CALL(encoder_mock_, InitEncode(_, _, _)).Times(1);
    EXPECT_CALL(encoder_mock_, RegisterEncodeCompleteCallback(_))
        .Times(AtLeast(1));
    EXPECT_CALL(decoder_mock_, InitDecode(_, _)).Times(1);
    EXPECT_CALL(decoder_mock_, RegisterDecodeCompleteCallback(_))
        .Times(AtLeast(1));
  }

  MockVideoEncoder encoder_mock_;
  MockVideoDecoder decoder_mock_;
  MockFrameReader frame_reader_mock_;
  MockFrameWriter frame_writer_mock_;
  MockPacketManipulator packet_manipulator_mock_;
  VideoCodec codec_settings_;
  TestConfig config_;
  Stats stats_;
  std::unique_ptr<VideoProcessor> video_processor_;
};

TEST_F(VideoProcessorTest, Init) {
  ExpectInit();
  video_processor_->Init();
}

TEST_F(VideoProcessorTest, ProcessFrames) {
  ExpectInit();
  video_processor_->Init();

  EXPECT_CALL(frame_reader_mock_, ReadFrame())
      .WillRepeatedly(Return(I420Buffer::Create(kWidth, kHeight)));
  EXPECT_CALL(encoder_mock_, Encode(testing::Property(&VideoFrame::timestamp,
                                                      1 * 90000 / kFramerate),
                                    _, _))
      .Times(1);
  video_processor_->ProcessFrame(0);

  EXPECT_CALL(encoder_mock_, Encode(testing::Property(&VideoFrame::timestamp,
                                                      2 * 90000 / kFramerate),
                                    _, _))
      .Times(1);
  video_processor_->ProcessFrame(1);
}

}  // namespace test
}  // namespace webrtc
