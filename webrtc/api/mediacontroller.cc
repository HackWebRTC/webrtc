/*
 *  Copyright 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/api/mediacontroller.h"

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
