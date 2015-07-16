/*
 * libjingle
 * Copyright 2004 Google Inc.
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

#ifndef TALK_MEDIA_BASE_FAKEVIDEOCAPTURER_H_
#define TALK_MEDIA_BASE_FAKEVIDEOCAPTURER_H_

#include <string.h>

#include <vector>

#include "talk/media/base/videocapturer.h"
#include "talk/media/base/videocommon.h"
#include "talk/media/base/videoframe.h"
#include "webrtc/base/timeutils.h"
#ifdef HAVE_WEBRTC_VIDEO
#include "talk/media/webrtc/webrtcvideoframefactory.h"
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
  bool CaptureCustomFrame(int width, int height, uint32 fourcc) {
    // default to 30fps
    return CaptureCustomFrame(width, height, 33333333, fourcc);
  }
  bool CaptureCustomFrame(int width,
                          int height,
                          int64_t timestamp_interval,
                          uint32 fourcc) {
    if (!running_) {
      return false;
    }
    // Currently, |fourcc| is always I420 or ARGB.
    // TODO(fbarchard): Extend SizeOf to take fourcc.
    uint32 size = 0u;
    if (fourcc == cricket::FOURCC_ARGB) {
      size = width * 4 * height;
    } else if (fourcc == cricket::FOURCC_I420) {
      size = static_cast<uint32>(cricket::VideoFrame::SizeOf(width, height));
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
    frame.elapsed_time = next_timestamp_;
    frame.time_stamp = initial_unix_timestamp_ + next_timestamp_;
    next_timestamp_ += timestamp_interval;

    rtc::scoped_ptr<char[]> data(new char[size]);
    frame.data = data.get();
    // Copy something non-zero into the buffer so Validate wont complain that
    // the frame is all duplicate.
    memset(frame.data, 1, size / 2);
    memset(reinterpret_cast<uint8*>(frame.data) + (size / 2), 2,
         size - (size / 2));
    memcpy(frame.data, reinterpret_cast<const uint8*>(&fourcc), 4);
    frame.rotation = rotation_;
    // TODO(zhurunz): SignalFrameCaptured carry returned value to be able to
    // capture results from downstream.
    SignalFrameCaptured(this, &frame);
    return true;
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
  bool GetPreferredFourccs(std::vector<uint32>* fourccs) {
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
  int64 initial_unix_timestamp_;
  int64 next_timestamp_;
  bool is_screencast_;
  webrtc::VideoRotation rotation_;
};

}  // namespace cricket

#endif  // TALK_MEDIA_BASE_FAKEVIDEOCAPTURER_H_
