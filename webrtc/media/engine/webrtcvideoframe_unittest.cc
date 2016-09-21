/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <string.h>

#include <memory>

#include "webrtc/media/base/videoframe_unittest.h"
#include "webrtc/media/engine/webrtcvideoframe.h"
#include "webrtc/test/fake_texture_frame.h"

namespace cricket {

class WebRtcVideoFrameTest : public VideoFrameTest<WebRtcVideoFrame> {
 public:
  WebRtcVideoFrameTest() {}

  void TestInit(int cropped_width, int cropped_height,
                webrtc::VideoRotation frame_rotation,
                bool apply_rotation) {
    const int frame_width = 1920;
    const int frame_height = 1080;

    // Build the CapturedFrame.
    CapturedFrame captured_frame;
    captured_frame.fourcc = FOURCC_I420;
    captured_frame.time_stamp = rtc::TimeNanos();
    captured_frame.rotation = frame_rotation;
    captured_frame.width = frame_width;
    captured_frame.height = frame_height;
    captured_frame.data_size = (frame_width * frame_height) +
        ((frame_width + 1) / 2) * ((frame_height + 1) / 2) * 2;
    std::unique_ptr<uint8_t[]> captured_frame_buffer(
        new uint8_t[captured_frame.data_size]);
    // Initialize memory to satisfy DrMemory tests.
    memset(captured_frame_buffer.get(), 0, captured_frame.data_size);
    captured_frame.data = captured_frame_buffer.get();

    // Create the new frame from the CapturedFrame.
    WebRtcVideoFrame frame;
    EXPECT_TRUE(
        frame.Init(&captured_frame, cropped_width, cropped_height,
                   apply_rotation));

    // Verify the new frame.
    EXPECT_EQ(captured_frame.time_stamp / rtc::kNumNanosecsPerMicrosec,
              frame.timestamp_us());
    if (apply_rotation)
      EXPECT_EQ(webrtc::kVideoRotation_0, frame.rotation());
    else
      EXPECT_EQ(frame_rotation, frame.rotation());
    // If |apply_rotation| and the frame rotation is 90 or 270, width and
    // height are flipped.
    if (apply_rotation && (frame_rotation == webrtc::kVideoRotation_90
        || frame_rotation == webrtc::kVideoRotation_270)) {
      EXPECT_EQ(cropped_width, frame.height());
      EXPECT_EQ(cropped_height, frame.width());
    } else {
      EXPECT_EQ(cropped_width, frame.width());
      EXPECT_EQ(cropped_height, frame.height());
    }
  }

