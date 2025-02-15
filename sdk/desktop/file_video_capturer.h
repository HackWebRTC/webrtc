#pragma once

#include "modules/transit_media/file_capturer.h"
#include "sdk/desktop/avcf_video_capturer.h"

namespace AvConf {

class FileVideoCapturer : public AvcfVideoCapturer,
                          public rtc::VideoSinkInterface<webrtc::VideoFrame> {
 public:
  static FileVideoCapturer* Create(int width, int height, const char* path);
  ~FileVideoCapturer();

  void Start() override;

  void Stop() override;

  void OnFrame(const webrtc::VideoFrame& frame) override;

  void set_loop(bool loop) { loop_ = loop; }

 private:
  FileVideoCapturer(webrtc::FileCapturer* capturer);

  std::unique_ptr<webrtc::FileCapturer> capturer_;
  bool loop_;
};

}  // namespace AvConf
