/*
 *  Copyright (c) 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_VIDEO_AUDIO_RECEIVE_STREAM_H_
#define WEBRTC_VIDEO_AUDIO_RECEIVE_STREAM_H_

#include "webrtc/audio_receive_stream.h"
#include "webrtc/modules/rtp_rtcp/interface/rtp_header_parser.h"

namespace webrtc {

class RemoteBitrateEstimator;

namespace internal {

class AudioReceiveStream : public webrtc::AudioReceiveStream {
 public:
  AudioReceiveStream(RemoteBitrateEstimator* remote_bitrate_estimator,
                     const webrtc::AudioReceiveStream::Config& config);
  ~AudioReceiveStream() override {}

  // webrtc::ReceiveStream implementation.
  void Start() override;
  void Stop() override;
  void SignalNetworkState(NetworkState state) override;
  bool DeliverRtcp(const uint8_t* packet, size_t length) override;
  bool DeliverRtp(const uint8_t* packet, size_t length) override;

  // webrtc::AudioReceiveStream implementation.
  webrtc::AudioReceiveStream::Stats GetStats() const override;

  const webrtc::AudioReceiveStream::Config& config() const {
    return config_;
  }

 private:
  RemoteBitrateEstimator* const remote_bitrate_estimator_;
  const webrtc::AudioReceiveStream::Config config_;
  rtc::scoped_ptr<RtpHeaderParser> rtp_header_parser_;
};
}  // namespace internal
}  // namespace webrtc

#endif  // WEBRTC_VIDEO_AUDIO_RECEIVE_STREAM_H_
