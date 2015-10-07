/*
 * libjingle
 * Copyright 2015 Google Inc.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 *  1. Redistributions of source code must retain the above copyright notice,
 *     this list of conditions and the following disclaimer.
 *  2. Redistributions in binary form must reproduce the above copyright notice,
 *     this list of conditions and the following disclaimer in the documentation
 *     and/or other materials provided with the distribution.
 *  3. The name of the author may not be used to endorse or promote products
 *     derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO
 * EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <string.h>

#include "talk/media/base/videoframe_unittest.h"
#include "talk/media/webrtc/webrtcvideoframe.h"
#include "talk/media/webrtc/webrtcvideoframefactory.h"

class WebRtcVideoFrameFactoryTest
    : public VideoFrameTest<cricket::WebRtcVideoFrameFactory> {
 public:
  WebRtcVideoFrameFactoryTest() {}

  void InitFrame(webrtc::VideoRotation frame_rotation) {
    const int frame_width = 1920;
    const int frame_height = 1080;

    // Build the CapturedFrame.
    captured_frame_.fourcc = cricket::FOURCC_I420;
    captured_frame_.pixel_width = 1;
    captured_frame_.pixel_height = 1;
    captured_frame_.time_stamp = 5678;
    captured_frame_.rotation = frame_rotation;
    captured_frame_.width = frame_width;
    captured_frame_.height = frame_height;
    captured_frame_.data_size =
        (frame_width * frame_height) +
        ((frame_width + 1) / 2) * ((frame_height + 1) / 2) * 2;
    captured_frame_buffer_.reset(new uint8_t[captured_frame_.data_size]);
    // Initialize memory to satisfy DrMemory tests.
    memset(captured_frame_buffer_.get(), 0, captured_frame_.data_size);
    captured_frame_.data = captured_frame_buffer_.get();
  }

  void VerifyFrame(cricket::VideoFrame* dest_frame,
                   webrtc::VideoRotation src_rotation,
                   int src_width,
                   int src_height,
                   bool apply_rotation) {
    if (!apply_rotation) {
      EXPECT_EQ(dest_frame->GetRotation(), src_rotation);
      EXPECT_EQ(dest_frame->GetWidth(), src_width);
      EXPECT_EQ(dest_frame->GetHeight(), src_height);
    } else {
      EXPECT_EQ(dest_frame->GetRotation(), webrtc::kVideoRotation_0);
      if (src_rotation == webrtc::kVideoRotation_90 ||
          src_rotation == webrtc::kVideoRotation_270) {
        EXPECT_EQ(dest_frame->GetWidth(), src_height);
        EXPECT_EQ(dest_frame->GetHeight(), src_width);
      } else {
        EXPECT_EQ(dest_frame->GetWidth(), src_width);
        EXPECT_EQ(dest_frame->GetHeight(), src_height);
      }
    }
  }

  void TestCreateAliasedFrame(bool apply_rotation) {
    cricket::VideoFrameFactory& factory = factory_;
    factory.SetApplyRotation(apply_rotation);
    InitFrame(webrtc::kVideoRotation_270);
    const cricket::CapturedFrame& captured_frame = get_captured_frame();
    // Create the new frame from the CapturedFrame.
    rtc::scoped_ptr<cricket::VideoFrame> frame;
    int new_width = captured_frame.width / 2;
    int new_height = captured_frame.height / 2;
    frame.reset(factory.CreateAliasedFrame(&captured_frame, new_width,
                                           new_height, new_width, new_height));
    VerifyFrame(frame.get(), webrtc::kVideoRotation_270, new_width, new_height,
                apply_rotation);

    frame.reset(factory.CreateAliasedFrame(
        &captured_frame, new_width, new_height, new_width / 2, new_height / 2));
    VerifyFrame(frame.get(), webrtc::kVideoRotation_270, new_width / 2,
                new_height / 2, apply_rotation);

    // Reset the frame first so it's exclusive hence we could go through the
    // StretchToFrame code path in CreateAliasedFrame.
    frame.reset();
    frame.reset(factory.CreateAliasedFrame(
        &captured_frame, new_width, new_height, new_width / 2, new_height / 2));
    VerifyFrame(frame.get(), webrtc::kVideoRotation_270, new_width / 2,
                new_height / 2, apply_rotation);
  }

  const cricket::CapturedFrame& get_captured_frame() { return captured_frame_; }

 private:
  cricket::CapturedFrame captured_frame_;
  rtc::scoped_ptr<uint8_t[]> captured_frame_buffer_;
  cricket::WebRtcVideoFrameFactory factory_;
};

TEST_F(WebRtcVideoFrameFactoryTest, NoApplyRotation) {
  TestCreateAliasedFrame(false);
}

TEST_F(WebRtcVideoFrameFactoryTest, ApplyRotation) {
  TestCreateAliasedFrame(true);
}
