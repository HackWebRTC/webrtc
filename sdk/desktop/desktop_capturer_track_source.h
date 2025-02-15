#pragma once

#include "pc/video_track_source.h"
#include "sdk/desktop/desktop_video_capturer.h"

namespace AvConf {

class DesktopCapturerTrackSource : public webrtc::VideoTrackSource {
 public:
  static rtc::scoped_refptr<DesktopCapturerTrackSource> Create(int width,
                                                               int height,
                                                               int fps);

  bool is_screencast() const override { return true; }

  void AdaptOutputFormat(int width, int height, int fps);

 protected:
  explicit DesktopCapturerTrackSource(DesktopVideoCapturer* capturer)
      : VideoTrackSource(/*remote=*/false), capturer_(capturer) {}

 private:
  rtc::VideoSourceInterface<webrtc::VideoFrame>* source() override;

  std::unique_ptr<DesktopVideoCapturer> capturer_;
};

}  // namespace AvConf
