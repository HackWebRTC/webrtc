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

#ifndef TALK_MEDIA_WEBRTC_WEBRTCVIDEOENGINE2_H_
#define TALK_MEDIA_WEBRTC_WEBRTCVIDEOENGINE2_H_

#include <map>
#include <string>
#include <vector>

#include "talk/media/base/mediaengine.h"
#include "talk/media/webrtc/webrtcvideochannelfactory.h"
#include "webrtc/base/cpumonitor.h"
#include "webrtc/base/scoped_ptr.h"
#include "webrtc/common_video/interface/i420_video_frame.h"
#include "webrtc/system_wrappers/interface/thread_annotations.h"
#include "webrtc/transport.h"
#include "webrtc/video_receive_stream.h"
#include "webrtc/video_renderer.h"
#include "webrtc/video_send_stream.h"

namespace webrtc {
class Call;
class VideoCaptureModule;
class VideoDecoder;
class VideoEncoder;
class VideoRender;
class VideoSendStreamInput;
class VideoReceiveStream;
}

namespace rtc {
class CpuMonitor;
class Thread;
}  // namespace rtc

namespace cricket {

class VideoCapturer;
class VideoFrame;
class VideoProcessor;
class VideoRenderer;
class VoiceMediaChannel;
class WebRtcVideoChannel2;
class WebRtcDecoderObserver;
class WebRtcEncoderObserver;
class WebRtcLocalStreamInfo;
class WebRtcRenderAdapter;
class WebRtcVideoChannelRecvInfo;
class WebRtcVideoChannelSendInfo;
class WebRtcVideoDecoderFactory;
class WebRtcVoiceEngine;

struct CapturedFrame;
struct Device;

class WebRtcVideoEngine2;
class WebRtcVideoChannel2;
class WebRtcVideoRenderer;

class UnsignalledSsrcHandler {
 public:
  enum Action {
    kDropPacket,
    kDeliverPacket,
  };
  virtual Action OnUnsignalledSsrc(VideoMediaChannel* engine,
                                   uint32_t ssrc) = 0;
};

// TODO(pbos): Remove, use external handlers only.
class DefaultUnsignalledSsrcHandler : public UnsignalledSsrcHandler {
 public:
  DefaultUnsignalledSsrcHandler();
  virtual Action OnUnsignalledSsrc(VideoMediaChannel* engine,
                                   uint32_t ssrc) OVERRIDE;

  VideoRenderer* GetDefaultRenderer() const;
  void SetDefaultRenderer(VideoMediaChannel* channel, VideoRenderer* renderer);

 private:
  uint32_t default_recv_ssrc_;
  VideoRenderer* default_renderer_;
};

class WebRtcVideoEncoderFactory2 {
 public:
  virtual ~WebRtcVideoEncoderFactory2();
  virtual std::vector<webrtc::VideoStream> CreateVideoStreams(
      const VideoCodec& codec,
      const VideoOptions& options,
      size_t num_streams);

  virtual webrtc::VideoEncoder* CreateVideoEncoder(
      const VideoCodec& codec,
      const VideoOptions& options);

  virtual void* CreateVideoEncoderSettings(
      const VideoCodec& codec,
      const VideoOptions& options);

  virtual void DestroyVideoEncoderSettings(const VideoCodec& codec,
                                           void* encoder_settings);

  virtual bool SupportsCodec(const cricket::VideoCodec& codec);
};

// WebRtcVideoEngine2 is used for the new native WebRTC Video API (webrtc:1667).
class WebRtcVideoEngine2 : public sigslot::has_slots<> {
 public:
  // Creates the WebRtcVideoEngine2 with internal VideoCaptureModule.
  WebRtcVideoEngine2();
  // Custom WebRtcVideoChannelFactory for testing purposes.
  explicit WebRtcVideoEngine2(WebRtcVideoChannelFactory* channel_factory);
  ~WebRtcVideoEngine2();

  // Basic video engine implementation.
  bool Init(rtc::Thread* worker_thread);
  void Terminate();

  int GetCapabilities();
  bool SetOptions(const VideoOptions& options);
  bool SetDefaultEncoderConfig(const VideoEncoderConfig& config);
  VideoEncoderConfig GetDefaultEncoderConfig() const;

