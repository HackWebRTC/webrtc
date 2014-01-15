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

#ifndef TALK_SESSION_MEDIA_CHANNEL_H_
#define TALK_SESSION_MEDIA_CHANNEL_H_

#include <string>
#include <vector>

#include "talk/base/asyncudpsocket.h"
#include "talk/base/criticalsection.h"
#include "talk/base/network.h"
#include "talk/base/sigslot.h"
#include "talk/base/window.h"
#include "talk/media/base/mediachannel.h"
#include "talk/media/base/mediaengine.h"
#include "talk/media/base/screencastid.h"
#include "talk/media/base/streamparams.h"
#include "talk/media/base/videocapturer.h"
#include "talk/p2p/base/session.h"
#include "talk/p2p/client/socketmonitor.h"
#include "talk/session/media/audiomonitor.h"
#include "talk/session/media/mediamonitor.h"
#include "talk/session/media/mediasession.h"
#include "talk/session/media/rtcpmuxfilter.h"
#include "talk/session/media/srtpfilter.h"
#include "talk/session/media/ssrcmuxfilter.h"

namespace cricket {

struct CryptoParams;
class MediaContentDescription;
struct TypingMonitorOptions;
class TypingMonitor;
struct ViewRequest;

enum SinkType {
  SINK_PRE_CRYPTO,  // Sink packets before encryption or after decryption.
  SINK_POST_CRYPTO  // Sink packets after encryption or before decryption.
};

// BaseChannel contains logic common to voice and video, including
// enable/mute, marshaling calls to a worker thread, and
// connection and media monitors.
//
// WARNING! SUBCLASSES MUST CALL Deinit() IN THEIR DESTRUCTORS!
// This is required to avoid a data race between the destructor modifying the
// vtable, and the media channel's thread using BaseChannel as the
// NetworkInterface.

class BaseChannel
    : public talk_base::MessageHandler, public sigslot::has_slots<>,
      public MediaChannel::NetworkInterface {
 public:
  BaseChannel(talk_base::Thread* thread, MediaEngineInterface* media_engine,
              MediaChannel* channel, BaseSession* session,
              const std::string& content_name, bool rtcp);
  virtual ~BaseChannel();
  bool Init(TransportChannel* transport_channel,
            TransportChannel* rtcp_transport_channel);
  // Deinit may be called multiple times and is simply ignored if it's alreay
  // done.
  void Deinit();

  talk_base::Thread* worker_thread() const { return worker_thread_; }
  BaseSession* session() const { return session_; }
  const std::string& content_name() { return content_name_; }
  TransportChannel* transport_channel() const {
    return transport_channel_;
  }
  TransportChannel* rtcp_transport_channel() const {
    return rtcp_transport_channel_;
  }
  bool enabled() const { return enabled_; }

  // This function returns true if we are using SRTP.
  bool secure() const { return srtp_filter_.IsActive(); }
  // The following function returns true if we are using
  // DTLS-based keying. If you turned off SRTP later, however
  // you could have secure() == false and dtls_secure() == true.
  bool secure_dtls() const { return dtls_keyed_; }
  // This function returns true if we require secure channel for call setup.
  bool secure_required() const { return secure_required_; }

  bool writable() const { return writable_; }
  bool IsStreamMuted(uint32 ssrc);

  // Channel control
  bool SetLocalContent(const MediaContentDescription* content,
                       ContentAction action,
                       std::string* error_desc);
  bool SetRemoteContent(const MediaContentDescription* content,
                        ContentAction action,
                        std::string* error_desc);

  bool Enable(bool enable);
  // Mute sending media on the stream with SSRC |ssrc|
  // If there is only one sending stream SSRC 0 can be used.
  bool MuteStream(uint32 ssrc, bool mute);

  // Multiplexing
  bool AddRecvStream(const StreamParams& sp);
  bool RemoveRecvStream(uint32 ssrc);
  bool AddSendStream(const StreamParams& sp);
  bool RemoveSendStream(uint32 ssrc);

  // Monitoring
  void StartConnectionMonitor(int cms);
  void StopConnectionMonitor();

  void set_srtp_signal_silent_time(uint32 silent_time) {
    srtp_filter_.set_signal_silent_time(silent_time);
  }

  void set_content_name(const std::string& content_name) {
    ASSERT(signaling_thread()->IsCurrent());
    ASSERT(!writable_);
    if (session_->state() != BaseSession::STATE_INIT) {
      LOG(LS_ERROR) << "Content name for a channel can be changed only "
                    << "when BaseSession is in STATE_INIT state.";
      return;
    }
    content_name_ = content_name;
  }

  template <class T>
  void RegisterSendSink(T* sink,
                        void (T::*OnPacket)(const void*, size_t, bool),
                        SinkType type) {
    talk_base::CritScope cs(&signal_send_packet_cs_);
    if (SINK_POST_CRYPTO == type) {
      SignalSendPacketPostCrypto.disconnect(sink);
      SignalSendPacketPostCrypto.connect(sink, OnPacket);
    } else {
      SignalSendPacketPreCrypto.disconnect(sink);
      SignalSendPacketPreCrypto.connect(sink, OnPacket);
    }
  }

  void UnregisterSendSink(sigslot::has_slots<>* sink,
                          SinkType type) {
    talk_base::CritScope cs(&signal_send_packet_cs_);
    if (SINK_POST_CRYPTO == type) {
      SignalSendPacketPostCrypto.disconnect(sink);
    } else {
      SignalSendPacketPreCrypto.disconnect(sink);
    }
  }

  bool HasSendSinks(SinkType type) {
    talk_base::CritScope cs(&signal_send_packet_cs_);
    if (SINK_POST_CRYPTO == type) {
      return !SignalSendPacketPostCrypto.is_empty();
    } else {
      return !SignalSendPacketPreCrypto.is_empty();
    }
  }

  template <class T>
  void RegisterRecvSink(T* sink,
                        void (T::*OnPacket)(const void*, size_t, bool),
                        SinkType type) {
    talk_base::CritScope cs(&signal_recv_packet_cs_);
    if (SINK_POST_CRYPTO == type) {
      SignalRecvPacketPostCrypto.disconnect(sink);
      SignalRecvPacketPostCrypto.connect(sink, OnPacket);
    } else {
      SignalRecvPacketPreCrypto.disconnect(sink);
      SignalRecvPacketPreCrypto.connect(sink, OnPacket);
    }
  }

  void UnregisterRecvSink(sigslot::has_slots<>* sink,
                          SinkType type) {
    talk_base::CritScope cs(&signal_recv_packet_cs_);
    if (SINK_POST_CRYPTO == type) {
      SignalRecvPacketPostCrypto.disconnect(sink);
    } else {
      SignalRecvPacketPreCrypto.disconnect(sink);
    }
  }

  bool HasRecvSinks(SinkType type) {
    talk_base::CritScope cs(&signal_recv_packet_cs_);
    if (SINK_POST_CRYPTO == type) {
      return !SignalRecvPacketPostCrypto.is_empty();
    } else {
      return !SignalRecvPacketPreCrypto.is_empty();
    }
  }

  SsrcMuxFilter* ssrc_filter() { return &ssrc_filter_; }

  const std::vector<StreamParams>& local_streams() const {
    return local_streams_;
  }
  const std::vector<StreamParams>& remote_streams() const {
    return remote_streams_;
  }

  // Used for latency measurements.
  sigslot::signal1<BaseChannel*> SignalFirstPacketReceived;

  // Used to alert UI when the muted status changes, perhaps autonomously.
  sigslot::repeater2<BaseChannel*, bool> SignalAutoMuted;

  // Made public for easier testing.
  void SetReadyToSend(TransportChannel* channel, bool ready);

 protected:
  MediaEngineInterface* media_engine() const { return media_engine_; }
  virtual MediaChannel* media_channel() const { return media_channel_; }
  void set_rtcp_transport_channel(TransportChannel* transport);
  bool was_ever_writable() const { return was_ever_writable_; }
  void set_local_content_direction(MediaContentDirection direction) {
    local_content_direction_ = direction;
  }
  void set_remote_content_direction(MediaContentDirection direction) {
    remote_content_direction_ = direction;
  }
  bool IsReadyToReceive() const;
  bool IsReadyToSend() const;
  talk_base::Thread* signaling_thread() { return session_->signaling_thread(); }
  SrtpFilter* srtp_filter() { return &srtp_filter_; }
  bool rtcp() const { return rtcp_; }

  void Send(uint32 id, talk_base::MessageData* pdata = NULL);
  void Post(uint32 id, talk_base::MessageData* pdata = NULL);
  void PostDelayed(int cmsDelay, uint32 id = 0,
                   talk_base::MessageData* pdata = NULL);
  void Clear(uint32 id = talk_base::MQID_ANY,
             talk_base::MessageList* removed = NULL);
  void FlushRtcpMessages();

  // NetworkInterface implementation, called by MediaEngine
  virtual bool SendPacket(talk_base::Buffer* packet,
                          talk_base::DiffServCodePoint dscp);
  virtual bool SendRtcp(talk_base::Buffer* packet,
                        talk_base::DiffServCodePoint dscp);
  virtual int SetOption(SocketType type, talk_base::Socket::Option o, int val);

  // From TransportChannel
  void OnWritableState(TransportChannel* channel);
  virtual void OnChannelRead(TransportChannel* channel,
                             const char* data,
                             size_t len,
                             const talk_base::PacketTime& packet_time,
                             int flags);
  void OnReadyToSend(TransportChannel* channel);

  bool PacketIsRtcp(const TransportChannel* channel, const char* data,
                    size_t len);
  bool SendPacket(bool rtcp, talk_base::Buffer* packet,
                  talk_base::DiffServCodePoint dscp);
  virtual bool WantsPacket(bool rtcp, talk_base::Buffer* packet);
  void HandlePacket(bool rtcp, talk_base::Buffer* packet,
                    const talk_base::PacketTime& packet_time);

  // Apply the new local/remote session description.
  void OnNewLocalDescription(BaseSession* session, ContentAction action);
  void OnNewRemoteDescription(BaseSession* session, ContentAction action);

  void EnableMedia_w();
  void DisableMedia_w();
  virtual bool MuteStream_w(uint32 ssrc, bool mute);
  bool IsStreamMuted_w(uint32 ssrc);
  void ChannelWritable_w();
  void ChannelNotWritable_w();
  bool AddRecvStream_w(const StreamParams& sp);
  bool RemoveRecvStream_w(uint32 ssrc);
  bool AddSendStream_w(const StreamParams& sp);
  bool RemoveSendStream_w(uint32 ssrc);
  virtual bool ShouldSetupDtlsSrtp() const;
  // Do the DTLS key expansion and impose it on the SRTP/SRTCP filters.
  // |rtcp_channel| indicates whether to set up the RTP or RTCP filter.
  bool SetupDtlsSrtp(bool rtcp_channel);
  // Set the DTLS-SRTP cipher policy on this channel as appropriate.
  bool SetDtlsSrtpCiphers(TransportChannel *tc, bool rtcp);

  virtual void ChangeState() = 0;

  // Gets the content info appropriate to the channel (audio or video).
  virtual const ContentInfo* GetFirstContent(
      const SessionDescription* sdesc) = 0;
  bool UpdateLocalStreams_w(const std::vector<StreamParams>& streams,
                            ContentAction action,
                            std::string* error_desc);
  bool UpdateRemoteStreams_w(const std::vector<StreamParams>& streams,
                             ContentAction action,
                             std::string* error_desc);
  bool SetBaseLocalContent_w(const MediaContentDescription* content,
                             ContentAction action,
                             std::string* error_desc);
  virtual bool SetLocalContent_w(const MediaContentDescription* content,
                                 ContentAction action,
                                 std::string* error_desc) = 0;
  bool SetBaseRemoteContent_w(const MediaContentDescription* content,
                              ContentAction action,
                              std::string* error_desc);
  virtual bool SetRemoteContent_w(const MediaContentDescription* content,
                                  ContentAction action,
                                  std::string* error_desc) = 0;

  bool CheckSrtpConfig(const std::vector<CryptoParams>& cryptos,
                       bool* dtls,
                       std::string* error_desc);
  bool SetSrtp_w(const std::vector<CryptoParams>& params,
                 ContentAction action,
                 ContentSource src,
                 std::string* error_desc);
  bool SetRtcpMux_w(bool enable,
                    ContentAction action,
                    ContentSource src,
                    std::string* error_desc);

  // From MessageHandler
  virtual void OnMessage(talk_base::Message* pmsg);

  // Handled in derived classes
  // Get the SRTP ciphers to use for RTP media
  virtual void GetSrtpCiphers(std::vector<std::string>* ciphers) const = 0;
  virtual void OnConnectionMonitorUpdate(SocketMonitor* monitor,
      const std::vector<ConnectionInfo>& infos) = 0;

 private:
  sigslot::signal3<const void*, size_t, bool> SignalSendPacketPreCrypto;
  sigslot::signal3<const void*, size_t, bool> SignalSendPacketPostCrypto;
  sigslot::signal3<const void*, size_t, bool> SignalRecvPacketPreCrypto;
  sigslot::signal3<const void*, size_t, bool> SignalRecvPacketPostCrypto;
  talk_base::CriticalSection signal_send_packet_cs_;
  talk_base::CriticalSection signal_recv_packet_cs_;

  talk_base::Thread* worker_thread_;
  MediaEngineInterface* media_engine_;
  BaseSession* session_;
  MediaChannel* media_channel_;
  std::vector<StreamParams> local_streams_;
  std::vector<StreamParams> remote_streams_;

  std::string content_name_;
  bool rtcp_;
  TransportChannel* transport_channel_;
  TransportChannel* rtcp_transport_channel_;
  SrtpFilter srtp_filter_;
  RtcpMuxFilter rtcp_mux_filter_;
  SsrcMuxFilter ssrc_filter_;
  talk_base::scoped_ptr<SocketMonitor> socket_monitor_;
  bool enabled_;
  bool writable_;
  bool rtp_ready_to_send_;
  bool rtcp_ready_to_send_;
  bool was_ever_writable_;
  MediaContentDirection local_content_direction_;
  MediaContentDirection remote_content_direction_;
  std::set<uint32> muted_streams_;
  bool has_received_packet_;
  bool dtls_keyed_;
  bool secure_required_;
};

// VoiceChannel is a specialization that adds support for early media, DTMF,
// and input/output level monitoring.
class VoiceChannel : public BaseChannel {
 public:
  VoiceChannel(talk_base::Thread* thread, MediaEngineInterface* media_engine,
               VoiceMediaChannel* channel, BaseSession* session,
               const std::string& content_name, bool rtcp);
  ~VoiceChannel();
  bool Init();
  bool SetRemoteRenderer(uint32 ssrc, AudioRenderer* renderer);
  bool SetLocalRenderer(uint32 ssrc, AudioRenderer* renderer);

