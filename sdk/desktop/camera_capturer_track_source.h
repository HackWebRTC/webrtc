#pragma once

#include "pc/video_track_source.h"
#include "sdk/desktop/vcm_capturer.h"

namespace AvConf {

class CameraCapturerTrackSource : public webrtc::VideoTrackSource {
 public:
  static rtc::scoped_refptr<CameraCapturerTrackSource> Create(int width,
                                                              int height,
                                                              int fps);

  void Start();

  void Stop();

  void AdaptOutputFormat(int width, int height, int fps);

 protected:
  explicit CameraCapturerTrackSource(std::unique_ptr<VcmCapturer> capturer)
      : VideoTrackSource(/*remote=*/false), capturer_(std::move(capturer)) {}

 private:
  rtc::VideoSourceInterface<webrtc::VideoFrame>* source() override;

  std::unique_ptr<VcmCapturer> capturer_;
};

}  // namespace AvConf
