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

#ifndef TALK_APP_WEBRTC_VIDEOMEDIAENGINE_H_
#define TALK_APP_WEBRTC_VIDEOMEDIAENGINE_H_

#include <vector>

#include "talk/base/scoped_ptr.h"
#include "talk/session/phone/videocommon.h"
#include "talk/session/phone/codec.h"
#include "talk/session/phone/channel.h"
#include "talk/session/phone/mediaengine.h"
#include "talk/app/videoengine.h"


namespace cricket {
class VoiceMediaChannel;
class Device;
class VideoRenderer;
}

namespace webrtc {
class RtcVideoMediaChannel;
class RtcVoiceEngine;
class ExternalRenderer;

// CricketWebRTCVideoFrame only supports I420
class CricketWebRTCVideoFrame : public cricket::VideoFrame {
 public:
  CricketWebRTCVideoFrame();
  ~CricketWebRTCVideoFrame();

  void Attach(unsigned char* buffer, int bufferSize, int w, int h);

  virtual size_t GetWidth() const;
  virtual size_t GetHeight() const;
  virtual const uint8* GetYPlane() const;
  virtual const uint8* GetUPlane() const;
  virtual const uint8* GetVPlane() const;
  virtual uint8* GetYPlane();
  virtual uint8* GetUPlane();
  virtual uint8* GetVPlane();
  virtual int32 GetYPitch() const { return video_frame_.Width(); }
  virtual int32 GetUPitch() const { return video_frame_.Width() / 2; }
  virtual int32 GetVPitch() const { return video_frame_.Width() / 2; }

  virtual size_t GetPixelWidth() const { return 1; }
  virtual size_t GetPixelHeight() const { return 1; }
  virtual int64 GetElapsedTime() const { return elapsed_time_; }
  virtual int64 GetTimeStamp() const { return video_frame_.TimeStamp(); }
  virtual void SetElapsedTime(int64 elapsed_time) {
    elapsed_time_ = elapsed_time;
  }
  virtual void SetTimeStamp(int64 time_stamp) {
    video_frame_.SetTimeStamp(time_stamp);
  }

  virtual VideoFrame* Copy() const;
  virtual size_t CopyToBuffer(uint8* buffer, size_t size) const;
  virtual size_t ConvertToRgbBuffer(uint32 to_fourcc, uint8* buffer,
                                    size_t size, size_t pitch_rgb) const;
  virtual void StretchToPlanes(uint8* y, uint8* u, uint8* v,
                               int32 pitchY, int32 pitchU, int32 pitchV,
                               size_t width, size_t height,
                               bool interpolate, bool crop) const;
  virtual size_t StretchToBuffer(size_t w, size_t h, uint8* buffer, size_t size,
                                 bool interpolate, bool crop) const;
  virtual void StretchToFrame(VideoFrame* target, bool interpolate,
                              bool crop) const;
  virtual VideoFrame* Stretch(size_t w, size_t h, bool interpolate,
                              bool crop) const;

 private:
  webrtc::VideoFrame video_frame_;
  int64 elapsed_time_;
};

class CricketWebRTCVideoRenderer : public ExternalRenderer {
 public:
  CricketWebRTCVideoRenderer(cricket::VideoRenderer* renderer);

  virtual int FrameSizeChange(unsigned int width, unsigned int height,
                              unsigned int numberOfStreams);
  virtual int DeliverFrame(unsigned char* buffer, int bufferSize);
  virtual ~CricketWebRTCVideoRenderer();

 private:
  cricket::VideoRenderer*  renderer_;
  CricketWebRTCVideoFrame  video_frame_;
  unsigned int width_;
  unsigned int height_;
  unsigned int number_of_streams_;
};

class RtcVideoEngine : public ViEBaseObserver, public TraceCallback {
 public:
  RtcVideoEngine();
  explicit RtcVideoEngine(RtcVoiceEngine* voice_engine);
  ~RtcVideoEngine();

  bool Init();
  void Terminate();

  RtcVideoMediaChannel* CreateChannel(
      cricket::VoiceMediaChannel* voice_channel);
  bool FindCodec(const cricket::VideoCodec& codec);
  bool SetDefaultEncoderConfig(const cricket::VideoEncoderConfig& config);

  void RegisterChannel(RtcVideoMediaChannel* channel);
  void UnregisterChannel(RtcVideoMediaChannel* channel);

  VideoEngineWrapper* video_engine() { return video_engine_.get(); }
  int GetLastVideoEngineError();
  int GetCapabilities();
  bool SetOptions(int options);
  //TODO - need to change this interface for webrtc
  bool SetCaptureDevice(const cricket::Device* device);
  bool SetLocalRenderer(cricket::VideoRenderer* renderer);
  cricket::CaptureResult SetCapture(bool capture);
  const std::vector<cricket::VideoCodec>& codecs() const;
  void SetLogging(int min_sev, const char* filter);

