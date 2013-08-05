/*
 * libjingle
 * Copyright 2013 Google Inc.
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

#include "talk/media/webrtc/webrtctexturevideoframe.h"

#include "talk/base/gunit.h"
#include "talk/media/base/videocommon.h"

class NativeHandleImpl : public webrtc::NativeHandle {
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

TEST(WebRtcTextureVideoFrameTest, InitialValues) {
  NativeHandleImpl handle;
  cricket::WebRtcTextureVideoFrame frame(&handle, 640, 480, 100, 200);
  EXPECT_EQ(&handle, frame.GetNativeHandle());
  EXPECT_EQ(640u, frame.GetWidth());
  EXPECT_EQ(480u, frame.GetHeight());
  EXPECT_EQ(100, frame.GetElapsedTime());
  EXPECT_EQ(200, frame.GetTimeStamp());
  frame.SetElapsedTime(300);
  EXPECT_EQ(300, frame.GetElapsedTime());
  frame.SetTimeStamp(400);
  EXPECT_EQ(400, frame.GetTimeStamp());
}

TEST(WebRtcTextureVideoFrameTest, CopyFrame) {
  NativeHandleImpl handle;
  cricket::WebRtcTextureVideoFrame frame1(&handle, 640, 480, 100, 200);
  cricket::VideoFrame* frame2 = frame1.Copy();
  EXPECT_EQ(frame1.GetNativeHandle(), frame2->GetNativeHandle());
  EXPECT_EQ(frame1.GetWidth(), frame2->GetWidth());
  EXPECT_EQ(frame1.GetHeight(), frame2->GetHeight());
  EXPECT_EQ(frame1.GetElapsedTime(), frame2->GetElapsedTime());
  EXPECT_EQ(frame1.GetTimeStamp(), frame2->GetTimeStamp());
  delete frame2;
}

TEST(WebRtcTextureVideoFrameTest, RefCount) {
  NativeHandleImpl handle;
  EXPECT_EQ(0, handle.ref_count());
  cricket::WebRtcTextureVideoFrame* frame1 =
      new cricket::WebRtcTextureVideoFrame(&handle, 640, 480, 100, 200);
  EXPECT_EQ(1, handle.ref_count());
  cricket::VideoFrame* frame2 = frame1->Copy();
  EXPECT_EQ(2, handle.ref_count());
  delete frame2;
  EXPECT_EQ(1, handle.ref_count());
  delete frame1;
  EXPECT_EQ(0, handle.ref_count());
}
