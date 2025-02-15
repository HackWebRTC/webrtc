#include "api/video/i420_buffer.h"
#include "rtc_base/arraysize.h"
#include "third_party/libyuv/include/libyuv/convert_argb.h"

#include "sdk/desktop/win32_video_renderer.h"

namespace AvConf {

Win32VideoRenderer::Win32VideoRenderer(int top,
                                       int left,
                                       int width,
                                       int height,
                                       int z_index,
                                       int scale_type)
    : top_(top),
      left_(left),
      width_(width),
      height_(height),
      z_index_(z_index),
      scale_type_(scale_type),
      rendered_track_(nullptr) {
  ::InitializeCriticalSection(&buffer_lock_);
  ZeroMemory(&bmi_, sizeof(bmi_));
  bmi_.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
  bmi_.bmiHeader.biPlanes = 1;
  bmi_.bmiHeader.biBitCount = 32;
  bmi_.bmiHeader.biCompression = BI_RGB;
  bmi_.bmiHeader.biWidth = width;
  bmi_.bmiHeader.biHeight = -height;
  bmi_.bmiHeader.biSizeImage =
      width * height * (bmi_.bmiHeader.biBitCount >> 3);
}

Win32VideoRenderer::~Win32VideoRenderer() {
  if (rendered_track_) {
    rendered_track_->RemoveSink(this);
  }
  ::DeleteCriticalSection(&buffer_lock_);
}

void Win32VideoRenderer::SetSize(int width, int height) {
  AutoLock<Win32VideoRenderer> lock(this);

  if (width == bmi_.bmiHeader.biWidth && height == bmi_.bmiHeader.biHeight) {
    return;
  }

  bmi_.bmiHeader.biWidth = width;
  bmi_.bmiHeader.biHeight = -height;
  bmi_.bmiHeader.biSizeImage =
      width * height * (bmi_.bmiHeader.biBitCount >> 3);
  image_.reset(new uint8_t[bmi_.bmiHeader.biSizeImage]);
}

void Win32VideoRenderer::RenderTrack(
    webrtc::VideoTrackInterface* track_to_render) {
  rendered_track_ = track_to_render;
  rendered_track_->AddOrUpdateSink(this, rtc::VideoSinkWants());
}

void Win32VideoRenderer::StopRendering() {
  if (rendered_track_) {
    rendered_track_->RemoveSink(this);
    rendered_track_ = nullptr;
  }
}

void Win32VideoRenderer::OnFrame(const webrtc::VideoFrame& video_frame) {
  {
    AutoLock<Win32VideoRenderer> lock(this);

    rtc::scoped_refptr<webrtc::I420BufferInterface> buffer(
        video_frame.video_frame_buffer()->ToI420());
    if (video_frame.rotation() != webrtc::kVideoRotation_0) {
      buffer = webrtc::I420Buffer::Rotate(*buffer, video_frame.rotation());
    }

    SetSize(buffer->width(), buffer->height());

    RTC_DCHECK(image_.get() != NULL);
    libyuv::I420ToARGB(buffer->DataY(), buffer->StrideY(), buffer->DataU(),
                       buffer->StrideU(), buffer->DataV(), buffer->StrideV(),
                       image_.get(),
                       bmi_.bmiHeader.biWidth * bmi_.bmiHeader.biBitCount / 8,
                       buffer->width(), buffer->height());
  }
}

}  // namespace AvConf
