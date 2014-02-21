/*
 * libjingle
 * Copyright 2012, Google Inc.
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

// FakePeriodicVideoCapturer implements a fake cricket::VideoCapturer that
// creates video frames periodically after it has been started.

#ifndef TALK_APP_WEBRTC_TEST_FAKEPERIODICVIDEOCAPTURER_H_
#define TALK_APP_WEBRTC_TEST_FAKEPERIODICVIDEOCAPTURER_H_

#include "talk/base/thread.h"
#include "talk/media/base/fakevideocapturer.h"

namespace webrtc {

class FakePeriodicVideoCapturer : public cricket::FakeVideoCapturer {
 public:
  FakePeriodicVideoCapturer() {
    std::vector<cricket::VideoFormat> formats;
    formats.push_back(cricket::VideoFormat(1280, 720,
            cricket::VideoFormat::FpsToInterval(30), cricket::FOURCC_I420));
    formats.push_back(cricket::VideoFormat(640, 480,
        cricket::VideoFormat::FpsToInterval(30), cricket::FOURCC_I420));
    formats.push_back(cricket::VideoFormat(640, 360,
            cricket::VideoFormat::FpsToInterval(30), cricket::FOURCC_I420));
    formats.push_back(cricket::VideoFormat(320, 240,
        cricket::VideoFormat::FpsToInterval(30), cricket::FOURCC_I420));
    formats.push_back(cricket::VideoFormat(160, 120,
        cricket::VideoFormat::FpsToInterval(30), cricket::FOURCC_I420));
    ResetSupportedFormats(formats);
  };

  virtual cricket::CaptureState Start(const cricket::VideoFormat& format) {
    cricket::CaptureState state = FakeVideoCapturer::Start(format);
    if (state != cricket::CS_FAILED) {
      set_enable_video_adapter(false);  // Simplify testing.
      talk_base::Thread::Current()->Post(this, MSG_CREATEFRAME);
    }
    return state;
  }
  virtual void Stop() {
    talk_base::Thread::Current()->Clear(this);
  }
  // Inherited from MesageHandler.
  virtual void OnMessage(talk_base::Message* msg) {
    if (msg->message_id == MSG_CREATEFRAME) {
      if (IsRunning()) {
        CaptureFrame();
        talk_base::Thread::Current()->PostDelayed(static_cast<int>(
            GetCaptureFormat()->interval / talk_base::kNumNanosecsPerMillisec),
            this, MSG_CREATEFRAME);
        }
    } else {
      FakeVideoCapturer::OnMessage(msg);
    }
  }

 private:
  enum {
    // Offset  0xFF to make sure this don't collide with base class messages.
    MSG_CREATEFRAME = 0xFF
  };
};

}  // namespace webrtc

#endif  //  TALK_APP_WEBRTC_TEST_FAKEPERIODICVIDEOCAPTURER_H_