  // downcasts a MediaChannel
  virtual VoiceMediaChannel* media_channel() const {
    return static_cast<VoiceMediaChannel*>(BaseChannel::media_channel());
  }

  bool SetRingbackTone(const void* buf, int len);
  void SetEarlyMedia(bool enable);
  // This signal is emitted when we have gone a period of time without
  // receiving early media. When received, a UI should start playing its
  // own ringing sound
  sigslot::signal1<VoiceChannel*> SignalEarlyMediaTimeout;

  bool PlayRingbackTone(uint32 ssrc, bool play, bool loop);
  // TODO(ronghuawu): Replace PressDTMF with InsertDtmf.
  bool PressDTMF(int digit, bool playout);
  // Returns if the telephone-event has been negotiated.
  bool CanInsertDtmf();
  // Send and/or play a DTMF |event| according to the |flags|.
  // The DTMF out-of-band signal will be used on sending.
  // The |ssrc| should be either 0 or a valid send stream ssrc.
  // The valid value for the |event| are 0 which corresponding to DTMF
  // event 0-9, *, #, A-D.
  bool InsertDtmf(uint32 ssrc, int event_code, int duration, int flags);
  bool SetOutputScaling(uint32 ssrc, double left, double right);
  // Get statistics about the current media session.
  bool GetStats(VoiceMediaInfo* stats);

