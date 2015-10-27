/*
 *  Copyright (c) 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_AUDIO_AUDIO_RECEIVE_STREAM_H_
#define WEBRTC_AUDIO_AUDIO_RECEIVE_STREAM_H_

#include "webrtc/audio_receive_stream.h"
#include "webrtc/audio/scoped_voe_interface.h"
#include "webrtc/base/thread_checker.h"
#include "webrtc/modules/rtp_rtcp/interface/rtp_header_parser.h"
#include "webrtc/voice_engine/include/voe_base.h"

namespace webrtc {

class RemoteBitrateEstimator;
class VoiceEngine;

namespace internal {

class AudioReceiveStream final : public webrtc::AudioReceiveStream {
 public:
  AudioReceiveStream(RemoteBitrateEstimator* remote_bitrate_estimator,
                     const webrtc::AudioReceiveStream::Config& config,
                     VoiceEngine* voice_engine);
  ~AudioReceiveStream() override;

  // webrtc::ReceiveStream implementation.
  void Start() override;
  void Stop() override;
  void SignalNetworkState(NetworkState state) override;
  bool DeliverRtcp(const uint8_t* packet, size_t length) override;
  bool DeliverRtp(const uint8_t* packet,
                  size_t length,
                  const PacketTime& packet_time) override;

  // webrtc::AudioReceiveStream implementation.
  webrtc::AudioReceiveStream::Stats GetStats() const override;

  const webrtc::AudioReceiveStream::Config& config() const;

 private:
  rtc::ThreadChecker thread_checker_;
  RemoteBitrateEstimator* const remote_bitrate_estimator_;
  const webrtc::AudioReceiveStream::Config config_;
  VoiceEngine* voice_engine_;
  // We hold one interface pointer to the VoE to make sure it is kept alive.
  ScopedVoEInterface<VoEBase> voe_base_;
  rtc::scoped_ptr<RtpHeaderParser> rtp_header_parser_;

  RTC_DISALLOW_IMPLICIT_CONSTRUCTORS(AudioReceiveStream);
};
}  // namespace internal
}  // namespace webrtc

#endif  // WEBRTC_AUDIO_AUDIO_RECEIVE_STREAM_H_
