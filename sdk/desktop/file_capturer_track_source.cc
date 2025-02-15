#include "sdk/desktop/file_capturer_track_source.h"

#include "rtc_base/logging.h"

namespace AvConf {

rtc::scoped_refptr<FileCapturerTrackSource>
FileCapturerTrackSource::Create(int width, int height, const char* path) {
  RTC_LOG(LS_INFO) << "FileCapturerTrackSource::Create " << path;
  return new rtc::RefCountedObject<FileCapturerTrackSource>(
      FileVideoCapturer::Create(width, height, path));
}

void FileCapturerTrackSource::Start() {
  if (capturer_) {
    capturer_->Start();
  }
}

void FileCapturerTrackSource::Stop() {
  if (capturer_) {
    capturer_->Stop();
  }
}

rtc::VideoSourceInterface<webrtc::VideoFrame>*
FileCapturerTrackSource::source() {
  return capturer_.get();
}

}  // namespace AvConf
