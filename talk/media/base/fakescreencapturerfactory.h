/*
 * libjingle
 * Copyright 2012 Google Inc.
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

#ifndef TALK_MEDIA_BASE_FAKESCREENCAPTURERFACTORY_H_
#define TALK_MEDIA_BASE_FAKESCREENCAPTURERFACTORY_H_

#include "talk/media/base/fakevideocapturer.h"
#include "talk/media/base/videocapturerfactory.h"

namespace cricket {

class FakeScreenCapturerFactory
    : public cricket::ScreenCapturerFactory,
      public sigslot::has_slots<> {
 public:
  FakeScreenCapturerFactory()
      : window_capturer_(NULL),
        capture_state_(cricket::CS_STOPPED) {}

  virtual cricket::VideoCapturer* Create(const ScreencastId& window) {
    if (window_capturer_ != NULL) {
      return NULL;
    }
    window_capturer_ = new cricket::FakeVideoCapturer;
    window_capturer_->SignalDestroyed.connect(
        this,
        &FakeScreenCapturerFactory::OnWindowCapturerDestroyed);
    window_capturer_->SignalStateChange.connect(
        this,
        &FakeScreenCapturerFactory::OnStateChange);
    return window_capturer_;
  }

  cricket::FakeVideoCapturer* window_capturer() { return window_capturer_; }

  cricket::CaptureState capture_state() { return capture_state_; }

 private:
  void OnWindowCapturerDestroyed(cricket::FakeVideoCapturer* capturer) {
    if (capturer == window_capturer_) {
      window_capturer_ = NULL;
    }
  }
  void OnStateChange(cricket::VideoCapturer*, cricket::CaptureState state) {
    capture_state_ = state;
  }

  cricket::FakeVideoCapturer* window_capturer_;
  cricket::CaptureState capture_state_;
};

}  // namespace cricket

#endif  // TALK_MEDIA_BASE_FAKESCREENCAPTURERFACTORY_H_
