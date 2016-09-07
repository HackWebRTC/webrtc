/*
 *  Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <string.h>

#include <algorithm>
#include <initializer_list>
#include <memory>
#include <utility>

#include "webrtc/modules/desktop_capture/screen_capturer.h"

#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "webrtc/base/checks.h"
#include "webrtc/base/constructormagic.h"
#include "webrtc/base/logging.h"
#include "webrtc/modules/desktop_capture/rgba_color.h"
#include "webrtc/modules/desktop_capture/desktop_capture_options.h"
#include "webrtc/modules/desktop_capture/desktop_frame.h"
#include "webrtc/modules/desktop_capture/desktop_region.h"
#include "webrtc/modules/desktop_capture/screen_capturer_mock_objects.h"
#include "webrtc/modules/desktop_capture/screen_drawer.h"
#include "webrtc/system_wrappers/include/sleep.h"

#if defined(WEBRTC_WIN)
#include "webrtc/modules/desktop_capture/win/screen_capturer_win_directx.h"
#endif  // defined(WEBRTC_WIN)

using ::testing::_;
using ::testing::AnyNumber;
using ::testing::Return;

const int kTestSharedMemoryId = 123;

namespace webrtc {

namespace {

ACTION_P(SaveUniquePtrArg, dest) {
  *dest = std::move(*arg1);
}

// Expects |capturer| to successfully capture a frame, and returns it.
std::unique_ptr<DesktopFrame> CaptureFrame(
    ScreenCapturer* capturer,
    MockScreenCapturerCallback* callback) {
  std::unique_ptr<DesktopFrame> frame;
  EXPECT_CALL(*callback,
              OnCaptureResultPtr(DesktopCapturer::Result::SUCCESS, _))
      .WillOnce(SaveUniquePtrArg(&frame));
  capturer->Capture(DesktopRegion());
  EXPECT_TRUE(frame);
  return frame;
}

// Returns true if color in |rect| of |frame| is |color|.
bool ArePixelsColoredBy(const DesktopFrame& frame,
                        DesktopRect rect,
                        RgbaColor color) {
  // updated_region() should cover the painted area.
  DesktopRegion updated_region(frame.updated_region());
  updated_region.IntersectWith(rect);
  if (!updated_region.Equals(DesktopRegion(rect))) {
    return false;
  }

  // Color in the |rect| should be |color|.
  uint8_t* row = frame.GetFrameDataAtPos(rect.top_left());
  for (int i = 0; i < rect.height(); i++) {
    uint8_t* column = row;
    for (int j = 0; j < rect.width(); j++) {
      if (color != RgbaColor(column)) {
        return false;
      }
      column += DesktopFrame::kBytesPerPixel;
    }
    row += frame.stride();
  }
  return true;
}

}  // namespace

class ScreenCapturerTest : public testing::Test {
 public:
  void SetUp() override {
    capturer_.reset(
        ScreenCapturer::Create(DesktopCaptureOptions::CreateDefault()));
  }

 protected:
  void TestCaptureUpdatedRegion(
      std::initializer_list<ScreenCapturer*> capturers) {
    RTC_DCHECK(capturers.size() > 0);
    // A large enough area for the tests, which should be able to fulfill by
    // most of systems.
    const int kTestArea = 512;
    const int kRectSize = 32;
    std::unique_ptr<ScreenDrawer> drawer = ScreenDrawer::Create();
    if (!drawer || drawer->DrawableRegion().is_empty()) {
      LOG(LS_WARNING) << "No ScreenDrawer implementation for current platform.";
      return;
    }
    if (drawer->DrawableRegion().width() < kTestArea ||
        drawer->DrawableRegion().height() < kTestArea) {
      LOG(LS_WARNING) << "ScreenDrawer::DrawableRegion() is too small for the "
                         "CaptureUpdatedRegion tests.";
      return;
    }

    for (ScreenCapturer* capturer : capturers) {
      capturer->Start(&callback_);
    }

    for (int c = 0; c < 3; c++) {
      for (int i = 0; i < kTestArea - kRectSize; i += 16) {
        DesktopRect rect = DesktopRect::MakeXYWH(i, i, kRectSize, kRectSize);
        rect.Translate(drawer->DrawableRegion().top_left());
        RgbaColor color((c == 0 ? (i & 0xff) : 0x7f),
                        (c == 1 ? (i & 0xff) : 0x7f),
                        (c == 2 ? (i & 0xff) : 0x7f));
        drawer->Clear();
        drawer->DrawRectangle(rect, color);

        const int wait_first_capture_round = 20;
        for (int j = 0; j < wait_first_capture_round; j++) {
          drawer->WaitForPendingDraws();
          std::unique_ptr<DesktopFrame> frame =
              CaptureFrame(*capturers.begin(), &callback_);
          if (!frame) {
            return;
          }

          if (ArePixelsColoredBy(*frame, rect, color)) {
            // The first capturer successfully captured the frame we expected.
            // So the others should also be able to capture it.
            break;
          } else {
            ASSERT_LT(j, wait_first_capture_round);
          }
        }

        for (ScreenCapturer* capturer : capturers) {
          if (capturer == *capturers.begin()) {
            // TODO(zijiehe): ScreenCapturerX11 and ScreenCapturerWinGdi cannot
            // capture a correct frame again if screen does not update.
            continue;
          }
          std::unique_ptr<DesktopFrame> frame =
              CaptureFrame(capturer, &callback_);
          if (!frame) {
            return;
          }

          ASSERT_TRUE(ArePixelsColoredBy(*frame, rect, color));
        }
      }
    }
  }

  void TestCaptureUpdatedRegion() {
    TestCaptureUpdatedRegion({capturer_.get()});
  }

#if defined(WEBRTC_WIN)
  bool SetDirectxCapturerMode() {
    if (!ScreenCapturerWinDirectx::IsSupported()) {
      LOG(LS_WARNING) << "Directx capturer is not supported";
      return false;
    }

    DesktopCaptureOptions options(DesktopCaptureOptions::CreateDefault());
    options.set_allow_directx_capturer(true);
    capturer_.reset(ScreenCapturer::Create(options));
    return true;
  }
#endif  // defined(WEBRTC_WIN)

  std::unique_ptr<ScreenCapturer> capturer_;
  MockScreenCapturerCallback callback_;
};

class FakeSharedMemory : public SharedMemory {
 public:
  FakeSharedMemory(char* buffer, size_t size)
    : SharedMemory(buffer, size, 0, kTestSharedMemoryId),
      buffer_(buffer) {
  }
  virtual ~FakeSharedMemory() {
    delete[] buffer_;
  }
 private:
  char* buffer_;
  RTC_DISALLOW_COPY_AND_ASSIGN(FakeSharedMemory);
};

class FakeSharedMemoryFactory : public SharedMemoryFactory {
 public:
  FakeSharedMemoryFactory() {}
  ~FakeSharedMemoryFactory() override {}

  std::unique_ptr<SharedMemory> CreateSharedMemory(size_t size) override {
    return std::unique_ptr<SharedMemory>(
        new FakeSharedMemory(new char[size], size));
  }

 private:
  RTC_DISALLOW_COPY_AND_ASSIGN(FakeSharedMemoryFactory);
};

TEST_F(ScreenCapturerTest, GetScreenListAndSelectScreen) {
  webrtc::ScreenCapturer::ScreenList screens;
  EXPECT_TRUE(capturer_->GetScreenList(&screens));
  for (webrtc::ScreenCapturer::ScreenList::iterator it = screens.begin();
       it != screens.end(); ++it) {
    EXPECT_TRUE(capturer_->SelectScreen(it->id));
  }
}

TEST_F(ScreenCapturerTest, StartCapturer) {
  capturer_->Start(&callback_);
}

TEST_F(ScreenCapturerTest, Capture) {
  // Assume that Start() treats the screen as invalid initially.
  std::unique_ptr<DesktopFrame> frame;
  EXPECT_CALL(callback_,
              OnCaptureResultPtr(DesktopCapturer::Result::SUCCESS, _))
      .WillOnce(SaveUniquePtrArg(&frame));

  capturer_->Start(&callback_);
  capturer_->Capture(DesktopRegion());

  ASSERT_TRUE(frame);
  EXPECT_GT(frame->size().width(), 0);
  EXPECT_GT(frame->size().height(), 0);
  EXPECT_GE(frame->stride(),
            frame->size().width() * DesktopFrame::kBytesPerPixel);
  EXPECT_TRUE(frame->shared_memory() == NULL);

  // Verify that the region contains whole screen.
  EXPECT_FALSE(frame->updated_region().is_empty());
  DesktopRegion::Iterator it(frame->updated_region());
  ASSERT_TRUE(!it.IsAtEnd());
  EXPECT_TRUE(it.rect().equals(DesktopRect::MakeSize(frame->size())));
  it.Advance();
  EXPECT_TRUE(it.IsAtEnd());
}

TEST_F(ScreenCapturerTest, CaptureUpdatedRegion) {
  TestCaptureUpdatedRegion();
}

#if defined(WEBRTC_WIN)

TEST_F(ScreenCapturerTest, UseSharedBuffers) {
  std::unique_ptr<DesktopFrame> frame;
  EXPECT_CALL(callback_,
              OnCaptureResultPtr(DesktopCapturer::Result::SUCCESS, _))
      .WillOnce(SaveUniquePtrArg(&frame));

  capturer_->Start(&callback_);
  capturer_->SetSharedMemoryFactory(
      std::unique_ptr<SharedMemoryFactory>(new FakeSharedMemoryFactory()));
  capturer_->Capture(DesktopRegion());

  ASSERT_TRUE(frame);
  ASSERT_TRUE(frame->shared_memory());
  EXPECT_EQ(frame->shared_memory()->id(), kTestSharedMemoryId);
}

TEST_F(ScreenCapturerTest, UseMagnifier) {
  DesktopCaptureOptions options(DesktopCaptureOptions::CreateDefault());
  options.set_allow_use_magnification_api(true);
  capturer_.reset(ScreenCapturer::Create(options));

  std::unique_ptr<DesktopFrame> frame;
  EXPECT_CALL(callback_,
              OnCaptureResultPtr(DesktopCapturer::Result::SUCCESS, _))
      .WillOnce(SaveUniquePtrArg(&frame));

  capturer_->Start(&callback_);
  capturer_->Capture(DesktopRegion());
  ASSERT_TRUE(frame);
}

TEST_F(ScreenCapturerTest, UseDirectxCapturer) {
  if (!SetDirectxCapturerMode()) {
    return;
  }

  std::unique_ptr<DesktopFrame> frame;
  EXPECT_CALL(callback_,
              OnCaptureResultPtr(DesktopCapturer::Result::SUCCESS, _))
      .WillOnce(SaveUniquePtrArg(&frame));

  capturer_->Start(&callback_);
  capturer_->Capture(DesktopRegion());
  ASSERT_TRUE(frame);
}

TEST_F(ScreenCapturerTest, UseDirectxCapturerWithSharedBuffers) {
  if (!SetDirectxCapturerMode()) {
    return;
  }

  std::unique_ptr<DesktopFrame> frame;
  EXPECT_CALL(callback_,
              OnCaptureResultPtr(DesktopCapturer::Result::SUCCESS, _))
      .WillOnce(SaveUniquePtrArg(&frame));

  capturer_->Start(&callback_);
  capturer_->SetSharedMemoryFactory(
      std::unique_ptr<SharedMemoryFactory>(new FakeSharedMemoryFactory()));
  capturer_->Capture(DesktopRegion());
  ASSERT_TRUE(frame);
  ASSERT_TRUE(frame->shared_memory());
  EXPECT_EQ(frame->shared_memory()->id(), kTestSharedMemoryId);
}

TEST_F(ScreenCapturerTest, CaptureUpdatedRegionWithDirectxCapturer) {
  if (!SetDirectxCapturerMode()) {
    return;
  }

  TestCaptureUpdatedRegion();
}

TEST_F(ScreenCapturerTest, TwoDirectxCapturers) {
  if (!SetDirectxCapturerMode()) {
    return;
  }

  std::unique_ptr<ScreenCapturer> capturer2(capturer_.release());
  RTC_CHECK(SetDirectxCapturerMode());
  TestCaptureUpdatedRegion({capturer_.get(), capturer2.get()});
}

#endif  // defined(WEBRTC_WIN)

}  // namespace webrtc