  // Monitoring functions
  sigslot::signal2<VoiceChannel*, const std::vector<ConnectionInfo>&>
      SignalConnectionMonitor;

  void StartMediaMonitor(int cms);
  void StopMediaMonitor();
  sigslot::signal2<VoiceChannel*, const VoiceMediaInfo&> SignalMediaMonitor;

  void StartAudioMonitor(int cms);
  void StopAudioMonitor();
  bool IsAudioMonitorRunning() const;
  sigslot::signal2<VoiceChannel*, const AudioInfo&> SignalAudioMonitor;

  void StartTypingMonitor(const TypingMonitorOptions& settings);
  void StopTypingMonitor();
  bool IsTypingMonitorRunning() const;

  // Overrides BaseChannel::MuteStream_w.
  virtual bool MuteStream_w(uint32 ssrc, bool mute);

  int GetInputLevel_w();
  int GetOutputLevel_w();
  void GetActiveStreams_w(AudioInfo::StreamList* actives);

  // Signal errors from VoiceMediaChannel.  Arguments are:
  //     ssrc(uint32), and error(VoiceMediaChannel::Error).
  sigslot::signal3<VoiceChannel*, uint32, VoiceMediaChannel::Error>
      SignalMediaError;

  // Configuration and setting.
  bool SetChannelOptions(const AudioOptions& options);

