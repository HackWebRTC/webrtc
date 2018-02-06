/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "media/base/fakevideocapturer.h"

#include "rtc_base/arraysize.h"

namespace cricket {

FakeVideoCapturer::FakeVideoCapturer(bool is_screencast)
    : running_(false),
      initial_timestamp_(rtc::TimeNanos()),
      next_timestamp_(rtc::kNumNanosecsPerMillisec),
      is_screencast_(is_screencast),
      rotation_(webrtc::kVideoRotation_0) {
  // Default supported formats. Use ResetSupportedFormats to over write.
  using cricket::VideoFormat;
  static const VideoFormat formats[] = {
      {1280, 720, VideoFormat::FpsToInterval(30), cricket::FOURCC_I420},
      {640, 480, VideoFormat::FpsToInterval(30), cricket::FOURCC_I420},
      {320, 240, VideoFormat::FpsToInterval(30), cricket::FOURCC_I420},
      {160, 120, VideoFormat::FpsToInterval(30), cricket::FOURCC_I420},
      {1280, 720, VideoFormat::FpsToInterval(60), cricket::FOURCC_I420},
  };
  ResetSupportedFormats({&formats[0], &formats[arraysize(formats)]});
}

FakeVideoCapturer::FakeVideoCapturer() : FakeVideoCapturer(false) {}

FakeVideoCapturer::~FakeVideoCapturer() {
  SignalDestroyed(this);
}

void FakeVideoCapturer::ResetSupportedFormats(
    const std::vector<cricket::VideoFormat>& formats) {
  SetSupportedFormats(formats);
}

bool FakeVideoCapturer::CaptureFrame() {
  if (!GetCaptureFormat()) {
    return false;
  }
  return CaptureCustomFrame(
      GetCaptureFormat()->width, GetCaptureFormat()->height,
      GetCaptureFormat()->interval, GetCaptureFormat()->fourcc);
}

bool FakeVideoCapturer::CaptureCustomFrame(int width,
                                           int height,
                                           uint32_t fourcc) {
  // Default to 30fps.
  return CaptureCustomFrame(width, height, rtc::kNumNanosecsPerSec / 30,
                            fourcc);
}

bool FakeVideoCapturer::CaptureCustomFrame(int width,
                                           int height,
                                           int64_t timestamp_interval,
                                           uint32_t fourcc) {
  if (!running_) {
    return false;
  }
  RTC_CHECK(fourcc == FOURCC_I420);
  RTC_CHECK(width > 0);
  RTC_CHECK(height > 0);

  int adapted_width;
  int adapted_height;
  int crop_width;
  int crop_height;
  int crop_x;
  int crop_y;

  // TODO(nisse): It's a bit silly to have this logic in a fake
  // class. Child classes of VideoCapturer are expected to call
  // AdaptFrame, and the test case
  // VideoCapturerTest.SinkWantsMaxPixelAndMaxPixelCountStepUp
  // depends on this.
  if (AdaptFrame(width, height, next_timestamp_ / rtc::kNumNanosecsPerMicrosec,
                 next_timestamp_ / rtc::kNumNanosecsPerMicrosec, &adapted_width,
                 &adapted_height, &crop_width, &crop_height, &crop_x, &crop_y,
                 nullptr)) {
    rtc::scoped_refptr<webrtc::I420Buffer> buffer(
        webrtc::I420Buffer::Create(adapted_width, adapted_height));
    buffer->InitializeData();

    OnFrame(webrtc::VideoFrame(buffer, rotation_,
                               next_timestamp_ / rtc::kNumNanosecsPerMicrosec),
            width, height);
  }
  next_timestamp_ += timestamp_interval;

  return true;
}

cricket::CaptureState FakeVideoCapturer::Start(
    const cricket::VideoFormat& format) {
  SetCaptureFormat(&format);
  running_ = true;
  SetCaptureState(cricket::CS_RUNNING);
  return cricket::CS_RUNNING;
}

void FakeVideoCapturer::Stop() {
  running_ = false;
  SetCaptureFormat(NULL);
  SetCaptureState(cricket::CS_STOPPED);
}

bool FakeVideoCapturer::IsRunning() {
  return running_;
}

bool FakeVideoCapturer::IsScreencast() const {
  return is_screencast_;
}

bool FakeVideoCapturer::GetPreferredFourccs(std::vector<uint32_t>* fourccs) {
  fourccs->push_back(cricket::FOURCC_I420);
  fourccs->push_back(cricket::FOURCC_MJPG);
  return true;
}

void FakeVideoCapturer::SetRotation(webrtc::VideoRotation rotation) {
  rotation_ = rotation;
}

webrtc::VideoRotation FakeVideoCapturer::GetRotation() {
  return rotation_;
}

FakeVideoCapturerWithTaskQueue::FakeVideoCapturerWithTaskQueue(
    bool is_screencast)
    : FakeVideoCapturer(is_screencast) {}

FakeVideoCapturerWithTaskQueue::FakeVideoCapturerWithTaskQueue() {}

bool FakeVideoCapturerWithTaskQueue::CaptureFrame() {
  bool ret = false;
  RunSynchronouslyOnTaskQueue(
      [this, &ret]() { ret = FakeVideoCapturer::CaptureFrame(); });
  return ret;
}

bool FakeVideoCapturerWithTaskQueue::CaptureCustomFrame(int width,
                                                        int height,
                                                        uint32_t fourcc) {
  bool ret = false;
  RunSynchronouslyOnTaskQueue([this, &ret, width, height, fourcc]() {
    ret = FakeVideoCapturer::CaptureCustomFrame(width, height, fourcc);
  });
  return ret;
}

bool FakeVideoCapturerWithTaskQueue::CaptureCustomFrame(
    int width,
    int height,
    int64_t timestamp_interval,
    uint32_t fourcc) {
  bool ret = false;
  RunSynchronouslyOnTaskQueue(
      [this, &ret, width, height, timestamp_interval, fourcc]() {
        ret = FakeVideoCapturer::CaptureCustomFrame(width, height,
                                                    timestamp_interval, fourcc);
      });
  return ret;
}

}  // namespace cricket
