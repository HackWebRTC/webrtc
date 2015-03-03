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

#include "talk/media/webrtc/webrtcvideoframe.h"
#include "webrtc/base/common.h"
#include "webrtc/base/json.h"
#include "webrtc/base/timeutils.h"
#include "webrtc/base/thread.h"

namespace webrtc {

using cricket::WebRtcVideoFrame;
using rtc::scoped_ptr;

// An implementation of cricket::VideoFrameFactory for frames that are not
// guaranteed to outlive the created cricket::VideoFrame.
// A frame is injected using UpdateCapturedFrame, and converted into a
// cricket::VideoFrame with
// CreateAliasedFrame. UpdateCapturedFrame should be called before
// CreateAliasedFrame for every frame.
class AndroidVideoCapturer::FrameFactory : public cricket::VideoFrameFactory {
 public:
  FrameFactory(int width, int height) : start_time_(rtc::TimeNanos()) {
    // Create a CapturedFrame that only contains header information, not the
    // actual pixel data.
    captured_frame_.width = width;
    captured_frame_.height = height;
    captured_frame_.pixel_height = 1;
    captured_frame_.pixel_width = 1;
    captured_frame_.rotation = 0;
    captured_frame_.data = NULL;
    captured_frame_.data_size = cricket::CapturedFrame::kUnknownDataSize;
    captured_frame_.fourcc = static_cast<uint32>(cricket::FOURCC_ANY);
  }

  void UpdateCapturedFrame(void* frame_data,
                           int length,
                           int rotation,
                           int64 time_stamp_in_ms) {
    captured_frame_.fourcc = static_cast<uint32>(cricket::FOURCC_YV12);
    captured_frame_.data = frame_data;
    captured_frame_.elapsed_time = rtc::TimeNanos() - start_time_;
    captured_frame_.time_stamp =
        time_stamp_in_ms * rtc::kNumNanosecsPerMillisec;
    captured_frame_.rotation = rotation;
    captured_frame_.data_size = length;
  }

  const cricket::CapturedFrame* GetCapturedFrame() const {
    return &captured_frame_;
  }

  cricket::VideoFrame* CreateAliasedFrame(
      const cricket::CapturedFrame* captured_frame,
      int dst_width,
      int dst_height) const override {
    // This override of CreateAliasedFrame creates a copy of the frame since
    // |captured_frame_.data| is only guaranteed to be valid during the scope
    // of |AndroidVideoCapturer::OnIncomingFrame_w|.
    // Check that captured_frame is actually our frame.
    DCHECK(captured_frame == &captured_frame_);
    scoped_ptr<WebRtcVideoFrame> frame(new WebRtcVideoFrame());
    frame->Init(captured_frame, dst_width, dst_height, apply_rotation_);
    return frame.release();
  }

 private:
  uint64 start_time_;
  cricket::CapturedFrame captured_frame_;
};

AndroidVideoCapturer::AndroidVideoCapturer(
    rtc::scoped_ptr<AndroidVideoCapturerDelegate> delegate)
    : running_(false),
      delegate_(delegate.Pass()),
      worker_thread_(NULL),
      frame_factory_(NULL),
      current_state_(cricket::CS_STOPPED) {
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
      DCHECK(!json_value["width"].isNull() && !json_value["height"].isNull() &&
             !json_value["framerate"].isNull());
      cricket::VideoFormat format(
          json_value["width"].asInt(),
          json_value["height"].asInt(),
          cricket::VideoFormat::FpsToInterval(json_value["framerate"].asInt()),
          cricket::FOURCC_YV12);
      formats.push_back(format);
  }
  SetSupportedFormats(formats);
}

AndroidVideoCapturer::~AndroidVideoCapturer() {
  DCHECK(!running_);
}

cricket::CaptureState AndroidVideoCapturer::Start(
    const cricket::VideoFormat& capture_format) {
  DCHECK(!running_);
  DCHECK(worker_thread_ == nullptr || worker_thread_ == rtc::Thread::Current());
  // TODO(perkj): Better way to get a handle to the worker thread?
  worker_thread_ = rtc::Thread::Current();

  LOG(LS_INFO) << " AndroidVideoCapturer::Start w = " << capture_format.width
               << " h = " << capture_format.height;
  frame_factory_ = new AndroidVideoCapturer::FrameFactory(
      capture_format.width, capture_format.height);
  set_frame_factory(frame_factory_);

  running_ = true;
  delegate_->Start(
      capture_format.width, capture_format.height,
      cricket::VideoFormat::IntervalToFps(capture_format.interval), this);
  SetCaptureFormat(&capture_format);
  current_state_ = cricket::CS_STARTING;
  return current_state_;
}

void AndroidVideoCapturer::Stop() {
  DCHECK(worker_thread_->IsCurrent());
  LOG(LS_INFO) << " AndroidVideoCapturer::Stop ";
  DCHECK(running_);
  running_ = false;
  SetCaptureFormat(NULL);

  delegate_->Stop();
  current_state_ = cricket::CS_STOPPED;
  SignalStateChange(this, current_state_);
}

bool AndroidVideoCapturer::IsRunning() {
  return running_;
}

bool AndroidVideoCapturer::GetPreferredFourccs(std::vector<uint32>* fourccs) {
  fourccs->push_back(cricket::FOURCC_YV12);
  return true;
}

void AndroidVideoCapturer::OnCapturerStarted(bool success) {
  DCHECK(worker_thread_->IsCurrent());
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

void AndroidVideoCapturer::OnIncomingFrame(void* frame_data,
                                           int length,
                                           int rotation,
                                           int64 time_stamp) {
  DCHECK(worker_thread_->IsCurrent());
  frame_factory_->UpdateCapturedFrame(frame_data, length, rotation, time_stamp);
  SignalFrameCaptured(this, frame_factory_->GetCapturedFrame());
  delegate_->ReturnBuffer(time_stamp);
}

}  // namespace webrtc
