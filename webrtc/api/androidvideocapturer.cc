/*
 *  Copyright 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/api/androidvideocapturer.h"

#include "webrtc/api/java/jni/native_handle_impl.h"
#include "webrtc/base/common.h"
#include "webrtc/base/timeutils.h"
#include "webrtc/media/engine/webrtcvideoframe.h"

namespace webrtc {

// A hack for avoiding deep frame copies in
// cricket::VideoCapturer.SignalFrameCaptured() using a custom FrameFactory.
// A frame is injected using UpdateCapturedFrame(), and converted into a
// cricket::VideoFrame with CreateAliasedFrame(). UpdateCapturedFrame() should
// be called before CreateAliasedFrame() for every frame.
// TODO(magjed): Add an interface cricket::VideoCapturer::OnFrameCaptured()
// for ref counted I420 frames instead of this hack.
class AndroidVideoCapturer::FrameFactory : public cricket::VideoFrameFactory {
 public:
  explicit FrameFactory(
      const rtc::scoped_refptr<AndroidVideoCapturerDelegate>& delegate)
      : delegate_(delegate) {
    // Create a CapturedFrame that only contains header information, not the
    // actual pixel data.
    captured_frame_.pixel_height = 1;
    captured_frame_.pixel_width = 1;
    captured_frame_.data = nullptr;
    captured_frame_.data_size = cricket::CapturedFrame::kUnknownDataSize;
    captured_frame_.fourcc = static_cast<uint32_t>(cricket::FOURCC_ANY);
  }

  void UpdateCapturedFrame(
      const rtc::scoped_refptr<webrtc::VideoFrameBuffer>& buffer,
      int rotation,
      int64_t time_stamp_in_ns) {
    RTC_DCHECK(rotation == 0 || rotation == 90 || rotation == 180 ||
               rotation == 270);
    buffer_ = buffer;
    captured_frame_.width = buffer->width();
    captured_frame_.height = buffer->height();
    captured_frame_.time_stamp = time_stamp_in_ns;
    captured_frame_.rotation = static_cast<webrtc::VideoRotation>(rotation);
  }

  void ClearCapturedFrame() {
    buffer_ = nullptr;
    captured_frame_.width = 0;
    captured_frame_.height = 0;
    captured_frame_.time_stamp = 0;
  }

  const cricket::CapturedFrame* GetCapturedFrame() const {
    return &captured_frame_;
  }

  cricket::VideoFrame* CreateAliasedFrame(
      const cricket::CapturedFrame* captured_frame,
      int dst_width,
      int dst_height) const override {
    // Check that captured_frame is actually our frame.
    RTC_CHECK(captured_frame == &captured_frame_);
    RTC_CHECK(buffer_->native_handle() == nullptr);

    rtc::scoped_ptr<cricket::VideoFrame> frame(new cricket::WebRtcVideoFrame(
        ShallowCenterCrop(buffer_, dst_width, dst_height),
        captured_frame->time_stamp, captured_frame->rotation));
    // Caller takes ownership.
    // TODO(magjed): Change CreateAliasedFrame() to return a rtc::scoped_ptr.
    return apply_rotation_ ? frame->GetCopyWithRotationApplied()->Copy()
                           : frame.release();
  }

  cricket::VideoFrame* CreateAliasedFrame(
      const cricket::CapturedFrame* input_frame,
      int cropped_input_width,
      int cropped_input_height,
      int output_width,
      int output_height) const override {
    if (buffer_->native_handle() != nullptr) {
      // TODO(perkj) Implement cropping.
      RTC_CHECK_EQ(cropped_input_width, buffer_->width());
      RTC_CHECK_EQ(cropped_input_height, buffer_->height());
      rtc::scoped_refptr<webrtc::VideoFrameBuffer> scaled_buffer(
          static_cast<webrtc_jni::AndroidTextureBuffer*>(buffer_.get())
              ->ScaleAndRotate(output_width, output_height,
                               apply_rotation_ ? input_frame->rotation :
                                   webrtc::kVideoRotation_0));
      return new cricket::WebRtcVideoFrame(
          scaled_buffer, input_frame->time_stamp,
          apply_rotation_ ? webrtc::kVideoRotation_0 : input_frame->rotation);
    }
    return VideoFrameFactory::CreateAliasedFrame(input_frame,
                                                 cropped_input_width,
                                                 cropped_input_height,
                                                 output_width,
                                                 output_height);
  }

 private:
  rtc::scoped_refptr<webrtc::VideoFrameBuffer> buffer_;
  cricket::CapturedFrame captured_frame_;
  rtc::scoped_refptr<AndroidVideoCapturerDelegate> delegate_;
};

AndroidVideoCapturer::AndroidVideoCapturer(
    const rtc::scoped_refptr<AndroidVideoCapturerDelegate>& delegate)
    : running_(false),
      delegate_(delegate),
      frame_factory_(NULL),
      current_state_(cricket::CS_STOPPED) {
  thread_checker_.DetachFromThread();
  SetSupportedFormats(delegate_->GetSupportedFormats());
}

AndroidVideoCapturer::~AndroidVideoCapturer() {
  RTC_CHECK(!running_);
}

cricket::CaptureState AndroidVideoCapturer::Start(
    const cricket::VideoFormat& capture_format) {
  RTC_CHECK(thread_checker_.CalledOnValidThread());
  RTC_CHECK(!running_);
  const int fps = cricket::VideoFormat::IntervalToFps(capture_format.interval);
  LOG(LS_INFO) << " AndroidVideoCapturer::Start " << capture_format.width << "x"
               << capture_format.height << "@" << fps;

  frame_factory_ = new AndroidVideoCapturer::FrameFactory(delegate_.get());
  set_frame_factory(frame_factory_);

  running_ = true;
  delegate_->Start(capture_format.width, capture_format.height, fps, this);
  SetCaptureFormat(&capture_format);
  current_state_ = cricket::CS_STARTING;
  return current_state_;
}

void AndroidVideoCapturer::Stop() {
  LOG(LS_INFO) << " AndroidVideoCapturer::Stop ";
  RTC_CHECK(thread_checker_.CalledOnValidThread());
  RTC_CHECK(running_);
  running_ = false;
  SetCaptureFormat(NULL);

  delegate_->Stop();
  current_state_ = cricket::CS_STOPPED;
  SetCaptureState(current_state_);
}

bool AndroidVideoCapturer::IsRunning() {
  RTC_CHECK(thread_checker_.CalledOnValidThread());
  return running_;
}

bool AndroidVideoCapturer::GetPreferredFourccs(std::vector<uint32_t>* fourccs) {
  RTC_CHECK(thread_checker_.CalledOnValidThread());
  fourccs->push_back(cricket::FOURCC_YV12);
  return true;
}

void AndroidVideoCapturer::OnCapturerStarted(bool success) {
  RTC_CHECK(thread_checker_.CalledOnValidThread());
  cricket::CaptureState new_state =
      success ? cricket::CS_RUNNING : cricket::CS_FAILED;
  if (new_state == current_state_)
    return;
  current_state_ = new_state;
  SetCaptureState(new_state);
}

void AndroidVideoCapturer::OnIncomingFrame(
    const rtc::scoped_refptr<webrtc::VideoFrameBuffer>& buffer,
    int rotation,
    int64_t time_stamp) {
  RTC_CHECK(thread_checker_.CalledOnValidThread());
  frame_factory_->UpdateCapturedFrame(buffer, rotation, time_stamp);
  SignalFrameCaptured(this, frame_factory_->GetCapturedFrame());
  frame_factory_->ClearCapturedFrame();
}

void AndroidVideoCapturer::OnOutputFormatRequest(
    int width, int height, int fps) {
  RTC_CHECK(thread_checker_.CalledOnValidThread());
  cricket::VideoFormat format(width, height,
                              cricket::VideoFormat::FpsToInterval(fps), 0);
  video_adapter()->OnOutputFormatRequest(format);
}

bool AndroidVideoCapturer::GetBestCaptureFormat(
    const cricket::VideoFormat& desired,
    cricket::VideoFormat* best_format) {
  // Delegate this choice to VideoCapturer.startCapture().
  *best_format = desired;
  return true;
}

}  // namespace webrtc
