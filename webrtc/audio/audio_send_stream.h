/*
 *  Copyright (c) 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_AUDIO_AUDIO_SEND_STREAM_H_
#define WEBRTC_AUDIO_AUDIO_SEND_STREAM_H_

#include "webrtc/audio_send_stream.h"
#include "webrtc/audio_state.h"
#include "webrtc/base/thread_checker.h"

namespace webrtc {
namespace internal {

class AudioSendStream final : public webrtc::AudioSendStream {
 public:
  AudioSendStream(const webrtc::AudioSendStream::Config& config,
                  const rtc::scoped_refptr<webrtc::AudioState>& audio_state);
  ~AudioSendStream() override;

  // webrtc::SendStream implementation.
  void Start() override;
  void Stop() override;
  void SignalNetworkState(NetworkState state) override;
  bool DeliverRtcp(const uint8_t* packet, size_t length) override;

  // webrtc::AudioSendStream implementation.
  webrtc::AudioSendStream::Stats GetStats() const override;

  const webrtc::AudioSendStream::Config& config() const;

 private:
  rtc::ThreadChecker thread_checker_;
  const webrtc::AudioSendStream::Config config_;
  rtc::scoped_refptr<webrtc::AudioState> audio_state_;

  RTC_DISALLOW_IMPLICIT_CONSTRUCTORS(AudioSendStream);
};
}  // namespace internal
}  // namespace webrtc

#endif  // WEBRTC_AUDIO_AUDIO_SEND_STREAM_H_
