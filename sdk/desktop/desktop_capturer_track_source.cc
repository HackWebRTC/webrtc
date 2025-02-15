#include "sdk/desktop/desktop_capturer_track_source.h"

namespace AvConf {

rtc::scoped_refptr<DesktopCapturerTrackSource>
DesktopCapturerTrackSource::Create(int width, int height, int fps) {
  return new rtc::RefCountedObject<DesktopCapturerTrackSource>(
      DesktopVideoCapturer::Create(width, height, fps));
}

void DesktopCapturerTrackSource::AdaptOutputFormat(int width,
                                                   int height,
                                                   int fps) {
  capturer_->OnOutputFormatRequest(width, height, fps);
}

rtc::VideoSourceInterface<webrtc::VideoFrame>*
DesktopCapturerTrackSource::source() {
  return capturer_.get();
}

}  // namespace AvConf