 private:
  // overrides from BaseChannel
  virtual void OnChannelRead(TransportChannel* channel,
                             const char* data, size_t len,
                             const talk_base::PacketTime& packet_time,
                             int flags);
  virtual void ChangeState();
  virtual const ContentInfo* GetFirstContent(const SessionDescription* sdesc);
  virtual bool SetLocalContent_w(const MediaContentDescription* content,
                                 ContentAction action,
                                 std::string* error_desc);
  virtual bool SetRemoteContent_w(const MediaContentDescription* content,
                                  ContentAction action,
                                  std::string* error_desc);
  bool SetRingbackTone_w(const void* buf, int len);
  bool PlayRingbackTone_w(uint32 ssrc, bool play, bool loop);
  void HandleEarlyMediaTimeout();
  bool CanInsertDtmf_w();
  bool InsertDtmf_w(uint32 ssrc, int event, int duration, int flags);
  bool SetOutputScaling_w(uint32 ssrc, double left, double right);
  bool GetStats_w(VoiceMediaInfo* stats);

  virtual void OnMessage(talk_base::Message* pmsg);
  virtual void GetSrtpCiphers(std::vector<std::string>* ciphers) const;
  virtual void OnConnectionMonitorUpdate(
      SocketMonitor* monitor, const std::vector<ConnectionInfo>& infos);
  virtual void OnMediaMonitorUpdate(
      VoiceMediaChannel* media_channel, const VoiceMediaInfo& info);
  void OnAudioMonitorUpdate(AudioMonitor* monitor, const AudioInfo& info);
  void OnVoiceChannelError(uint32 ssrc, VoiceMediaChannel::Error error);
  void SendLastMediaError();
  void OnSrtpError(uint32 ssrc, SrtpFilter::Mode mode, SrtpFilter::Error error);
  // Configuration and setting.
  bool SetChannelOptions_w(const AudioOptions& options);
  bool SetRenderer_w(uint32 ssrc, AudioRenderer* renderer, bool is_local);

