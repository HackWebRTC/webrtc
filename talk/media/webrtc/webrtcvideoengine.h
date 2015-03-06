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

#include "talk/media/base/codec.h"
#include "talk/media/base/videocommon.h"
#include "talk/media/webrtc/webrtccommon.h"
#include "talk/media/webrtc/webrtcexport.h"
#include "talk/media/webrtc/webrtcvideoencoderfactory.h"
#include "talk/session/media/channel.h"
#include "webrtc/base/scoped_ptr.h"
#include "webrtc/video_engine/include/vie_base.h"

#if !defined(LIBPEERCONNECTION_LIB) && \
    !defined(LIBPEERCONNECTION_IMPLEMENTATION)
// If you hit this, then you've tried to include this header from outside
// a shared library.  An instance of this class must only be created from
// within the library that actually implements it.
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

namespace rtc {
class CpuMonitor;
}  // namespace rtc

namespace cricket {

class CoordinatedVideoAdapter;
class ViETraceWrapper;
class ViEWrapper;
class VideoCapturer;
class VideoFrame;
class VideoProcessor;
class VideoRenderer;
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

// This set of methods is declared here for the sole purpose of sharing code
// between webrtc video engine v1 and v2.
std::vector<VideoCodec> DefaultVideoCodecList();
bool CodecNameMatches(const std::string& name1, const std::string& name2);
bool CodecIsInternallySupported(const std::string& codec_name);
bool IsNackEnabled(const VideoCodec& codec);
bool IsRembEnabled(const VideoCodec& codec);
void AddDefaultFeedbackParams(VideoCodec* codec);

class WebRtcVideoEngine : public sigslot::has_slots<> {
 public:
  // Creates the WebRtcVideoEngine with internal VideoCaptureModule.
  explicit WebRtcVideoEngine(WebRtcVoiceEngine* voice_engine);
  // For testing purposes. Allows the WebRtcVoiceEngine,
  // ViEWrapper and CpuMonitor to be mocks.
  // TODO(juberti): Remove the 3-arg ctor once fake tracing is implemented.
  WebRtcVideoEngine(WebRtcVoiceEngine* voice_engine,
                    ViEWrapper* vie_wrapper,
                    rtc::CpuMonitor* cpu_monitor);
  WebRtcVideoEngine(WebRtcVoiceEngine* voice_engine,
                    ViEWrapper* vie_wrapper,
                    ViETraceWrapper* tracing,
                    rtc::CpuMonitor* cpu_monitor);
  virtual ~WebRtcVideoEngine();

  // Basic video engine implementation.
  bool Init(rtc::Thread* worker_thread);
  void Terminate();

  int GetCapabilities();
  bool SetDefaultEncoderConfig(const VideoEncoderConfig& config);

  // TODO(pbos): Remove when all call sites use VideoOptions.
  virtual WebRtcVideoMediaChannel* CreateChannel(
      VoiceMediaChannel* voice_channel);
  virtual WebRtcVideoMediaChannel* CreateChannel(
      const VideoOptions& options,
      VoiceMediaChannel* voice_channel);

  const std::vector<VideoCodec>& codecs() const;
  const std::vector<RtpHeaderExtension>& rtp_header_extensions() const;
  void SetLogging(int min_sev, const char* filter);

  sigslot::repeater2<VideoCapturer*, CaptureState> SignalCaptureStateChange;

  // Set a WebRtcVideoDecoderFactory for external decoding. Video engine does
  // not take the ownership of |decoder_factory|. The caller needs to make sure
  // that |decoder_factory| outlives the video engine.
  void SetExternalDecoderFactory(WebRtcVideoDecoderFactory* decoder_factory);
  // Set a WebRtcVideoEncoderFactory for external encoding. Video engine does
  // not take the ownership of |encoder_factory|. The caller needs to make sure
  // that |encoder_factory| outlives the video engine.
  virtual void SetExternalEncoderFactory(
      WebRtcVideoEncoderFactory* encoder_factory);
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
  rtc::Thread* worker_thread() { return worker_thread_; }
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
  int GetNumOfChannels();

  VideoFormat GetStartCaptureFormat() const { return default_codec_format_; }

  rtc::CpuMonitor* cpu_monitor() { return cpu_monitor_.get(); }

