#pragma once

#include "pc/video_track_source.h"
#include "sdk/desktop/file_video_capturer.h"

namespace AvConf {

class FileCapturerTrackSource : public webrtc::VideoTrackSource {
 public:
  static rtc::scoped_refptr<FileCapturerTrackSource> Create(const char* path,
                                                            const char* dump_path);

  void Start();

  void Stop();

 protected:
  explicit FileCapturerTrackSource(FileVideoCapturer* capturer)
      : VideoTrackSource(/*remote=*/false), capturer_(capturer) {}

 private:
  rtc::VideoSourceInterface<webrtc::VideoFrame>* source() override;

  std::unique_ptr<FileVideoCapturer> capturer_;
};

}  // namespace AvConf
