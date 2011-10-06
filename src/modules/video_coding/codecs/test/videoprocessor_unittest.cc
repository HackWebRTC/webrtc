/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#include "gtest/gtest.h"
#include "gmock/gmock.h"
#include "mocks.h"
#include "packet_reader.h"
#include "packet_manipulator.h"
#include "typedefs.h"
#include "unittest_utils.h"
#include "videoprocessor.h"

using ::testing::_;
using ::testing::AtLeast;
using ::testing::Return;

namespace webrtc {
namespace test {

// Very basic testing for VideoProcessor. It's mostly tested by running the
// video_quality_measurement program.
class VideoProcessorTest: public testing::Test {
 protected:
  MockVideoEncoder encoder_mock_;
  MockVideoDecoder decoder_mock_;
  MockFileHandler file_handler_mock_;
  MockPacketManipulator packet_manipulator_mock_;
  Stats stats_;
  TestConfig config_;

  VideoProcessorTest() {
    // To avoid warnings when using ASSERT_DEATH
    ::testing::FLAGS_gtest_death_test_style = "threadsafe";
  }

  virtual ~VideoProcessorTest() {
  }

  void SetUp() {
  }

  void TearDown() {
  }

  void ExpectInit() {
    EXPECT_CALL(encoder_mock_, InitEncode(_, _, _))
      .Times(1);
    EXPECT_CALL(encoder_mock_, RegisterEncodeCompleteCallback(_))
      .Times(AtLeast(1));
    EXPECT_CALL(decoder_mock_, InitDecode(_, _))
      .Times(1);
    EXPECT_CALL(decoder_mock_, RegisterDecodeCompleteCallback(_))
      .Times(AtLeast(1));
    EXPECT_CALL(file_handler_mock_, GetNumberOfFrames())
      .WillOnce(Return(1));
    EXPECT_CALL(file_handler_mock_, GetFrameLength())
      .WillOnce(Return(150000));
  }
};

TEST_F(VideoProcessorTest, ConstructorNullEncoder) {
  ASSERT_DEATH(VideoProcessorImpl video_processor(NULL,
                                                  &decoder_mock_,
                                                  &file_handler_mock_,
                                                  &packet_manipulator_mock_,
                                                  config_,
                                                  &stats_), "");
}

TEST_F(VideoProcessorTest, ConstructorNullDecoder) {
  ASSERT_DEATH(VideoProcessorImpl video_processor(&encoder_mock_,
                                                  NULL,
                                                  &file_handler_mock_,
                                                  &packet_manipulator_mock_,
                                                  config_,
                                                  &stats_), "");
}

TEST_F(VideoProcessorTest, ConstructorNullFileHandler) {
  ASSERT_DEATH(VideoProcessorImpl video_processor(&encoder_mock_,
                                                  &decoder_mock_,
                                                  NULL,
                                                  &packet_manipulator_mock_,
                                                  config_,
                                                  &stats_), "");
}

TEST_F(VideoProcessorTest, ConstructorNullPacketManipulator) {
  ASSERT_DEATH(VideoProcessorImpl video_processor(&encoder_mock_,
                                                  &decoder_mock_,
                                                  &file_handler_mock_,
                                                  NULL,
                                                  config_,
                                                  &stats_), "");
}

TEST_F(VideoProcessorTest, ConstructorNullStats) {
  ASSERT_DEATH(VideoProcessorImpl video_processor(&encoder_mock_,
                                                  &decoder_mock_,
                                                  &file_handler_mock_,
                                                  &packet_manipulator_mock_,
                                                  config_,
                                                  NULL), "");
}

TEST_F(VideoProcessorTest, Init) {
  ExpectInit();
  VideoProcessorImpl video_processor(&encoder_mock_, &decoder_mock_,
                                     &file_handler_mock_,
                                     &packet_manipulator_mock_, config_,
                                     &stats_);
  video_processor.Init();
}

TEST_F(VideoProcessorTest, ProcessFrame) {
  ExpectInit();
  EXPECT_CALL(encoder_mock_, Encode(_, _, _))
    .Times(1);
  EXPECT_CALL(file_handler_mock_, ReadFrame(_))
  .WillOnce(Return(true));
  // Since we don't return any callback from the mock, the decoder will not
  // be more than initialized...
  VideoProcessorImpl video_processor(&encoder_mock_, &decoder_mock_,
                                     &file_handler_mock_,
                                     &packet_manipulator_mock_, config_,
                                     &stats_);
  video_processor.Init();
  video_processor.ProcessFrame(0);
}

TEST_F(VideoProcessorTest, ProcessFrameInvalidArgument) {
  ExpectInit();
  VideoProcessorImpl video_processor(&encoder_mock_, &decoder_mock_,
                                     &file_handler_mock_,
                                     &packet_manipulator_mock_, config_,
                                     &stats_);
  video_processor.Init();
  ASSERT_DEATH(video_processor.ProcessFrame(-1), "");
}


}  // namespace test
}  // namespace webrtc
