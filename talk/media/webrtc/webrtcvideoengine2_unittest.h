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
class FakeVideoSendStream : public webrtc::VideoSendStream {
 public:
  FakeVideoSendStream(const webrtc::VideoSendStream::Config& config,
                      const std::vector<webrtc::VideoStream>& video_streams,
                      const void* encoder_settings);
  webrtc::VideoSendStream::Config GetConfig();
  std::vector<webrtc::VideoStream> GetVideoStreams();

  bool IsSending() const;
  bool GetVp8Settings(webrtc::VideoCodecVP8* settings) const;

 private:
  virtual webrtc::VideoSendStream::Stats GetStats() const OVERRIDE;

  virtual bool ReconfigureVideoEncoder(
      const std::vector<webrtc::VideoStream>& streams,
      const void* encoder_specific);

  virtual webrtc::VideoSendStreamInput* Input() OVERRIDE;

  virtual void Start() OVERRIDE;
  virtual void Stop() OVERRIDE;

  bool sending_;
  webrtc::VideoSendStream::Config config_;
  std::vector<webrtc::VideoStream> video_streams_;
  bool codec_settings_set_;
  webrtc::VideoCodecVP8 vp8_settings_;
};

class FakeVideoReceiveStream : public webrtc::VideoReceiveStream {
 public:
  explicit FakeVideoReceiveStream(
      const webrtc::VideoReceiveStream::Config& config);

  webrtc::VideoReceiveStream::Config GetConfig();

  bool IsReceiving() const;

 private:
  virtual webrtc::VideoReceiveStream::Stats GetStats() const OVERRIDE;

  virtual void Start() OVERRIDE;
  virtual void Stop() OVERRIDE;
  virtual void GetCurrentReceiveCodec(webrtc::VideoCodec* codec);

  webrtc::VideoReceiveStream::Config config_;
  bool receiving_;
};

class FakeCall : public webrtc::Call {
 public:
  FakeCall();
  ~FakeCall();

  void SetVideoCodecs(const std::vector<webrtc::VideoCodec> codecs);

  std::vector<FakeVideoSendStream*> GetVideoSendStreams();
  std::vector<FakeVideoReceiveStream*> GetVideoReceiveStreams();

  webrtc::VideoCodec GetEmptyVideoCodec();

  webrtc::VideoCodec GetVideoCodecVp8();
  webrtc::VideoCodec GetVideoCodecVp9();

  std::vector<webrtc::VideoCodec> GetDefaultVideoCodecs();

 private:
  virtual webrtc::VideoSendStream* CreateVideoSendStream(
      const webrtc::VideoSendStream::Config& config,
      const std::vector<webrtc::VideoStream>& video_streams,
      const void* encoder_settings) OVERRIDE;

  virtual void DestroyVideoSendStream(
      webrtc::VideoSendStream* send_stream) OVERRIDE;

  virtual webrtc::VideoReceiveStream* CreateVideoReceiveStream(
      const webrtc::VideoReceiveStream::Config& config) OVERRIDE;

  virtual void DestroyVideoReceiveStream(
      webrtc::VideoReceiveStream* receive_stream) OVERRIDE;
  virtual webrtc::PacketReceiver* Receiver() OVERRIDE;

  virtual uint32_t SendBitrateEstimate() OVERRIDE;
  virtual uint32_t ReceiveBitrateEstimate() OVERRIDE;

  std::vector<webrtc::VideoCodec> codecs_;
  std::vector<FakeVideoSendStream*> video_send_streams_;
  std::vector<FakeVideoReceiveStream*> video_receive_streams_;
};

class FakeWebRtcVideoChannel2 : public WebRtcVideoChannel2 {
 public:
  FakeWebRtcVideoChannel2(FakeCall* call,
                          WebRtcVideoEngine2* engine,
                          VoiceMediaChannel* voice_channel);
  virtual ~FakeWebRtcVideoChannel2();

  VoiceMediaChannel* GetVoiceChannel();
  FakeCall* GetFakeCall();

 private:
  FakeCall* fake_call_;
  VoiceMediaChannel* voice_channel_;
};

class FakeWebRtcVideoMediaChannelFactory : public WebRtcVideoChannelFactory {
 public:
  FakeWebRtcVideoChannel2* GetFakeChannel(VideoMediaChannel* channel);

 private:
  virtual WebRtcVideoChannel2* Create(
      WebRtcVideoEngine2* engine,
      VoiceMediaChannel* voice_channel) OVERRIDE;

  std::map<VideoMediaChannel*, FakeWebRtcVideoChannel2*> channel_map_;
};


}  // namespace cricket
#endif  // TALK_MEDIA_WEBRTC_WEBRTCVIDEOENGINE2_UNITTEST_H_
