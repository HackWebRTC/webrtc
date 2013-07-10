/*
 * libjingle
 * Copyright 2004 Google Inc.
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

#include <stdio.h>

#include <string>
#include <vector>

#include "talk/base/gunit.h"
#include "talk/base/logging.h"
#include "talk/base/thread.h"
#include "talk/media/base/testutils.h"
#include "talk/media/devices/filevideocapturer.h"

namespace {

class FileVideoCapturerTest : public testing::Test {
 public:
  virtual void SetUp() {
    capturer_.reset(new cricket::FileVideoCapturer);
  }

  bool OpenFile(const std::string& filename) {
    return capturer_->Init(cricket::GetTestFilePath(filename));
  }

 protected:
  class VideoCapturerListener : public sigslot::has_slots<> {
   public:
    VideoCapturerListener()
        : frame_count_(0),
          frame_width_(0),
          frame_height_(0),
          resolution_changed_(false) {
    }

    void OnFrameCaptured(cricket::VideoCapturer* capturer,
                         const cricket::CapturedFrame* frame) {
      ++frame_count_;
      if (1 == frame_count_) {
        frame_width_ = frame->width;
        frame_height_ = frame->height;
      } else if (frame_width_ != frame->width ||
          frame_height_ != frame->height) {
        resolution_changed_ = true;
      }
    }

    int frame_count() const { return frame_count_; }
    int frame_width() const { return frame_width_; }
    int frame_height() const { return frame_height_; }
    bool resolution_changed() const { return resolution_changed_; }

   private:
    int frame_count_;
    int frame_width_;
    int frame_height_;
    bool resolution_changed_;
  };

  talk_base::scoped_ptr<cricket::FileVideoCapturer> capturer_;
  cricket::VideoFormat capture_format_;
};

TEST_F(FileVideoCapturerTest, TestNotOpened) {
  EXPECT_EQ("", capturer_->GetId());
  EXPECT_TRUE(capturer_->GetSupportedFormats()->empty());
  EXPECT_EQ(NULL, capturer_->GetCaptureFormat());
  EXPECT_FALSE(capturer_->IsRunning());
}

TEST_F(FileVideoCapturerTest, TestInvalidOpen) {
  EXPECT_FALSE(OpenFile("NotmeNotme"));
}

TEST_F(FileVideoCapturerTest, TestOpen) {
  EXPECT_TRUE(OpenFile("captured-320x240-2s-48.frames"));
  EXPECT_NE("", capturer_->GetId());
  EXPECT_TRUE(NULL != capturer_->GetSupportedFormats());
  EXPECT_EQ(1U, capturer_->GetSupportedFormats()->size());
  EXPECT_EQ(NULL, capturer_->GetCaptureFormat());  // not started yet
  EXPECT_FALSE(capturer_->IsRunning());
}

TEST_F(FileVideoCapturerTest, TestLargeSmallDesiredFormat) {
  EXPECT_TRUE(OpenFile("captured-320x240-2s-48.frames"));
  // desired format with large resolution.
  cricket::VideoFormat desired(
      3200, 2400, cricket::VideoFormat::FpsToInterval(30), cricket::FOURCC_ANY);
  EXPECT_TRUE(capturer_->GetBestCaptureFormat(desired, &capture_format_));
  EXPECT_EQ(320, capture_format_.width);
  EXPECT_EQ(240, capture_format_.height);

  // Desired format with small resolution.
  desired.width = 0;
  desired.height = 0;
  EXPECT_TRUE(capturer_->GetBestCaptureFormat(desired, &capture_format_));
  EXPECT_EQ(320, capture_format_.width);
  EXPECT_EQ(240, capture_format_.height);
}

TEST_F(FileVideoCapturerTest, TestSupportedAsDesiredFormat) {
  EXPECT_TRUE(OpenFile("captured-320x240-2s-48.frames"));
  // desired format same as the capture format supported by the file
  cricket::VideoFormat desired = capturer_->GetSupportedFormats()->at(0);
  EXPECT_TRUE(capturer_->GetBestCaptureFormat(desired, &capture_format_));
  EXPECT_TRUE(desired == capture_format_);

  // desired format same as the supported capture format except the fourcc
  desired.fourcc = cricket::FOURCC_ANY;
  EXPECT_TRUE(capturer_->GetBestCaptureFormat(desired, &capture_format_));
  EXPECT_NE(capture_format_.fourcc, desired.fourcc);

  // desired format with minimum interval
  desired.interval = cricket::VideoFormat::kMinimumInterval;
  EXPECT_TRUE(capturer_->GetBestCaptureFormat(desired, &capture_format_));
}

TEST_F(FileVideoCapturerTest, TestNoRepeat) {
  EXPECT_TRUE(OpenFile("captured-320x240-2s-48.frames"));
  VideoCapturerListener listener;
  capturer_->SignalFrameCaptured.connect(
      &listener, &VideoCapturerListener::OnFrameCaptured);
  capturer_->set_repeat(0);
  capture_format_ = capturer_->GetSupportedFormats()->at(0);
  EXPECT_EQ(cricket::CS_RUNNING, capturer_->Start(capture_format_));
  EXPECT_TRUE_WAIT(!capturer_->IsRunning(), 20000);
  EXPECT_EQ(48, listener.frame_count());
}

TEST_F(FileVideoCapturerTest, TestRepeatForever) {
  // Start the capturer_ with 50 fps and read no less than 150 frames.
  EXPECT_TRUE(OpenFile("captured-320x240-2s-48.frames"));
  VideoCapturerListener listener;
  capturer_->SignalFrameCaptured.connect(
      &listener, &VideoCapturerListener::OnFrameCaptured);
  capturer_->set_repeat(talk_base::kForever);
  capture_format_ = capturer_->GetSupportedFormats()->at(0);
  capture_format_.interval = cricket::VideoFormat::FpsToInterval(50);
  EXPECT_EQ(cricket::CS_RUNNING, capturer_->Start(capture_format_));
  EXPECT_TRUE(NULL != capturer_->GetCaptureFormat());
  EXPECT_TRUE(capture_format_ == *capturer_->GetCaptureFormat());
  EXPECT_TRUE_WAIT(!capturer_->IsRunning() ||
                   listener.frame_count() >= 150, 20000);
  capturer_->Stop();
  EXPECT_FALSE(capturer_->IsRunning());
  EXPECT_GE(listener.frame_count(), 150);
  EXPECT_FALSE(listener.resolution_changed());
  EXPECT_EQ(listener.frame_width(), capture_format_.width);
  EXPECT_EQ(listener.frame_height(), capture_format_.height);
}

TEST_F(FileVideoCapturerTest, TestPartialFrameHeader) {
  EXPECT_TRUE(OpenFile("1.frame_plus_1.byte"));
  VideoCapturerListener listener;
  capturer_->SignalFrameCaptured.connect(
      &listener, &VideoCapturerListener::OnFrameCaptured);
  capturer_->set_repeat(0);
  capture_format_ = capturer_->GetSupportedFormats()->at(0);
  EXPECT_EQ(cricket::CS_RUNNING, capturer_->Start(capture_format_));
  EXPECT_TRUE_WAIT(!capturer_->IsRunning(), 1000);
  EXPECT_EQ(1, listener.frame_count());
}

TEST_F(FileVideoCapturerTest, TestFileDevices) {
  cricket::Device not_a_file("I'm a camera", "with an id");
  EXPECT_FALSE(
      cricket::FileVideoCapturer::IsFileVideoCapturerDevice(not_a_file));
  const std::string test_file =
      cricket::GetTestFilePath("captured-320x240-2s-48.frames");
  cricket::Device file_device =
      cricket::FileVideoCapturer::CreateFileVideoCapturerDevice(test_file);
  EXPECT_TRUE(
      cricket::FileVideoCapturer::IsFileVideoCapturerDevice(file_device));
  EXPECT_TRUE(capturer_->Init(file_device));
  EXPECT_EQ(file_device.id, capturer_->GetId());
}

}  // unnamed namespace
