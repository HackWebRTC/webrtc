/*
 * libjingle
 * Copyright 2014 Google Inc.
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

#ifndef TALK_MEDIA_WEBRTC_WEBRTCVIDEOENGINE2_UNITTEST_H_
#define TALK_MEDIA_WEBRTC_WEBRTCVIDEOENGINE2_UNITTEST_H_

#include <map>
#include <vector>

#include "webrtc/call.h"
#include "webrtc/video_receive_stream.h"
#include "webrtc/video_send_stream.h"

namespace cricket {
class FakeVideoSendStream : public webrtc::VideoSendStream,
                            public webrtc::VideoSendStreamInput {
 public:
  FakeVideoSendStream(const webrtc::VideoSendStream::Config& config,
                      const webrtc::VideoEncoderConfig& encoder_config);
  webrtc::VideoSendStream::Config GetConfig() const;
  webrtc::VideoEncoderConfig GetEncoderConfig() const;
  std::vector<webrtc::VideoStream> GetVideoStreams();

  bool IsSending() const;
  bool GetVp8Settings(webrtc::VideoCodecVP8* settings) const;

  int GetNumberOfSwappedFrames() const;
  int GetLastWidth() const;
  int GetLastHeight() const;
  void SetStats(const webrtc::VideoSendStream::Stats& stats);

 private:
  void SwapFrame(webrtc::I420VideoFrame* frame) override;
  webrtc::VideoSendStream::Stats GetStats() override;

  bool ReconfigureVideoEncoder(
      const webrtc::VideoEncoderConfig& config) override;

  webrtc::VideoSendStreamInput* Input() override;

  void Start() override;
  void Stop() override;

  bool sending_;
  webrtc::VideoSendStream::Config config_;
  webrtc::VideoEncoderConfig encoder_config_;
  bool codec_settings_set_;
  webrtc::VideoCodecVP8 vp8_settings_;
  int num_swapped_frames_;
  webrtc::I420VideoFrame last_frame_;
  webrtc::VideoSendStream::Stats stats_;
};

class FakeVideoReceiveStream : public webrtc::VideoReceiveStream {
 public:
  explicit FakeVideoReceiveStream(
      const webrtc::VideoReceiveStream::Config& config);

  webrtc::VideoReceiveStream::Config GetConfig();

  bool IsReceiving() const;

  void InjectFrame(const webrtc::I420VideoFrame& frame, int time_to_render_ms);

  void SetStats(const webrtc::VideoReceiveStream::Stats& stats);

 private:
  webrtc::VideoReceiveStream::Stats GetStats() const override;

  void Start() override;
  void Stop() override;

  webrtc::VideoReceiveStream::Config config_;
  bool receiving_;
  webrtc::VideoReceiveStream::Stats stats_;
};

class FakeCall : public webrtc::Call, public webrtc::PacketReceiver {
 public:
  FakeCall(const webrtc::Call::Config& config);
  ~FakeCall();

  void SetVideoCodecs(const std::vector<webrtc::VideoCodec> codecs);

  webrtc::Call::Config GetConfig() const;
  std::vector<FakeVideoSendStream*> GetVideoSendStreams();
  std::vector<FakeVideoReceiveStream*> GetVideoReceiveStreams();

  webrtc::VideoCodec GetEmptyVideoCodec();

  webrtc::VideoCodec GetVideoCodecVp8();
  webrtc::VideoCodec GetVideoCodecVp9();

  std::vector<webrtc::VideoCodec> GetDefaultVideoCodecs();

  webrtc::Call::NetworkState GetNetworkState() const;
  int GetNumCreatedSendStreams() const;
  int GetNumCreatedReceiveStreams() const;
  void SetStats(const webrtc::Call::Stats& stats);

 private:
  webrtc::VideoSendStream* CreateVideoSendStream(
      const webrtc::VideoSendStream::Config& config,
      const webrtc::VideoEncoderConfig& encoder_config) override;

  void DestroyVideoSendStream(webrtc::VideoSendStream* send_stream) override;

  webrtc::VideoReceiveStream* CreateVideoReceiveStream(
      const webrtc::VideoReceiveStream::Config& config) override;

  void DestroyVideoReceiveStream(
      webrtc::VideoReceiveStream* receive_stream) override;
  webrtc::PacketReceiver* Receiver() override;
  DeliveryStatus DeliverPacket(const uint8_t* packet, size_t length) override;

  webrtc::Call::Stats GetStats() const override;

  void SetBitrateConfig(
      const webrtc::Call::Config::BitrateConfig& bitrate_config) override;
  void SignalNetworkState(webrtc::Call::NetworkState state) override;

  webrtc::Call::Config config_;
  webrtc::Call::NetworkState network_state_;
  webrtc::Call::Stats stats_;
  std::vector<webrtc::VideoCodec> codecs_;
  std::vector<FakeVideoSendStream*> video_send_streams_;
  std::vector<FakeVideoReceiveStream*> video_receive_streams_;

  int num_created_send_streams_;
  int num_created_receive_streams_;
};

}  // namespace cricket
#endif  // TALK_MEDIA_WEBRTC_WEBRTCVIDEOENGINE2_UNITTEST_H_
