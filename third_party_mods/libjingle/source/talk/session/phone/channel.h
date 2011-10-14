/*
 * libjingle
 * Copyright 2004--2007, Google Inc.
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

#ifndef TALK_SESSION_PHONE_CHANNEL_H_
#define TALK_SESSION_PHONE_CHANNEL_H_

#include <string>
#include <vector>

#include "talk/base/asyncudpsocket.h"
#include "talk/base/criticalsection.h"
#include "talk/base/network.h"
#include "talk/base/sigslot.h"
#include "talk/p2p/client/socketmonitor.h"
#include "talk/p2p/base/session.h"
#include "talk/session/phone/audiomonitor.h"
#include "talk/session/phone/mediachannel.h"
#include "talk/session/phone/mediaengine.h"
#include "talk/session/phone/mediamonitor.h"
#include "talk/session/phone/rtcpmuxfilter.h"
#include "talk/session/phone/srtpfilter.h"

namespace webrtc {
class VideoCaptureModule;
}

namespace cricket {

class MediaContentDescription;
struct CryptoParams;

enum {
  MSG_ENABLE = 1,
  MSG_DISABLE = 2,
  MSG_MUTE = 3,
  MSG_UNMUTE = 4,
  MSG_SETREMOTECONTENT = 5,
  MSG_SETLOCALCONTENT = 6,
  MSG_EARLYMEDIATIMEOUT = 8,
  MSG_PRESSDTMF = 9,
  MSG_SETRENDERER = 10,
  MSG_ADDSTREAM = 11,
  MSG_REMOVESTREAM = 12,
  MSG_SETRINGBACKTONE = 13,
  MSG_PLAYRINGBACKTONE = 14,
  MSG_SETMAXSENDBANDWIDTH = 15,
  MSG_SETRTCPCNAME = 18,
  MSG_SENDINTRAFRAME = 19,
  MSG_REQUESTINTRAFRAME = 20,
  MSG_RTPPACKET = 22,
  MSG_RTCPPACKET = 23,
  MSG_CHANNEL_ERROR = 24,
  MSG_ENABLECPUADAPTATION = 25,
  MSG_DISABLECPUADAPTATION = 26,
  MSG_SCALEVOLUME = 27
};

// BaseChannel contains logic common to voice and video, including
// enable/mute, marshaling calls to a worker thread, and
// connection and media monitors.
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
  bool secure() const { return srtp_filter_.IsActive(); }

  // Channel control
  bool SetRtcpCName(const std::string& cname);
  bool SetLocalContent(const MediaContentDescription* content,
                       ContentAction action);
  bool SetRemoteContent(const MediaContentDescription* content,
                        ContentAction action);
  bool SetMaxSendBandwidth(int max_bandwidth);

  bool Enable(bool enable);
  bool Mute(bool mute);

  // Multiplexing
  bool RemoveStream(uint32 ssrc);

  // Monitoring
  void StartConnectionMonitor(int cms);
  void StopConnectionMonitor();

  void set_srtp_signal_silent_time(uint32 silent_time) {
    srtp_filter_.set_signal_silent_time(silent_time);
  }

  template <class T>
  void RegisterSendSink(T* sink,
                        void (T::*OnPacket)(const void*, size_t, bool)) {
    talk_base::CritScope cs(&signal_send_packet_cs_);
    SignalSendPacket.disconnect(sink);
    SignalSendPacket.connect(sink, OnPacket);
  }

  void UnregisterSendSink(sigslot::has_slots<>* sink) {
    talk_base::CritScope cs(&signal_send_packet_cs_);
    SignalSendPacket.disconnect(sink);
  }

  bool HasSendSinks() {
    talk_base::CritScope cs(&signal_send_packet_cs_);
    return !SignalSendPacket.is_empty();
  }

  template <class T>
  void RegisterRecvSink(T* sink,
                        void (T::*OnPacket)(const void*, size_t, bool)) {
    talk_base::CritScope cs(&signal_recv_packet_cs_);
    SignalRecvPacket.disconnect(sink);
    SignalRecvPacket.connect(sink, OnPacket);
  }

  void UnregisterRecvSink(sigslot::has_slots<>* sink) {
    talk_base::CritScope cs(&signal_recv_packet_cs_);
    SignalRecvPacket.disconnect(sink);
  }

  bool HasRecvSinks() {
    talk_base::CritScope cs(&signal_recv_packet_cs_);
    return !SignalRecvPacket.is_empty();
  }

 protected:
  MediaEngineInterface* media_engine() const { return media_engine_; }
  virtual MediaChannel* media_channel() const { return media_channel_; }
  void set_rtcp_transport_channel(TransportChannel* transport);
  bool writable() const { return writable_; }
  bool was_ever_writable() const { return was_ever_writable_; }
  bool has_local_content() const { return has_local_content_; }
  bool has_remote_content() const { return has_remote_content_; }
  void set_has_local_content(bool has) { has_local_content_ = has; }
  void set_has_remote_content(bool has) { has_remote_content_ = has; }
  bool muted() const { return muted_; }
  talk_base::Thread* signaling_thread() { return session_->signaling_thread(); }
  SrtpFilter* srtp_filter() { return &srtp_filter_; }
  bool rtcp() const { return rtcp_; }

  void Send(uint32 id, talk_base::MessageData *pdata = NULL);
  void Post(uint32 id, talk_base::MessageData *pdata = NULL);
  void PostDelayed(int cmsDelay, uint32 id = 0,
                   talk_base::MessageData *pdata = NULL);
  void Clear(uint32 id = talk_base::MQID_ANY,
             talk_base::MessageList* removed = NULL);
  void FlushRtcpMessages();

  // NetworkInterface implementation, called by MediaEngine
  virtual bool SendPacket(talk_base::Buffer* packet);
  virtual bool SendRtcp(talk_base::Buffer* packet);
  virtual int SetOption(SocketType type, talk_base::Socket::Option o, int val);

  // From TransportChannel
  void OnWritableState(TransportChannel* channel);
  virtual void OnChannelRead(TransportChannel* channel, const char* data,
                             size_t len);

  bool PacketIsRtcp(const TransportChannel* channel, const char* data,
                    size_t len);
  bool SendPacket(bool rtcp, talk_base::Buffer* packet);
  void HandlePacket(bool rtcp, talk_base::Buffer* packet);

  // Setting the send codec based on the remote description.
  void OnSessionState(BaseSession* session, BaseSession::State state);
  void OnRemoteDescriptionUpdate(BaseSession* session);

  void EnableMedia_w();
  void DisableMedia_w();
  void MuteMedia_w();
  void UnmuteMedia_w();
  void ChannelWritable_w();
  void ChannelNotWritable_w();

  struct StreamMessageData : public talk_base::MessageData {
    StreamMessageData(uint32 s1, uint32 s2) : ssrc1(s1), ssrc2(s2) {}
    uint32 ssrc1;
    uint32 ssrc2;
  };
  virtual void RemoveStream_w(uint32 ssrc) = 0;

  virtual void ChangeState() = 0;

  struct SetRtcpCNameData : public talk_base::MessageData {
    explicit SetRtcpCNameData(const std::string& cname)
        : cname(cname), result(false) {}
    std::string cname;
    bool result;
  };
  bool SetRtcpCName_w(const std::string& cname);

  struct SetContentData : public talk_base::MessageData {
    SetContentData(const MediaContentDescription* content,
                   ContentAction action)
        : content(content), action(action), result(false) {}
    const MediaContentDescription* content;
    ContentAction action;
    bool result;
  };

  // Gets the content appropriate to the channel (audio or video).
  virtual const MediaContentDescription* GetFirstContent(
      const SessionDescription* sdesc) = 0;
  virtual bool SetLocalContent_w(const MediaContentDescription* content,
                                 ContentAction action) = 0;
  virtual bool SetRemoteContent_w(const MediaContentDescription* content,
                                  ContentAction action) = 0;

  bool SetSrtp_w(const std::vector<CryptoParams>& params, ContentAction action,
                 ContentSource src);
  bool SetRtcpMux_w(bool enable, ContentAction action, ContentSource src);

  struct SetBandwidthData : public talk_base::MessageData {
    explicit SetBandwidthData(int value) : value(value), result(false) {}
    int value;
    bool result;
  };
  bool SetMaxSendBandwidth_w(int max_bandwidth);

  // From MessageHandler
  virtual void OnMessage(talk_base::Message *pmsg);

  // Handled in derived classes
  virtual void OnConnectionMonitorUpdate(SocketMonitor *monitor,
      const std::vector<ConnectionInfo> &infos) = 0;

 private:
  sigslot::signal3<const void*, size_t, bool> SignalSendPacket;
  sigslot::signal3<const void*, size_t, bool> SignalRecvPacket;
  talk_base::CriticalSection signal_send_packet_cs_;
  talk_base::CriticalSection signal_recv_packet_cs_;

  talk_base::Thread *worker_thread_;
  MediaEngineInterface *media_engine_;
  BaseSession *session_;
  MediaChannel *media_channel_;

  std::string content_name_;
  bool rtcp_;
  TransportChannel *transport_channel_;
  TransportChannel *rtcp_transport_channel_;
  SrtpFilter srtp_filter_;
  RtcpMuxFilter rtcp_mux_filter_;
  talk_base::scoped_ptr<SocketMonitor> socket_monitor_;
  bool enabled_;
  bool writable_;
  bool was_ever_writable_;
  bool has_local_content_;
  bool has_remote_content_;
  bool muted_;
};

// VoiceChannel is a specialization that adds support for early media, DTMF,
// and input/output level monitoring.
class VoiceChannel : public BaseChannel {
 public:
  VoiceChannel(talk_base::Thread *thread, MediaEngineInterface *media_engine,
               VoiceMediaChannel *channel, BaseSession *session,
               const std::string& content_name, bool rtcp);
  ~VoiceChannel();
  bool Init();

  // downcasts a MediaChannel
  virtual VoiceMediaChannel* media_channel() const {
    return static_cast<VoiceMediaChannel*>(BaseChannel::media_channel());
  }

  // Add an incoming stream with the specified SSRC.
  bool AddStream(uint32 ssrc);

  bool SetRingbackTone(const void* buf, int len);
  void SetEarlyMedia(bool enable);
  // This signal is emitted when we have gone a period of time without
  // receiving early media. When received, a UI should start playing its
  // own ringing sound
  sigslot::signal1<VoiceChannel*> SignalEarlyMediaTimeout;

  bool PlayRingbackTone(uint32 ssrc, bool play, bool loop);
  bool PressDTMF(int digit, bool playout);
  bool SetOutputScaling(uint32 ssrc, double left, double right);

  // Monitoring functions
  sigslot::signal2<VoiceChannel*, const std::vector<ConnectionInfo> &>
      SignalConnectionMonitor;

  void StartMediaMonitor(int cms);
  void StopMediaMonitor();
  sigslot::signal2<VoiceChannel*, const VoiceMediaInfo&> SignalMediaMonitor;

  void StartAudioMonitor(int cms);
  void StopAudioMonitor();
  bool IsAudioMonitorRunning() const;
  sigslot::signal2<VoiceChannel*, const AudioInfo&> SignalAudioMonitor;

  int GetInputLevel_w();
  int GetOutputLevel_w();
  void GetActiveStreams_w(AudioInfo::StreamList* actives);

  // Signal errors from VoiceMediaChannel.  Arguments are:
  //     ssrc(uint32), and error(VoiceMediaChannel::Error).
  sigslot::signal3<VoiceChannel*, uint32, VoiceMediaChannel::Error>
      SignalMediaError;

 private:
  struct SetRingbackToneMessageData : public talk_base::MessageData {
    SetRingbackToneMessageData(const void* b, int l)
        : buf(b),
          len(l),
          result(false) {
    }
    const void* buf;
    int len;
    bool result;
  };
  struct PlayRingbackToneMessageData : public talk_base::MessageData {
    PlayRingbackToneMessageData(uint32 s, bool p, bool l)
        : ssrc(s),
          play(p),
          loop(l),
          result(false) {
    }
    uint32 ssrc;
    bool play;
    bool loop;
    bool result;
  };
  struct DtmfMessageData : public talk_base::MessageData {
    DtmfMessageData(int d, bool p)
        : digit(d),
          playout(p),
          result(false) {
    }
    int digit;
    bool playout;
    bool result;
  };
  struct ScaleVolumeMessageData : public talk_base::MessageData {
    ScaleVolumeMessageData(uint32 s, double l, double r)
        : ssrc(s),
          left(l),
          right(r),
          result(false) {
    }
    uint32 ssrc;
    double left;
    double right;
    bool result;
  };

  // overrides from BaseChannel
  virtual void OnChannelRead(TransportChannel* channel,
                             const char *data, size_t len);
  virtual void ChangeState();
  virtual const MediaContentDescription* GetFirstContent(
      const SessionDescription* sdesc);
  virtual bool SetLocalContent_w(const MediaContentDescription* content,
                                 ContentAction action);
  virtual bool SetRemoteContent_w(const MediaContentDescription* content,
                                  ContentAction action);

  void AddStream_w(uint32 ssrc);
  void RemoveStream_w(uint32 ssrc);

  bool SetRingbackTone_w(const void* buf, int len);
  bool PlayRingbackTone_w(uint32 ssrc, bool play, bool loop);
  void HandleEarlyMediaTimeout();
  bool PressDTMF_w(int digit, bool playout);
  bool SetOutputScaling_w(uint32 ssrc, double left, double right);

  virtual void OnMessage(talk_base::Message *pmsg);
  virtual void OnConnectionMonitorUpdate(
      SocketMonitor *monitor, const std::vector<ConnectionInfo> &infos);
  virtual void OnMediaMonitorUpdate(
      VoiceMediaChannel *media_channel, const VoiceMediaInfo& info);
  void OnAudioMonitorUpdate(AudioMonitor *monitor, const AudioInfo& info);
  void OnVoiceChannelError(uint32 ssrc, VoiceMediaChannel::Error error);
  void SendLastMediaError();
  void OnSrtpError(uint32 ssrc, SrtpFilter::Mode mode, SrtpFilter::Error error);

  static const int kEarlyMediaTimeout = 1000;
  bool received_media_;
  talk_base::scoped_ptr<VoiceMediaMonitor> media_monitor_;
  talk_base::scoped_ptr<AudioMonitor> audio_monitor_;
};

// VideoChannel is a specialization for video.
class VideoChannel : public BaseChannel {
 public:
  VideoChannel(talk_base::Thread *thread, MediaEngineInterface *media_engine,
               VideoMediaChannel *channel, BaseSession *session,
               const std::string& content_name, bool rtcp,
               VoiceChannel *voice_channel);
  ~VideoChannel();
  bool Init();

  // downcasts a MediaChannel
  virtual VideoMediaChannel* media_channel() const {
    return static_cast<VideoMediaChannel*>(BaseChannel::media_channel());
  }

  // Add an incoming stream with the specified SSRC.
  bool AddStream(uint32 ssrc, uint32 voice_ssrc);

  bool SetRenderer(uint32 ssrc, VideoRenderer* renderer);


  sigslot::signal2<VideoChannel*, const std::vector<ConnectionInfo> &>
      SignalConnectionMonitor;

  void StartMediaMonitor(int cms);
  void StopMediaMonitor();
  sigslot::signal2<VideoChannel*, const VideoMediaInfo&> SignalMediaMonitor;

  bool SendIntraFrame();
  bool RequestIntraFrame();
  void EnableCpuAdaptation(bool enable);

  sigslot::signal3<VideoChannel*, uint32, VideoMediaChannel::Error>
      SignalMediaError;

  void SetCaptureDevice(uint32 ssrc, webrtc::VideoCaptureModule* camera);
  void SetLocalRenderer(uint32 ssrc, VideoRenderer* renderer);

 private:
  // overrides from BaseChannel
  virtual void ChangeState();
  virtual const MediaContentDescription* GetFirstContent(
      const SessionDescription* sdesc);
  virtual bool SetLocalContent_w(const MediaContentDescription* content,
                                 ContentAction action);
  virtual bool SetRemoteContent_w(const MediaContentDescription* content,
                                  ContentAction action);

  void AddStream_w(uint32 ssrc, uint32 voice_ssrc);
  void RemoveStream_w(uint32 ssrc);

  void SendIntraFrame_w() {
    media_channel()->SendIntraFrame();
  }
  void RequestIntraFrame_w() {
    media_channel()->RequestIntraFrame();
  }
  void EnableCpuAdaptation_w(bool enable) {
    // TODO: The following call will clear all other options, which is
    // OK now since SetOptions is not used in video media channel. In the
    // future, add GetOptions() method and change the options.
    media_channel()->SetOptions(enable ? OPT_CPU_ADAPTATION : 0);
  }

  struct RenderMessageData : public talk_base::MessageData {
    RenderMessageData(uint32 s, VideoRenderer* r) : ssrc(s), renderer(r) {}
    uint32 ssrc;
    VideoRenderer* renderer;
  };


  void SetRenderer_w(uint32 ssrc, VideoRenderer* renderer);


  virtual void OnMessage(talk_base::Message *pmsg);
  virtual void OnConnectionMonitorUpdate(
      SocketMonitor *monitor, const std::vector<ConnectionInfo> &infos);
  virtual void OnMediaMonitorUpdate(
      VideoMediaChannel *media_channel, const VideoMediaInfo& info);
  void OnVideoChannelError(uint32 ssrc, VideoMediaChannel::Error error);
  void OnSrtpError(uint32 ssrc, SrtpFilter::Mode mode, SrtpFilter::Error error);

  VoiceChannel *voice_channel_;
  VideoRenderer *renderer_;
  talk_base::scoped_ptr<VideoMediaMonitor> media_monitor_;
};

}  // namespace cricket

#endif  // TALK_SESSION_PHONE_CHANNEL_H_