 protected:
  class TraceCallbackImpl : public webrtc::TraceCallback {
   public:
    TraceCallbackImpl(WebRtcVoiceEngine* voice_engine)
        : voice_engine_(voice_engine) {}
    ~TraceCallbackImpl() override {}

   private:
    void Print(webrtc::TraceLevel level, const char* trace,
               int length) override;

    WebRtcVoiceEngine* const voice_engine_;
  };

  bool initialized() const {
    return initialized_;
  }

  // When a video processor registers with the engine.
  // SignalMediaFrame will be invoked for every video frame.
  // See videoprocessor.h for param reference.
  sigslot::signal3<uint32, VideoFrame*, bool*> SignalMediaFrame;

 private:
  typedef std::vector<WebRtcVideoMediaChannel*> VideoChannels;

  void Construct(ViEWrapper* vie_wrapper,
                 ViETraceWrapper* tracing,
                 WebRtcVoiceEngine* voice_engine,
                 rtc::CpuMonitor* cpu_monitor);
  bool SetDefaultCodec(const VideoCodec& codec);
  bool RebuildCodecList(const VideoCodec& max_codec);
  void SetTraceFilter(int filter);
  void SetTraceOptions(const std::string& options);
  bool InitVideoEngine();
  bool VerifyApt(const VideoCodec& in, int expected_apt) const;

  rtc::Thread* worker_thread_;
  rtc::scoped_ptr<ViEWrapper> vie_wrapper_;
  bool vie_wrapper_base_initialized_;
  rtc::scoped_ptr<ViETraceWrapper> tracing_;
  WebRtcVoiceEngine* const voice_engine_;
  rtc::scoped_ptr<webrtc::VideoRender> render_module_;
  rtc::scoped_ptr<WebRtcVideoEncoderFactory> simulcast_encoder_factory_;
  WebRtcVideoEncoderFactory* encoder_factory_;
  WebRtcVideoDecoderFactory* decoder_factory_;
  std::vector<VideoCodec> video_codecs_;
  std::vector<VideoCodec> default_video_codec_list_;
  std::vector<RtpHeaderExtension> rtp_header_extensions_;
  VideoFormat default_codec_format_;
  TraceCallbackImpl trace_callback_;

  bool initialized_;
  rtc::CriticalSection channels_crit_;
  VideoChannels channels_;

  bool capture_started_;