  void SetFrameRotation(WebRtcVideoFrame* frame,
                        webrtc::VideoRotation rotation) {
    frame->rotation_ = rotation;
  }
};

#define TEST_WEBRTCVIDEOFRAME(X) \
  TEST_F(WebRtcVideoFrameTest, X) { VideoFrameTest<WebRtcVideoFrame>::X(); }

TEST_WEBRTCVIDEOFRAME(ConstructI420)
TEST_WEBRTCVIDEOFRAME(ConstructI422)
TEST_WEBRTCVIDEOFRAME(ConstructYuy2)
TEST_WEBRTCVIDEOFRAME(ConstructYuy2Unaligned)
TEST_WEBRTCVIDEOFRAME(ConstructYuy2Wide)
TEST_WEBRTCVIDEOFRAME(ConstructYV12)
TEST_WEBRTCVIDEOFRAME(ConstructUyvy)
TEST_WEBRTCVIDEOFRAME(ConstructM420)
TEST_WEBRTCVIDEOFRAME(ConstructNV21)
TEST_WEBRTCVIDEOFRAME(ConstructNV12)
TEST_WEBRTCVIDEOFRAME(ConstructABGR)
TEST_WEBRTCVIDEOFRAME(ConstructARGB)
TEST_WEBRTCVIDEOFRAME(ConstructARGBWide)
TEST_WEBRTCVIDEOFRAME(ConstructBGRA)
TEST_WEBRTCVIDEOFRAME(Construct24BG)
TEST_WEBRTCVIDEOFRAME(ConstructRaw)

TEST_WEBRTCVIDEOFRAME(ConstructI420Mirror)
TEST_WEBRTCVIDEOFRAME(ConstructI420Rotate0)
TEST_WEBRTCVIDEOFRAME(ConstructI420Rotate90)
TEST_WEBRTCVIDEOFRAME(ConstructI420Rotate180)
TEST_WEBRTCVIDEOFRAME(ConstructI420Rotate270)
TEST_WEBRTCVIDEOFRAME(ConstructYV12Rotate0)
TEST_WEBRTCVIDEOFRAME(ConstructYV12Rotate90)
TEST_WEBRTCVIDEOFRAME(ConstructYV12Rotate180)
TEST_WEBRTCVIDEOFRAME(ConstructYV12Rotate270)
TEST_WEBRTCVIDEOFRAME(ConstructNV12Rotate0)
TEST_WEBRTCVIDEOFRAME(ConstructNV12Rotate90)
TEST_WEBRTCVIDEOFRAME(ConstructNV12Rotate180)
TEST_WEBRTCVIDEOFRAME(ConstructNV12Rotate270)
TEST_WEBRTCVIDEOFRAME(ConstructNV21Rotate0)
TEST_WEBRTCVIDEOFRAME(ConstructNV21Rotate90)
TEST_WEBRTCVIDEOFRAME(ConstructNV21Rotate180)
TEST_WEBRTCVIDEOFRAME(ConstructNV21Rotate270)
TEST_WEBRTCVIDEOFRAME(ConstructUYVYRotate0)
TEST_WEBRTCVIDEOFRAME(ConstructUYVYRotate90)
TEST_WEBRTCVIDEOFRAME(ConstructUYVYRotate180)
TEST_WEBRTCVIDEOFRAME(ConstructUYVYRotate270)
TEST_WEBRTCVIDEOFRAME(ConstructYUY2Rotate0)
TEST_WEBRTCVIDEOFRAME(ConstructYUY2Rotate90)
TEST_WEBRTCVIDEOFRAME(ConstructYUY2Rotate180)
TEST_WEBRTCVIDEOFRAME(ConstructYUY2Rotate270)
TEST_WEBRTCVIDEOFRAME(ConstructI4201Pixel)
TEST_WEBRTCVIDEOFRAME(ConstructI4205Pixel)
// TODO(juberti): WebRtcVideoFrame does not support horizontal crop.
// Re-evaluate once it supports 3 independent planes, since we might want to
// just Init normally and then crop by adjusting pointers.
// TEST_WEBRTCVIDEOFRAME(ConstructI420CropHorizontal)
TEST_WEBRTCVIDEOFRAME(ConstructI420CropVertical)
// TODO(juberti): WebRtcVideoFrame is not currently refcounted.
// TEST_WEBRTCVIDEOFRAME(ConstructCopy)
// TEST_WEBRTCVIDEOFRAME(ConstructCopyIsRef)
// TODO(fbarchard): Implement Jpeg
// TEST_WEBRTCVIDEOFRAME(ConstructMjpgI420)
TEST_WEBRTCVIDEOFRAME(ConstructMjpgI422)
// TEST_WEBRTCVIDEOFRAME(ConstructMjpgI444)
// TEST_WEBRTCVIDEOFRAME(ConstructMjpgI411)
// TEST_WEBRTCVIDEOFRAME(ConstructMjpgI400)
// TEST_WEBRTCVIDEOFRAME(ValidateMjpgI420)
// TEST_WEBRTCVIDEOFRAME(ValidateMjpgI422)
// TEST_WEBRTCVIDEOFRAME(ValidateMjpgI444)
// TEST_WEBRTCVIDEOFRAME(ValidateMjpgI411)
// TEST_WEBRTCVIDEOFRAME(ValidateMjpgI400)
TEST_WEBRTCVIDEOFRAME(ValidateI420)
TEST_WEBRTCVIDEOFRAME(ValidateI420SmallSize)
TEST_WEBRTCVIDEOFRAME(ValidateI420LargeSize)
TEST_WEBRTCVIDEOFRAME(ValidateI420HugeSize)
// TEST_WEBRTCVIDEOFRAME(ValidateMjpgI420InvalidSize)
// TEST_WEBRTCVIDEOFRAME(ValidateI420InvalidSize)

// TODO(fbarchard): WebRtcVideoFrame does not support odd sizes.
// Re-evaluate once WebRTC switches to libyuv
// TEST_WEBRTCVIDEOFRAME(ConstructYuy2AllSizes)
// TEST_WEBRTCVIDEOFRAME(ConstructARGBAllSizes)
// TEST_WEBRTCVIDEOFRAME(ConvertToI422Buffer)
// TEST_WEBRTCVIDEOFRAME(ConstructARGBBlackWhitePixel)

// These functions test implementation-specific details.
// Tests the Init function with different cropped size.
TEST_F(WebRtcVideoFrameTest, InitEvenSize) {
  TestInit(640, 360, webrtc::kVideoRotation_0, true);
}

TEST_F(WebRtcVideoFrameTest, InitOddWidth) {
  TestInit(601, 480, webrtc::kVideoRotation_0, true);
}

TEST_F(WebRtcVideoFrameTest, InitOddHeight) {
  TestInit(360, 765, webrtc::kVideoRotation_0, true);
}

TEST_F(WebRtcVideoFrameTest, InitOddWidthHeight) {
  TestInit(355, 1021, webrtc::kVideoRotation_0, true);
}

TEST_F(WebRtcVideoFrameTest, InitRotated90ApplyRotation) {
  TestInit(640, 360, webrtc::kVideoRotation_90, true);
}

TEST_F(WebRtcVideoFrameTest, InitRotated90DontApplyRotation) {
  TestInit(640, 360, webrtc::kVideoRotation_90, false);
}

TEST_F(WebRtcVideoFrameTest, TextureInitialValues) {
  webrtc::test::FakeNativeHandle* dummy_handle =
      new webrtc::test::FakeNativeHandle();
  webrtc::NativeHandleBuffer* buffer =
      new rtc::RefCountedObject<webrtc::test::FakeNativeHandleBuffer>(
          dummy_handle, 640, 480);

  WebRtcVideoFrame frame(buffer, webrtc::kVideoRotation_0, 20);
  EXPECT_EQ(dummy_handle, frame.video_frame_buffer()->native_handle());
  EXPECT_EQ(640, frame.width());
  EXPECT_EQ(480, frame.height());
  EXPECT_EQ(20, frame.timestamp_us());
  frame.set_timestamp_us(40);
  EXPECT_EQ(40, frame.timestamp_us());
}

TEST_F(WebRtcVideoFrameTest, ApplyRotationToFrame) {
  WebRtcVideoFrame applied0;
  EXPECT_TRUE(IsNull(applied0));
  EXPECT_TRUE(LoadFrame(CreateYuvSample(kWidth, kHeight, 12).get(), FOURCC_I420,
                        kWidth, kHeight, &applied0));

  // Claim that this frame needs to be rotated for 90 degree.
  SetFrameRotation(&applied0, webrtc::kVideoRotation_90);
  EXPECT_EQ(applied0.rotation(), webrtc::kVideoRotation_90);

  // Apply rotation on frame 1. Output should be different from frame 1.
  WebRtcVideoFrame applied90(
      webrtc::I420Buffer::Rotate(applied0.video_frame_buffer(),
                                 applied0.rotation()),
      webrtc::kVideoRotation_0, applied0.timestamp_us());

  EXPECT_EQ(applied90.rotation(), webrtc::kVideoRotation_0);
  EXPECT_FALSE(IsEqual(applied0, applied90, 0));

  // Claim the frame 2 needs to be rotated for another 270 degree. The output
  // from frame 2 rotation should be the same as frame 1.
  SetFrameRotation(&applied90, webrtc::kVideoRotation_270);
  WebRtcVideoFrame applied360(
      webrtc::I420Buffer::Rotate(applied90.video_frame_buffer(),
                                 applied90.rotation()),
      webrtc::kVideoRotation_0, applied90.timestamp_us());
  EXPECT_EQ(applied360.rotation(), webrtc::kVideoRotation_0);
  EXPECT_TRUE(IsEqual(applied0, applied360, 0));
}

}  // namespace cricket