  static const int kEarlyMediaTimeout = 1000;
  bool received_media_;
  talk_base::scoped_ptr<VoiceMediaMonitor> media_monitor_;
  talk_base::scoped_ptr<AudioMonitor> audio_monitor_;
  talk_base::scoped_ptr<TypingMonitor> typing_monitor_;
};

// VideoChannel is a specialization for video.
class VideoChannel : public BaseChannel {
 public:
  // Make screen capturer virtual so that it can be overriden in testing.
  // E.g. used to test that window events are triggered correctly.
  class ScreenCapturerFactory {
   public:
    virtual VideoCapturer* CreateScreenCapturer(const ScreencastId& window) = 0;
    virtual ~ScreenCapturerFactory() {}
  };

  VideoChannel(talk_base::Thread* thread, MediaEngineInterface* media_engine,
               VideoMediaChannel* channel, BaseSession* session,
               const std::string& content_name, bool rtcp,
               VoiceChannel* voice_channel);
  ~VideoChannel();
  bool Init();

  bool SetRenderer(uint32 ssrc, VideoRenderer* renderer);
  bool ApplyViewRequest(const ViewRequest& request);

  // TODO(pthatcher): Refactor to use a "capture id" instead of an
  // ssrc here as the "key".
  VideoCapturer* AddScreencast(uint32 ssrc, const ScreencastId& id);
  bool SetCapturer(uint32 ssrc, VideoCapturer* capturer);
  bool RemoveScreencast(uint32 ssrc);
  // True if we've added a screencast.  Doesn't matter if the capturer
  // has been started or not.
  bool IsScreencasting();
  int GetScreencastFps(uint32 ssrc);
  int GetScreencastMaxPixels(uint32 ssrc);
  // Get statistics about the current media session.
  bool GetStats(VideoMediaInfo* stats);

