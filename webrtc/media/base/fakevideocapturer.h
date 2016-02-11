/*
 *  Copyright (c) 2004 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MEDIA_BASE_FAKEVIDEOCAPTURER_H_
#define WEBRTC_MEDIA_BASE_FAKEVIDEOCAPTURER_H_

#include <string.h>

#include <vector>

#include "webrtc/base/timeutils.h"
#include "webrtc/media/base/videocapturer.h"
#include "webrtc/media/base/videocommon.h"
#include "webrtc/media/base/videoframe.h"
#ifdef HAVE_WEBRTC_VIDEO
#include "webrtc/media/webrtc/webrtcvideoframefactory.h"
#endif

namespace cricket {

// Fake video capturer that allows the test to manually pump in frames.
class FakeVideoCapturer : public cricket::VideoCapturer {
 public:
  FakeVideoCapturer()
      : running_(false),
        initial_unix_timestamp_(time(NULL) * rtc::kNumNanosecsPerSec),
        next_timestamp_(rtc::kNumNanosecsPerMillisec),
        is_screencast_(false),
        rotation_(webrtc::kVideoRotation_0) {
#ifdef HAVE_WEBRTC_VIDEO
    set_frame_factory(new cricket::WebRtcVideoFrameFactory());
#endif
    // Default supported formats. Use ResetSupportedFormats to over write.
    std::vector<cricket::VideoFormat> formats;
    formats.push_back(cricket::VideoFormat(1280, 720,
        cricket::VideoFormat::FpsToInterval(30), cricket::FOURCC_I420));
    formats.push_back(cricket::VideoFormat(640, 480,
        cricket::VideoFormat::FpsToInterval(30), cricket::FOURCC_I420));
    formats.push_back(cricket::VideoFormat(320, 240,
        cricket::VideoFormat::FpsToInterval(30), cricket::FOURCC_I420));
    formats.push_back(cricket::VideoFormat(160, 120,
        cricket::VideoFormat::FpsToInterval(30), cricket::FOURCC_I420));
    formats.push_back(cricket::VideoFormat(1280, 720,
        cricket::VideoFormat::FpsToInterval(60), cricket::FOURCC_I420));
    ResetSupportedFormats(formats);
  }
  ~FakeVideoCapturer() {
    SignalDestroyed(this);
  }

  void ResetSupportedFormats(const std::vector<cricket::VideoFormat>& formats) {
    SetSupportedFormats(formats);
  }
  bool CaptureFrame() {
    if (!GetCaptureFormat()) {
      return false;
    }
    return CaptureCustomFrame(GetCaptureFormat()->width,
                              GetCaptureFormat()->height,
                              GetCaptureFormat()->interval,
                              GetCaptureFormat()->fourcc);
  }
  bool CaptureCustomFrame(int width, int height, uint32_t fourcc) {
    // default to 30fps
    return CaptureCustomFrame(width, height, 33333333, fourcc);
  }
  bool CaptureCustomFrame(int width,
                          int height,
                          int64_t timestamp_interval,
                          uint32_t fourcc) {
    if (!running_) {
      return false;
    }
    // Currently, |fourcc| is always I420 or ARGB.
    // TODO(fbarchard): Extend SizeOf to take fourcc.
    uint32_t size = 0u;
    if (fourcc == cricket::FOURCC_ARGB) {
      size = width * 4 * height;
    } else if (fourcc == cricket::FOURCC_I420) {
      size = static_cast<uint32_t>(cricket::VideoFrame::SizeOf(width, height));
    } else {
      return false;  // Unsupported FOURCC.
    }
    if (size == 0u) {
      return false;  // Width and/or Height were zero.
    }

    cricket::CapturedFrame frame;
    frame.width = width;
    frame.height = height;
    frame.fourcc = fourcc;
    frame.data_size = size;
    frame.time_stamp = initial_unix_timestamp_ + next_timestamp_;
    next_timestamp_ += timestamp_interval;

    rtc::scoped_ptr<char[]> data(new char[size]);
    frame.data = data.get();
    // Copy something non-zero into the buffer so Validate wont complain that
    // the frame is all duplicate.
    memset(frame.data, 1, size / 2);
    memset(reinterpret_cast<uint8_t*>(frame.data) + (size / 2), 2,
           size - (size / 2));
    memcpy(frame.data, reinterpret_cast<const uint8_t*>(&fourcc), 4);
    frame.rotation = rotation_;
    // TODO(zhurunz): SignalFrameCaptured carry returned value to be able to
    // capture results from downstream.
    SignalFrameCaptured(this, &frame);
    return true;
  }

  void SignalCapturedFrame(cricket::CapturedFrame* frame) {
    SignalFrameCaptured(this, frame);
  }

  sigslot::signal1<FakeVideoCapturer*> SignalDestroyed;

  virtual cricket::CaptureState Start(const cricket::VideoFormat& format) {
    cricket::VideoFormat supported;
    if (GetBestCaptureFormat(format, &supported)) {
      SetCaptureFormat(&supported);
    }
    running_ = true;
    SetCaptureState(cricket::CS_RUNNING);
    return cricket::CS_RUNNING;
  }
  virtual void Stop() {
    running_ = false;
    SetCaptureFormat(NULL);
    SetCaptureState(cricket::CS_STOPPED);
  }
  virtual bool IsRunning() { return running_; }
  void SetScreencast(bool is_screencast) {
    is_screencast_ = is_screencast;
  }
  virtual bool IsScreencast() const { return is_screencast_; }
  bool GetPreferredFourccs(std::vector<uint32_t>* fourccs) {
    fourccs->push_back(cricket::FOURCC_I420);
    fourccs->push_back(cricket::FOURCC_MJPG);
    return true;
  }

  void SetRotation(webrtc::VideoRotation rotation) {
    rotation_ = rotation;
  }

  webrtc::VideoRotation GetRotation() { return rotation_; }

 private:
  bool running_;
  int64_t initial_unix_timestamp_;
  int64_t next_timestamp_;
  bool is_screencast_;
  webrtc::VideoRotation rotation_;
};

}  // namespace cricket

#endif  // WEBRTC_MEDIA_BASE_FAKEVIDEOCAPTURER_H_
