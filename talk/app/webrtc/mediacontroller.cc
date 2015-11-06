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

#include "talk/session/media/channelmanager.h"
#include "webrtc/base/bind.h"
#include "webrtc/base/checks.h"
#include "webrtc/call.h"

namespace {

const int kMinBandwidthBps = 30000;
const int kStartBandwidthBps = 300000;
const int kMaxBandwidthBps = 2000000;

class MediaController : public webrtc::MediaControllerInterface,
                        public sigslot::has_slots<> {
 public:
  MediaController(rtc::Thread* worker_thread,
                  cricket::ChannelManager* channel_manager)
      : worker_thread_(worker_thread), channel_manager_(channel_manager) {
    RTC_DCHECK(nullptr != worker_thread);
    worker_thread_->Invoke<void>(
        rtc::Bind(&MediaController::Construct_w, this,
                  channel_manager_->media_engine()));
  }
  ~MediaController() override {
    worker_thread_->Invoke<void>(rtc::Bind(&MediaController::Destruct_w, this));
  }

  webrtc::Call* call_w() override {
    RTC_DCHECK(worker_thread_->IsCurrent());
    return call_.get();
  }

  cricket::ChannelManager* channel_manager() const override {
    return channel_manager_;
  }

 private:
  void Construct_w(cricket::MediaEngineInterface* media_engine) {
    RTC_DCHECK(worker_thread_->IsCurrent());
    RTC_DCHECK(media_engine);
    webrtc::Call::Config config;
    config.audio_state = media_engine->GetAudioState();
    config.bitrate_config.min_bitrate_bps = kMinBandwidthBps;
    config.bitrate_config.start_bitrate_bps = kStartBandwidthBps;
    config.bitrate_config.max_bitrate_bps = kMaxBandwidthBps;
    call_.reset(webrtc::Call::Create(config));
  }
  void Destruct_w() {
    RTC_DCHECK(worker_thread_->IsCurrent());
    call_.reset();
  }

  rtc::Thread* const worker_thread_;
  cricket::ChannelManager* const channel_manager_;
  rtc::scoped_ptr<webrtc::Call> call_;

  RTC_DISALLOW_IMPLICIT_CONSTRUCTORS(MediaController);
};
}  // namespace {

namespace webrtc {

MediaControllerInterface* MediaControllerInterface::Create(
    rtc::Thread* worker_thread,
    cricket::ChannelManager* channel_manager) {
  return new MediaController(worker_thread, channel_manager);
}
}  // namespace webrtc
