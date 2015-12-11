/*
 * libjingle
 * Copyright 2015 Google Inc.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 *  1. Redistributions of source code must retain the above copyright notice,
 *     this list of conditions and the following disclaimer.
 *  2. Redistributions in binary form must reproduce the above copyright notice,
 *     this list of conditions and the following disclaimer in the documentation
 *     and/or other materials provided with the distribution.
 *  3. The name of the author may not be used to endorse or promote products
 *     derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO
 * EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
#include "talk/app/webrtc/androidvideocapturer.h"

#include "talk/app/webrtc/java/jni/native_handle_impl.h"
#include "talk/media/webrtc/webrtcvideoframe.h"
#include "webrtc/base/common.h"
#include "webrtc/base/json.h"
#include "webrtc/base/timeutils.h"

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
  FrameFactory(const rtc::scoped_refptr<AndroidVideoCapturerDelegate>& delegate)
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
  std::string json_string = delegate_->GetSupportedFormats();
  LOG(LS_INFO) << json_string;

  Json::Value json_values;
  Json::Reader reader(Json::Features::strictMode());
  if (!reader.parse(json_string, json_values)) {
    LOG(LS_ERROR) << "Failed to parse formats.";
  }

  std::vector<cricket::VideoFormat> formats;
  for (Json::ArrayIndex i = 0; i < json_values.size(); ++i) {
      const Json::Value& json_value = json_values[i];
      RTC_CHECK(!json_value["width"].isNull() &&
                !json_value["height"].isNull() &&
                !json_value["framerate"].isNull());
      cricket::VideoFormat format(
          json_value["width"].asInt(),
          json_value["height"].asInt(),
          cricket::VideoFormat::FpsToInterval(json_value["framerate"].asInt()),
          cricket::FOURCC_YV12);
      formats.push_back(format);
  }
  SetSupportedFormats(formats);
  // Do not apply frame rotation by default.
  SetApplyRotation(false);
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
  SignalStateChange(this, current_state_);
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

  // TODO(perkj): SetCaptureState can not be used since it posts to |thread_|.
  // But |thread_ | is currently just the thread that happened to create the
  // cricket::VideoCapturer.
  SignalStateChange(this, new_state);
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
  const cricket::VideoFormat& current = video_adapter()->output_format();
  cricket::VideoFormat format(
      width, height, cricket::VideoFormat::FpsToInterval(fps), current.fourcc);
  video_adapter()->OnOutputFormatRequest(format);
}

bool AndroidVideoCapturer::GetBestCaptureFormat(
    const cricket::VideoFormat& desired,
    cricket::VideoFormat* best_format) {
  // Delegate this choice to VideoCapturerAndroid.startCapture().
  *best_format = desired;
  return true;
}

}  // namespace webrtc
