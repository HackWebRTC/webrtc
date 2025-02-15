#pragma once

#include "api/media_stream_interface.h"
#include "api/video/video_frame.h"
#include "media/base/media_channel.h"
#include "media/base/video_common.h"
#if defined(WEBRTC_WIN)
#include "rtc_base/win32.h"
#endif  // WEBRTC_WIN

namespace AvConf {

#define SCALE_TYPE_CENTER_INSIDE 1
#define SCALE_TYPE_CENTER_CROP 2

// A little helper class to make sure we always to proper locking and
// unlocking when working with VideoRenderer buffers.
template <typename T>
class AutoLock {
 public:
  explicit AutoLock(T* obj) : obj_(obj) { obj_->Lock(); }
  ~AutoLock() { obj_->Unlock(); }

 protected:
  T* obj_;
};

class Win32VideoRenderer : public rtc::VideoSinkInterface<webrtc::VideoFrame> {
 public:
  Win32VideoRenderer(int top,
                     int left,
                     int width,
                     int height,
                     int z_index,
                     int scale_type);
  ~Win32VideoRenderer();

  void Lock() { ::EnterCriticalSection(&buffer_lock_); }

  void Unlock() { ::LeaveCriticalSection(&buffer_lock_); }

  void RenderTrack(webrtc::VideoTrackInterface* track_to_render);
  void StopRendering();

  // VideoSinkInterface implementation
  void OnFrame(const webrtc::VideoFrame& frame) override;

  const BITMAPINFO& bmi() const { return bmi_; }
  const uint8_t* image() const { return image_.get(); }
  int top() const { return top_; }
  int left() const { return left_; }
  int width() const { return width_; }
  int height() const { return height_; }
  int z_index() const { return z_index_; }
  int scale_type() const { return scale_type_; }

 private:
  void SetSize(int width, int height);

  enum {
    SET_SIZE,
    RENDER_FRAME,
  };

  int top_;
  int left_;
  int width_;
  int height_;
  int z_index_;

  int scale_type_;

  BITMAPINFO bmi_;
  std::unique_ptr<uint8_t[]> image_;
  CRITICAL_SECTION buffer_lock_;
  rtc::scoped_refptr<webrtc::VideoTrackInterface> rendered_track_;
};

}  // namespace AvConf
