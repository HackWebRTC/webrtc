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

#ifndef TALK_MEDIA_WEBRTC_FAKEWEBRTCCALL_H_
#define TALK_MEDIA_WEBRTC_FAKEWEBRTCCALL_H_

#include <vector>

#include "webrtc/call.h"
#include "webrtc/audio_receive_stream.h"
#include "webrtc/video_frame.h"
#include "webrtc/video_receive_stream.h"
#include "webrtc/video_send_stream.h"

namespace cricket {
class FakeAudioReceiveStream : public webrtc::AudioReceiveStream {
 public:
  explicit FakeAudioReceiveStream(
      const webrtc::AudioReceiveStream::Config& config);

  // webrtc::AudioReceiveStream implementation.
  webrtc::AudioReceiveStream::Stats GetStats() const override;

  const webrtc::AudioReceiveStream::Config& GetConfig() const;

  int received_packets() const { return received_packets_; }
  void IncrementReceivedPackets();

 private:
  // webrtc::ReceiveStream implementation.
  void Start() override {}
  void Stop() override {}
  void SignalNetworkState(webrtc::NetworkState state) override {}
  bool DeliverRtcp(const uint8_t* packet, size_t length) override {
    return true;
  }
  bool DeliverRtp(const uint8_t* packet, size_t length) override {
    return true;
  }

  webrtc::AudioReceiveStream::Config config_;
  int received_packets_;
};

class FakeVideoSendStream : public webrtc::VideoSendStream,
                            public webrtc::VideoCaptureInput {
 public:
  FakeVideoSendStream(const webrtc::VideoSendStream::Config& config,
                      const webrtc::VideoEncoderConfig& encoder_config);
  webrtc::VideoSendStream::Config GetConfig() const;
  webrtc::VideoEncoderConfig GetEncoderConfig() const;
  std::vector<webrtc::VideoStream> GetVideoStreams();

  bool IsSending() const;
  bool GetVp8Settings(webrtc::VideoCodecVP8* settings) const;
  bool GetVp9Settings(webrtc::VideoCodecVP9* settings) const;

  int GetNumberOfSwappedFrames() const;
  int GetLastWidth() const;
  int GetLastHeight() const;
  int64_t GetLastTimestamp() const;
  void SetStats(const webrtc::VideoSendStream::Stats& stats);

 private:
  void IncomingCapturedFrame(const webrtc::VideoFrame& frame) override;

  // webrtc::SendStream implementation.
  void Start() override;
  void Stop() override;
  void SignalNetworkState(webrtc::NetworkState state) override {}
  bool DeliverRtcp(const uint8_t* packet, size_t length) override {
    return true;
  }

  // webrtc::VideoSendStream implementation.
  webrtc::VideoSendStream::Stats GetStats() override;
  bool ReconfigureVideoEncoder(
      const webrtc::VideoEncoderConfig& config) override;
  webrtc::VideoCaptureInput* Input() override;

  bool sending_;
  webrtc::VideoSendStream::Config config_;
  webrtc::VideoEncoderConfig encoder_config_;
  bool codec_settings_set_;
  union VpxSettings {
    webrtc::VideoCodecVP8 vp8;
    webrtc::VideoCodecVP9 vp9;
  } vpx_settings_;
  int num_swapped_frames_;
  webrtc::VideoFrame last_frame_;
  webrtc::VideoSendStream::Stats stats_;
};

class FakeVideoReceiveStream : public webrtc::VideoReceiveStream {
 public:
  explicit FakeVideoReceiveStream(
      const webrtc::VideoReceiveStream::Config& config);

  webrtc::VideoReceiveStream::Config GetConfig();

  bool IsReceiving() const;

  void InjectFrame(const webrtc::VideoFrame& frame, int time_to_render_ms);

  void SetStats(const webrtc::VideoReceiveStream::Stats& stats);

 private:
  // webrtc::ReceiveStream implementation.
  void Start() override;
  void Stop() override;
  void SignalNetworkState(webrtc::NetworkState state) override {}
  bool DeliverRtcp(const uint8_t* packet, size_t length) override {
    return true;
  }
  bool DeliverRtp(const uint8_t* packet, size_t length) override {
    return true;
  }

  // webrtc::VideoReceiveStream implementation.
  webrtc::VideoReceiveStream::Stats GetStats() const override;

  webrtc::VideoReceiveStream::Config config_;
  bool receiving_;
  webrtc::VideoReceiveStream::Stats stats_;
};

class FakeCall : public webrtc::Call, public webrtc::PacketReceiver {
 public:
  explicit FakeCall(const webrtc::Call::Config& config);
  ~FakeCall() override;

  webrtc::Call::Config GetConfig() const;
  const std::vector<FakeVideoSendStream*>& GetVideoSendStreams();
  const std::vector<FakeVideoReceiveStream*>& GetVideoReceiveStreams();

  const std::vector<FakeAudioReceiveStream*>& GetAudioReceiveStreams();
  const FakeAudioReceiveStream* GetAudioReceiveStream(uint32_t ssrc);

  webrtc::NetworkState GetNetworkState() const;
  int GetNumCreatedSendStreams() const;
  int GetNumCreatedReceiveStreams() const;
  void SetStats(const webrtc::Call::Stats& stats);

 private:
  webrtc::AudioSendStream* CreateAudioSendStream(
      const webrtc::AudioSendStream::Config& config) override;
  void DestroyAudioSendStream(webrtc::AudioSendStream* send_stream) override;

  webrtc::AudioReceiveStream* CreateAudioReceiveStream(
      const webrtc::AudioReceiveStream::Config& config) override;
  void DestroyAudioReceiveStream(
      webrtc::AudioReceiveStream* receive_stream) override;

  webrtc::VideoSendStream* CreateVideoSendStream(
      const webrtc::VideoSendStream::Config& config,
      const webrtc::VideoEncoderConfig& encoder_config) override;
  void DestroyVideoSendStream(webrtc::VideoSendStream* send_stream) override;

  webrtc::VideoReceiveStream* CreateVideoReceiveStream(
      const webrtc::VideoReceiveStream::Config& config) override;
  void DestroyVideoReceiveStream(
      webrtc::VideoReceiveStream* receive_stream) override;
  webrtc::PacketReceiver* Receiver() override;

  DeliveryStatus DeliverPacket(webrtc::MediaType media_type,
                               const uint8_t* packet, size_t length) override;

  webrtc::Call::Stats GetStats() const override;

  void SetBitrateConfig(
      const webrtc::Call::Config::BitrateConfig& bitrate_config) override;
  void SignalNetworkState(webrtc::NetworkState state) override;

  webrtc::Call::Config config_;
  webrtc::NetworkState network_state_;
  webrtc::Call::Stats stats_;
  std::vector<FakeVideoSendStream*> video_send_streams_;
  std::vector<FakeVideoReceiveStream*> video_receive_streams_;
  std::vector<FakeAudioReceiveStream*> audio_receive_streams_;

  int num_created_send_streams_;
  int num_created_receive_streams_;
};

}  // namespace cricket
#endif  // TALK_MEDIA_WEBRTC_WEBRTCVIDEOENGINE2_UNITTEST_H_
