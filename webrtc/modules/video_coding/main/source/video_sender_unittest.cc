/*
 *  Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <vector>

#include "testing/gtest/include/gtest/gtest.h"
#include "webrtc/modules/video_coding/codecs/interface/mock/mock_video_codec_interface.h"
#include "webrtc/modules/video_coding/main/interface/mock/mock_vcm_callbacks.h"
#include "webrtc/modules/video_coding/main/interface/video_coding.h"
#include "webrtc/modules/video_coding/main/source/video_coding_impl.h"
#include "webrtc/modules/video_coding/main/test/test_util.h"
#include "webrtc/system_wrappers/interface/clock.h"
#include "webrtc/system_wrappers/interface/scoped_ptr.h"

using ::testing::_;
using ::testing::AllOf;
using ::testing::ElementsAre;
using ::testing::ElementsAreArray;
using ::testing::Field;
using ::testing::NiceMock;
using ::testing::Pointee;
using ::testing::Return;

namespace webrtc {
namespace vcm {
namespace {

class TestVideoSender : public ::testing::Test {
 protected:
  TestVideoSender() : clock_(0) {}

  virtual void SetUp() {
    sender_.reset(new VideoSender(0, &clock_));
    EXPECT_EQ(0, sender_->InitializeSender());
  }

  virtual void TearDown() { sender_.reset(); }

  SimulatedClock clock_;
  I420VideoFrame input_frame_;
  scoped_ptr<VideoSender> sender_;
};

class TestVideoSenderWithMockEncoder : public TestVideoSender {
 protected:
  static const int kDefaultWidth = 1280;
  static const int kDefaultHeight = 720;
  static const int kNumberOfStreams = 3;
  static const int kNumberOfLayers = 3;
  static const int kUnusedPayloadType = 10;

  virtual void SetUp() {
    TestVideoSender::SetUp();
    EXPECT_EQ(
        0,
        sender_->RegisterExternalEncoder(&encoder_, kUnusedPayloadType, false));
    memset(&settings_, 0, sizeof(settings_));
    EXPECT_EQ(0, VideoCodingModule::Codec(kVideoCodecVP8, &settings_));
    settings_.numberOfSimulcastStreams = kNumberOfStreams;
    ConfigureStream(kDefaultWidth / 4,
                    kDefaultHeight / 4,
                    100,
                    &settings_.simulcastStream[0]);
    ConfigureStream(kDefaultWidth / 2,
                    kDefaultHeight / 2,
                    500,
                    &settings_.simulcastStream[1]);
    ConfigureStream(
        kDefaultWidth, kDefaultHeight, 1200, &settings_.simulcastStream[2]);
    settings_.plType = kUnusedPayloadType;  // Use the mocked encoder.
    EXPECT_EQ(0, sender_->RegisterSendCodec(&settings_, 1, 1200));
  }

  void ExpectIntraRequest(int stream) {
    if (stream == -1) {
      // No intra request expected.
      EXPECT_CALL(
          encoder_,
          Encode(_,
                 _,
                 Pointee(ElementsAre(kDeltaFrame, kDeltaFrame, kDeltaFrame))))
          .Times(1).WillRepeatedly(Return(0));
      return;
    }
    assert(stream >= 0);
    assert(stream < kNumberOfStreams);
    std::vector<VideoFrameType> frame_types(kNumberOfStreams, kDeltaFrame);
    frame_types[stream] = kKeyFrame;
    EXPECT_CALL(
        encoder_,
        Encode(_,
               _,
               Pointee(ElementsAreArray(&frame_types[0], frame_types.size()))))
        .Times(1).WillRepeatedly(Return(0));
  }

  static void ConfigureStream(int width,
                              int height,
                              int max_bitrate,
                              SimulcastStream* stream) {
    assert(stream);
    stream->width = width;
    stream->height = height;
    stream->maxBitrate = max_bitrate;
    stream->numberOfTemporalLayers = kNumberOfLayers;
    stream->qpMax = 45;
  }

  VideoCodec settings_;
  NiceMock<MockVideoEncoder> encoder_;
};

TEST_F(TestVideoSenderWithMockEncoder, TestIntraRequests) {
  EXPECT_EQ(0, sender_->IntraFrameRequest(0));
  ExpectIntraRequest(0);
  EXPECT_EQ(0, sender_->AddVideoFrame(input_frame_, NULL, NULL));
  ExpectIntraRequest(-1);
  EXPECT_EQ(0, sender_->AddVideoFrame(input_frame_, NULL, NULL));

  EXPECT_EQ(0, sender_->IntraFrameRequest(1));
  ExpectIntraRequest(1);
  EXPECT_EQ(0, sender_->AddVideoFrame(input_frame_, NULL, NULL));
  ExpectIntraRequest(-1);
  EXPECT_EQ(0, sender_->AddVideoFrame(input_frame_, NULL, NULL));

  EXPECT_EQ(0, sender_->IntraFrameRequest(2));
  ExpectIntraRequest(2);
  EXPECT_EQ(0, sender_->AddVideoFrame(input_frame_, NULL, NULL));
  ExpectIntraRequest(-1);
  EXPECT_EQ(0, sender_->AddVideoFrame(input_frame_, NULL, NULL));

  EXPECT_EQ(-1, sender_->IntraFrameRequest(3));
  ExpectIntraRequest(-1);
  EXPECT_EQ(0, sender_->AddVideoFrame(input_frame_, NULL, NULL));

  EXPECT_EQ(-1, sender_->IntraFrameRequest(-1));
  ExpectIntraRequest(-1);
  EXPECT_EQ(0, sender_->AddVideoFrame(input_frame_, NULL, NULL));
}

TEST_F(TestVideoSenderWithMockEncoder, TestIntraRequestsInternalCapture) {
  // De-register current external encoder.
  EXPECT_EQ(0,
            sender_->RegisterExternalEncoder(NULL, kUnusedPayloadType, false));
  // Register encoder with internal capture.
  EXPECT_EQ(
      0, sender_->RegisterExternalEncoder(&encoder_, kUnusedPayloadType, true));
  EXPECT_EQ(0, sender_->RegisterSendCodec(&settings_, 1, 1200));
  ExpectIntraRequest(0);
  EXPECT_EQ(0, sender_->IntraFrameRequest(0));
  ExpectIntraRequest(1);
  EXPECT_EQ(0, sender_->IntraFrameRequest(1));
  ExpectIntraRequest(2);
  EXPECT_EQ(0, sender_->IntraFrameRequest(2));
  // No requests expected since these indices are out of bounds.
  EXPECT_EQ(-1, sender_->IntraFrameRequest(3));
  EXPECT_EQ(-1, sender_->IntraFrameRequest(-1));
}
}  // namespace
}  // namespace vcm
}  // namespace webrtc
