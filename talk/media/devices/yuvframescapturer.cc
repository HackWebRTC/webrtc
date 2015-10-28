/*
 * libjingle
 * Copyright 2004--2014 Google Inc.
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

#include "talk/media/devices/yuvframescapturer.h"

#include "webrtc/base/bytebuffer.h"
#include "webrtc/base/criticalsection.h"
#include "webrtc/base/logging.h"
#include "webrtc/base/thread.h"

#include "webrtc/system_wrappers/include/clock.h"

namespace cricket {
///////////////////////////////////////////////////////////////////////
// Definition of private class YuvFramesThread that periodically generates
// frames.
///////////////////////////////////////////////////////////////////////
class YuvFramesCapturer::YuvFramesThread
    : public rtc::Thread, public rtc::MessageHandler {
 public:
  explicit YuvFramesThread(YuvFramesCapturer* capturer)
      : capturer_(capturer),
        finished_(false) {
  }

  virtual ~YuvFramesThread() {
    Stop();
  }

  // Override virtual method of parent Thread. Context: Worker Thread.
  virtual void Run() {
    // Read the first frame and start the message pump. The pump runs until
    // Stop() is called externally or Quit() is called by OnMessage().
    int waiting_time_ms = 0;
    if (capturer_) {
      capturer_->ReadFrame(true);
      PostDelayed(waiting_time_ms, this);
      Thread::Run();
    }

    rtc::CritScope cs(&crit_);
    finished_ = true;
  }

  // Override virtual method of parent MessageHandler. Context: Worker Thread.
  virtual void OnMessage(rtc::Message* /*pmsg*/) {
    int waiting_time_ms = 0;
    if (capturer_) {
      capturer_->ReadFrame(false);
      PostDelayed(waiting_time_ms, this);
    } else {
      Quit();
    }
  }

  // Check if Run() is finished.
  bool Finished() const {
    rtc::CritScope cs(&crit_);
    return finished_;
  }

 private:
  YuvFramesCapturer* capturer_;
  mutable rtc::CriticalSection crit_;
  bool finished_;

  RTC_DISALLOW_COPY_AND_ASSIGN(YuvFramesThread);
};

/////////////////////////////////////////////////////////////////////
// Implementation of class YuvFramesCapturer.
/////////////////////////////////////////////////////////////////////

const char* YuvFramesCapturer::kYuvFrameDeviceName = "YuvFramesGenerator";

// TODO(shaowei): allow width_ and height_ to be configurable.
YuvFramesCapturer::YuvFramesCapturer()
    : frames_generator_thread(NULL),
      width_(640),
      height_(480),
      frame_index_(0),
      barcode_interval_(1) {
}

YuvFramesCapturer::~YuvFramesCapturer() {
  Stop();
  delete[] static_cast<char*>(captured_frame_.data);
}

void YuvFramesCapturer::Init() {
  int size = width_ * height_;
  int qsize = size / 4;
  frame_generator_ = new YuvFrameGenerator(width_, height_, true);
  frame_data_size_ = size + 2 * qsize;
  captured_frame_.data = new char[frame_data_size_];
  captured_frame_.fourcc = FOURCC_IYUV;
  captured_frame_.pixel_height = 1;
  captured_frame_.pixel_width = 1;
  captured_frame_.width = width_;
  captured_frame_.height = height_;
  captured_frame_.data_size = frame_data_size_;

  // Enumerate the supported formats. We have only one supported format.
  VideoFormat format(width_, height_, VideoFormat::kMinimumInterval,
                     FOURCC_IYUV);
  std::vector<VideoFormat> supported;
  supported.push_back(format);
  SetSupportedFormats(supported);
}

CaptureState YuvFramesCapturer::Start(const VideoFormat& capture_format) {
  if (IsRunning()) {
    LOG(LS_ERROR) << "Yuv Frame Generator is already running";
    return CS_FAILED;
  }
  SetCaptureFormat(&capture_format);

  barcode_reference_timestamp_millis_ =
      static_cast<int64_t>(rtc::Time()) * 1000;
  // Create a thread to generate frames.
  frames_generator_thread = new YuvFramesThread(this);
  bool ret = frames_generator_thread->Start();
  if (ret) {
    LOG(LS_INFO) << "Yuv Frame Generator started";
    return CS_RUNNING;
  } else {
    LOG(LS_ERROR) << "Yuv Frame Generator failed to start";
    return CS_FAILED;
  }
}

bool YuvFramesCapturer::IsRunning() {
  return frames_generator_thread && !frames_generator_thread->Finished();
}

void YuvFramesCapturer::Stop() {
  if (frames_generator_thread) {
    frames_generator_thread->Stop();
    frames_generator_thread = NULL;
    LOG(LS_INFO) << "Yuv Frame Generator stopped";
  }
  SetCaptureFormat(NULL);
}

bool YuvFramesCapturer::GetPreferredFourccs(std::vector<uint32_t>* fourccs) {
  if (!fourccs) {
    return false;
  }
  fourccs->push_back(GetSupportedFormats()->at(0).fourcc);
  return true;
}

// Executed in the context of YuvFramesThread.
void YuvFramesCapturer::ReadFrame(bool first_frame) {
  // 1. Signal the previously read frame to downstream.
  if (!first_frame) {
    SignalFrameCaptured(this, &captured_frame_);
  }
  uint8_t* buffer = new uint8_t[frame_data_size_];
  frame_generator_->GenerateNextFrame(buffer, GetBarcodeValue());
  frame_index_++;
  memmove(captured_frame_.data, buffer, frame_data_size_);
  delete[] buffer;
}

int32_t YuvFramesCapturer::GetBarcodeValue() {
  if (barcode_reference_timestamp_millis_ == -1 ||
       frame_index_ % barcode_interval_ != 0) {
     return -1;
  }
  int64_t now_millis = static_cast<int64_t>(rtc::Time()) * 1000;
  return static_cast<int32_t>(now_millis - barcode_reference_timestamp_millis_);
}

}  // namespace cricket
