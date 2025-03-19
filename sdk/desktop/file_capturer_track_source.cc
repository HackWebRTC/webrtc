#include "sdk/desktop/file_capturer_track_source.h"

#include "rtc_base/logging.h"

namespace AvConf {

rtc::scoped_refptr<FileCapturerTrackSource>
FileCapturerTrackSource::Create(const char* path, const char* dump_path) {
  RTC_LOG(LS_INFO) << "FileCapturerTrackSource::Create " << path;
  return rtc::make_ref_counted<FileCapturerTrackSource>(
      FileVideoCapturer::Create(path, dump_path));
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
