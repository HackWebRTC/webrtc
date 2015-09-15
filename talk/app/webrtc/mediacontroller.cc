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

#include "talk/app/webrtc/mediacontroller.h"

#include "webrtc/base/bind.h"
#include "webrtc/base/checks.h"
#include "webrtc/call.h"

namespace {

const int kMinBandwidthBps = 30000;
const int kStartBandwidthBps = 300000;
const int kMaxBandwidthBps = 2000000;

class MediaController : public webrtc::MediaControllerInterface {
 public:
  MediaController(rtc::Thread* worker_thread,
                  webrtc::VoiceEngine* voice_engine)
      : worker_thread_(worker_thread) {
    DCHECK(nullptr != worker_thread);
    worker_thread_->Invoke<void>(
        rtc::Bind(&MediaController::Construct_w, this, voice_engine));
  }
  ~MediaController() override {
    worker_thread_->Invoke<void>(
        rtc::Bind(&MediaController::Destruct_w, this));
  }

  webrtc::Call* call_w() override {
    DCHECK(worker_thread_->IsCurrent());
    return call_.get();
  }

 private:
  void Construct_w(webrtc::VoiceEngine* voice_engine)  {
    DCHECK(worker_thread_->IsCurrent());
    webrtc::Call::Config config;
    config.voice_engine = voice_engine;
    config.bitrate_config.min_bitrate_bps = kMinBandwidthBps;
    config.bitrate_config.start_bitrate_bps = kStartBandwidthBps;
    config.bitrate_config.max_bitrate_bps = kMaxBandwidthBps;
    call_.reset(webrtc::Call::Create(config));
  }
  void Destruct_w() {
    DCHECK(worker_thread_->IsCurrent());
    call_.reset(nullptr);
  }

  rtc::Thread* worker_thread_;
  rtc::scoped_ptr<webrtc::Call> call_;

  DISALLOW_IMPLICIT_CONSTRUCTORS(MediaController);
};
} // namespace {

namespace webrtc {

MediaControllerInterface* MediaControllerInterface::Create(
    rtc::Thread* worker_thread, webrtc::VoiceEngine* voice_engine) {
  return new MediaController(worker_thread, voice_engine);
}
} // namespace webrtc
