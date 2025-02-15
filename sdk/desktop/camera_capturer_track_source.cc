#include "modules/video_capture/video_capture.h"
#include "modules/video_capture/video_capture_factory.h"
#include "sdk/desktop/camera_capturer_track_source.h"

namespace AvConf {

rtc::scoped_refptr<CameraCapturerTrackSource>
CameraCapturerTrackSource::Create(int width, int height, int fps) {
  std::unique_ptr<VcmCapturer> capturer;
  std::unique_ptr<webrtc::VideoCaptureModule::DeviceInfo> info(
      webrtc::VideoCaptureFactory::CreateDeviceInfo());
  if (!info) {
    return nullptr;
  }
  int num_devices = info->NumberOfDevices();
  for (int i = 0; i < num_devices; ++i) {
    capturer = absl::WrapUnique(VcmCapturer::Create(width, height, fps, i));
    if (capturer) {
      return new rtc::RefCountedObject<CameraCapturerTrackSource>(
          std::move(capturer));
    }
  }

  return nullptr;
}

void CameraCapturerTrackSource::Start() {
  if (capturer_) {
    capturer_->Start();
  }
}

void CameraCapturerTrackSource::Stop() {
  if (capturer_) {
    capturer_->Stop();
  }
}

void CameraCapturerTrackSource::AdaptOutputFormat(int width,
                                                  int height,
                                                  int fps) {
  capturer_->OnOutputFormatRequest(width, height, fps);
}

rtc::VideoSourceInterface<webrtc::VideoFrame>*
CameraCapturerTrackSource::source() {
  return capturer_.get();
}

}  // namespace AvConf