  WebRtcVideoChannel2* CreateChannel(VoiceMediaChannel* voice_channel);

  const std::vector<VideoCodec>& codecs() const;
  const std::vector<RtpHeaderExtension>& rtp_header_extensions() const;
  void SetLogging(int min_sev, const char* filter);

  bool EnableTimedRender();
  // No-op, never used.
  bool SetLocalRenderer(VideoRenderer* renderer);
  // This is currently ignored.
  sigslot::repeater2<VideoCapturer*, CaptureState> SignalCaptureStateChange;

  // Set the VoiceEngine for A/V sync. This can only be called before Init.
  bool SetVoiceEngine(WebRtcVoiceEngine* voice_engine);

  // Functions called by WebRtcVideoChannel2.
  const VideoFormat& default_codec_format() const {
    return default_codec_format_;
  }

  bool FindCodec(const VideoCodec& in);
  bool CanSendCodec(const VideoCodec& in,
                    const VideoCodec& current,
                    VideoCodec* out);
  // Check whether the supplied trace should be ignored.
  bool ShouldIgnoreTrace(const std::string& trace);

  VideoFormat GetStartCaptureFormat() const { return default_codec_format_; }

  rtc::CpuMonitor* cpu_monitor() { return cpu_monitor_.get(); }

  virtual WebRtcVideoEncoderFactory2* GetVideoEncoderFactory();

 private:
  void Construct(WebRtcVideoChannelFactory* channel_factory,
                 WebRtcVoiceEngine* voice_engine,
                 rtc::CpuMonitor* cpu_monitor);

  rtc::Thread* worker_thread_;
  WebRtcVoiceEngine* voice_engine_;
  std::vector<VideoCodec> video_codecs_;
  std::vector<RtpHeaderExtension> rtp_header_extensions_;
  VideoFormat default_codec_format_;

  bool initialized_;

  bool capture_started_;

  // Critical section to protect the media processor register/unregister
  // while processing a frame
  rtc::CriticalSection signal_media_critical_;

