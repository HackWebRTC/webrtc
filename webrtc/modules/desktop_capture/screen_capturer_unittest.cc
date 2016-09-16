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

    // Draw a set of |kRectSize| by |kRectSize| rectangles at (|i|, |i|). One of
    // (controlled by |c|) its primary colors is |i|, and the other two are
    // 0xff. So we won't draw a white rectangle.
    for (int c = 0; c < 3; c++) {
      for (int i = 0; i < kTestArea - kRectSize; i += 16) {
        DesktopRect rect = DesktopRect::MakeXYWH(i, i, kRectSize, kRectSize);
        rect.Translate(drawer->DrawableRegion().top_left());
        RgbaColor color((c == 0 ? (i & 0xff) : 0x7f),
                        (c == 1 ? (i & 0xff) : 0x7f),
                        (c == 2 ? (i & 0xff) : 0x7f));
        drawer->Clear();
        drawer->DrawRectangle(rect, color);
        TestCaptureOneFrame(capturers, drawer.get(), rect, color);
      }
    }
  }

  void TestCaptureUpdatedRegion() {
    TestCaptureUpdatedRegion({capturer_.get()});
  }

#if defined(WEBRTC_WIN)
  // Enable allow_directx_capturer in DesktopCaptureOptions, but let
  // ScreenCapturer::Create to decide whether a DirectX capturer should be used.
  void MaybeCreateDirectxCapturer() {
    DesktopCaptureOptions options(DesktopCaptureOptions::CreateDefault());
    options.set_allow_directx_capturer(true);
    capturer_.reset(ScreenCapturer::Create(options));
  }

  bool CreateDirectxCapturer() {
    if (!ScreenCapturerWinDirectx::IsSupported()) {
      LOG(LS_WARNING) << "Directx capturer is not supported";
      return false;
    }

    MaybeCreateDirectxCapturer();
    return true;
  }

  void CreateMagnifierCapturer() {
    DesktopCaptureOptions options(DesktopCaptureOptions::CreateDefault());
    options.set_allow_use_magnification_api(true);
    capturer_.reset(ScreenCapturer::Create(options));
  }
#endif  // defined(WEBRTC_WIN)

  std::unique_ptr<ScreenCapturer> capturer_;
  MockScreenCapturerCallback callback_;

 private:
  // Repeats capturing the frame by using |capturers| one-by-one for 600 times,
  // typically 30 seconds, until they succeeded captured a |color| rectangle at
  // |rect|. This function uses |drawer|->WaitForPendingDraws() between two
  // attempts to wait for the screen to update.
  void TestCaptureOneFrame(std::vector<ScreenCapturer*> capturers,
                           ScreenDrawer* drawer,
                           DesktopRect rect,
                           RgbaColor color) {
    size_t succeeded_capturers = 0;
    const int wait_capture_round = 600;
    for (int i = 0; i < wait_capture_round; i++) {
      drawer->WaitForPendingDraws();
      for (size_t j = 0; j < capturers.size(); j++) {
        if (capturers[j] == nullptr) {
          // ScreenCapturer should return an empty updated_region() if no
          // update detected. So we won't test it again if it has captured
          // the rectangle we drew.
          continue;
        }
        std::unique_ptr<DesktopFrame> frame = CaptureFrame(capturers[j]);
        if (!frame) {
          // CaptureFrame() has triggered an assertion failure already, we
          // only need to return here.
          return;
        }

        if (ArePixelsColoredBy(*frame, rect, color)) {
          capturers[j] = nullptr;
          succeeded_capturers++;
        }
      }

      if (succeeded_capturers == capturers.size()) {
        break;
      }
    }

    ASSERT_EQ(succeeded_capturers, capturers.size());
  }

  // Expects |capturer| to successfully capture a frame, and returns it.
  std::unique_ptr<DesktopFrame> CaptureFrame(ScreenCapturer* capturer) {
    std::unique_ptr<DesktopFrame> frame;
    EXPECT_CALL(callback_,
                OnCaptureResultPtr(DesktopCapturer::Result::SUCCESS, _))
        .WillOnce(SaveUniquePtrArg(&frame));
    capturer->Capture(DesktopRegion());
    EXPECT_TRUE(frame);
    return frame;
  }
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

// Disabled due to being flaky due to the fact that it useds rendering / UI,
// see webrtc/6366.
TEST_F(ScreenCapturerTest, DISABLED_CaptureUpdatedRegion) {
  TestCaptureUpdatedRegion();
}

// Disabled due to being flaky due to the fact that it useds rendering / UI,
// see webrtc/6366.
// TODO(zijiehe): Find out the reason of failure of this test on trybot.
TEST_F(ScreenCapturerTest, DISABLED_TwoCapturers) {
  std::unique_ptr<ScreenCapturer> capturer2 = std::move(capturer_);
  SetUp();
  TestCaptureUpdatedRegion({capturer_.get(), capturer2.get()});
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
  CreateMagnifierCapturer();

  std::unique_ptr<DesktopFrame> frame;
  EXPECT_CALL(callback_,
              OnCaptureResultPtr(DesktopCapturer::Result::SUCCESS, _))
      .WillOnce(SaveUniquePtrArg(&frame));

  capturer_->Start(&callback_);
  capturer_->Capture(DesktopRegion());
  ASSERT_TRUE(frame);
}

TEST_F(ScreenCapturerTest, UseDirectxCapturer) {
  if (!CreateDirectxCapturer()) {
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
  if (!CreateDirectxCapturer()) {
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

// Disabled due to being flaky due to the fact that it useds rendering / UI,
// see webrtc/6366.
TEST_F(ScreenCapturerTest, DISABLED_CaptureUpdatedRegionWithDirectxCapturer) {
  if (!CreateDirectxCapturer()) {
    return;
  }

  TestCaptureUpdatedRegion();
}

// Disabled due to being flaky due to the fact that it useds rendering / UI,
// see webrtc/6366.
TEST_F(ScreenCapturerTest, DISABLED_TwoDirectxCapturers) {
  if (!CreateDirectxCapturer()) {
    return;
  }

  std::unique_ptr<ScreenCapturer> capturer2 = std::move(capturer_);
  RTC_CHECK(CreateDirectxCapturer());
  TestCaptureUpdatedRegion({capturer_.get(), capturer2.get()});
}

// Disabled due to being flaky due to the fact that it useds rendering / UI,
// see webrtc/6366.
TEST_F(ScreenCapturerTest, DISABLED_TwoMagnifierCapturers) {
  CreateMagnifierCapturer();
  std::unique_ptr<ScreenCapturer> capturer2 = std::move(capturer_);
  CreateMagnifierCapturer();
  TestCaptureUpdatedRegion({capturer_.get(), capturer2.get()});
}

// Disabled due to being flaky due to the fact that it useds rendering / UI,
// see webrtc/6366.
TEST_F(ScreenCapturerTest,
       DISABLED_MaybeCaptureUpdatedRegionWithDirectxCapturer) {
  // Even DirectX capturer is not supported in current system, we should be able
  // to select a usable capturer.
  MaybeCreateDirectxCapturer();
  TestCaptureUpdatedRegion();
}

#endif  // defined(WEBRTC_WIN)

}  // namespace webrtc