  cricket::VideoEncoderConfig& default_encoder_config() {
    return default_encoder_config_;
  }
  cricket::VideoCodec& default_codec() {
    return default_codec_;
  }
  bool SetDefaultCodec(const cricket::VideoCodec& codec);

  void ConvertToCricketVideoCodec(const VideoCodec& in_codec,
                                  cricket::VideoCodec& out_codec);

  void ConvertFromCricketVideoCodec(const cricket::VideoCodec& in_codec,
                                    VideoCodec& out_codec);

  bool SetCaptureDevice(void* external_capture);

  sigslot::signal1<cricket::CaptureResult> SignalCaptureResult;
 private:

  struct VideoCodecPref {
    const char* payload_name;
    int payload_type;
    int pref;
  };

  static const VideoCodecPref kVideoCodecPrefs[];
  int GetCodecPreference(const char* name);

  void ApplyLogging();
  bool InitVideoEngine(RtcVoiceEngine* voice_engine);
  void PerformanceAlarm(const unsigned int cpuLoad);
  bool ReleaseCaptureDevice();
  virtual void Print(const TraceLevel level, const char *traceString,
                       const int length);

  typedef std::vector<RtcVideoMediaChannel*> VideoChannels;

  talk_base::scoped_ptr<VideoEngineWrapper> video_engine_;
  VideoCaptureModule* capture_;
  int capture_id_;
  RtcVoiceEngine* voice_engine_;
  std::vector<cricket::VideoCodec> video_codecs_;
  VideoChannels channels_;
  talk_base::CriticalSection channels_cs_;
  bool initialized_;
  int log_level_;
  cricket::VideoEncoderConfig default_encoder_config_;
  cricket::VideoCodec default_codec_;
  bool capture_started_;
  talk_base::scoped_ptr<CricketWebRTCVideoRenderer> local_renderer_;
};

class RtcVideoMediaChannel: public cricket::VideoMediaChannel,
                            public webrtc::Transport {
 public:
  RtcVideoMediaChannel(
      RtcVideoEngine* engine, cricket::VoiceMediaChannel* voice_channel);
  ~RtcVideoMediaChannel();

  bool Init();
  virtual bool SetRecvCodecs(const std::vector<cricket::VideoCodec> &codecs);
  virtual bool SetSendCodecs(const std::vector<cricket::VideoCodec> &codecs);
  virtual bool SetRender(bool render);
  virtual bool SetSend(bool send);
  virtual bool AddStream(uint32 ssrc, uint32 voice_ssrc);
  virtual bool RemoveStream(uint32 ssrc);
  virtual bool SetRenderer(uint32 ssrc, cricket::VideoRenderer* renderer);
  virtual bool SetExternalRenderer(uint32 ssrc, void* renderer);
  virtual bool GetStats(cricket::VideoMediaInfo* info);
  virtual bool SendIntraFrame();
  virtual bool RequestIntraFrame();

  virtual void OnPacketReceived(talk_base::Buffer* packet);
  virtual void OnRtcpReceived(talk_base::Buffer* packet);
  virtual void SetSendSsrc(uint32 id);
  virtual bool SetRtcpCName(const std::string& cname);
  virtual bool Mute(bool on);
  virtual bool SetRecvRtpHeaderExtensions(
      const std::vector<cricket::RtpHeaderExtension>& extensions) { return false; }
  virtual bool SetSendRtpHeaderExtensions(
      const std::vector<cricket::RtpHeaderExtension>& extensions) { return false; }
  virtual bool SetSendBandwidth(bool autobw, int bps);
  virtual bool SetOptions(int options);

  RtcVideoEngine* engine() { return engine_; }
  cricket::VoiceMediaChannel* voice_channel() { return voice_channel_; }
  int video_channel() { return video_channel_; }
  bool sending() { return sending_; }
  int GetMediaChannelId() { return video_channel_; }

 protected:
  virtual int SendPacket(int channel, const void* data, int len);
  virtual int SendRTCPPacket(int channel, const void* data, int len);

 private:
  void EnableRtcp();
  void EnablePLI();
  void EnableTMMBR();

  RtcVideoEngine* engine_;
  cricket::VoiceMediaChannel* voice_channel_;
  int video_channel_;
  bool sending_;
  bool render_started_;
  webrtc::VideoCodec send_codec_;
  talk_base::scoped_ptr<CricketWebRTCVideoRenderer> remote_renderer_;
};

}

#endif /* TALK_APP_WEBRTC_VIDEOMEDIAENGINE_H_ */