  sigslot::signal2<VideoChannel*, const std::vector<ConnectionInfo>&>
      SignalConnectionMonitor;

  void StartMediaMonitor(int cms);
  void StopMediaMonitor();
  sigslot::signal2<VideoChannel*, const VideoMediaInfo&> SignalMediaMonitor;
  sigslot::signal2<uint32, talk_base::WindowEvent> SignalScreencastWindowEvent;

  bool SendIntraFrame();
  bool RequestIntraFrame();
  sigslot::signal3<VideoChannel*, uint32, VideoMediaChannel::Error>
      SignalMediaError;

  void SetScreenCaptureFactory(
      ScreenCapturerFactory* screencapture_factory);

  // Configuration and setting.
  bool SetChannelOptions(const VideoOptions& options);

 protected:
  // downcasts a MediaChannel
  virtual VideoMediaChannel* media_channel() const {
    return static_cast<VideoMediaChannel*>(BaseChannel::media_channel());
  }

 private:
  typedef std::map<uint32, VideoCapturer*> ScreencastMap;
  struct ScreencastDetailsMessageData;

  // overrides from BaseChannel
  virtual void ChangeState();
  virtual const ContentInfo* GetFirstContent(const SessionDescription* sdesc);
  virtual bool SetLocalContent_w(const MediaContentDescription* content,
                                 ContentAction action,
                                 std::string* error_desc);
  virtual bool SetRemoteContent_w(const MediaContentDescription* content,
                                  ContentAction action,
                                  std::string* error_desc);
  void SendIntraFrame_w() {
    media_channel()->SendIntraFrame();
  }
  void RequestIntraFrame_w() {
    media_channel()->RequestIntraFrame();
  }

  bool ApplyViewRequest_w(const ViewRequest& request);
  void SetRenderer_w(uint32 ssrc, VideoRenderer* renderer);

  VideoCapturer* AddScreencast_w(uint32 ssrc, const ScreencastId& id);
  bool SetCapturer_w(uint32 ssrc, VideoCapturer* capturer);
  bool RemoveScreencast_w(uint32 ssrc);
  void OnScreencastWindowEvent_s(uint32 ssrc, talk_base::WindowEvent we);
  bool IsScreencasting_w() const;
  void ScreencastDetails_w(ScreencastDetailsMessageData* d) const;
  void SetScreenCaptureFactory_w(
      ScreenCapturerFactory* screencapture_factory);
  bool GetStats_w(VideoMediaInfo* stats);

  virtual void OnMessage(talk_base::Message* pmsg);
  virtual void GetSrtpCiphers(std::vector<std::string>* ciphers) const;
  virtual void OnConnectionMonitorUpdate(
      SocketMonitor* monitor, const std::vector<ConnectionInfo>& infos);
  virtual void OnMediaMonitorUpdate(
      VideoMediaChannel* media_channel, const VideoMediaInfo& info);
  virtual void OnScreencastWindowEvent(uint32 ssrc,
                                       talk_base::WindowEvent event);
  virtual void OnStateChange(VideoCapturer* capturer, CaptureState ev);
  bool GetLocalSsrc(const VideoCapturer* capturer, uint32* ssrc);

  void OnVideoChannelError(uint32 ssrc, VideoMediaChannel::Error error);
  void OnSrtpError(uint32 ssrc, SrtpFilter::Mode mode, SrtpFilter::Error error);
  // Configuration and setting.
  bool SetChannelOptions_w(const VideoOptions& options);

  VoiceChannel* voice_channel_;
  VideoRenderer* renderer_;
  talk_base::scoped_ptr<ScreenCapturerFactory> screencapture_factory_;
  ScreencastMap screencast_capturers_;
  talk_base::scoped_ptr<VideoMediaMonitor> media_monitor_;

  talk_base::WindowEvent previous_we_;
};

// DataChannel is a specialization for data.
class DataChannel : public BaseChannel {
 public:
  DataChannel(talk_base::Thread* thread,
              DataMediaChannel* media_channel,
              BaseSession* session,
              const std::string& content_name,
              bool rtcp);
  ~DataChannel();
  bool Init();

