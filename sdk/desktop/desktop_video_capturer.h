#pragma once

#include "api/video/i420_buffer.h"
#include "media/base/video_common.h"
#include "modules/desktop_capture/desktop_capturer.h"
#include "modules/desktop_capture/desktop_frame.h"
#include "rtc_base/synchronization/mutex.h"
#include "rtc_base/thread.h"
#if defined(WEBRTC_WIN)
#include "rtc_base/win32.h"
#endif  // WEBRTC_WIN

#include "sdk/desktop/avcf_video_capturer.h"

namespace AvConf {

class DesktopVideoCapturer : public AvcfVideoCapturer,
                             public webrtc::DesktopCapturer::Callback {
 public:
  static DesktopVideoCapturer* Create(int width,
                                      int height,
                                      int fps);
  ~DesktopVideoCapturer();

  void Start() override;

  void Stop() override;

  void OnCaptureResult(webrtc::DesktopCapturer::Result result,
                       std::unique_ptr<webrtc::DesktopFrame> frame) override;

 private:
  DesktopVideoCapturer();
  bool Init(int target_fps);
  void Destroy();

  void capture_loop();

  std::unique_ptr<webrtc::DesktopCapturer> desktop_capturer_;
  bool running_;
  rtc::scoped_refptr<webrtc::I420Buffer> i420_buffer_;

  int frame_interval_ms_;
  std::unique_ptr<rtc::Thread> capture_thread_;
  webrtc::Mutex capturer_lock_;
};

}  // namespace AvConf
