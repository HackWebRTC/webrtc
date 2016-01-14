/*
 * libjingle
 * Copyright 2011 Google Inc.
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
#include "webrtc/test/fake_texture_frame.h"

namespace {

class WebRtcVideoTestFrame : public cricket::WebRtcVideoFrame {
 public:
  using cricket::WebRtcVideoFrame::SetRotation;

  virtual VideoFrame* CreateEmptyFrame(int w,
                                       int h,
                                       size_t pixel_width,
                                       size_t pixel_height,
                                       int64_t time_stamp) const override {
    WebRtcVideoTestFrame* frame = new WebRtcVideoTestFrame();
    frame->InitToBlack(w, h, pixel_width, pixel_height, time_stamp);
    return frame;
  }
};

}  // namespace

class WebRtcVideoFrameTest : public VideoFrameTest<cricket::WebRtcVideoFrame> {
 public:
  WebRtcVideoFrameTest() {
  }

  void TestInit(int cropped_width, int cropped_height,
                webrtc::VideoRotation frame_rotation,
                bool apply_rotation) {
    const int frame_width = 1920;
    const int frame_height = 1080;

    // Build the CapturedFrame.
    cricket::CapturedFrame captured_frame;
    captured_frame.fourcc = cricket::FOURCC_I420;
    captured_frame.pixel_width = 1;
    captured_frame.pixel_height = 1;
    captured_frame.time_stamp = 5678;
    captured_frame.rotation = frame_rotation;
    captured_frame.width = frame_width;
    captured_frame.height = frame_height;
    captured_frame.data_size = (frame_width * frame_height) +
        ((frame_width + 1) / 2) * ((frame_height + 1) / 2) * 2;
    rtc::scoped_ptr<uint8_t[]> captured_frame_buffer(
        new uint8_t[captured_frame.data_size]);
    // Initialize memory to satisfy DrMemory tests.
    memset(captured_frame_buffer.get(), 0, captured_frame.data_size);
    captured_frame.data = captured_frame_buffer.get();

    // Create the new frame from the CapturedFrame.
    cricket::WebRtcVideoFrame frame;
    EXPECT_TRUE(
        frame.Init(&captured_frame, cropped_width, cropped_height,
                   apply_rotation));

    // Verify the new frame.
    EXPECT_EQ(1u, frame.GetPixelWidth());
    EXPECT_EQ(1u, frame.GetPixelHeight());
    EXPECT_EQ(5678, frame.GetTimeStamp());
    if (apply_rotation)
      EXPECT_EQ(webrtc::kVideoRotation_0, frame.GetRotation());
    else
      EXPECT_EQ(frame_rotation, frame.GetRotation());
    // If |apply_rotation| and the frame rotation is 90 or 270, width and
    // height are flipped.
    if (apply_rotation && (frame_rotation == webrtc::kVideoRotation_90
        || frame_rotation == webrtc::kVideoRotation_270)) {
      EXPECT_EQ(static_cast<size_t>(cropped_width), frame.GetHeight());
      EXPECT_EQ(static_cast<size_t>(cropped_height), frame.GetWidth());
    } else {
      EXPECT_EQ(static_cast<size_t>(cropped_width), frame.GetWidth());
      EXPECT_EQ(static_cast<size_t>(cropped_height), frame.GetHeight());
    }
  }
};

#define TEST_WEBRTCVIDEOFRAME(X) TEST_F(WebRtcVideoFrameTest, X) { \
  VideoFrameTest<cricket::WebRtcVideoFrame>::X(); \
}

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
TEST_WEBRTCVIDEOFRAME(ConstructRGB565)
TEST_WEBRTCVIDEOFRAME(ConstructARGB1555)
TEST_WEBRTCVIDEOFRAME(ConstructARGB4444)

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
TEST_WEBRTCVIDEOFRAME(ConstructBlack)
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
TEST_WEBRTCVIDEOFRAME(ResetAndApplyRotation)
TEST_WEBRTCVIDEOFRAME(ResetAndDontApplyRotation)
TEST_WEBRTCVIDEOFRAME(ConvertToABGRBuffer)
TEST_WEBRTCVIDEOFRAME(ConvertToABGRBufferStride)
TEST_WEBRTCVIDEOFRAME(ConvertToABGRBufferInverted)
TEST_WEBRTCVIDEOFRAME(ConvertToARGB1555Buffer)
TEST_WEBRTCVIDEOFRAME(ConvertToARGB1555BufferStride)
TEST_WEBRTCVIDEOFRAME(ConvertToARGB1555BufferInverted)
TEST_WEBRTCVIDEOFRAME(ConvertToARGB4444Buffer)
TEST_WEBRTCVIDEOFRAME(ConvertToARGB4444BufferStride)
TEST_WEBRTCVIDEOFRAME(ConvertToARGB4444BufferInverted)
TEST_WEBRTCVIDEOFRAME(ConvertToARGBBuffer)
TEST_WEBRTCVIDEOFRAME(ConvertToARGBBufferStride)
TEST_WEBRTCVIDEOFRAME(ConvertToARGBBufferInverted)
TEST_WEBRTCVIDEOFRAME(ConvertToBGRABuffer)
TEST_WEBRTCVIDEOFRAME(ConvertToBGRABufferStride)
TEST_WEBRTCVIDEOFRAME(ConvertToBGRABufferInverted)
TEST_WEBRTCVIDEOFRAME(ConvertToRAWBuffer)
TEST_WEBRTCVIDEOFRAME(ConvertToRAWBufferStride)
TEST_WEBRTCVIDEOFRAME(ConvertToRAWBufferInverted)
TEST_WEBRTCVIDEOFRAME(ConvertToRGB24Buffer)
TEST_WEBRTCVIDEOFRAME(ConvertToRGB24BufferStride)
TEST_WEBRTCVIDEOFRAME(ConvertToRGB24BufferInverted)
TEST_WEBRTCVIDEOFRAME(ConvertToRGB565Buffer)
TEST_WEBRTCVIDEOFRAME(ConvertToRGB565BufferStride)
TEST_WEBRTCVIDEOFRAME(ConvertToRGB565BufferInverted)
TEST_WEBRTCVIDEOFRAME(ConvertToI400Buffer)
TEST_WEBRTCVIDEOFRAME(ConvertToI400BufferStride)
TEST_WEBRTCVIDEOFRAME(ConvertToI400BufferInverted)
TEST_WEBRTCVIDEOFRAME(ConvertToYUY2Buffer)
TEST_WEBRTCVIDEOFRAME(ConvertToYUY2BufferStride)
TEST_WEBRTCVIDEOFRAME(ConvertToYUY2BufferInverted)
TEST_WEBRTCVIDEOFRAME(ConvertToUYVYBuffer)
TEST_WEBRTCVIDEOFRAME(ConvertToUYVYBufferStride)
TEST_WEBRTCVIDEOFRAME(ConvertToUYVYBufferInverted)
TEST_WEBRTCVIDEOFRAME(ConvertFromABGRBuffer)
TEST_WEBRTCVIDEOFRAME(ConvertFromABGRBufferStride)
TEST_WEBRTCVIDEOFRAME(ConvertFromABGRBufferInverted)
TEST_WEBRTCVIDEOFRAME(ConvertFromARGB1555Buffer)
TEST_WEBRTCVIDEOFRAME(ConvertFromARGB1555BufferStride)
TEST_WEBRTCVIDEOFRAME(ConvertFromARGB1555BufferInverted)
TEST_WEBRTCVIDEOFRAME(ConvertFromARGB4444Buffer)
TEST_WEBRTCVIDEOFRAME(ConvertFromARGB4444BufferStride)
TEST_WEBRTCVIDEOFRAME(ConvertFromARGB4444BufferInverted)
TEST_WEBRTCVIDEOFRAME(ConvertFromARGBBuffer)
TEST_WEBRTCVIDEOFRAME(ConvertFromARGBBufferStride)
TEST_WEBRTCVIDEOFRAME(ConvertFromARGBBufferInverted)
TEST_WEBRTCVIDEOFRAME(ConvertFromBGRABuffer)
TEST_WEBRTCVIDEOFRAME(ConvertFromBGRABufferStride)
TEST_WEBRTCVIDEOFRAME(ConvertFromBGRABufferInverted)
TEST_WEBRTCVIDEOFRAME(ConvertFromRAWBuffer)
TEST_WEBRTCVIDEOFRAME(ConvertFromRAWBufferStride)
TEST_WEBRTCVIDEOFRAME(ConvertFromRAWBufferInverted)
TEST_WEBRTCVIDEOFRAME(ConvertFromRGB24Buffer)
TEST_WEBRTCVIDEOFRAME(ConvertFromRGB24BufferStride)
TEST_WEBRTCVIDEOFRAME(ConvertFromRGB24BufferInverted)
TEST_WEBRTCVIDEOFRAME(ConvertFromRGB565Buffer)
TEST_WEBRTCVIDEOFRAME(ConvertFromRGB565BufferStride)
TEST_WEBRTCVIDEOFRAME(ConvertFromRGB565BufferInverted)
TEST_WEBRTCVIDEOFRAME(ConvertFromI400Buffer)
TEST_WEBRTCVIDEOFRAME(ConvertFromI400BufferStride)
TEST_WEBRTCVIDEOFRAME(ConvertFromI400BufferInverted)
TEST_WEBRTCVIDEOFRAME(ConvertFromYUY2Buffer)
TEST_WEBRTCVIDEOFRAME(ConvertFromYUY2BufferStride)
TEST_WEBRTCVIDEOFRAME(ConvertFromYUY2BufferInverted)
TEST_WEBRTCVIDEOFRAME(ConvertFromUYVYBuffer)
TEST_WEBRTCVIDEOFRAME(ConvertFromUYVYBufferStride)
TEST_WEBRTCVIDEOFRAME(ConvertFromUYVYBufferInverted)
// TEST_WEBRTCVIDEOFRAME(ConvertToI422Buffer)
TEST_WEBRTCVIDEOFRAME(CopyToBuffer)
TEST_WEBRTCVIDEOFRAME(CopyToFrame)
TEST_WEBRTCVIDEOFRAME(Write)
TEST_WEBRTCVIDEOFRAME(CopyToBuffer1Pixel)
// TEST_WEBRTCVIDEOFRAME(ConstructARGBBlackWhitePixel)

TEST_WEBRTCVIDEOFRAME(StretchToFrame)
TEST_WEBRTCVIDEOFRAME(Copy)
TEST_WEBRTCVIDEOFRAME(CopyIsRef)
TEST_WEBRTCVIDEOFRAME(MakeExclusive)

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
  cricket::WebRtcVideoFrame frame(buffer, 200, webrtc::kVideoRotation_0);
  EXPECT_EQ(dummy_handle, frame.GetNativeHandle());
  EXPECT_EQ(640u, frame.GetWidth());
  EXPECT_EQ(480u, frame.GetHeight());
  EXPECT_EQ(200, frame.GetTimeStamp());
  frame.SetTimeStamp(400);
  EXPECT_EQ(400, frame.GetTimeStamp());
}

TEST_F(WebRtcVideoFrameTest, CopyTextureFrame) {
  webrtc::test::FakeNativeHandle* dummy_handle =
      new webrtc::test::FakeNativeHandle();
  webrtc::NativeHandleBuffer* buffer =
      new rtc::RefCountedObject<webrtc::test::FakeNativeHandleBuffer>(
          dummy_handle, 640, 480);
  cricket::WebRtcVideoFrame frame1(buffer, 200, webrtc::kVideoRotation_0);
  cricket::VideoFrame* frame2 = frame1.Copy();
  EXPECT_EQ(frame1.GetNativeHandle(), frame2->GetNativeHandle());
  EXPECT_EQ(frame1.GetWidth(), frame2->GetWidth());
  EXPECT_EQ(frame1.GetHeight(), frame2->GetHeight());
  EXPECT_EQ(frame1.GetTimeStamp(), frame2->GetTimeStamp());
  delete frame2;
}

TEST_F(WebRtcVideoFrameTest, ApplyRotationToFrame) {
  WebRtcVideoTestFrame applied0;
  EXPECT_TRUE(IsNull(applied0));
  rtc::scoped_ptr<rtc::MemoryStream> ms(CreateYuvSample(kWidth, kHeight, 12));
  EXPECT_TRUE(
      LoadFrame(ms.get(), cricket::FOURCC_I420, kWidth, kHeight, &applied0));

  // Claim that this frame needs to be rotated for 90 degree.
  applied0.SetRotation(webrtc::kVideoRotation_90);

  // Apply rotation on frame 1. Output should be different from frame 1.
  WebRtcVideoTestFrame* applied90 = const_cast<WebRtcVideoTestFrame*>(
      static_cast<const WebRtcVideoTestFrame*>(
          applied0.GetCopyWithRotationApplied()));
  EXPECT_TRUE(applied90);
  EXPECT_EQ(applied90->GetVideoRotation(), webrtc::kVideoRotation_0);
  EXPECT_FALSE(IsEqual(applied0, *applied90, 0));

  // Claim the frame 2 needs to be rotated for another 270 degree. The output
  // from frame 2 rotation should be the same as frame 1.
  applied90->SetRotation(webrtc::kVideoRotation_270);
  const cricket::VideoFrame* applied360 =
      applied90->GetCopyWithRotationApplied();
  EXPECT_TRUE(applied360);
  EXPECT_EQ(applied360->GetVideoRotation(), webrtc::kVideoRotation_0);
  EXPECT_TRUE(IsEqual(applied0, *applied360, 0));
}