  virtual bool SendData(const SendDataParams& params,
                        const talk_base::Buffer& payload,
                        SendDataResult* result);

  void StartMediaMonitor(int cms);
  void StopMediaMonitor();

  // Should be called on the signaling thread only.
  bool ready_to_send_data() const {
    return ready_to_send_data_;
  }

  sigslot::signal2<DataChannel*, const DataMediaInfo&> SignalMediaMonitor;
  sigslot::signal2<DataChannel*, const std::vector<ConnectionInfo>&>
      SignalConnectionMonitor;
  sigslot::signal3<DataChannel*, uint32, DataMediaChannel::Error>
      SignalMediaError;
  sigslot::signal3<DataChannel*,
                   const ReceiveDataParams&,
                   const talk_base::Buffer&>
      SignalDataReceived;
  // Signal for notifying when the channel becomes ready to send data.
  // That occurs when the channel is enabled, the transport is writable,
  // both local and remote descriptions are set, and the channel is unblocked.
  sigslot::signal1<bool> SignalReadyToSendData;

 protected:
  // downcasts a MediaChannel.
  virtual DataMediaChannel* media_channel() const {
    return static_cast<DataMediaChannel*>(BaseChannel::media_channel());
  }

 private:
  struct SendDataMessageData : public talk_base::MessageData {
    SendDataMessageData(const SendDataParams& params,
                        const talk_base::Buffer* payload,
                        SendDataResult* result)
        : params(params),
          payload(payload),
          result(result),
          succeeded(false) {
    }

    const SendDataParams& params;
    const talk_base::Buffer* payload;
    SendDataResult* result;
    bool succeeded;
  };

  struct DataReceivedMessageData : public talk_base::MessageData {
    // We copy the data because the data will become invalid after we
    // handle DataMediaChannel::SignalDataReceived but before we fire
    // SignalDataReceived.
    DataReceivedMessageData(
        const ReceiveDataParams& params, const char* data, size_t len)
        : params(params),
          payload(data, len) {
    }
    const ReceiveDataParams params;
    const talk_base::Buffer payload;
  };

  typedef talk_base::TypedMessageData<bool> DataChannelReadyToSendMessageData;

  // overrides from BaseChannel
  virtual const ContentInfo* GetFirstContent(const SessionDescription* sdesc);
  // If data_channel_type_ is DCT_NONE, set it.  Otherwise, check that
  // it's the same as what was set previously.  Returns false if it's
  // set to one type one type and changed to another type later.
  bool SetDataChannelType(DataChannelType new_data_channel_type,
                          std::string* error_desc);
  // Same as SetDataChannelType, but extracts the type from the
  // DataContentDescription.
  bool SetDataChannelTypeFromContent(const DataContentDescription* content,
                                     std::string* error_desc);
  virtual bool SetLocalContent_w(const MediaContentDescription* content,
                                 ContentAction action,
                                 std::string* error_desc);
  virtual bool SetRemoteContent_w(const MediaContentDescription* content,
                                  ContentAction action,
                                  std::string* error_desc);
  virtual void ChangeState();
  virtual bool WantsPacket(bool rtcp, talk_base::Buffer* packet);

  virtual void OnMessage(talk_base::Message* pmsg);
  virtual void GetSrtpCiphers(std::vector<std::string>* ciphers) const;
  virtual void OnConnectionMonitorUpdate(
      SocketMonitor* monitor, const std::vector<ConnectionInfo>& infos);
  virtual void OnMediaMonitorUpdate(
      DataMediaChannel* media_channel, const DataMediaInfo& info);
  virtual bool ShouldSetupDtlsSrtp() const;
  void OnDataReceived(
      const ReceiveDataParams& params, const char* data, size_t len);
  void OnDataChannelError(uint32 ssrc, DataMediaChannel::Error error);
  void OnDataChannelReadyToSend(bool writable);
  void OnSrtpError(uint32 ssrc, SrtpFilter::Mode mode, SrtpFilter::Error error);

  talk_base::scoped_ptr<DataMediaMonitor> media_monitor_;
  // TODO(pthatcher): Make a separate SctpDataChannel and
  // RtpDataChannel instead of using this.
  DataChannelType data_channel_type_;
  bool ready_to_send_data_;
};

}  // namespace cricket

#endif  // TALK_SESSION_MEDIA_CHANNEL_H_
