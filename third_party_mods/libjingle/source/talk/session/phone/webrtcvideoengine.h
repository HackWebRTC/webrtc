/*
 * libjingle
 * Copyright 2004--2011, Google Inc.
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

#ifndef TALK_SESSION_PHONE_WEBRTCVIDEOENGINE_H_
#define TALK_SESSION_PHONE_WEBRTCVIDEOENGINE_H_

#include <vector>

#include "talk/base/scoped_ptr.h"
#include "talk/session/phone/videocommon.h"
#include "talk/session/phone/codec.h"
#include "talk/session/phone/channel.h"
#include "talk/session/phone/mediaengine.h"
#include "talk/session/phone/webrtccommon.h"

namespace webrtc {
class VideoCaptureModule;
class VideoRender;
}

namespace cricket {
struct Device;
class VideoRenderer;
class ViEWrapper;
class VoiceMediaChannel;
class WebRtcRenderAdapter;
class WebRtcVideoMediaChannel;
class WebRtcVoiceEngine;

class WebRtcVideoEngine : public webrtc::ViEBaseObserver,
                          public webrtc::TraceCallback {
 public:
  // Creates the WebRtcVideoEngine with internal VideoCaptureModule.
  WebRtcVideoEngine();
  // Creates the WebRtcVideoEngine, and specifies the WebRtcVoiceEngine and
  // external VideoCaptureModule to use.
  WebRtcVideoEngine(WebRtcVoiceEngine* voice_engine,
                    webrtc::VideoCaptureModule* capture);
  // For testing purposes. Allows the WebRtcVoiceEngine and
  // ViEWrapper to be mocks.
  WebRtcVideoEngine(WebRtcVoiceEngine* voice_engine, ViEWrapper* vie_wrapper);
  ~WebRtcVideoEngine();

  bool Init();
  void Terminate();

  WebRtcVideoMediaChannel* CreateChannel(
      VoiceMediaChannel* voice_channel);
  bool FindCodec(const VideoCodec& codec);
  bool SetDefaultEncoderConfig(const VideoEncoderConfig& config);

  void RegisterChannel(WebRtcVideoMediaChannel* channel);
  void UnregisterChannel(WebRtcVideoMediaChannel* channel);

  ViEWrapper* video_engine() { return vie_wrapper_.get(); }
  int GetLastVideoEngineError();
  int GetCapabilities();
  bool SetOptions(int options);
  bool SetCaptureDevice(const Device* device);
  bool SetCaptureModule(webrtc::VideoCaptureModule* vcm);
  bool SetLocalRenderer(VideoRenderer* renderer);
  CaptureResult SetCapture(bool capture);
  const std::vector<VideoCodec>& codecs() const;
  void SetLogging(int min_sev, const char* filter);

  int GetLastEngineError();

  VideoEncoderConfig& default_encoder_config() {
    return default_encoder_config_;
  }

  void ConvertToCricketVideoCodec(const webrtc::VideoCodec& in_codec,
                                  VideoCodec& out_codec);

  bool ConvertFromCricketVideoCodec(const VideoCodec& in_codec,
                                    webrtc::VideoCodec& out_codec);

  sigslot::signal1<CaptureResult> SignalCaptureResult;

 private:
  struct VideoCodecPref {
    const char* payload_name;
    int payload_type;
    int pref;
  };

  static const VideoCodecPref kVideoCodecPrefs[];
  int GetCodecPreference(const char* name);

  void ApplyLogging();
  bool InitVideoEngine();
  void PerformanceAlarm(const unsigned int cpu_load);
  bool ReleaseCaptureDevice();
  virtual void Print(const webrtc::TraceLevel level, const char* trace_string,
                     const int length);

  typedef std::vector<WebRtcVideoMediaChannel*> VideoChannels;

  talk_base::scoped_ptr<ViEWrapper> vie_wrapper_;
  webrtc::VideoCaptureModule* capture_;
  bool external_capture_;
  int capture_id_;
  webrtc::VideoRender* renderer_;
  WebRtcVoiceEngine* voice_engine_;
  std::vector<VideoCodec> video_codecs_;
  VideoChannels channels_;
  int log_level_;
  VideoEncoderConfig default_encoder_config_;
  bool capture_started_;
  talk_base::scoped_ptr<WebRtcRenderAdapter> local_renderer_;
};

class WebRtcVideoMediaChannel : public VideoMediaChannel,
                                public webrtc::Transport {
 public:
  WebRtcVideoMediaChannel(
      WebRtcVideoEngine* engine, VoiceMediaChannel* voice_channel);
  ~WebRtcVideoMediaChannel();

  bool Init();
  virtual bool SetRecvCodecs(const std::vector<VideoCodec> &codecs);
  virtual bool SetSendCodecs(const std::vector<VideoCodec> &codecs);
  virtual bool SetRender(bool render);
  virtual bool SetSend(bool send);
  virtual bool AddStream(uint32 ssrc, uint32 voice_ssrc);
  virtual bool RemoveStream(uint32 ssrc);
  virtual bool SetRenderer(uint32 ssrc, VideoRenderer* renderer);
  virtual bool GetStats(VideoMediaInfo* info);
  virtual bool SendIntraFrame();
  virtual bool RequestIntraFrame();

  virtual void OnPacketReceived(talk_base::Buffer* packet);
  virtual void OnRtcpReceived(talk_base::Buffer* packet);
  virtual void SetSendSsrc(uint32 id);
  virtual bool SetRtcpCName(const std::string& cname);
  virtual bool Mute(bool on);
  virtual bool SetRecvRtpHeaderExtensions(
      const std::vector<RtpHeaderExtension>& extensions) {
    return false;
  }
  virtual bool SetSendRtpHeaderExtensions(
      const std::vector<RtpHeaderExtension>& extensions) {
    return false;
  }
  virtual bool SetSendBandwidth(bool autobw, int bps);
  virtual bool SetOptions(int options);

  WebRtcVideoEngine* engine() { return engine_; }
  VoiceMediaChannel* voice_channel() { return voice_channel_; }
  int video_channel() { return vie_channel_; }
  bool sending() { return sending_; }

 protected:
  int GetLastEngineError() { return engine()->GetLastEngineError(); }
  virtual int SendPacket(int channel, const void* data, int len);
  virtual int SendRTCPPacket(int channel, const void* data, int len);

 private:
  void EnableRtcp();
  void EnablePLI();
  void EnableTMMBR();

  WebRtcVideoEngine* engine_;
  VoiceMediaChannel* voice_channel_;
  int vie_channel_;
  bool sending_;
  bool render_started_;
  talk_base::scoped_ptr<webrtc::VideoCodec> send_codec_;
  talk_base::scoped_ptr<WebRtcRenderAdapter> remote_renderer_;
};
}  // namespace cricket

#endif  // TALK_SESSION_PHONE_WEBRTCVIDEOENGINE_H_