  rtc::scoped_ptr<rtc::CpuMonitor> cpu_monitor_;
  WebRtcVideoChannelFactory* channel_factory_;
  WebRtcVideoEncoderFactory2 default_video_encoder_factory_;
};

class WebRtcVideoChannel2 : public rtc::MessageHandler,
                            public VideoMediaChannel,
                            public webrtc::newapi::Transport {
 public:
  WebRtcVideoChannel2(WebRtcVideoEngine2* engine,
                      VoiceMediaChannel* voice_channel,
                      WebRtcVideoEncoderFactory2* encoder_factory);
  // For testing purposes insert a pre-constructed call to verify that
  // WebRtcVideoChannel2 calls the correct corresponding methods.
  WebRtcVideoChannel2(webrtc::Call* call,
                      WebRtcVideoEngine2* engine,
                      WebRtcVideoEncoderFactory2* encoder_factory);
  ~WebRtcVideoChannel2();
  bool Init();

  // VideoMediaChannel implementation
  virtual bool SetRecvCodecs(const std::vector<VideoCodec>& codecs) OVERRIDE;
  virtual bool SetSendCodecs(const std::vector<VideoCodec>& codecs) OVERRIDE;
  virtual bool GetSendCodec(VideoCodec* send_codec) OVERRIDE;
  virtual bool SetSendStreamFormat(uint32 ssrc,
                                   const VideoFormat& format) OVERRIDE;
  virtual bool SetRender(bool render) OVERRIDE;
  virtual bool SetSend(bool send) OVERRIDE;

  virtual bool AddSendStream(const StreamParams& sp) OVERRIDE;
  virtual bool RemoveSendStream(uint32 ssrc) OVERRIDE;
  virtual bool AddRecvStream(const StreamParams& sp) OVERRIDE;
  virtual bool RemoveRecvStream(uint32 ssrc) OVERRIDE;
  virtual bool SetRenderer(uint32 ssrc, VideoRenderer* renderer) OVERRIDE;
  virtual bool GetStats(const StatsOptions& options,
                        VideoMediaInfo* info) OVERRIDE;
  virtual bool SetCapturer(uint32 ssrc, VideoCapturer* capturer) OVERRIDE;
  virtual bool SendIntraFrame() OVERRIDE;
  virtual bool RequestIntraFrame() OVERRIDE;

  virtual void OnPacketReceived(rtc::Buffer* packet,
                                const rtc::PacketTime& packet_time)
      OVERRIDE;
  virtual void OnRtcpReceived(rtc::Buffer* packet,
                              const rtc::PacketTime& packet_time)
      OVERRIDE;
  virtual void OnReadyToSend(bool ready) OVERRIDE;
  virtual bool MuteStream(uint32 ssrc, bool mute) OVERRIDE;

  // Set send/receive RTP header extensions. This must be done before creating
  // streams as it only has effect on future streams.
  virtual bool SetRecvRtpHeaderExtensions(
      const std::vector<RtpHeaderExtension>& extensions) OVERRIDE;
  virtual bool SetSendRtpHeaderExtensions(
      const std::vector<RtpHeaderExtension>& extensions) OVERRIDE;
  virtual bool SetStartSendBandwidth(int bps) OVERRIDE;
  virtual bool SetMaxSendBandwidth(int bps) OVERRIDE;
  virtual bool SetOptions(const VideoOptions& options) OVERRIDE;
  virtual bool GetOptions(VideoOptions* options) const OVERRIDE {
    *options = options_;
    return true;
  }
  virtual void SetInterface(NetworkInterface* iface) OVERRIDE;
  virtual void UpdateAspectRatio(int ratio_w, int ratio_h) OVERRIDE;

  virtual void OnMessage(rtc::Message* msg) OVERRIDE;

  // Implemented for VideoMediaChannelTest.
  bool sending() const { return sending_; }
  uint32 GetDefaultSendChannelSsrc() { return default_send_ssrc_; }
  bool GetRenderer(uint32 ssrc, VideoRenderer** renderer);

 private:
  void ConfigureReceiverRtp(webrtc::VideoReceiveStream::Config* config,
                            const StreamParams& sp) const;

  struct VideoCodecSettings {
    VideoCodecSettings();

    VideoCodec codec;
    webrtc::FecConfig fec;
    int rtx_payload_type;
  };

  // Wrapper for the sender part, this is where the capturer is connected and
  // frames are then converted from cricket frames to webrtc frames.
  class WebRtcVideoSendStream : public sigslot::has_slots<> {
   public:
    WebRtcVideoSendStream(
        webrtc::Call* call,
        WebRtcVideoEncoderFactory2* encoder_factory,
        const VideoOptions& options,
        const Settable<VideoCodecSettings>& codec_settings,
        const StreamParams& sp,
        const std::vector<webrtc::RtpExtension>& rtp_extensions);

    ~WebRtcVideoSendStream();
    void SetOptions(const VideoOptions& options);
    void SetCodec(const VideoCodecSettings& codec);
    void SetRtpExtensions(
        const std::vector<webrtc::RtpExtension>& rtp_extensions);

    void InputFrame(VideoCapturer* capturer, const VideoFrame* frame);
    bool SetCapturer(VideoCapturer* capturer);
    bool SetVideoFormat(const VideoFormat& format);
    void MuteStream(bool mute);
    bool DisconnectCapturer();

    void Start();
    void Stop();

    VideoSenderInfo GetVideoSenderInfo();

   private:
    // Parameters needed to reconstruct the underlying stream.
    // webrtc::VideoSendStream doesn't support setting a lot of options on the
    // fly, so when those need to be changed we tear down and reconstruct with
    // similar parameters depending on which options changed etc.
    struct VideoSendStreamParameters {
      VideoSendStreamParameters(
          const webrtc::VideoSendStream::Config& config,
          const VideoOptions& options,
          const Settable<VideoCodecSettings>& codec_settings);
      webrtc::VideoSendStream::Config config;
      VideoOptions options;
      Settable<VideoCodecSettings> codec_settings;
      // Sent resolutions + bitrates etc. by the underlying VideoSendStream,
      // typically changes when setting a new resolution or reconfiguring
      // bitrates.
      std::vector<webrtc::VideoStream> video_streams;
    };

    void SetCodecAndOptions(const VideoCodecSettings& codec,
                            const VideoOptions& options);
    void RecreateWebRtcStream();
    void SetDimensions(int width, int height);

    webrtc::Call* const call_;
    WebRtcVideoEncoderFactory2* const encoder_factory_;

    rtc::CriticalSection lock_;
    webrtc::VideoSendStream* stream_ GUARDED_BY(lock_);
    VideoSendStreamParameters parameters_ GUARDED_BY(lock_);

    VideoCapturer* capturer_ GUARDED_BY(lock_);
    bool sending_ GUARDED_BY(lock_);
    bool muted_ GUARDED_BY(lock_);
    VideoFormat format_ GUARDED_BY(lock_);

    rtc::CriticalSection frame_lock_;
    webrtc::I420VideoFrame video_frame_ GUARDED_BY(frame_lock_);
  };

  // Wrapper for the receiver part, contains configs etc. that are needed to
  // reconstruct the underlying VideoReceiveStream. Also serves as a wrapper
  // between webrtc::VideoRenderer and cricket::VideoRenderer.
  class WebRtcVideoReceiveStream : public webrtc::VideoRenderer {
   public:
    WebRtcVideoReceiveStream(
        webrtc::Call*,
        const webrtc::VideoReceiveStream::Config& config,
        const std::vector<VideoCodecSettings>& recv_codecs);
    ~WebRtcVideoReceiveStream();

    void SetRecvCodecs(const std::vector<VideoCodecSettings>& recv_codecs);
    void SetRtpExtensions(const std::vector<webrtc::RtpExtension>& extensions);

    virtual void RenderFrame(const webrtc::I420VideoFrame& frame,
                             int time_to_render_ms) OVERRIDE;

    void SetRenderer(cricket::VideoRenderer* renderer);
    cricket::VideoRenderer* GetRenderer();

    VideoReceiverInfo GetVideoReceiverInfo();

   private:
    void SetSize(int width, int height);
    void RecreateWebRtcStream();

    webrtc::Call* const call_;

    webrtc::VideoReceiveStream* stream_;
    webrtc::VideoReceiveStream::Config config_;

    rtc::CriticalSection renderer_lock_;
    cricket::VideoRenderer* renderer_ GUARDED_BY(renderer_lock_);
    int last_width_ GUARDED_BY(renderer_lock_);
    int last_height_ GUARDED_BY(renderer_lock_);
  };

  void Construct(webrtc::Call* call, WebRtcVideoEngine2* engine);
  void SetDefaultOptions();

  virtual bool SendRtp(const uint8_t* data, size_t len) OVERRIDE;
  virtual bool SendRtcp(const uint8_t* data, size_t len) OVERRIDE;

  void StartAllSendStreams();
  void StopAllSendStreams();

  static std::vector<VideoCodecSettings> MapCodecs(
      const std::vector<VideoCodec>& codecs);
  std::vector<VideoCodecSettings> FilterSupportedCodecs(
      const std::vector<VideoCodecSettings>& mapped_codecs);

  void FillSenderStats(VideoMediaInfo* info);
  void FillReceiverStats(VideoMediaInfo* info);
  void FillBandwidthEstimationStats(VideoMediaInfo* info);

  uint32_t rtcp_receiver_report_ssrc_;
  bool sending_;
  rtc::scoped_ptr<webrtc::Call> call_;
  uint32_t default_send_ssrc_;

  DefaultUnsignalledSsrcHandler default_unsignalled_ssrc_handler_;
  UnsignalledSsrcHandler* const unsignalled_ssrc_handler_;

  // Using primary-ssrc (first ssrc) as key.
  std::map<uint32, WebRtcVideoSendStream*> send_streams_;
  std::map<uint32, WebRtcVideoReceiveStream*> receive_streams_;

  Settable<VideoCodecSettings> send_codec_;
  std::vector<webrtc::RtpExtension> send_rtp_extensions_;

  WebRtcVideoEncoderFactory2* const encoder_factory_;
  std::vector<VideoCodecSettings> recv_codecs_;
  std::vector<webrtc::RtpExtension> recv_rtp_extensions_;
  VideoOptions options_;
};

}  // namespace cricket

#endif  // TALK_MEDIA_WEBRTC_WEBRTCVIDEOENGINE2_H_
