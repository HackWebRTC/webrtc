#include "sdk/desktop/desktop_video_capturer.h"

#include "api/units/time_delta.h"
#include "modules/desktop_capture/desktop_and_cursor_composer.h"
#include "modules/desktop_capture/desktop_capture_options.h"
#include "modules/desktop_capture/desktop_capturer.h"
#include "rtc_base/logging.h"
#include "third_party/libyuv/include/libyuv.h"

namespace AvConf {

DesktopVideoCapturer* DesktopVideoCapturer::Create(int width,
                                                   int height,
                                                   int fps) {
  std::unique_ptr<DesktopVideoCapturer> capturer(new DesktopVideoCapturer());
  if (!capturer->Init(fps)) {
    RTC_LOG(LS_WARNING) << "Failed to create DesktopVideoCapturer(fps = " << fps
                        << ")";
    return nullptr;
  }
  return capturer.release();
}

DesktopVideoCapturer::DesktopVideoCapturer()
    : desktop_capturer_(nullptr),
      running_(false),
      i420_buffer_(nullptr),
      capture_thread_(rtc::Thread::Create()) {
  capture_thread_->SetName("desktop_capture_thread", nullptr);
  RTC_CHECK(capture_thread_->Start())
      << "Failed to start desktop capture thread";
}

DesktopVideoCapturer::~DesktopVideoCapturer() {}

void DesktopVideoCapturer::Start() {}

void DesktopVideoCapturer::Stop() {}

bool DesktopVideoCapturer::Init(int target_fps) {
  RTC_LOG(LS_INFO) << "DesktopVideoCapturer::Init";

  if (running_) {
    RTC_LOG(LS_INFO) << "DesktopVideoCapturer::Init, already running";
    return false;
  }

  frame_interval_ms_ = 1000 / target_fps;

  webrtc::DesktopCaptureOptions options;
#if defined(WEBRTC_WIN)
  options.set_allow_directx_capturer(false);
#endif
  std::unique_ptr<webrtc::DesktopCapturer> capturer =
      webrtc::DesktopCapturer::CreateScreenCapturer(options);

  webrtc::DesktopCapturer::SourceList desktop_screens;
  capturer->GetSourceList(&desktop_screens);
  if (desktop_screens.size() == 0) {
    RTC_LOG(LS_INFO) << "DesktopVideoCapturer::Init fail: no screen";
    return false;
  }

  for (auto& s : desktop_screens) {
    RTC_LOG(LS_INFO) << "screen: " << s.id << " -> " << s.title;
  }
  capturer->SelectSource(desktop_screens[0].id);

  desktop_capturer_.reset(
      new webrtc::DesktopAndCursorComposer(std::move(capturer), options));

  desktop_capturer_->Start(this);
  running_ = true;

  capture_thread_->PostTask([this]() { capture_loop(); });

  RTC_LOG(LS_INFO) << "DesktopVideoCapturer::Init success";
  return true;
}

void DesktopVideoCapturer::Destroy() {
  webrtc::MutexLock lock(&capturer_lock_);
  running_ = false;
  desktop_capturer_.reset();
  capture_thread_->Quit();
}

void DesktopVideoCapturer::OnCaptureResult(
    webrtc::DesktopCapturer::Result result,
    std::unique_ptr<webrtc::DesktopFrame> frame) {
  if (result != webrtc::DesktopCapturer::Result::SUCCESS) {
    return;
  }

  int width = frame->size().width();
  int height = frame->size().height();

  if (!i420_buffer_.get() ||
      i420_buffer_->width() * i420_buffer_->height() < width * height) {
    i420_buffer_ = webrtc::I420Buffer::Create(width, height);
  }
  libyuv::ConvertToI420(frame->data(), 0, i420_buffer_->MutableDataY(),
                        i420_buffer_->StrideY(), i420_buffer_->MutableDataU(),
                        i420_buffer_->StrideU(), i420_buffer_->MutableDataV(),
                        i420_buffer_->StrideV(), 0, 0, width, height, width,
                        height, libyuv::kRotate0, libyuv::FOURCC_ARGB);

  OnFrame(webrtc::VideoFrame(i420_buffer_, 0, rtc::TimeMillis(),
                             webrtc::kVideoRotation_0));
}

void DesktopVideoCapturer::capture_loop() {
  webrtc::MutexLock lock(&capturer_lock_);
  if (running_) {
    desktop_capturer_->CaptureFrame();

    capture_thread_->PostDelayedTask([this]() { capture_loop(); }, webrtc::TimeDelta::Millis(frame_interval_ms_));
  }
}

}  // namespace AvConf