  rtc::scoped_ptr<rtc::CpuMonitor> cpu_monitor_;
};

struct CapturedFrameInfo;

// TODO(pthatcher): Add VideoOptions.
struct VideoSendParams {
  webrtc::VideoCodec codec;
  StreamParams stream;
};

class WebRtcVideoMediaChannel : public rtc::MessageHandler,
                                public VideoMediaChannel,
                                public webrtc::Transport {
 public:
  WebRtcVideoMediaChannel(WebRtcVideoEngine* engine,
                          VoiceMediaChannel* voice_channel);
  virtual ~WebRtcVideoMediaChannel();
  bool Init();

  WebRtcVideoEngine* engine() { return engine_; }
  VoiceMediaChannel* voice_channel() { return voice_channel_; }
  bool sending() const { return sending_; }

  // Public for testing purpose.
  uint32 GetDefaultSendChannelSsrc();
  int GetDefaultChannelId() const { return default_channel_id_; }

  // VideoMediaChannel implementation
  bool SetRecvCodecs(const std::vector<VideoCodec>& codecs) override;
  bool SetSendCodecs(const std::vector<VideoCodec>& codecs) override;
  bool GetSendCodec(VideoCodec* send_codec) override;
  bool SetSendStreamFormat(uint32 ssrc, const VideoFormat& format) override;
  bool SetRender(bool render) override;
  bool SetSend(bool send) override;

  bool AddSendStream(const StreamParams& sp) override;
  bool RemoveSendStream(uint32 ssrc) override;
  bool AddRecvStream(const StreamParams& sp) override;
  bool RemoveRecvStream(uint32 ssrc) override;
  bool SetRenderer(uint32 ssrc, VideoRenderer* renderer) override;
  bool GetStats(VideoMediaInfo* info) override;
  bool SetCapturer(uint32 ssrc, VideoCapturer* capturer) override;
  bool SendIntraFrame() override;
  bool RequestIntraFrame() override;

  void OnPacketReceived(rtc::Buffer* packet,
                        const rtc::PacketTime& packet_time) override;
  void OnRtcpReceived(rtc::Buffer* packet,
                      const rtc::PacketTime& packet_time) override;
  void OnReadyToSend(bool ready) override;
  bool MuteStream(uint32 ssrc, bool on) override;
  bool SetRecvRtpHeaderExtensions(
      const std::vector<RtpHeaderExtension>& extensions) override;
  bool SetSendRtpHeaderExtensions(
      const std::vector<RtpHeaderExtension>& extensions) override;
  int GetRtpSendTimeExtnId() const override;
  bool SetMaxSendBandwidth(int bps) override;
  bool SetOptions(const VideoOptions& options) override;
  bool GetOptions(VideoOptions* options) const override {
    *options = options_;
    return true;
  }
  void SetInterface(NetworkInterface* iface) override;
  void UpdateAspectRatio(int ratio_w, int ratio_h) override;

  // Public functions for use by tests and other specialized code.
  uint32 send_ssrc() const { return 0; }
  bool GetRenderer(uint32 ssrc, VideoRenderer** renderer);
  bool GetVideoAdapter(uint32 ssrc, CoordinatedVideoAdapter** video_adapter);
  void SendFrame(VideoCapturer* capturer, const VideoFrame* frame);
  bool SendFrame(WebRtcVideoChannelSendInfo* channel_info,
                 const VideoFrame* frame, bool is_screencast);

  // Thunk functions for use with HybridVideoEngine
  void OnLocalFrame(VideoCapturer* capturer, const VideoFrame* frame) {
    SendFrame(0u, frame, capturer->IsScreencast());
  }
  void OnLocalFrameFormat(VideoCapturer* capturer, const VideoFormat* format) {
  }

  // rtc::MessageHandler:
  void OnMessage(rtc::Message* msg) override;

 protected:
  void Terminate();
  int GetLastEngineError() { return engine()->GetLastEngineError(); }

  // webrtc::Transport:
  int SendPacket(int channel, const void* data, size_t len) override;
  int SendRTCPPacket(int channel, const void* data, size_t len) override;

  bool ConferenceModeIsEnabled() const {
    return options_.conference_mode.GetWithDefaultIfUnset(false);
  }

  // We take lots of things as input from applications (packaged in
  // params), but ViE wants lots of those packed instead as a
  // webrtc::VideoCodec.  This is where we convert between the inputs
  // we get from the applications and the input to give to ViE.  We
  // also configure the codec differently depending on the latest
  // frame that we have received (in particular, depending on the
  // resolution and whether the it was a screencast frame or not).
  virtual bool ConfigureVieCodecFromSendParams(
      int channel_id,
      const VideoSendParams& send_params,
      const CapturedFrameInfo& last_captured_frame_info,
      webrtc::VideoCodec* codec);
  // Checks the current bitrate estimate and modifies the bitrates
  // accordingly, including converting kAutoBandwidth to the correct defaults.
  virtual void SanitizeBitrates(
      int channel_id, webrtc::VideoCodec* video_codec);
  virtual void LogSendCodecChange(const std::string& reason);
  bool SetPrimaryAndRtxSsrcs(
      int channel_id, int idx, uint32 primary_ssrc,
      const StreamParams& send_params);
  bool SetLimitedNumberOfSendSsrcs(
      int channel_id, const StreamParams& send_params, size_t limit);
  virtual bool SetSendSsrcs(
      int channel_id, const StreamParams& send_params,
      const webrtc::VideoCodec& codec);

 private:
  typedef std::map<uint32, WebRtcVideoChannelRecvInfo*> RecvChannelMap;
  typedef std::map<uint32, WebRtcVideoChannelSendInfo*> SendChannelMap;
  typedef std::map<uint32, uint32> SsrcMap;
  typedef int (webrtc::ViERTP_RTCP::* ExtensionSetterFunction)(int, bool, int);

  enum MediaDirection { MD_RECV, MD_SEND, MD_SENDRECV };

  // Creates and initializes a ViE channel. When successful
  // |channel_id| will contain the new channel's ID. If |receiving| is
  // true |ssrc| is the remote ssrc. If |sending| is true the ssrc is
  // local ssrc. If both |receiving| and |sending| is true the ssrc
  // must be kDefaultChannelSsrcKey and the channel will be created as
  // a default channel. The ssrc must be different for receive
  // channels and it must be different for send channels. If the same
  // SSRC is being used for creating channel more than once, this
  // function will fail returning false.
  bool CreateChannel(uint32 ssrc_key, MediaDirection direction,
                     int* channel_id);
  bool CreateUnsignalledRecvChannel(uint32 ssrc_key, int* channel_id);
  bool ConfigureChannel(int channel_id, MediaDirection direction,
                        uint32 ssrc_key);
  bool ConfigureReceiving(int channel_id, uint32 remote_ssrc_key);
  bool ConfigureSending(int channel_id, uint32 local_ssrc_key);
  bool SetNackFec(int channel_id, int red_payload_type, int fec_payload_type,
                  bool nack_enabled);
  bool SetSendCodec(const webrtc::VideoCodec& codec);
  bool SetSendCodec(WebRtcVideoChannelSendInfo* send_channel,
                    const webrtc::VideoCodec& codec);
  bool SetSendParams(WebRtcVideoChannelSendInfo* send_channel,
                     const VideoSendParams& params);

  // Prepares the channel with channel id |info->channel_id()| to receive all
  // codecs in |receive_codecs_| and start receive packets.
  bool SetReceiveCodecs(WebRtcVideoChannelRecvInfo* info);
  // Returns the channel ID that receives the stream with SSRC |ssrc|.
  int GetRecvChannelId(uint32 ssrc);
  bool MaybeSetRtxSsrc(const StreamParams& sp, int channel_id);
  // Create and register an external endcoder if it's possible to do
  // so and one isn't already registered.
  bool MaybeRegisterExternalEncoder(
      WebRtcVideoChannelSendInfo* send_channel,
      const webrtc::VideoCodec& codec);
  // Helper function for starting the sending of media on all channels or
  // |channel_id|. Note that these two function do not change |sending_|.
  bool StartSend();
  bool StartSend(WebRtcVideoChannelSendInfo* send_channel);
  // Helper function for stop the sending of media on all channels or
  // |channel_id|. Note that these two function do not change |sending_|.
  bool StopSend();
  bool StopSend(WebRtcVideoChannelSendInfo* send_channel);
  bool SendIntraFrame(int channel_id);

  bool HasReadySendChannels();
  bool DefaultSendChannelIsActive();

  // Returns the ssrc key corresponding to the provided local SSRC in
  // |ssrc_key|. The return value is true upon success.  If the local
  // ssrc correspond to that of the default channel the key is
  // kDefaultChannelSsrcKey.  For all other channels the returned ssrc
  // key will be the same as the local ssrc.  If a stream has more
  // than one ssrc, the first (corresponding to
  // StreamParams::first_ssrc()) is used as the key.
  bool GetSendChannelSsrcKey(uint32 local_ssrc, uint32* ssrc_key);
  WebRtcVideoChannelSendInfo* GetDefaultSendChannel();
  WebRtcVideoChannelSendInfo* GetSendChannelBySsrcKey(uint32 ssrc_key);
  WebRtcVideoChannelSendInfo* GetSendChannelBySsrc(uint32 local_ssrc);
  // Creates a new unique ssrc key that can be used for inserting a
  // new send channel into |send_channels_|
  bool CreateSendChannelSsrcKey(uint32 local_ssrc, uint32* ssrc_key);
  // Get the number of the send channels |capturer| registered with.
  int GetSendChannelNum(VideoCapturer* capturer);

  bool IsDefaultChannelId(int channel_id) const {
    return channel_id == default_channel_id_;
  }
  bool DeleteSendChannel(uint32 ssrc_key);

  WebRtcVideoChannelRecvInfo* GetDefaultRecvChannel();
  WebRtcVideoChannelRecvInfo* GetRecvChannelBySsrc(uint32 ssrc);

  bool RemoveCapturer(uint32 ssrc);

  rtc::MessageQueue* worker_thread() { return engine_->worker_thread(); }
  void QueueBlackFrame(uint32 ssrc, int64 timestamp, int interval);
  void FlushBlackFrame(uint32 ssrc, int64 timestamp, int interval);

  void SetNetworkTransmissionState(bool is_transmitting);

  bool SetHeaderExtension(ExtensionSetterFunction setter, int channel_id,
                          const RtpHeaderExtension* extension);
  bool SetHeaderExtension(ExtensionSetterFunction setter, int channel_id,
                          const std::vector<RtpHeaderExtension>& extensions,
                          const char header_extension_uri[]);

  // Signal when cpu adaptation has no further scope to adapt.
  void OnCpuAdaptationUnable();

  // Connect |capturer| to WebRtcVideoMediaChannel if it is only registered
  // to one send channel, i.e. the first send channel.
  void MaybeConnectCapturer(VideoCapturer* capturer);
  // Disconnect |capturer| from WebRtcVideoMediaChannel if it is only registered
  // to one send channel, i.e. the last send channel.
  void MaybeDisconnectCapturer(VideoCapturer* capturer);

  bool RemoveRecvStreamInternal(uint32 ssrc);

  // Set the ssrc to use for RTCP receiver reports.
  void SetReceiverReportSsrc(uint32 ssrc);

  // Global state.
  WebRtcVideoEngine* engine_;
  VoiceMediaChannel* voice_channel_;
  int default_channel_id_;
  bool nack_enabled_;
  // Receiver Estimated Max Bitrate
  bool remb_enabled_;
  VideoOptions options_;

  // Global recv side state.
  // Note the default channel (default_channel_id_), i.e. the send channel
  // corresponding to all the receive channels (this must be done for REMB to
  // work properly), resides in both recv_channels_ and send_channels_ with the
  // ssrc key kDefaultChannelSsrcKey.
  RecvChannelMap recv_channels_;  // Contains all receive channels.
  // A map from the SSRCs on which RTX packets are received to the media SSRCs
  // the RTX packets are associated with. RTX packets will be delivered to the
  // streams matching the primary SSRC.
  SsrcMap rtx_to_primary_ssrc_;
  std::vector<webrtc::VideoCodec> receive_codecs_;
  // A map from codec payload types to their associated payload types, if any.
  // TODO(holmer): This is a temporary solution until webrtc::VideoCodec has
  // an associated payload type member, when it does we can rely on
  // receive_codecs_.
  std::map<int, int> associated_payload_types_;
  bool render_started_;
  uint32 first_receive_ssrc_;
  uint32 receiver_report_ssrc_;
  std::vector<RtpHeaderExtension> receive_extensions_;
  int num_unsignalled_recv_channels_;

  // Global send side state.
  SendChannelMap send_channels_;
  rtc::scoped_ptr<webrtc::VideoCodec> send_codec_;
  int send_rtx_type_;
  int send_red_type_;
  int send_fec_type_;
  bool sending_;
  std::vector<RtpHeaderExtension> send_extensions_;

  // The aspect ratio that the channel desires. 0 means there is no desired
  // aspect ratio
  int ratio_w_;
  int ratio_h_;
};

// An encoder factory that wraps Create requests for simulcastable codec types
// with a webrtc::SimulcastEncoderAdapter. Non simulcastable codec type
// requests are just passed through to the contained encoder factory.
// Exposed here for code to be shared with WebRtcVideoEngine2, not to be used
// externally.
class WebRtcSimulcastEncoderFactory
    : public cricket::WebRtcVideoEncoderFactory {
 public:
  // WebRtcSimulcastEncoderFactory doesn't take ownership of |factory|, which is
  // owned by e.g. PeerConnectionFactory.
  explicit WebRtcSimulcastEncoderFactory(
      cricket::WebRtcVideoEncoderFactory* factory);
  virtual ~WebRtcSimulcastEncoderFactory();

  static bool UseSimulcastEncoderFactory(const std::vector<VideoCodec>& codecs);

  webrtc::VideoEncoder* CreateVideoEncoder(
      webrtc::VideoCodecType type) override;
  const std::vector<VideoCodec>& codecs() const override;
  void DestroyVideoEncoder(webrtc::VideoEncoder* encoder) override;

 private:
  cricket::WebRtcVideoEncoderFactory* factory_;
  // A list of encoders that were created without being wrapped in a
  // SimulcastEncoderAdapter.
  std::vector<webrtc::VideoEncoder*> non_simulcast_encoders_;
};

}  // namespace cricket

#endif  // TALK_MEDIA_WEBRTCVIDEOENGINE_H_
