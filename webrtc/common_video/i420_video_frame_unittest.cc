/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <math.h>
#include <string.h>

#include "testing/gtest/include/gtest/gtest.h"
#include "webrtc/base/scoped_ptr.h"
#include "webrtc/common_video/interface/i420_video_frame.h"

namespace webrtc {

class NativeHandleImpl : public NativeHandle {
 public:
  NativeHandleImpl() : ref_count_(0) {}
  virtual ~NativeHandleImpl() {}
  virtual int32_t AddRef() { return ++ref_count_; }
  virtual int32_t Release() { return --ref_count_; }
  virtual void* GetHandle() { return NULL; }

  int32_t ref_count() { return ref_count_; }
 private:
  int32_t ref_count_;
};

bool EqualPlane(const uint8_t* data1,
                const uint8_t* data2,
                int stride,
                int width,
                int height);
bool EqualFrames(const I420VideoFrame& frame1, const I420VideoFrame& frame2);
bool EqualTextureFrames(const I420VideoFrame& frame1,
                        const I420VideoFrame& frame2);
int ExpectedSize(int plane_stride, int image_height, PlaneType type);

TEST(TestI420VideoFrame, InitialValues) {
  I420VideoFrame frame;
  // Invalid arguments - one call for each variable.
  EXPECT_TRUE(frame.IsZeroSize());
  EXPECT_EQ(kVideoRotation_0, frame.rotation());
  EXPECT_EQ(-1, frame.CreateEmptyFrame(0, 10, 10, 14, 14));
  EXPECT_EQ(-1, frame.CreateEmptyFrame(10, -1, 10, 90, 14));
  EXPECT_EQ(-1, frame.CreateEmptyFrame(10, 10, 0, 14, 18));
  EXPECT_EQ(-1, frame.CreateEmptyFrame(10, 10, 10, -2, 13));
  EXPECT_EQ(-1, frame.CreateEmptyFrame(10, 10, 10, 14, 0));
  EXPECT_EQ(0, frame.CreateEmptyFrame(10, 10, 10, 14, 90));
  EXPECT_FALSE(frame.IsZeroSize());
}

TEST(TestI420VideoFrame, WidthHeightValues) {
  I420VideoFrame frame;
  const int valid_value = 10;
  EXPECT_EQ(0, frame.CreateEmptyFrame(10, 10, 10, 14, 90));
  EXPECT_EQ(valid_value, frame.width());
  EXPECT_EQ(valid_value, frame.height());
  frame.set_timestamp(123u);
  EXPECT_EQ(123u, frame.timestamp());
  frame.set_ntp_time_ms(456);
  EXPECT_EQ(456, frame.ntp_time_ms());
  frame.set_render_time_ms(789);
  EXPECT_EQ(789, frame.render_time_ms());
}

TEST(TestI420VideoFrame, SizeAllocation) {
  I420VideoFrame frame;
  EXPECT_EQ(0, frame. CreateEmptyFrame(10, 10, 12, 14, 220));
  int height = frame.height();
  int stride_y = frame.stride(kYPlane);
  int stride_u = frame.stride(kUPlane);
  int stride_v = frame.stride(kVPlane);
  // Verify that allocated size was computed correctly.
  EXPECT_EQ(ExpectedSize(stride_y, height, kYPlane),
            frame.allocated_size(kYPlane));
  EXPECT_EQ(ExpectedSize(stride_u, height, kUPlane),
            frame.allocated_size(kUPlane));
  EXPECT_EQ(ExpectedSize(stride_v, height, kVPlane),
            frame.allocated_size(kVPlane));
}

TEST(TestI420VideoFrame, CopyFrame) {
  uint32_t timestamp = 1;
  int64_t ntp_time_ms = 2;
  int64_t render_time_ms = 3;
  int stride_y = 15;
  int stride_u = 10;
  int stride_v = 10;
  int width = 15;
  int height = 15;
  // Copy frame.
  I420VideoFrame small_frame;
  EXPECT_EQ(0, small_frame.CreateEmptyFrame(width, height,
                                            stride_y, stride_u, stride_v));
  small_frame.set_timestamp(timestamp);
  small_frame.set_ntp_time_ms(ntp_time_ms);
  small_frame.set_render_time_ms(render_time_ms);
  const int kSizeY = 400;
  const int kSizeU = 100;
  const int kSizeV = 100;
  const VideoRotation kRotation = kVideoRotation_270;
  uint8_t buffer_y[kSizeY];
  uint8_t buffer_u[kSizeU];
  uint8_t buffer_v[kSizeV];
  memset(buffer_y, 16, kSizeY);
  memset(buffer_u, 8, kSizeU);
  memset(buffer_v, 4, kSizeV);
  I420VideoFrame big_frame;
  EXPECT_EQ(0,
            big_frame.CreateFrame(kSizeY, buffer_y, kSizeU, buffer_u, kSizeV,
                                  buffer_v, width + 5, height + 5, stride_y + 5,
                                  stride_u, stride_v, kRotation));
  // Frame of smaller dimensions.
  EXPECT_EQ(0, small_frame.CopyFrame(big_frame));
  EXPECT_TRUE(EqualFrames(small_frame, big_frame));
  EXPECT_EQ(kRotation, small_frame.rotation());

  // Frame of larger dimensions.
  EXPECT_EQ(0, small_frame.CreateEmptyFrame(width, height,
                                            stride_y, stride_u, stride_v));
  memset(small_frame.buffer(kYPlane), 1, small_frame.allocated_size(kYPlane));
  memset(small_frame.buffer(kUPlane), 2, small_frame.allocated_size(kUPlane));
  memset(small_frame.buffer(kVPlane), 3, small_frame.allocated_size(kVPlane));
  EXPECT_EQ(0, big_frame.CopyFrame(small_frame));
  EXPECT_TRUE(EqualFrames(small_frame, big_frame));
}

TEST(TestI420VideoFrame, Reset) {
  I420VideoFrame frame;
  ASSERT_TRUE(frame.CreateEmptyFrame(5, 5, 5, 5, 5) == 0);
  frame.set_ntp_time_ms(1);
  frame.set_timestamp(2);
  frame.set_render_time_ms(3);
  ASSERT_TRUE(frame.video_frame_buffer() != NULL);

  frame.Reset();
  EXPECT_EQ(0u, frame.ntp_time_ms());
  EXPECT_EQ(0u, frame.render_time_ms());
  EXPECT_EQ(0u, frame.timestamp());
  EXPECT_TRUE(frame.video_frame_buffer() == NULL);
}

TEST(TestI420VideoFrame, CloneFrame) {
  I420VideoFrame frame1;
  rtc::scoped_ptr<I420VideoFrame> frame2;
  const int kSizeY = 400;
  const int kSizeU = 100;
  const int kSizeV = 100;
  uint8_t buffer_y[kSizeY];
  uint8_t buffer_u[kSizeU];
  uint8_t buffer_v[kSizeV];
  memset(buffer_y, 16, kSizeY);
  memset(buffer_u, 8, kSizeU);
  memset(buffer_v, 4, kSizeV);
  frame1.CreateFrame(
      kSizeY, buffer_y, kSizeU, buffer_u, kSizeV, buffer_v, 20, 20, 20, 10, 10);
  frame1.set_timestamp(1);
  frame1.set_ntp_time_ms(2);
  frame1.set_render_time_ms(3);

  frame2.reset(frame1.CloneFrame());
  EXPECT_TRUE(frame2.get() != NULL);
  EXPECT_TRUE(EqualFrames(frame1, *frame2));
}

TEST(TestI420VideoFrame, CopyBuffer) {
  I420VideoFrame frame1, frame2;
  int width = 15;
  int height = 15;
  int stride_y = 15;
  int stride_uv = 10;
  const int kSizeY = 225;
  const int kSizeUv = 80;
  EXPECT_EQ(0, frame2.CreateEmptyFrame(width, height,
                                       stride_y, stride_uv, stride_uv));
  uint8_t buffer_y[kSizeY];
  uint8_t buffer_u[kSizeUv];
  uint8_t buffer_v[kSizeUv];
  memset(buffer_y, 16, kSizeY);
  memset(buffer_u, 8, kSizeUv);
  memset(buffer_v, 4, kSizeUv);
  frame2.CreateFrame(kSizeY, buffer_y,
                     kSizeUv, buffer_u,
                     kSizeUv, buffer_v,
                     width, height, stride_y, stride_uv, stride_uv);
  // Expect exactly the same pixel data.
  EXPECT_TRUE(EqualPlane(buffer_y, frame2.buffer(kYPlane), stride_y, 15, 15));
  EXPECT_TRUE(EqualPlane(buffer_u, frame2.buffer(kUPlane), stride_uv, 8, 8));
  EXPECT_TRUE(EqualPlane(buffer_v, frame2.buffer(kVPlane), stride_uv, 8, 8));

  // Compare size.
  EXPECT_LE(kSizeY, frame2.allocated_size(kYPlane));
  EXPECT_LE(kSizeUv, frame2.allocated_size(kUPlane));
  EXPECT_LE(kSizeUv, frame2.allocated_size(kVPlane));
}

TEST(TestI420VideoFrame, FrameSwap) {
  I420VideoFrame frame1, frame2;
  uint32_t timestamp1 = 1;
  int64_t ntp_time_ms1 = 2;
  int64_t render_time_ms1 = 3;
  int stride_y1 = 15;
  int stride_u1 = 10;
  int stride_v1 = 10;
  int width1 = 15;
  int height1 = 15;
  const int kSizeY1 = 225;
  const int kSizeU1 = 80;
  const int kSizeV1 = 80;
  uint32_t timestamp2 = 4;
  int64_t ntp_time_ms2 = 5;
  int64_t render_time_ms2 = 6;
  int stride_y2 = 30;
  int stride_u2 = 20;
  int stride_v2 = 20;
  int width2 = 30;
  int height2 = 30;
  const int kSizeY2 = 900;
  const int kSizeU2 = 300;
  const int kSizeV2 = 300;
  // Initialize frame1 values.
  EXPECT_EQ(0, frame1.CreateEmptyFrame(width1, height1,
                                       stride_y1, stride_u1, stride_v1));
  frame1.set_timestamp(timestamp1);
  frame1.set_ntp_time_ms(ntp_time_ms1);
  frame1.set_render_time_ms(render_time_ms1);
  // Set memory for frame1.
  uint8_t buffer_y1[kSizeY1];
  uint8_t buffer_u1[kSizeU1];
  uint8_t buffer_v1[kSizeV1];
  memset(buffer_y1, 2, kSizeY1);
  memset(buffer_u1, 4, kSizeU1);
  memset(buffer_v1, 8, kSizeV1);
  frame1.CreateFrame(kSizeY1, buffer_y1,
                     kSizeU1, buffer_u1,
                     kSizeV1, buffer_v1,
                     width1, height1, stride_y1, stride_u1, stride_v1);
  // Initialize frame2 values.
  EXPECT_EQ(0, frame2.CreateEmptyFrame(width2, height2,
                                       stride_y2, stride_u2, stride_v2));
  frame2.set_timestamp(timestamp2);
  frame1.set_ntp_time_ms(ntp_time_ms2);
  frame2.set_render_time_ms(render_time_ms2);
  // Set memory for frame2.
  uint8_t buffer_y2[kSizeY2];
  uint8_t buffer_u2[kSizeU2];
  uint8_t buffer_v2[kSizeV2];
  memset(buffer_y2, 0, kSizeY2);
  memset(buffer_u2, 1, kSizeU2);
  memset(buffer_v2, 2, kSizeV2);
  frame2.CreateFrame(kSizeY2, buffer_y2,
                     kSizeU2, buffer_u2,
                     kSizeV2, buffer_v2,
                     width2, height2, stride_y2, stride_u2, stride_v2);
  // Copy frames for subsequent comparison.
  I420VideoFrame frame1_copy, frame2_copy;
  frame1_copy.CopyFrame(frame1);
  frame2_copy.CopyFrame(frame2);
  // Swap frames.
  frame1.SwapFrame(&frame2);
  // Verify swap.
  EXPECT_TRUE(EqualFrames(frame1_copy, frame2));
  EXPECT_TRUE(EqualFrames(frame2_copy, frame1));
}

TEST(TestI420VideoFrame, ReuseAllocation) {
  I420VideoFrame frame;
  frame.CreateEmptyFrame(640, 320, 640, 320, 320);
  const uint8_t* y = frame.buffer(kYPlane);
  const uint8_t* u = frame.buffer(kUPlane);
  const uint8_t* v = frame.buffer(kVPlane);
  frame.CreateEmptyFrame(640, 320, 640, 320, 320);
  EXPECT_EQ(y, frame.buffer(kYPlane));
  EXPECT_EQ(u, frame.buffer(kUPlane));
  EXPECT_EQ(v, frame.buffer(kVPlane));
}

TEST(TestI420VideoFrame, FailToReuseAllocation) {
  I420VideoFrame frame1;
  frame1.CreateEmptyFrame(640, 320, 640, 320, 320);
  const uint8_t* y = frame1.buffer(kYPlane);
  const uint8_t* u = frame1.buffer(kUPlane);
  const uint8_t* v = frame1.buffer(kVPlane);
  // Make a shallow copy of |frame1|.
  I420VideoFrame frame2(frame1.video_frame_buffer(), 0, 0, kVideoRotation_0);
  frame1.CreateEmptyFrame(640, 320, 640, 320, 320);
  EXPECT_NE(y, frame1.buffer(kYPlane));
  EXPECT_NE(u, frame1.buffer(kUPlane));
  EXPECT_NE(v, frame1.buffer(kVPlane));
}

TEST(TestI420VideoFrame, TextureInitialValues) {
  NativeHandleImpl handle;
  I420VideoFrame frame(&handle, 640, 480, 100, 10);
  EXPECT_EQ(640, frame.width());
  EXPECT_EQ(480, frame.height());
  EXPECT_EQ(100u, frame.timestamp());
  EXPECT_EQ(10, frame.render_time_ms());
  EXPECT_EQ(&handle, frame.native_handle());

  frame.set_timestamp(200);
  EXPECT_EQ(200u, frame.timestamp());
  frame.set_render_time_ms(20);
  EXPECT_EQ(20, frame.render_time_ms());
}

TEST(TestI420VideoFrame, RefCount) {
  NativeHandleImpl handle;
  EXPECT_EQ(0, handle.ref_count());
  I420VideoFrame *frame = new I420VideoFrame(&handle, 640, 480, 100, 200);
  EXPECT_EQ(1, handle.ref_count());
  delete frame;
  EXPECT_EQ(0, handle.ref_count());
}

TEST(TestI420VideoFrame, CloneTextureFrame) {
  NativeHandleImpl handle;
  I420VideoFrame frame1(&handle, 640, 480, 100, 200);
  rtc::scoped_ptr<I420VideoFrame> frame2(frame1.CloneFrame());
  EXPECT_TRUE(frame2.get() != NULL);
  EXPECT_TRUE(EqualTextureFrames(frame1, *frame2));
}

bool EqualPlane(const uint8_t* data1,
                const uint8_t* data2,
                int stride,
                int width,
                int height) {
  for (int y = 0; y < height; ++y) {
    if (memcmp(data1, data2, width) != 0)
      return false;
    data1 += stride;
    data2 += stride;
  }
  return true;
}

bool EqualFrames(const I420VideoFrame& frame1, const I420VideoFrame& frame2) {
  if ((frame1.width() != frame2.width()) ||
      (frame1.height() != frame2.height()) ||
      (frame1.stride(kYPlane) != frame2.stride(kYPlane)) ||
      (frame1.stride(kUPlane) != frame2.stride(kUPlane)) ||
      (frame1.stride(kVPlane) != frame2.stride(kVPlane)) ||
      (frame1.timestamp() != frame2.timestamp()) ||
      (frame1.ntp_time_ms() != frame2.ntp_time_ms()) ||
      (frame1.render_time_ms() != frame2.render_time_ms())) {
    return false;
  }
  const int half_width = (frame1.width() + 1) / 2;
  const int half_height = (frame1.height() + 1) / 2;
  return EqualPlane(frame1.buffer(kYPlane), frame2.buffer(kYPlane),
                    frame1.stride(kYPlane), frame1.width(), frame1.height()) &&
         EqualPlane(frame1.buffer(kUPlane), frame2.buffer(kUPlane),
                    frame1.stride(kUPlane), half_width, half_height) &&
         EqualPlane(frame1.buffer(kVPlane), frame2.buffer(kVPlane),
                    frame1.stride(kVPlane), half_width, half_height);
}

bool EqualTextureFrames(const I420VideoFrame& frame1,
                        const I420VideoFrame& frame2) {
  return ((frame1.native_handle() == frame2.native_handle()) &&
          (frame1.width() == frame2.width()) &&
          (frame1.height() == frame2.height()) &&
          (frame1.timestamp() == frame2.timestamp()) &&
          (frame1.render_time_ms() == frame2.render_time_ms()));
}

int ExpectedSize(int plane_stride, int image_height, PlaneType type) {
  if (type == kYPlane) {
    return (plane_stride * image_height);
  } else {
    int half_height = (image_height + 1) / 2;
    return (plane_stride * half_height);
  }
}

}  // namespace webrtc
