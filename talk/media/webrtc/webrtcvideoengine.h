/*
 * libjingle
 * Copyright 2004 Google Inc.
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

#ifndef TALK_MEDIA_WEBRTCVIDEOENGINE_H_
#define TALK_MEDIA_WEBRTCVIDEOENGINE_H_

#include <map>
#include <vector>

#include "talk/base/scoped_ptr.h"
#include "talk/media/base/codec.h"
#include "talk/media/base/videocommon.h"
#include "talk/media/webrtc/webrtccommon.h"
#include "talk/media/webrtc/webrtcexport.h"
#include "talk/media/webrtc/webrtcvideoencoderfactory.h"
#include "talk/session/media/channel.h"
#include "webrtc/video_engine/include/vie_base.h"

#if !defined(LIBPEERCONNECTION_LIB) && \
    !defined(LIBPEERCONNECTION_IMPLEMENTATION)
#error "Bogus include."
#endif

namespace webrtc {
class VideoCaptureModule;
class VideoDecoder;
class VideoEncoder;
class VideoRender;
class ViEExternalCapture;
class ViERTP_RTCP;
}

namespace talk_base {
class CpuMonitor;
}  // namespace talk_base

namespace cricket {

class VideoCapturer;
class VideoFrame;
class VideoProcessor;
class VideoRenderer;
class ViETraceWrapper;
class ViEWrapper;
class VoiceMediaChannel;
class WebRtcDecoderObserver;
class WebRtcEncoderObserver;
class WebRtcLocalStreamInfo;
class WebRtcRenderAdapter;
class WebRtcVideoChannelRecvInfo;
class WebRtcVideoChannelSendInfo;
class WebRtcVideoDecoderFactory;
class WebRtcVideoEncoderFactory;
class WebRtcVideoMediaChannel;
class WebRtcVoiceEngine;

struct CapturedFrame;
struct Device;

class WebRtcVideoEngine : public sigslot::has_slots<>,
                          public webrtc::TraceCallback,
                          public WebRtcVideoEncoderFactory::Observer {
 public:
  // Creates the WebRtcVideoEngine with internal VideoCaptureModule.
  WebRtcVideoEngine();
  // For testing purposes. Allows the WebRtcVoiceEngine,
  // ViEWrapper and CpuMonitor to be mocks.
  // TODO(juberti): Remove the 3-arg ctor once fake tracing is implemented.
  WebRtcVideoEngine(WebRtcVoiceEngine* voice_engine,
                    ViEWrapper* vie_wrapper,
                    talk_base::CpuMonitor* cpu_monitor);
  WebRtcVideoEngine(WebRtcVoiceEngine* voice_engine,
                    ViEWrapper* vie_wrapper,
                    ViETraceWrapper* tracing,
                    talk_base::CpuMonitor* cpu_monitor);
  ~WebRtcVideoEngine();

  // Basic video engine implementation.
  bool Init(talk_base::Thread* worker_thread);
  void Terminate();

  int GetCapabilities();
  bool SetOptions(int options);
  bool SetDefaultEncoderConfig(const VideoEncoderConfig& config);

  WebRtcVideoMediaChannel* CreateChannel(VoiceMediaChannel* voice_channel);

  const std::vector<VideoCodec>& codecs() const;
  const std::vector<RtpHeaderExtension>& rtp_header_extensions() const;
  void SetLogging(int min_sev, const char* filter);

  bool SetLocalRenderer(VideoRenderer* renderer);
  sigslot::repeater2<VideoCapturer*, CaptureState> SignalCaptureStateChange;

  // Set the VoiceEngine for A/V sync. This can only be called before Init.
  bool SetVoiceEngine(WebRtcVoiceEngine* voice_engine);
  // Set a WebRtcVideoDecoderFactory for external decoding. Video engine does
  // not take the ownership of |decoder_factory|. The caller needs to make sure
  // that |decoder_factory| outlives the video engine.
  void SetExternalDecoderFactory(WebRtcVideoDecoderFactory* decoder_factory);
  // Set a WebRtcVideoEncoderFactory for external encoding. Video engine does
  // not take the ownership of |encoder_factory|. The caller needs to make sure
  // that |encoder_factory| outlives the video engine.
  void SetExternalEncoderFactory(WebRtcVideoEncoderFactory* encoder_factory);
  // Enable the render module with timing control.
  bool EnableTimedRender();

  // Returns an external decoder for the given codec type. The return value
  // can be NULL if decoder factory is not given or it does not support the
  // codec type. The caller takes the ownership of the returned object.
  webrtc::VideoDecoder* CreateExternalDecoder(webrtc::VideoCodecType type);
  // Releases the decoder instance created by CreateExternalDecoder().
  void DestroyExternalDecoder(webrtc::VideoDecoder* decoder);

  // Returns an external encoder for the given codec type. The return value
  // can be NULL if encoder factory is not given or it does not support the
  // codec type. The caller takes the ownership of the returned object.
  webrtc::VideoEncoder* CreateExternalEncoder(webrtc::VideoCodecType type);
  // Releases the encoder instance created by CreateExternalEncoder().
  void DestroyExternalEncoder(webrtc::VideoEncoder* encoder);

  // Returns true if the codec type is supported by the external encoder.
  bool IsExternalEncoderCodecType(webrtc::VideoCodecType type) const;

  // Functions called by WebRtcVideoMediaChannel.
  talk_base::Thread* worker_thread() { return worker_thread_; }
  ViEWrapper* vie() { return vie_wrapper_.get(); }
  const VideoFormat& default_codec_format() const {
    return default_codec_format_;
  }
  int GetLastEngineError();
  bool FindCodec(const VideoCodec& in);
  bool CanSendCodec(const VideoCodec& in, const VideoCodec& current,
                    VideoCodec* out);
  void RegisterChannel(WebRtcVideoMediaChannel* channel);
  void UnregisterChannel(WebRtcVideoMediaChannel* channel);
  bool ConvertFromCricketVideoCodec(const VideoCodec& in_codec,
                                    webrtc::VideoCodec* out_codec);
  // Check whether the supplied trace should be ignored.
  bool ShouldIgnoreTrace(const std::string& trace);
  int GetNumOfChannels();

  VideoFormat GetStartCaptureFormat() const { return default_codec_format_; }

  talk_base::CpuMonitor* cpu_monitor() { return cpu_monitor_.get(); }

 protected:
  // When a video processor registers with the engine.
  // SignalMediaFrame will be invoked for every video frame.
  // See videoprocessor.h for param reference.
  sigslot::signal3<uint32, VideoFrame*, bool*> SignalMediaFrame;

 private:
  typedef std::vector<WebRtcVideoMediaChannel*> VideoChannels;
  struct VideoCodecPref {
    const char* name;
    int payload_type;
    int pref;
  };

  static const VideoCodecPref kVideoCodecPrefs[];
  static const VideoFormatPod kVideoFormats[];
  static const VideoFormatPod kDefaultVideoFormat;

  void Construct(ViEWrapper* vie_wrapper,
                 ViETraceWrapper* tracing,
                 WebRtcVoiceEngine* voice_engine,
                 talk_base::CpuMonitor* cpu_monitor);
  bool SetDefaultCodec(const VideoCodec& codec);
  bool RebuildCodecList(const VideoCodec& max_codec);
  void SetTraceFilter(int filter);
  void SetTraceOptions(const std::string& options);
  bool InitVideoEngine();

  // webrtc::TraceCallback implementation.
  virtual void Print(webrtc::TraceLevel level, const char* trace, int length);

  // WebRtcVideoEncoderFactory::Observer implementation.
  virtual void OnCodecsAvailable();

  talk_base::Thread* worker_thread_;
  talk_base::scoped_ptr<ViEWrapper> vie_wrapper_;
  bool vie_wrapper_base_initialized_;
  talk_base::scoped_ptr<ViETraceWrapper> tracing_;
  WebRtcVoiceEngine* voice_engine_;
  talk_base::scoped_ptr<webrtc::VideoRender> render_module_;
  WebRtcVideoEncoderFactory* encoder_factory_;
  WebRtcVideoDecoderFactory* decoder_factory_;
  std::vector<VideoCodec> video_codecs_;
  std::vector<RtpHeaderExtension> rtp_header_extensions_;
  VideoFormat default_codec_format_;

  bool initialized_;
  talk_base::CriticalSection channels_crit_;
  VideoChannels channels_;

  bool capture_started_;
  int local_renderer_w_;
  int local_renderer_h_;
  VideoRenderer* local_renderer_;

  // Critical section to protect the media processor register/unregister
  // while processing a frame
  talk_base::CriticalSection signal_media_critical_;

  talk_base::scoped_ptr<talk_base::CpuMonitor> cpu_monitor_;
};

class WebRtcVideoMediaChannel : public talk_base::MessageHandler,
                                public VideoMediaChannel,
                                public webrtc::Transport {
 public:
  WebRtcVideoMediaChannel(WebRtcVideoEngine* engine,
                          VoiceMediaChannel* voice_channel);
  ~WebRtcVideoMediaChannel();
  bool Init();

  WebRtcVideoEngine* engine() { return engine_; }
  VoiceMediaChannel* voice_channel() { return voice_channel_; }
  int video_channel() const { return vie_channel_; }
  bool sending() const { return sending_; }

  // VideoMediaChannel implementation
  virtual bool SetRecvCodecs(const std::vector<VideoCodec> &codecs);
  virtual bool SetSendCodecs(const std::vector<VideoCodec> &codecs);
  virtual bool GetSendCodec(VideoCodec* send_codec);
  virtual bool SetSendStreamFormat(uint32 ssrc, const VideoFormat& format);
  virtual bool SetRender(bool render);
  virtual bool SetSend(bool send);

  virtual bool AddSendStream(const StreamParams& sp);
  virtual bool RemoveSendStream(uint32 ssrc);
  virtual bool AddRecvStream(const StreamParams& sp);
  virtual bool RemoveRecvStream(uint32 ssrc);
  virtual bool SetRenderer(uint32 ssrc, VideoRenderer* renderer);
  virtual bool GetStats(VideoMediaInfo* info);
  virtual bool SetCapturer(uint32 ssrc, VideoCapturer* capturer);
  virtual bool SendIntraFrame();
  virtual bool RequestIntraFrame();

  virtual void OnPacketReceived(talk_base::Buffer* packet);
  virtual void OnRtcpReceived(talk_base::Buffer* packet);
  virtual void OnReadyToSend(bool ready);
  virtual bool MuteStream(uint32 ssrc, bool on);
  virtual bool SetRecvRtpHeaderExtensions(
      const std::vector<RtpHeaderExtension>& extensions);
  virtual bool SetSendRtpHeaderExtensions(
      const std::vector<RtpHeaderExtension>& extensions);
  virtual bool SetSendBandwidth(bool autobw, int bps);
  virtual bool SetOptions(const VideoOptions &options);
  virtual bool GetOptions(VideoOptions *options) const {
    *options = options_;
    return true;
  }
  virtual void SetInterface(NetworkInterface* iface);
  virtual void UpdateAspectRatio(int ratio_w, int ratio_h);

  // Public functions for use by tests and other specialized code.
  uint32 send_ssrc() const { return 0; }
  bool GetRenderer(uint32 ssrc, VideoRenderer** renderer);
  void SendFrame(VideoCapturer* capturer, const VideoFrame* frame);
  bool SendFrame(WebRtcVideoChannelSendInfo* channel_info,
                 const VideoFrame* frame, bool is_screencast);

  void AdaptAndSendFrame(VideoCapturer* capturer, const VideoFrame* frame);

  // Thunk functions for use with HybridVideoEngine
  void OnLocalFrame(VideoCapturer* capturer, const VideoFrame* frame) {
    SendFrame(0u, frame, capturer->IsScreencast());
  }
  void OnLocalFrameFormat(VideoCapturer* capturer, const VideoFormat* format) {
  }

  virtual void OnMessage(talk_base::Message* msg);

 protected:
  int GetLastEngineError() { return engine()->GetLastEngineError(); }
  virtual int SendPacket(int channel, const void* data, int len);
  virtual int SendRTCPPacket(int channel, const void* data, int len);

 private:
  typedef std::map<uint32, WebRtcVideoChannelRecvInfo*> RecvChannelMap;
  typedef std::map<uint32, WebRtcVideoChannelSendInfo*> SendChannelMap;
  typedef int (webrtc::ViERTP_RTCP::* ExtensionSetterFunction)(int, bool, int);

  enum MediaDirection { MD_RECV, MD_SEND, MD_SENDRECV };

  // Creates and initializes a ViE channel. When successful |channel_id| will
  // contain the new channel's ID. If |receiving| is true |ssrc| is the
  // remote ssrc. If |sending| is true the ssrc is local ssrc. If both
  // |receiving| and |sending| is true the ssrc must be 0 and the channel will
  // be created as a default channel. The ssrc must be different for receive
  // channels and it must be different for send channels. If the same SSRC is
  // being used for creating channel more than once, this function will fail
  // returning false.
  bool CreateChannel(uint32 ssrc_key, MediaDirection direction,
                     int* channel_id);
  bool ConfigureChannel(int channel_id, MediaDirection direction,
                        uint32 ssrc_key);
  bool ConfigureReceiving(int channel_id, uint32 remote_ssrc_key);
  bool ConfigureSending(int channel_id, uint32 local_ssrc_key);
  bool SetNackFec(int channel_id, int red_payload_type, int fec_payload_type,
                  bool nack_enabled);
  bool SetSendCodec(const webrtc::VideoCodec& codec, int min_bitrate,
                    int start_bitrate, int max_bitrate);
  bool SetSendCodec(WebRtcVideoChannelSendInfo* send_channel,
                    const webrtc::VideoCodec& codec, int min_bitrate,
                    int start_bitrate, int max_bitrate);
  void LogSendCodecChange(const std::string& reason);
  // Prepares the channel with channel id |info->channel_id()| to receive all
  // codecs in |receive_codecs_| and start receive packets.
  bool SetReceiveCodecs(WebRtcVideoChannelRecvInfo* info);
  // Returns the channel number that receives the stream with SSRC |ssrc|.
  int GetRecvChannelNum(uint32 ssrc);
  // Given captured video frame size, checks if we need to reset vie send codec.
  // |reset| is set to whether resetting has happened on vie or not.
  // Returns false on error.
  bool MaybeResetVieSendCodec(WebRtcVideoChannelSendInfo* send_channel,
                              int new_width, int new_height, bool is_screencast,
                              bool* reset);
  // Checks the current bitrate estimate and modifies the start bitrate
  // accordingly.
  void MaybeChangeStartBitrate(int channel_id, webrtc::VideoCodec* video_codec);
  // Helper function for starting the sending of media on all channels or
  // |channel_id|. Note that these two function do not change |sending_|.
  bool StartSend();
  bool StartSend(WebRtcVideoChannelSendInfo* send_channel);
  // Helper function for stop the sending of media on all channels or
  // |channel_id|. Note that these two function do not change |sending_|.
  bool StopSend();
  bool StopSend(WebRtcVideoChannelSendInfo* send_channel);
  bool SendIntraFrame(int channel_id);

  // Send with one local SSRC. Normal case.
  bool IsOneSsrcStream(const StreamParams& sp);

  bool HasReadySendChannels();

  // Send channel key returns the key corresponding to the provided local SSRC
  // in |key|. The return value is true upon success.
  // If the local ssrc correspond to that of the default channel the key is 0.
  // For all other channels the returned key will be the same as the local ssrc.
  bool GetSendChannelKey(uint32 local_ssrc, uint32* key);
  WebRtcVideoChannelSendInfo* GetSendChannel(VideoCapturer* video_capturer);
  WebRtcVideoChannelSendInfo* GetSendChannel(uint32 local_ssrc);
  // Creates a new unique key that can be used for inserting a new send channel
  // into |send_channels_|
  bool CreateSendChannelKey(uint32 local_ssrc, uint32* key);

  bool IsDefaultChannel(int channel_id) const {
    return channel_id == vie_channel_;
  }
  uint32 GetDefaultChannelSsrc();

  bool DeleteSendChannel(uint32 ssrc_key);

  bool InConferenceMode() const {
    return options_.conference_mode.GetWithDefaultIfUnset(false);
  }
  bool RemoveCapturer(uint32 ssrc);


  talk_base::MessageQueue* worker_thread() { return engine_->worker_thread(); }
  void QueueBlackFrame(uint32 ssrc, int64 timestamp, int framerate);
  void FlushBlackFrame(uint32 ssrc, int64 timestamp);

  void SetNetworkTransmissionState(bool is_transmitting);

  bool SetHeaderExtension(ExtensionSetterFunction setter, int channel_id,
                          const RtpHeaderExtension* extension);
  bool SetHeaderExtension(ExtensionSetterFunction setter, int channel_id,
                          const std::vector<RtpHeaderExtension>& extensions,
                          const char header_extension_uri[]);

  // Global state.
  WebRtcVideoEngine* engine_;
  VoiceMediaChannel* voice_channel_;
  int vie_channel_;
  bool nack_enabled_;
  // Receiver Estimated Max Bitrate
  bool remb_enabled_;
  VideoOptions options_;

  // Global recv side state.
  // Note the default channel (vie_channel_), i.e. the send channel
  // corresponding to all the receive channels (this must be done for REMB to
  // work properly), resides in both recv_channels_ and send_channels_ with the
  // ssrc key 0.
  RecvChannelMap recv_channels_;  // Contains all receive channels.
  std::vector<webrtc::VideoCodec> receive_codecs_;
  bool render_started_;
  uint32 first_receive_ssrc_;
  std::vector<RtpHeaderExtension> receive_extensions_;

  // Global send side state.
  SendChannelMap send_channels_;
  talk_base::scoped_ptr<webrtc::VideoCodec> send_codec_;
  int send_red_type_;
  int send_fec_type_;
  int send_min_bitrate_;
  int send_start_bitrate_;
  int send_max_bitrate_;
  bool sending_;
  std::vector<RtpHeaderExtension> send_extensions_;

  // The aspect ratio that the channel desires. 0 means there is no desired
  // aspect ratio
  int ratio_w_;
  int ratio_h_;
};

}  // namespace cricket

#endif  // TALK_MEDIA_WEBRTCVIDEOENGINE_H_
