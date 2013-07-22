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

#include "talk/session/media/channel.h"

#include "talk/base/buffer.h"
#include "talk/base/byteorder.h"
#include "talk/base/common.h"
#include "talk/base/logging.h"
#include "talk/media/base/rtputils.h"
#include "talk/p2p/base/transportchannel.h"
#include "talk/session/media/channelmanager.h"
#include "talk/session/media/mediamessages.h"
#include "talk/session/media/rtcpmuxfilter.h"
#include "talk/session/media/typingmonitor.h"


namespace cricket {

enum {
  MSG_ENABLE = 1,
  MSG_DISABLE,
  MSG_MUTESTREAM,
  MSG_ISSTREAMMUTED,
  MSG_SETREMOTECONTENT,
  MSG_SETLOCALCONTENT,
  MSG_EARLYMEDIATIMEOUT,
  MSG_CANINSERTDTMF,
  MSG_INSERTDTMF,
  MSG_GETSTATS,
  MSG_SETRENDERER,
  MSG_ADDRECVSTREAM,
  MSG_REMOVERECVSTREAM,
  MSG_SETRINGBACKTONE,
  MSG_PLAYRINGBACKTONE,
  MSG_SETMAXSENDBANDWIDTH,
  MSG_ADDSCREENCAST,
  MSG_REMOVESCREENCAST,
  MSG_SENDINTRAFRAME,
  MSG_REQUESTINTRAFRAME,
  MSG_SCREENCASTWINDOWEVENT,
  MSG_RTPPACKET,
  MSG_RTCPPACKET,
  MSG_CHANNEL_ERROR,
  MSG_SETCHANNELOPTIONS,
  MSG_SCALEVOLUME,
  MSG_HANDLEVIEWREQUEST,
  MSG_READYTOSENDDATA,
  MSG_SENDDATA,
  MSG_DATARECEIVED,
  MSG_SETCAPTURER,
  MSG_ISSCREENCASTING,
  MSG_SCREENCASTFPS,
  MSG_SETSCREENCASTFACTORY,
  MSG_FIRSTPACKETRECEIVED,
  MSG_SESSION_ERROR,
};

// Value specified in RFC 5764.
static const char kDtlsSrtpExporterLabel[] = "EXTRACTOR-dtls_srtp";

static const int kAgcMinus10db = -10;

// TODO(hellner): use the device manager for creation of screen capturers when
// the cl enabling it has landed.
class NullScreenCapturerFactory : public VideoChannel::ScreenCapturerFactory {
 public:
  VideoCapturer* CreateScreenCapturer(const ScreencastId& window) {
    return NULL;
  }
};


VideoChannel::ScreenCapturerFactory* CreateScreenCapturerFactory() {
  return new NullScreenCapturerFactory();
}

struct SetContentData : public talk_base::MessageData {
  SetContentData(const MediaContentDescription* content, ContentAction action)
      : content(content),
        action(action),
        result(false) {
  }
  const MediaContentDescription* content;
  ContentAction action;
  bool result;
};

struct SetBandwidthData : public talk_base::MessageData {
  explicit SetBandwidthData(int value) : value(value), result(false) {}
  int value;
  bool result;
};

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
typedef talk_base::TypedMessageData<bool> BoolMessageData;
struct DtmfMessageData : public talk_base::MessageData {
  DtmfMessageData(uint32 ssrc, int event, int duration, int flags)
      : ssrc(ssrc),
        event(event),
        duration(duration),
        flags(flags),
        result(false) {
  }
  uint32 ssrc;
  int event;
  int duration;
  int flags;
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

struct VoiceStatsMessageData : public talk_base::MessageData {
  explicit VoiceStatsMessageData(VoiceMediaInfo* stats)
      : result(false),
        stats(stats) {
  }
  bool result;
  VoiceMediaInfo* stats;
};

struct VideoStatsMessageData : public talk_base::MessageData {
  explicit VideoStatsMessageData(VideoMediaInfo* stats)
      : result(false),
        stats(stats) {
  }
  bool result;
  VideoMediaInfo* stats;
};

struct PacketMessageData : public talk_base::MessageData {
  talk_base::Buffer packet;
};

struct AudioRenderMessageData: public talk_base::MessageData {
  AudioRenderMessageData(uint32 s, AudioRenderer* r)
      : ssrc(s), renderer(r), result(false) {}
  uint32 ssrc;
  AudioRenderer* renderer;
  bool result;
};

struct VideoRenderMessageData : public talk_base::MessageData {
  VideoRenderMessageData(uint32 s, VideoRenderer* r) : ssrc(s), renderer(r) {}
  uint32 ssrc;
  VideoRenderer* renderer;
};

struct AddScreencastMessageData : public talk_base::MessageData {
  AddScreencastMessageData(uint32 s, const ScreencastId& id)
      : ssrc(s),
        window_id(id),
        result(NULL) {
  }
  uint32 ssrc;
  ScreencastId window_id;
  VideoCapturer* result;
};

struct RemoveScreencastMessageData : public talk_base::MessageData {
  explicit RemoveScreencastMessageData(uint32 s) : ssrc(s), result(false) {}
  uint32 ssrc;
  bool result;
};

struct ScreencastEventMessageData : public talk_base::MessageData {
  ScreencastEventMessageData(uint32 s, talk_base::WindowEvent we)
      : ssrc(s),
        event(we) {
  }
  uint32 ssrc;
  talk_base::WindowEvent event;
};

struct ViewRequestMessageData : public talk_base::MessageData {
  explicit ViewRequestMessageData(const ViewRequest& r)
      : request(r),
        result(false) {
  }
  ViewRequest request;
  bool result;
};

struct VoiceChannelErrorMessageData : public talk_base::MessageData {
  VoiceChannelErrorMessageData(uint32 in_ssrc,
                               VoiceMediaChannel::Error in_error)
      : ssrc(in_ssrc),
        error(in_error) {
  }
  uint32 ssrc;
  VoiceMediaChannel::Error error;
};

struct VideoChannelErrorMessageData : public talk_base::MessageData {
  VideoChannelErrorMessageData(uint32 in_ssrc,
                               VideoMediaChannel::Error in_error)
      : ssrc(in_ssrc),
        error(in_error) {
  }
  uint32 ssrc;
  VideoMediaChannel::Error error;
};

struct DataChannelErrorMessageData : public talk_base::MessageData {
  DataChannelErrorMessageData(uint32 in_ssrc,
                              DataMediaChannel::Error in_error)
      : ssrc(in_ssrc),
        error(in_error) {}
  uint32 ssrc;
  DataMediaChannel::Error error;
};

struct SessionErrorMessageData : public talk_base::MessageData {
  explicit SessionErrorMessageData(cricket::BaseSession::Error error)
      : error_(error) {}

  BaseSession::Error error_;
};

struct SsrcMessageData : public talk_base::MessageData {
  explicit SsrcMessageData(uint32 ssrc) : ssrc(ssrc), result(false) {}
  uint32 ssrc;
  bool result;
};

struct StreamMessageData : public talk_base::MessageData {
  explicit StreamMessageData(const StreamParams& in_sp)
      : sp(in_sp),
        result(false) {
  }
  StreamParams sp;
  bool result;
};

struct MuteStreamData : public talk_base::MessageData {
  MuteStreamData(uint32 ssrc, bool mute)
      : ssrc(ssrc), mute(mute), result(false) {}
  uint32 ssrc;
  bool mute;
  bool result;
};

struct AudioOptionsMessageData : public talk_base::MessageData {
  explicit AudioOptionsMessageData(const AudioOptions& options)
      : options(options),
        result(false) {
  }
  AudioOptions options;
  bool result;
};

struct VideoOptionsMessageData : public talk_base::MessageData {
  explicit VideoOptionsMessageData(const VideoOptions& options)
      : options(options),
        result(false) {
  }
  VideoOptions options;
  bool result;
};

struct SetCapturerMessageData : public talk_base::MessageData {
  SetCapturerMessageData(uint32 s, VideoCapturer* c)
      : ssrc(s),
        capturer(c),
        result(false) {
  }
  uint32 ssrc;
  VideoCapturer* capturer;
  bool result;
};

struct IsScreencastingMessageData : public talk_base::MessageData {
  IsScreencastingMessageData()
      : result(false) {
  }
  bool result;
};

struct ScreencastFpsMessageData : public talk_base::MessageData {
  explicit ScreencastFpsMessageData(uint32 s)
      : ssrc(s), result(0) {
  }
  uint32 ssrc;
  int result;
};

struct SetScreenCaptureFactoryMessageData : public talk_base::MessageData {
  explicit SetScreenCaptureFactoryMessageData(
      VideoChannel::ScreenCapturerFactory* f)
      : screencapture_factory(f) {
  }
  VideoChannel::ScreenCapturerFactory* screencapture_factory;
};

static const char* PacketType(bool rtcp) {
  return (!rtcp) ? "RTP" : "RTCP";
}

static bool ValidPacket(bool rtcp, const talk_base::Buffer* packet) {
  // Check the packet size. We could check the header too if needed.
  return (packet &&
      packet->length() >= (!rtcp ? kMinRtpPacketLen : kMinRtcpPacketLen) &&
      packet->length() <= kMaxRtpPacketLen);
}

static bool IsReceiveContentDirection(MediaContentDirection direction) {
  return direction == MD_SENDRECV || direction == MD_RECVONLY;
}

static bool IsSendContentDirection(MediaContentDirection direction) {
  return direction == MD_SENDRECV || direction == MD_SENDONLY;
}

static const MediaContentDescription* GetContentDescription(
    const ContentInfo* cinfo) {
  if (cinfo == NULL)
    return NULL;
  return static_cast<const MediaContentDescription*>(cinfo->description);
}

BaseChannel::BaseChannel(talk_base::Thread* thread,
                         MediaEngineInterface* media_engine,
                         MediaChannel* media_channel, BaseSession* session,
                         const std::string& content_name, bool rtcp)
    : worker_thread_(thread),
      media_engine_(media_engine),
      session_(session),
      media_channel_(media_channel),
      content_name_(content_name),
      rtcp_(rtcp),
      transport_channel_(NULL),
      rtcp_transport_channel_(NULL),
      enabled_(false),
      writable_(false),
      rtp_ready_to_send_(false),
      rtcp_ready_to_send_(false),
      optimistic_data_send_(false),
      was_ever_writable_(false),
      local_content_direction_(MD_INACTIVE),
      remote_content_direction_(MD_INACTIVE),
      has_received_packet_(false),
      dtls_keyed_(false),
      secure_required_(false) {
  ASSERT(worker_thread_ == talk_base::Thread::Current());
  LOG(LS_INFO) << "Created channel for " << content_name;
}

BaseChannel::~BaseChannel() {
  ASSERT(worker_thread_ == talk_base::Thread::Current());
  StopConnectionMonitor();
  FlushRtcpMessages();  // Send any outstanding RTCP packets.
  Clear();  // eats any outstanding messages or packets
  // We must destroy the media channel before the transport channel, otherwise
  // the media channel may try to send on the dead transport channel. NULLing
  // is not an effective strategy since the sends will come on another thread.
  delete media_channel_;
  set_rtcp_transport_channel(NULL);
  if (transport_channel_ != NULL)
    session_->DestroyChannel(content_name_, transport_channel_->component());
  LOG(LS_INFO) << "Destroyed channel";
}

bool BaseChannel::Init(TransportChannel* transport_channel,
                       TransportChannel* rtcp_transport_channel) {
  if (transport_channel == NULL) {
    return false;
  }
  if (rtcp() && rtcp_transport_channel == NULL) {
    return false;
  }
  transport_channel_ = transport_channel;

  if (!SetDtlsSrtpCiphers(transport_channel_, false)) {
    return false;
  }

  media_channel_->SetInterface(this);
  transport_channel_->SignalWritableState.connect(
      this, &BaseChannel::OnWritableState);
  transport_channel_->SignalReadPacket.connect(
      this, &BaseChannel::OnChannelRead);
  transport_channel_->SignalReadyToSend.connect(
      this, &BaseChannel::OnReadyToSend);

  session_->SignalNewLocalDescription.connect(
      this, &BaseChannel::OnNewLocalDescription);
  session_->SignalNewRemoteDescription.connect(
      this, &BaseChannel::OnNewRemoteDescription);

  set_rtcp_transport_channel(rtcp_transport_channel);
  return true;
}

// Can be called from thread other than worker thread
bool BaseChannel::Enable(bool enable) {
  Send(enable ? MSG_ENABLE : MSG_DISABLE);
  return true;
}

// Can be called from thread other than worker thread
bool BaseChannel::MuteStream(uint32 ssrc, bool mute) {
  MuteStreamData data(ssrc, mute);
  Send(MSG_MUTESTREAM, &data);
  return data.result;
}

bool BaseChannel::IsStreamMuted(uint32 ssrc) {
  SsrcMessageData data(ssrc);
  Send(MSG_ISSTREAMMUTED, &data);
  return data.result;
}

bool BaseChannel::AddRecvStream(const StreamParams& sp) {
  StreamMessageData data(sp);
  Send(MSG_ADDRECVSTREAM, &data);
  return data.result;
}

bool BaseChannel::RemoveRecvStream(uint32 ssrc) {
  SsrcMessageData data(ssrc);
  Send(MSG_REMOVERECVSTREAM, &data);
  return data.result;
}

bool BaseChannel::SetLocalContent(const MediaContentDescription* content,
                                  ContentAction action) {
  SetContentData data(content, action);
  Send(MSG_SETLOCALCONTENT, &data);
  return data.result;
}

bool BaseChannel::SetRemoteContent(const MediaContentDescription* content,
                                   ContentAction action) {
  SetContentData data(content, action);
  Send(MSG_SETREMOTECONTENT, &data);
  return data.result;
}

bool BaseChannel::SetMaxSendBandwidth(int max_bandwidth) {
  SetBandwidthData data(max_bandwidth);
  Send(MSG_SETMAXSENDBANDWIDTH, &data);
  return data.result;
}

void BaseChannel::StartConnectionMonitor(int cms) {
  socket_monitor_.reset(new SocketMonitor(transport_channel_,
                                          worker_thread(),
                                          talk_base::Thread::Current()));
  socket_monitor_->SignalUpdate.connect(
      this, &BaseChannel::OnConnectionMonitorUpdate);
  socket_monitor_->Start(cms);
}

void BaseChannel::StopConnectionMonitor() {
  if (socket_monitor_) {
    socket_monitor_->Stop();
    socket_monitor_.reset();
  }
}

void BaseChannel::set_rtcp_transport_channel(TransportChannel* channel) {
  if (rtcp_transport_channel_ != channel) {
    if (rtcp_transport_channel_) {
      session_->DestroyChannel(
          content_name_, rtcp_transport_channel_->component());
    }
    rtcp_transport_channel_ = channel;
    if (rtcp_transport_channel_) {
      // TODO(juberti): Propagate this error code
      VERIFY(SetDtlsSrtpCiphers(rtcp_transport_channel_, true));
      rtcp_transport_channel_->SignalWritableState.connect(
          this, &BaseChannel::OnWritableState);
      rtcp_transport_channel_->SignalReadPacket.connect(
          this, &BaseChannel::OnChannelRead);
      rtcp_transport_channel_->SignalReadyToSend.connect(
          this, &BaseChannel::OnReadyToSend);
    }
  }
}

bool BaseChannel::IsReadyToReceive() const {
  // Receive data if we are enabled and have local content,
  return enabled() && IsReceiveContentDirection(local_content_direction_);
}

bool BaseChannel::IsReadyToSend() const {
  // Send outgoing data if we are enabled, have local and remote content,
  // and we have had some form of connectivity.
  return enabled() &&
         IsReceiveContentDirection(remote_content_direction_) &&
         IsSendContentDirection(local_content_direction_) &&
         was_ever_writable();
}

bool BaseChannel::SendPacket(talk_base::Buffer* packet) {
  return SendPacket(false, packet);
}

bool BaseChannel::SendRtcp(talk_base::Buffer* packet) {
  return SendPacket(true, packet);
}

int BaseChannel::SetOption(SocketType type, talk_base::Socket::Option opt,
                           int value) {
  switch (type) {
    case ST_RTP: return transport_channel_->SetOption(opt, value);
    case ST_RTCP: return rtcp_transport_channel_->SetOption(opt, value);
    default: return -1;
  }
}

void BaseChannel::OnWritableState(TransportChannel* channel) {
  ASSERT(channel == transport_channel_ || channel == rtcp_transport_channel_);
  if (transport_channel_->writable()
      && (!rtcp_transport_channel_ || rtcp_transport_channel_->writable())) {
    ChannelWritable_w();
  } else {
    ChannelNotWritable_w();
  }
}

void BaseChannel::OnChannelRead(TransportChannel* channel,
                                const char* data, size_t len, int flags) {
  // OnChannelRead gets called from P2PSocket; now pass data to MediaEngine
  ASSERT(worker_thread_ == talk_base::Thread::Current());

  // When using RTCP multiplexing we might get RTCP packets on the RTP
  // transport. We feed RTP traffic into the demuxer to determine if it is RTCP.
  bool rtcp = PacketIsRtcp(channel, data, len);
  talk_base::Buffer packet(data, len);
  HandlePacket(rtcp, &packet);
}

void BaseChannel::OnReadyToSend(TransportChannel* channel) {
  SetReadyToSend(channel, true);
}

void BaseChannel::SetReadyToSend(TransportChannel* channel, bool ready) {
  ASSERT(channel == transport_channel_ || channel == rtcp_transport_channel_);
  if (channel == transport_channel_) {
    rtp_ready_to_send_ = ready;
  }
  if (channel == rtcp_transport_channel_) {
    rtcp_ready_to_send_ = ready;
  }

  if (!ready) {
    // Notify the MediaChannel when either rtp or rtcp channel can't send.
    media_channel_->OnReadyToSend(false);
  } else if (rtp_ready_to_send_ &&
             // In the case of rtcp mux |rtcp_transport_channel_| will be null.
             (rtcp_ready_to_send_ || !rtcp_transport_channel_)) {
    // Notify the MediaChannel when both rtp and rtcp channel can send.
    media_channel_->OnReadyToSend(true);
  }
}

bool BaseChannel::PacketIsRtcp(const TransportChannel* channel,
                               const char* data, size_t len) {
  return (channel == rtcp_transport_channel_ ||
          rtcp_mux_filter_.DemuxRtcp(data, static_cast<int>(len)));
}

bool BaseChannel::SendPacket(bool rtcp, talk_base::Buffer* packet) {
  // Unless we're sending optimistically, we only allow packets through when we
  // are completely writable.
  if (!optimistic_data_send_ && !writable_) {
    return false;
  }

  // SendPacket gets called from MediaEngine, typically on an encoder thread.
  // If the thread is not our worker thread, we will post to our worker
  // so that the real work happens on our worker. This avoids us having to
  // synchronize access to all the pieces of the send path, including
  // SRTP and the inner workings of the transport channels.
  // The only downside is that we can't return a proper failure code if
  // needed. Since UDP is unreliable anyway, this should be a non-issue.
  if (talk_base::Thread::Current() != worker_thread_) {
    // Avoid a copy by transferring the ownership of the packet data.
    int message_id = (!rtcp) ? MSG_RTPPACKET : MSG_RTCPPACKET;
    PacketMessageData* data = new PacketMessageData;
    packet->TransferTo(&data->packet);
    worker_thread_->Post(this, message_id, data);
    return true;
  }

  // Now that we are on the correct thread, ensure we have a place to send this
  // packet before doing anything. (We might get RTCP packets that we don't
  // intend to send.) If we've negotiated RTCP mux, send RTCP over the RTP
  // transport.
  TransportChannel* channel = (!rtcp || rtcp_mux_filter_.IsActive()) ?
      transport_channel_ : rtcp_transport_channel_;
  if (!channel || (!optimistic_data_send_ && !channel->writable())) {
    return false;
  }

  // Protect ourselves against crazy data.
  if (!ValidPacket(rtcp, packet)) {
    LOG(LS_ERROR) << "Dropping outgoing " << content_name_ << " "
                  << PacketType(rtcp) << " packet: wrong size="
                  << packet->length();
    return false;
  }

  // Signal to the media sink before protecting the packet.
  {
    talk_base::CritScope cs(&signal_send_packet_cs_);
    SignalSendPacketPreCrypto(packet->data(), packet->length(), rtcp);
  }

  // Protect if needed.
  if (srtp_filter_.IsActive()) {
    bool res;
    char* data = packet->data();
    int len = static_cast<int>(packet->length());
    if (!rtcp) {
      res = srtp_filter_.ProtectRtp(data, len,
                                    static_cast<int>(packet->capacity()), &len);
      if (!res) {
        int seq_num = -1;
        uint32 ssrc = 0;
        GetRtpSeqNum(data, len, &seq_num);
        GetRtpSsrc(data, len, &ssrc);
        LOG(LS_ERROR) << "Failed to protect " << content_name_
                      << " RTP packet: size=" << len
                      << ", seqnum=" << seq_num << ", SSRC=" << ssrc;
        return false;
      }
    } else {
      res = srtp_filter_.ProtectRtcp(data, len,
                                     static_cast<int>(packet->capacity()),
                                     &len);
      if (!res) {
        int type = -1;
        GetRtcpType(data, len, &type);
        LOG(LS_ERROR) << "Failed to protect " << content_name_
                      << " RTCP packet: size=" << len << ", type=" << type;
        return false;
      }
    }

    // Update the length of the packet now that we've added the auth tag.
    packet->SetLength(len);
  } else if (secure_required_) {
    // This is a double check for something that supposedly can't happen.
    LOG(LS_ERROR) << "Can't send outgoing " << PacketType(rtcp)
                  << " packet when SRTP is inactive and crypto is required";

    ASSERT(false);
    return false;
  }

  // Signal to the media sink after protecting the packet.
  {
    talk_base::CritScope cs(&signal_send_packet_cs_);
    SignalSendPacketPostCrypto(packet->data(), packet->length(), rtcp);
  }

  // Bon voyage.
  int ret = channel->SendPacket(packet->data(), packet->length(),
      (secure() && secure_dtls()) ? PF_SRTP_BYPASS : 0);
  if (ret != static_cast<int>(packet->length())) {
    if (channel->GetError() == EWOULDBLOCK) {
      LOG(LS_WARNING) << "Got EWOULDBLOCK from socket.";
      SetReadyToSend(channel, false);
    }
    return false;
  }
  return true;
}

bool BaseChannel::WantsPacket(bool rtcp, talk_base::Buffer* packet) {
  // Protect ourselves against crazy data.
  if (!ValidPacket(rtcp, packet)) {
    LOG(LS_ERROR) << "Dropping incoming " << content_name_ << " "
                  << PacketType(rtcp) << " packet: wrong size="
                  << packet->length();
    return false;
  }
  // If this channel is suppose to handle RTP data, that is determined by
  // checking against ssrc filter. This is necessary to do it here to avoid
  // double decryption.
  if (ssrc_filter_.IsActive() &&
      !ssrc_filter_.DemuxPacket(packet->data(), packet->length(), rtcp)) {
    return false;
  }

  return true;
}

void BaseChannel::HandlePacket(bool rtcp, talk_base::Buffer* packet) {
  if (!WantsPacket(rtcp, packet)) {
    return;
  }

  if (!has_received_packet_) {
    has_received_packet_ = true;
    signaling_thread()->Post(this, MSG_FIRSTPACKETRECEIVED);
  }

  // Signal to the media sink before unprotecting the packet.
  {
    talk_base::CritScope cs(&signal_recv_packet_cs_);
    SignalRecvPacketPostCrypto(packet->data(), packet->length(), rtcp);
  }

  // Unprotect the packet, if needed.
  if (srtp_filter_.IsActive()) {
    char* data = packet->data();
    int len = static_cast<int>(packet->length());
    bool res;
    if (!rtcp) {
      res = srtp_filter_.UnprotectRtp(data, len, &len);
      if (!res) {
        int seq_num = -1;
        uint32 ssrc = 0;
        GetRtpSeqNum(data, len, &seq_num);
        GetRtpSsrc(data, len, &ssrc);
        LOG(LS_ERROR) << "Failed to unprotect " << content_name_
                      << " RTP packet: size=" << len
                      << ", seqnum=" << seq_num << ", SSRC=" << ssrc;
        return;
      }
    } else {
      res = srtp_filter_.UnprotectRtcp(data, len, &len);
      if (!res) {
        int type = -1;
        GetRtcpType(data, len, &type);
        LOG(LS_ERROR) << "Failed to unprotect " << content_name_
                      << " RTCP packet: size=" << len << ", type=" << type;
        return;
      }
    }

    packet->SetLength(len);
  } else if (secure_required_) {
    // Our session description indicates that SRTP is required, but we got a
    // packet before our SRTP filter is active. This means either that
    // a) we got SRTP packets before we received the SDES keys, in which case
    //    we can't decrypt it anyway, or
    // b) we got SRTP packets before DTLS completed on both the RTP and RTCP
    //    channels, so we haven't yet extracted keys, even if DTLS did complete
    //    on the channel that the packets are being sent on. It's really good
    //    practice to wait for both RTP and RTCP to be good to go before sending
    //    media, to prevent weird failure modes, so it's fine for us to just eat
    //    packets here. This is all sidestepped if RTCP mux is used anyway.
    LOG(LS_WARNING) << "Can't process incoming " << PacketType(rtcp)
                    << " packet when SRTP is inactive and crypto is required";
    return;
  }

  // Signal to the media sink after unprotecting the packet.
  {
    talk_base::CritScope cs(&signal_recv_packet_cs_);
    SignalRecvPacketPreCrypto(packet->data(), packet->length(), rtcp);
  }

  // Push it down to the media channel.
  if (!rtcp) {
    media_channel_->OnPacketReceived(packet);
  } else {
    media_channel_->OnRtcpReceived(packet);
  }
}

void BaseChannel::OnNewLocalDescription(
    BaseSession* session, ContentAction action) {
  const ContentInfo* content_info =
      GetFirstContent(session->local_description());
  const MediaContentDescription* content_desc =
      GetContentDescription(content_info);
  if (content_desc && content_info && !content_info->rejected &&
      !SetLocalContent(content_desc, action)) {
    LOG(LS_ERROR) << "Failure in SetLocalContent with action " << action;
    session->SetError(BaseSession::ERROR_CONTENT);
  }
}

void BaseChannel::OnNewRemoteDescription(
    BaseSession* session, ContentAction action) {
  const ContentInfo* content_info =
      GetFirstContent(session->remote_description());
  const MediaContentDescription* content_desc =
      GetContentDescription(content_info);
  if (content_desc && content_info && !content_info->rejected &&
      !SetRemoteContent(content_desc, action)) {
    LOG(LS_ERROR) << "Failure in SetRemoteContent with  action " << action;
    session->SetError(BaseSession::ERROR_CONTENT);
  }
}

void BaseChannel::EnableMedia_w() {
  ASSERT(worker_thread_ == talk_base::Thread::Current());
  if (enabled_)
    return;

  LOG(LS_INFO) << "Channel enabled";
  enabled_ = true;
  ChangeState();
}

void BaseChannel::DisableMedia_w() {
  ASSERT(worker_thread_ == talk_base::Thread::Current());
  if (!enabled_)
    return;

  LOG(LS_INFO) << "Channel disabled";
  enabled_ = false;
  ChangeState();
}

bool BaseChannel::MuteStream_w(uint32 ssrc, bool mute) {
  ASSERT(worker_thread_ == talk_base::Thread::Current());
  bool ret = media_channel()->MuteStream(ssrc, mute);
  if (ret) {
    if (mute)
      muted_streams_.insert(ssrc);
    else
      muted_streams_.erase(ssrc);
  }
  return ret;
}

bool BaseChannel::IsStreamMuted_w(uint32 ssrc) {
  ASSERT(worker_thread_ == talk_base::Thread::Current());
  return muted_streams_.find(ssrc) != muted_streams_.end();
}

void BaseChannel::ChannelWritable_w() {
  ASSERT(worker_thread_ == talk_base::Thread::Current());
  if (writable_)
    return;

  LOG(LS_INFO) << "Channel socket writable ("
               << transport_channel_->content_name() << ", "
               << transport_channel_->component() << ")"
               << (was_ever_writable_ ? "" : " for the first time");

  std::vector<ConnectionInfo> infos;
  transport_channel_->GetStats(&infos);
  for (std::vector<ConnectionInfo>::const_iterator it = infos.begin();
       it != infos.end(); ++it) {
    if (it->best_connection) {
      LOG(LS_INFO) << "Using " << it->local_candidate.ToSensitiveString()
                   << "->" << it->remote_candidate.ToSensitiveString();
      break;
    }
  }

  // If we're doing DTLS-SRTP, now is the time.
  if (!was_ever_writable_ && ShouldSetupDtlsSrtp()) {
    if (!SetupDtlsSrtp(false)) {
      LOG(LS_ERROR) << "Couldn't finish DTLS-SRTP on RTP channel";
      SessionErrorMessageData data(BaseSession::ERROR_TRANSPORT);
      // Sent synchronously.
      signaling_thread()->Send(this, MSG_SESSION_ERROR, &data);
      return;
    }

    if (rtcp_transport_channel_) {
      if (!SetupDtlsSrtp(true)) {
        LOG(LS_ERROR) << "Couldn't finish DTLS-SRTP on RTCP channel";
        SessionErrorMessageData data(BaseSession::ERROR_TRANSPORT);
        // Sent synchronously.
        signaling_thread()->Send(this, MSG_SESSION_ERROR, &data);
        return;
      }
    }
  }

  was_ever_writable_ = true;
  writable_ = true;
  ChangeState();
}

bool BaseChannel::SetDtlsSrtpCiphers(TransportChannel *tc, bool rtcp) {
  std::vector<std::string> ciphers;
  // We always use the default SRTP ciphers for RTCP, but we may use different
  // ciphers for RTP depending on the media type.
  if (!rtcp) {
    GetSrtpCiphers(&ciphers);
  } else {
    GetSupportedDefaultCryptoSuites(&ciphers);
  }
  return tc->SetSrtpCiphers(ciphers);
}

bool BaseChannel::ShouldSetupDtlsSrtp() const {
  return true;
}

// This function returns true if either DTLS-SRTP is not in use
// *or* DTLS-SRTP is successfully set up.
bool BaseChannel::SetupDtlsSrtp(bool rtcp_channel) {
  bool ret = false;

  TransportChannel *channel = rtcp_channel ?
      rtcp_transport_channel_ : transport_channel_;

  // No DTLS
  if (!channel->IsDtlsActive())
    return true;

  std::string selected_cipher;

  if (!channel->GetSrtpCipher(&selected_cipher)) {
    LOG(LS_ERROR) << "No DTLS-SRTP selected cipher";
    return false;
  }

  LOG(LS_INFO) << "Installing keys from DTLS-SRTP on "
               << content_name() << " "
               << PacketType(rtcp_channel);

  // OK, we're now doing DTLS (RFC 5764)
  std::vector<unsigned char> dtls_buffer(SRTP_MASTER_KEY_KEY_LEN * 2 +
                                         SRTP_MASTER_KEY_SALT_LEN * 2);

  // RFC 5705 exporter using the RFC 5764 parameters
  if (!channel->ExportKeyingMaterial(
          kDtlsSrtpExporterLabel,
          NULL, 0, false,
          &dtls_buffer[0], dtls_buffer.size())) {
    LOG(LS_WARNING) << "DTLS-SRTP key export failed";
    ASSERT(false);  // This should never happen
    return false;
  }

  // Sync up the keys with the DTLS-SRTP interface
  std::vector<unsigned char> client_write_key(SRTP_MASTER_KEY_KEY_LEN +
    SRTP_MASTER_KEY_SALT_LEN);
  std::vector<unsigned char> server_write_key(SRTP_MASTER_KEY_KEY_LEN +
    SRTP_MASTER_KEY_SALT_LEN);
  size_t offset = 0;
  memcpy(&client_write_key[0], &dtls_buffer[offset],
    SRTP_MASTER_KEY_KEY_LEN);
  offset += SRTP_MASTER_KEY_KEY_LEN;
  memcpy(&server_write_key[0], &dtls_buffer[offset],
    SRTP_MASTER_KEY_KEY_LEN);
  offset += SRTP_MASTER_KEY_KEY_LEN;
  memcpy(&client_write_key[SRTP_MASTER_KEY_KEY_LEN],
    &dtls_buffer[offset], SRTP_MASTER_KEY_SALT_LEN);
  offset += SRTP_MASTER_KEY_SALT_LEN;
  memcpy(&server_write_key[SRTP_MASTER_KEY_KEY_LEN],
    &dtls_buffer[offset], SRTP_MASTER_KEY_SALT_LEN);

  std::vector<unsigned char> *send_key, *recv_key;

  if (channel->GetRole() == ROLE_CONTROLLING) {
    send_key = &server_write_key;
    recv_key = &client_write_key;
  } else {
    send_key = &client_write_key;
    recv_key = &server_write_key;
  }

  if (rtcp_channel) {
    ret = srtp_filter_.SetRtcpParams(
        selected_cipher,
        &(*send_key)[0],
        static_cast<int>(send_key->size()),
        selected_cipher,
        &(*recv_key)[0],
        static_cast<int>(recv_key->size()));
  } else {
    ret = srtp_filter_.SetRtpParams(
        selected_cipher,
        &(*send_key)[0],
        static_cast<int>(send_key->size()),
        selected_cipher,
        &(*recv_key)[0],
        static_cast<int>(recv_key->size()));
  }

  if (!ret)
    LOG(LS_WARNING) << "DTLS-SRTP key installation failed";
  else
    dtls_keyed_ = true;

  return ret;
}

void BaseChannel::ChannelNotWritable_w() {
  ASSERT(worker_thread_ == talk_base::Thread::Current());
  if (!writable_)
    return;

  LOG(LS_INFO) << "Channel socket not writable ("
               << transport_channel_->content_name() << ", "
               << transport_channel_->component() << ")";
  writable_ = false;
  ChangeState();
}

// Sets the maximum video bandwidth for automatic bandwidth adjustment.
bool BaseChannel::SetMaxSendBandwidth_w(int max_bandwidth) {
  return media_channel()->SetSendBandwidth(true, max_bandwidth);
}

bool BaseChannel::SetSrtp_w(const std::vector<CryptoParams>& cryptos,
                            ContentAction action, ContentSource src) {
  bool ret = false;
  switch (action) {
    case CA_OFFER:
      ret = srtp_filter_.SetOffer(cryptos, src);
      break;
    case CA_PRANSWER:
      // If we're doing DTLS-SRTP, we don't want to update the filter
      // with an answer, because we already have SRTP parameters.
      if (transport_channel_->IsDtlsActive()) {
        LOG(LS_INFO) <<
          "Ignoring SDES answer parameters because we are using DTLS-SRTP";
        ret = true;
      } else {
        ret = srtp_filter_.SetProvisionalAnswer(cryptos, src);
      }
      break;
    case CA_ANSWER:
      // If we're doing DTLS-SRTP, we don't want to update the filter
      // with an answer, because we already have SRTP parameters.
      if (transport_channel_->IsDtlsActive()) {
        LOG(LS_INFO) <<
          "Ignoring SDES answer parameters because we are using DTLS-SRTP";
        ret = true;
      } else {
        ret = srtp_filter_.SetAnswer(cryptos, src);
      }
      break;
    case CA_UPDATE:
      // no crypto params.
      ret = true;
      break;
    default:
      break;
  }
  return ret;
}

bool BaseChannel::SetRtcpMux_w(bool enable, ContentAction action,
                               ContentSource src) {
  bool ret = false;
  switch (action) {
    case CA_OFFER:
      ret = rtcp_mux_filter_.SetOffer(enable, src);
      break;
    case CA_PRANSWER:
      ret = rtcp_mux_filter_.SetProvisionalAnswer(enable, src);
      break;
    case CA_ANSWER:
      ret = rtcp_mux_filter_.SetAnswer(enable, src);
      if (ret && rtcp_mux_filter_.IsActive()) {
        // We activated RTCP mux, close down the RTCP transport.
        set_rtcp_transport_channel(NULL);
      }
      break;
    case CA_UPDATE:
      // No RTCP mux info.
      ret = true;
    default:
      break;
  }
  // |rtcp_mux_filter_| can be active if |action| is CA_PRANSWER or
  // CA_ANSWER, but we only want to tear down the RTCP transport channel if we
  // received a final answer.
  if (ret && rtcp_mux_filter_.IsActive()) {
    // If the RTP transport is already writable, then so are we.
    if (transport_channel_->writable()) {
      ChannelWritable_w();
    }
  }

  return ret;
}

bool BaseChannel::AddRecvStream_w(const StreamParams& sp) {
  ASSERT(worker_thread() == talk_base::Thread::Current());
  if (!media_channel()->AddRecvStream(sp))
    return false;

  return ssrc_filter_.AddStream(sp);
}

bool BaseChannel::RemoveRecvStream_w(uint32 ssrc) {
  ASSERT(worker_thread() == talk_base::Thread::Current());
  ssrc_filter_.RemoveStream(ssrc);
  return media_channel()->RemoveRecvStream(ssrc);
}

bool BaseChannel::UpdateLocalStreams_w(const std::vector<StreamParams>& streams,
                                       ContentAction action) {
  if (!VERIFY(action == CA_OFFER || action == CA_ANSWER ||
              action == CA_PRANSWER || action == CA_UPDATE))
    return false;

  // If this is an update, streams only contain streams that have changed.
  if (action == CA_UPDATE) {
    for (StreamParamsVec::const_iterator it = streams.begin();
         it != streams.end(); ++it) {
      StreamParams existing_stream;
      bool stream_exist = GetStreamByIds(local_streams_, it->groupid,
                                         it->id, &existing_stream);
      if (!stream_exist && it->has_ssrcs()) {
        if (media_channel()->AddSendStream(*it)) {
          local_streams_.push_back(*it);
          LOG(LS_INFO) << "Add send stream ssrc: " << it->first_ssrc();
        } else {
          LOG(LS_INFO) << "Failed to add send stream ssrc: "
                       << it->first_ssrc();
          return false;
        }
      } else if (stream_exist && !it->has_ssrcs()) {
        if (!media_channel()->RemoveSendStream(existing_stream.first_ssrc())) {
            LOG(LS_ERROR) << "Failed to remove send stream with ssrc "
                          << it->first_ssrc() << ".";
            return false;
        }
        RemoveStreamBySsrc(&local_streams_, existing_stream.first_ssrc());
      } else {
        LOG(LS_WARNING) << "Ignore unsupported stream update";
      }
    }
    return true;
  }
  // Else streams are all the streams we want to send.

  // Check for streams that have been removed.
  bool ret = true;
  for (StreamParamsVec::const_iterator it = local_streams_.begin();
       it != local_streams_.end(); ++it) {
    if (!GetStreamBySsrc(streams, it->first_ssrc(), NULL)) {
      if (!media_channel()->RemoveSendStream(it->first_ssrc())) {
        LOG(LS_ERROR) << "Failed to remove send stream with ssrc "
                      << it->first_ssrc() << ".";
        ret = false;
      }
    }
  }
  // Check for new streams.
  for (StreamParamsVec::const_iterator it = streams.begin();
       it != streams.end(); ++it) {
    if (!GetStreamBySsrc(local_streams_, it->first_ssrc(), NULL)) {
      if (media_channel()->AddSendStream(*it)) {
        LOG(LS_INFO) << "Add send ssrc: " << it->ssrcs[0];
      } else {
        LOG(LS_INFO) << "Failed to add send stream ssrc: " << it->first_ssrc();
        ret = false;
      }
    }
  }
  local_streams_ = streams;
  return ret;
}

bool BaseChannel::UpdateRemoteStreams_w(
    const std::vector<StreamParams>& streams,
    ContentAction action) {
  if (!VERIFY(action == CA_OFFER || action == CA_ANSWER ||
              action == CA_PRANSWER || action == CA_UPDATE))
    return false;

  // If this is an update, streams only contain streams that have changed.
  if (action == CA_UPDATE) {
    for (StreamParamsVec::const_iterator it = streams.begin();
         it != streams.end(); ++it) {
      StreamParams existing_stream;
      bool stream_exists = GetStreamByIds(remote_streams_, it->groupid,
                                          it->id, &existing_stream);
      if (!stream_exists && it->has_ssrcs()) {
        if (AddRecvStream_w(*it)) {
          remote_streams_.push_back(*it);
          LOG(LS_INFO) << "Add remote stream ssrc: " << it->first_ssrc();
        } else {
          LOG(LS_INFO) << "Failed to add remote stream ssrc: "
                       << it->first_ssrc();
          return false;
        }
      } else if (stream_exists && !it->has_ssrcs()) {
        if (!RemoveRecvStream_w(existing_stream.first_ssrc())) {
            LOG(LS_ERROR) << "Failed to remove remote stream with ssrc "
                          << it->first_ssrc() << ".";
            return false;
        }
        RemoveStreamBySsrc(&remote_streams_, existing_stream.first_ssrc());
      } else {
        LOG(LS_WARNING) << "Ignore unsupported stream update."
                        << " Stream exists? " << stream_exists
                        << " existing stream = " << existing_stream.ToString()
                        << " new stream = " << it->ToString();
      }
    }
    return true;
  }
  // Else streams are all the streams we want to receive.

  // Check for streams that have been removed.
  bool ret = true;
  for (StreamParamsVec::const_iterator it = remote_streams_.begin();
       it != remote_streams_.end(); ++it) {
    if (!GetStreamBySsrc(streams, it->first_ssrc(), NULL)) {
      if (!RemoveRecvStream_w(it->first_ssrc())) {
        LOG(LS_ERROR) << "Failed to remove remote stream with ssrc "
                      << it->first_ssrc() << ".";
        ret = false;
      }
    }
  }
  // Check for new streams.
  for (StreamParamsVec::const_iterator it = streams.begin();
      it != streams.end(); ++it) {
    if (!GetStreamBySsrc(remote_streams_, it->first_ssrc(), NULL)) {
      if (AddRecvStream_w(*it)) {
        LOG(LS_INFO) << "Add remote ssrc: " << it->ssrcs[0];
      } else {
        LOG(LS_INFO) << "Failed to add remote stream ssrc: "
                     << it->first_ssrc();
        ret = false;
      }
    }
  }
  remote_streams_ = streams;
  return ret;
}

bool BaseChannel::SetBaseLocalContent_w(const MediaContentDescription* content,
                                        ContentAction action) {
  // Cache secure_required_ for belt and suspenders check on SendPacket
  secure_required_ = content->crypto_required();
  bool ret = UpdateLocalStreams_w(content->streams(), action);
  // Set local SRTP parameters (what we will encrypt with).
  ret &= SetSrtp_w(content->cryptos(), action, CS_LOCAL);
  // Set local RTCP mux parameters.
  ret &= SetRtcpMux_w(content->rtcp_mux(), action, CS_LOCAL);
  // Set local RTP header extensions.
  if (content->rtp_header_extensions_set()) {
    ret &= media_channel()->SetRecvRtpHeaderExtensions(
        content->rtp_header_extensions());
  }
  set_local_content_direction(content->direction());
  return ret;
}

bool BaseChannel::SetBaseRemoteContent_w(const MediaContentDescription* content,
                                         ContentAction action) {
  bool ret = UpdateRemoteStreams_w(content->streams(), action);
  // Set remote SRTP parameters (what the other side will encrypt with).
  ret &= SetSrtp_w(content->cryptos(), action, CS_REMOTE);
  // Set remote RTCP mux parameters.
  ret &= SetRtcpMux_w(content->rtcp_mux(), action, CS_REMOTE);
  // Set remote RTP header extensions.
  if (content->rtp_header_extensions_set()) {
    ret &= media_channel()->SetSendRtpHeaderExtensions(
        content->rtp_header_extensions());
  }
  if (content->bandwidth() != kAutoBandwidth) {
    ret &= media_channel()->SetSendBandwidth(false, content->bandwidth());
  }
  set_remote_content_direction(content->direction());
  return ret;
}

void BaseChannel::OnMessage(talk_base::Message *pmsg) {
  switch (pmsg->message_id) {
    case MSG_ENABLE:
      EnableMedia_w();
      break;
    case MSG_DISABLE:
      DisableMedia_w();
      break;
    case MSG_MUTESTREAM: {
      MuteStreamData* data = static_cast<MuteStreamData*>(pmsg->pdata);
      data->result = MuteStream_w(data->ssrc, data->mute);
      break;
    }
    case MSG_ISSTREAMMUTED: {
      SsrcMessageData* data = static_cast<SsrcMessageData*>(pmsg->pdata);
      data->result = IsStreamMuted_w(data->ssrc);
      break;
    }
    case MSG_SETLOCALCONTENT: {
      SetContentData* data = static_cast<SetContentData*>(pmsg->pdata);
      data->result = SetLocalContent_w(data->content, data->action);
      break;
    }
    case MSG_SETREMOTECONTENT: {
      SetContentData* data = static_cast<SetContentData*>(pmsg->pdata);
      data->result = SetRemoteContent_w(data->content, data->action);
      break;
    }
    case MSG_ADDRECVSTREAM: {
      StreamMessageData* data = static_cast<StreamMessageData*>(pmsg->pdata);
      data->result = AddRecvStream_w(data->sp);
      break;
    }
    case MSG_REMOVERECVSTREAM: {
      SsrcMessageData* data = static_cast<SsrcMessageData*>(pmsg->pdata);
      data->result = RemoveRecvStream_w(data->ssrc);
      break;
    }
    case MSG_SETMAXSENDBANDWIDTH: {
      SetBandwidthData* data = static_cast<SetBandwidthData*>(pmsg->pdata);
      data->result = SetMaxSendBandwidth_w(data->value);
      break;
    }

    case MSG_RTPPACKET:
    case MSG_RTCPPACKET: {
      PacketMessageData* data = static_cast<PacketMessageData*>(pmsg->pdata);
      SendPacket(pmsg->message_id == MSG_RTCPPACKET, &data->packet);
      delete data;  // because it is Posted
      break;
    }
    case MSG_FIRSTPACKETRECEIVED: {
      SignalFirstPacketReceived(this);
      break;
    }
    case MSG_SESSION_ERROR: {
      SessionErrorMessageData* data = static_cast<SessionErrorMessageData*>
          (pmsg->pdata);
      session_->SetError(data->error_);
      break;
    }
  }
}

void BaseChannel::Send(uint32 id, talk_base::MessageData *pdata) {
  worker_thread_->Send(this, id, pdata);
}

void BaseChannel::Post(uint32 id, talk_base::MessageData *pdata) {
  worker_thread_->Post(this, id, pdata);
}

void BaseChannel::PostDelayed(int cmsDelay, uint32 id,
                              talk_base::MessageData *pdata) {
  worker_thread_->PostDelayed(cmsDelay, this, id, pdata);
}

void BaseChannel::Clear(uint32 id, talk_base::MessageList* removed) {
  worker_thread_->Clear(this, id, removed);
}

void BaseChannel::FlushRtcpMessages() {
  // Flush all remaining RTCP messages. This should only be called in
  // destructor.
  ASSERT(talk_base::Thread::Current() == worker_thread_);
  talk_base::MessageList rtcp_messages;
  Clear(MSG_RTCPPACKET, &rtcp_messages);
  for (talk_base::MessageList::iterator it = rtcp_messages.begin();
       it != rtcp_messages.end(); ++it) {
    Send(MSG_RTCPPACKET, it->pdata);
  }
}

VoiceChannel::VoiceChannel(talk_base::Thread* thread,
                           MediaEngineInterface* media_engine,
                           VoiceMediaChannel* media_channel,
                           BaseSession* session,
                           const std::string& content_name,
                           bool rtcp)
    : BaseChannel(thread, media_engine, media_channel, session, content_name,
                  rtcp),
      received_media_(false) {
}

VoiceChannel::~VoiceChannel() {
  StopAudioMonitor();
  StopMediaMonitor();
  // this can't be done in the base class, since it calls a virtual
  DisableMedia_w();
}

bool VoiceChannel::Init() {
  TransportChannel* rtcp_channel = rtcp() ? session()->CreateChannel(
      content_name(), "rtcp", ICE_CANDIDATE_COMPONENT_RTCP) : NULL;
  if (!BaseChannel::Init(session()->CreateChannel(
          content_name(), "rtp", ICE_CANDIDATE_COMPONENT_RTP),
          rtcp_channel)) {
    return false;
  }
  media_channel()->SignalMediaError.connect(
      this, &VoiceChannel::OnVoiceChannelError);
  srtp_filter()->SignalSrtpError.connect(
      this, &VoiceChannel::OnSrtpError);
  return true;
}

bool VoiceChannel::SetRenderer(uint32 ssrc, AudioRenderer* renderer) {
  AudioRenderMessageData data(ssrc, renderer);
  Send(MSG_SETRENDERER, &data);
  return data.result;
}

bool VoiceChannel::SetRingbackTone(const void* buf, int len) {
  SetRingbackToneMessageData data(buf, len);
  Send(MSG_SETRINGBACKTONE, &data);
  return data.result;
}

// TODO(juberti): Handle early media the right way. We should get an explicit
// ringing message telling us to start playing local ringback, which we cancel
// if any early media actually arrives. For now, we do the opposite, which is
// to wait 1 second for early media, and start playing local ringback if none
// arrives.
void VoiceChannel::SetEarlyMedia(bool enable) {
  if (enable) {
    // Start the early media timeout
    PostDelayed(kEarlyMediaTimeout, MSG_EARLYMEDIATIMEOUT);
  } else {
    // Stop the timeout if currently going.
    Clear(MSG_EARLYMEDIATIMEOUT);
  }
}

bool VoiceChannel::PlayRingbackTone(uint32 ssrc, bool play, bool loop) {
  PlayRingbackToneMessageData data(ssrc, play, loop);
  Send(MSG_PLAYRINGBACKTONE, &data);
  return data.result;
}

bool VoiceChannel::PressDTMF(int digit, bool playout) {
  int flags = DF_SEND;
  if (playout) {
    flags |= DF_PLAY;
  }
  int duration_ms = 160;
  return InsertDtmf(0, digit, duration_ms, flags);
}

bool VoiceChannel::CanInsertDtmf() {
  BoolMessageData data(false);
  Send(MSG_CANINSERTDTMF, &data);
  return data.data();
}

bool VoiceChannel::InsertDtmf(uint32 ssrc, int event_code, int duration,
                              int flags) {
  DtmfMessageData data(ssrc, event_code, duration, flags);
  Send(MSG_INSERTDTMF, &data);
  return data.result;
}

bool VoiceChannel::SetOutputScaling(uint32 ssrc, double left, double right) {
  ScaleVolumeMessageData data(ssrc, left, right);
  Send(MSG_SCALEVOLUME, &data);
  return data.result;
}
bool VoiceChannel::GetStats(VoiceMediaInfo* stats) {
  VoiceStatsMessageData data(stats);
  Send(MSG_GETSTATS, &data);
  return data.result;
}

void VoiceChannel::StartMediaMonitor(int cms) {
  media_monitor_.reset(new VoiceMediaMonitor(media_channel(), worker_thread(),
      talk_base::Thread::Current()));
  media_monitor_->SignalUpdate.connect(
      this, &VoiceChannel::OnMediaMonitorUpdate);
  media_monitor_->Start(cms);
}

void VoiceChannel::StopMediaMonitor() {
  if (media_monitor_) {
    media_monitor_->Stop();
    media_monitor_->SignalUpdate.disconnect(this);
    media_monitor_.reset();
  }
}

void VoiceChannel::StartAudioMonitor(int cms) {
  audio_monitor_.reset(new AudioMonitor(this, talk_base::Thread::Current()));
  audio_monitor_
    ->SignalUpdate.connect(this, &VoiceChannel::OnAudioMonitorUpdate);
  audio_monitor_->Start(cms);
}

void VoiceChannel::StopAudioMonitor() {
  if (audio_monitor_) {
    audio_monitor_->Stop();
    audio_monitor_.reset();
  }
}

bool VoiceChannel::IsAudioMonitorRunning() const {
  return (audio_monitor_.get() != NULL);
}

void VoiceChannel::StartTypingMonitor(const TypingMonitorOptions& settings) {
  typing_monitor_.reset(new TypingMonitor(this, worker_thread(), settings));
  SignalAutoMuted.repeat(typing_monitor_->SignalMuted);
}

void VoiceChannel::StopTypingMonitor() {
  typing_monitor_.reset();
}

bool VoiceChannel::IsTypingMonitorRunning() const {
  return typing_monitor_;
}

bool VoiceChannel::MuteStream_w(uint32 ssrc, bool mute) {
  bool ret = BaseChannel::MuteStream_w(ssrc, mute);
  if (typing_monitor_ && mute)
    typing_monitor_->OnChannelMuted();
  return ret;
}

int VoiceChannel::GetInputLevel_w() {
  return media_engine()->GetInputLevel();
}

int VoiceChannel::GetOutputLevel_w() {
  return media_channel()->GetOutputLevel();
}

void VoiceChannel::GetActiveStreams_w(AudioInfo::StreamList* actives) {
  media_channel()->GetActiveStreams(actives);
}

void VoiceChannel::OnChannelRead(TransportChannel* channel,
                                 const char* data, size_t len, int flags) {
  BaseChannel::OnChannelRead(channel, data, len, flags);

  // Set a flag when we've received an RTP packet. If we're waiting for early
  // media, this will disable the timeout.
  if (!received_media_ && !PacketIsRtcp(channel, data, len)) {
    received_media_ = true;
  }
}

void VoiceChannel::ChangeState() {
  // Render incoming data if we're the active call, and we have the local
  // content. We receive data on the default channel and multiplexed streams.
  bool recv = IsReadyToReceive();
  if (!media_channel()->SetPlayout(recv)) {
    SendLastMediaError();
  }

  // Send outgoing data if we're the active call, we have the remote content,
  // and we have had some form of connectivity.
  bool send = IsReadyToSend();
  SendFlags send_flag = send ? SEND_MICROPHONE : SEND_NOTHING;
  if (!media_channel()->SetSend(send_flag)) {
    LOG(LS_ERROR) << "Failed to SetSend " << send_flag << " on voice channel";
    SendLastMediaError();
  }

  LOG(LS_INFO) << "Changing voice state, recv=" << recv << " send=" << send;
}

const ContentInfo* VoiceChannel::GetFirstContent(
    const SessionDescription* sdesc) {
  return GetFirstAudioContent(sdesc);
}

bool VoiceChannel::SetLocalContent_w(const MediaContentDescription* content,
                                     ContentAction action) {
  ASSERT(worker_thread() == talk_base::Thread::Current());
  LOG(LS_INFO) << "Setting local voice description";

  const AudioContentDescription* audio =
      static_cast<const AudioContentDescription*>(content);
  ASSERT(audio != NULL);
  if (!audio) return false;

  bool ret = SetBaseLocalContent_w(content, action);
  // Set local audio codecs (what we want to receive).
  // TODO(whyuan): Change action != CA_UPDATE to !audio->partial() when partial
  // is set properly.
  if (action != CA_UPDATE || audio->has_codecs()) {
    ret &= media_channel()->SetRecvCodecs(audio->codecs());
  }

  // If everything worked, see if we can start receiving.
  if (ret) {
    ChangeState();
  } else {
    LOG(LS_WARNING) << "Failed to set local voice description";
  }
  return ret;
}

bool VoiceChannel::SetRemoteContent_w(const MediaContentDescription* content,
                                      ContentAction action) {
  ASSERT(worker_thread() == talk_base::Thread::Current());
  LOG(LS_INFO) << "Setting remote voice description";

  const AudioContentDescription* audio =
      static_cast<const AudioContentDescription*>(content);
  ASSERT(audio != NULL);
  if (!audio) return false;

  bool ret = true;
  // Set remote video codecs (what the other side wants to receive).
  if (action != CA_UPDATE || audio->has_codecs()) {
    ret &= media_channel()->SetSendCodecs(audio->codecs());
  }

  ret &= SetBaseRemoteContent_w(content, action);

  if (action != CA_UPDATE) {
    // Tweak our audio processing settings, if needed.
    AudioOptions audio_options;
    if (!media_channel()->GetOptions(&audio_options)) {
      LOG(LS_WARNING) << "Can not set audio options from on remote content.";
    } else {
      if (audio->conference_mode()) {
        audio_options.conference_mode.Set(true);
      }
      if (audio->agc_minus_10db()) {
        audio_options.adjust_agc_delta.Set(kAgcMinus10db);
      }
      if (!media_channel()->SetOptions(audio_options)) {
        // Log an error on failure, but don't abort the call.
        LOG(LS_ERROR) << "Failed to set voice channel options";
      }
    }
  }

  // If everything worked, see if we can start sending.
  if (ret) {
    ChangeState();
  } else {
    LOG(LS_WARNING) << "Failed to set remote voice description";
  }
  return ret;
}

bool VoiceChannel::SetRingbackTone_w(const void* buf, int len) {
  ASSERT(worker_thread() == talk_base::Thread::Current());
  return media_channel()->SetRingbackTone(static_cast<const char*>(buf), len);
}

bool VoiceChannel::PlayRingbackTone_w(uint32 ssrc, bool play, bool loop) {
  ASSERT(worker_thread() == talk_base::Thread::Current());
  if (play) {
    LOG(LS_INFO) << "Playing ringback tone, loop=" << loop;
  } else {
    LOG(LS_INFO) << "Stopping ringback tone";
  }
  return media_channel()->PlayRingbackTone(ssrc, play, loop);
}

void VoiceChannel::HandleEarlyMediaTimeout() {
  // This occurs on the main thread, not the worker thread.
  if (!received_media_) {
    LOG(LS_INFO) << "No early media received before timeout";
    SignalEarlyMediaTimeout(this);
  }
}

bool VoiceChannel::CanInsertDtmf_w() {
  return media_channel()->CanInsertDtmf();
}

bool VoiceChannel::InsertDtmf_w(uint32 ssrc, int event, int duration,
                                int flags) {
  if (!enabled()) {
    return false;
  }

  return media_channel()->InsertDtmf(ssrc, event, duration, flags);
}

bool VoiceChannel::SetOutputScaling_w(uint32 ssrc, double left, double right) {
  return media_channel()->SetOutputScaling(ssrc, left, right);
}

bool VoiceChannel::GetStats_w(VoiceMediaInfo* stats) {
  return media_channel()->GetStats(stats);
}

bool VoiceChannel::SetChannelOptions(const AudioOptions& options) {
  AudioOptionsMessageData data(options);
  Send(MSG_SETCHANNELOPTIONS, &data);
  return data.result;
}

bool VoiceChannel::SetChannelOptions_w(const AudioOptions& options) {
  return media_channel()->SetOptions(options);
}

bool VoiceChannel::SetRenderer_w(uint32 ssrc, AudioRenderer* renderer) {
  return media_channel()->SetRenderer(ssrc, renderer);
}

void VoiceChannel::OnMessage(talk_base::Message *pmsg) {
  switch (pmsg->message_id) {
    case MSG_SETRINGBACKTONE: {
      SetRingbackToneMessageData* data =
          static_cast<SetRingbackToneMessageData*>(pmsg->pdata);
      data->result = SetRingbackTone_w(data->buf, data->len);
      break;
    }
    case MSG_PLAYRINGBACKTONE: {
      PlayRingbackToneMessageData* data =
          static_cast<PlayRingbackToneMessageData*>(pmsg->pdata);
      data->result = PlayRingbackTone_w(data->ssrc, data->play, data->loop);
      break;
    }
    case MSG_EARLYMEDIATIMEOUT:
      HandleEarlyMediaTimeout();
      break;
    case MSG_CANINSERTDTMF: {
      BoolMessageData* data =
          static_cast<BoolMessageData*>(pmsg->pdata);
      data->data() = CanInsertDtmf_w();
      break;
    }
    case MSG_INSERTDTMF: {
      DtmfMessageData* data =
          static_cast<DtmfMessageData*>(pmsg->pdata);
      data->result = InsertDtmf_w(data->ssrc, data->event, data->duration,
                                  data->flags);
      break;
    }
    case MSG_SCALEVOLUME: {
      ScaleVolumeMessageData* data =
          static_cast<ScaleVolumeMessageData*>(pmsg->pdata);
      data->result = SetOutputScaling_w(data->ssrc, data->left, data->right);
      break;
    }
    case MSG_GETSTATS: {
      VoiceStatsMessageData* data =
          static_cast<VoiceStatsMessageData*>(pmsg->pdata);
      data->result = GetStats_w(data->stats);
      break;
    }
    case MSG_CHANNEL_ERROR: {
      VoiceChannelErrorMessageData* data =
          static_cast<VoiceChannelErrorMessageData*>(pmsg->pdata);
      SignalMediaError(this, data->ssrc, data->error);
      delete data;
      break;
    }
    case MSG_SETCHANNELOPTIONS: {
      AudioOptionsMessageData* data =
          static_cast<AudioOptionsMessageData*>(pmsg->pdata);
      data->result = SetChannelOptions_w(data->options);
      break;
    }
    case MSG_SETRENDERER: {
      AudioRenderMessageData* data =
          static_cast<AudioRenderMessageData*>(pmsg->pdata);
      data->result = SetRenderer_w(data->ssrc, data->renderer);
      break;
    }
    default:
      BaseChannel::OnMessage(pmsg);
      break;
  }
}

void VoiceChannel::OnConnectionMonitorUpdate(
    SocketMonitor* monitor, const std::vector<ConnectionInfo>& infos) {
  SignalConnectionMonitor(this, infos);
}

void VoiceChannel::OnMediaMonitorUpdate(
    VoiceMediaChannel* media_channel, const VoiceMediaInfo& info) {
  ASSERT(media_channel == this->media_channel());
  SignalMediaMonitor(this, info);
}

void VoiceChannel::OnAudioMonitorUpdate(AudioMonitor* monitor,
                                        const AudioInfo& info) {
  SignalAudioMonitor(this, info);
}

void VoiceChannel::OnVoiceChannelError(
    uint32 ssrc, VoiceMediaChannel::Error err) {
  VoiceChannelErrorMessageData* data = new VoiceChannelErrorMessageData(
      ssrc, err);
  signaling_thread()->Post(this, MSG_CHANNEL_ERROR, data);
}

void VoiceChannel::OnSrtpError(uint32 ssrc, SrtpFilter::Mode mode,
                               SrtpFilter::Error error) {
  switch (error) {
    case SrtpFilter::ERROR_FAIL:
      OnVoiceChannelError(ssrc, (mode == SrtpFilter::PROTECT) ?
                          VoiceMediaChannel::ERROR_REC_SRTP_ERROR :
                          VoiceMediaChannel::ERROR_PLAY_SRTP_ERROR);
      break;
    case SrtpFilter::ERROR_AUTH:
      OnVoiceChannelError(ssrc, (mode == SrtpFilter::PROTECT) ?
                          VoiceMediaChannel::ERROR_REC_SRTP_AUTH_FAILED :
                          VoiceMediaChannel::ERROR_PLAY_SRTP_AUTH_FAILED);
      break;
    case SrtpFilter::ERROR_REPLAY:
      // Only receving channel should have this error.
      ASSERT(mode == SrtpFilter::UNPROTECT);
      OnVoiceChannelError(ssrc, VoiceMediaChannel::ERROR_PLAY_SRTP_REPLAY);
      break;
    default:
      break;
  }
}

void VoiceChannel::GetSrtpCiphers(std::vector<std::string>* ciphers) const {
  GetSupportedAudioCryptoSuites(ciphers);
}

VideoChannel::VideoChannel(talk_base::Thread* thread,
                           MediaEngineInterface* media_engine,
                           VideoMediaChannel* media_channel,
                           BaseSession* session,
                           const std::string& content_name,
                           bool rtcp,
                           VoiceChannel* voice_channel)
    : BaseChannel(thread, media_engine, media_channel, session, content_name,
                  rtcp),
      voice_channel_(voice_channel),
      renderer_(NULL),
      screencapture_factory_(CreateScreenCapturerFactory()),
      previous_we_(talk_base::WE_CLOSE) {
}

bool VideoChannel::Init() {
  TransportChannel* rtcp_channel = rtcp() ? session()->CreateChannel(
      content_name(), "video_rtcp", ICE_CANDIDATE_COMPONENT_RTCP) : NULL;
  if (!BaseChannel::Init(session()->CreateChannel(
          content_name(), "video_rtp", ICE_CANDIDATE_COMPONENT_RTP),
          rtcp_channel)) {
    return false;
  }
  media_channel()->SignalMediaError.connect(
      this, &VideoChannel::OnVideoChannelError);
  srtp_filter()->SignalSrtpError.connect(
      this, &VideoChannel::OnSrtpError);
  return true;
}

void VoiceChannel::SendLastMediaError() {
  uint32 ssrc;
  VoiceMediaChannel::Error error;
  media_channel()->GetLastMediaError(&ssrc, &error);
  SignalMediaError(this, ssrc, error);
}

VideoChannel::~VideoChannel() {
  std::vector<uint32> screencast_ssrcs;
  ScreencastMap::iterator iter;
  while (!screencast_capturers_.empty()) {
    if (!RemoveScreencast(screencast_capturers_.begin()->first)) {
      LOG(LS_ERROR) << "Unable to delete screencast with ssrc "
                    << screencast_capturers_.begin()->first;
      ASSERT(false);
      break;
    }
  }

  StopMediaMonitor();
  // this can't be done in the base class, since it calls a virtual
  DisableMedia_w();
}

bool VideoChannel::SetRenderer(uint32 ssrc, VideoRenderer* renderer) {
  VideoRenderMessageData data(ssrc, renderer);
  Send(MSG_SETRENDERER, &data);
  return true;
}

bool VideoChannel::ApplyViewRequest(const ViewRequest& request) {
  ViewRequestMessageData data(request);
  Send(MSG_HANDLEVIEWREQUEST, &data);
  return data.result;
}

VideoCapturer* VideoChannel::AddScreencast(
    uint32 ssrc, const ScreencastId& id) {
  AddScreencastMessageData data(ssrc, id);
  Send(MSG_ADDSCREENCAST, &data);
  return data.result;
}

bool VideoChannel::SetCapturer(uint32 ssrc, VideoCapturer* capturer) {
  SetCapturerMessageData data(ssrc, capturer);
  Send(MSG_SETCAPTURER, &data);
  return data.result;
}

bool VideoChannel::RemoveScreencast(uint32 ssrc) {
  RemoveScreencastMessageData data(ssrc);
  Send(MSG_REMOVESCREENCAST, &data);
  return data.result;
}

bool VideoChannel::IsScreencasting() {
  IsScreencastingMessageData data;
  Send(MSG_ISSCREENCASTING, &data);
  return data.result;
}

int VideoChannel::ScreencastFps(uint32 ssrc) {
  ScreencastFpsMessageData data(ssrc);
  Send(MSG_SCREENCASTFPS, &data);
  return data.result;
}

bool VideoChannel::SendIntraFrame() {
  Send(MSG_SENDINTRAFRAME);
  return true;
}

bool VideoChannel::RequestIntraFrame() {
  Send(MSG_REQUESTINTRAFRAME);
  return true;
}

void VideoChannel::SetScreenCaptureFactory(
    ScreenCapturerFactory* screencapture_factory) {
  SetScreenCaptureFactoryMessageData data(screencapture_factory);
  Send(MSG_SETSCREENCASTFACTORY, &data);
}

void VideoChannel::ChangeState() {
  // Render incoming data if we're the active call, and we have the local
  // content. We receive data on the default channel and multiplexed streams.
  bool recv = IsReadyToReceive();
  if (!media_channel()->SetRender(recv)) {
    LOG(LS_ERROR) << "Failed to SetRender on video channel";
    // TODO(gangji): Report error back to server.
  }

  // Send outgoing data if we're the active call, we have the remote content,
  // and we have had some form of connectivity.
  bool send = IsReadyToSend();
  if (!media_channel()->SetSend(send)) {
    LOG(LS_ERROR) << "Failed to SetSend on video channel";
    // TODO(gangji): Report error back to server.
  }

  LOG(LS_INFO) << "Changing video state, recv=" << recv << " send=" << send;
}

bool VideoChannel::GetStats(VideoMediaInfo* stats) {
  VideoStatsMessageData data(stats);
  Send(MSG_GETSTATS, &data);
  return data.result;
}

void VideoChannel::StartMediaMonitor(int cms) {
  media_monitor_.reset(new VideoMediaMonitor(media_channel(), worker_thread(),
      talk_base::Thread::Current()));
  media_monitor_->SignalUpdate.connect(
      this, &VideoChannel::OnMediaMonitorUpdate);
  media_monitor_->Start(cms);
}

void VideoChannel::StopMediaMonitor() {
  if (media_monitor_) {
    media_monitor_->Stop();
    media_monitor_.reset();
  }
}

const ContentInfo* VideoChannel::GetFirstContent(
    const SessionDescription* sdesc) {
  return GetFirstVideoContent(sdesc);
}

bool VideoChannel::SetLocalContent_w(const MediaContentDescription* content,
                                     ContentAction action) {
  ASSERT(worker_thread() == talk_base::Thread::Current());
  LOG(LS_INFO) << "Setting local video description";

  const VideoContentDescription* video =
      static_cast<const VideoContentDescription*>(content);
  ASSERT(video != NULL);
  if (!video) return false;

  bool ret = SetBaseLocalContent_w(content, action);
  // Set local video codecs (what we want to receive).
  if (action != CA_UPDATE || video->has_codecs()) {
    ret &= media_channel()->SetRecvCodecs(video->codecs());
  }

  if (action != CA_UPDATE) {
    VideoOptions video_options;
    media_channel()->GetOptions(&video_options);
    video_options.buffered_mode_latency.Set(video->buffered_mode_latency());

    if (!media_channel()->SetOptions(video_options)) {
      // Log an error on failure, but don't abort the call.
      LOG(LS_ERROR) << "Failed to set video channel options";
    }
  }

  // If everything worked, see if we can start receiving.
  if (ret) {
    ChangeState();
  } else {
    LOG(LS_WARNING) << "Failed to set local video description";
  }
  return ret;
}

bool VideoChannel::SetRemoteContent_w(const MediaContentDescription* content,
                                      ContentAction action) {
  ASSERT(worker_thread() == talk_base::Thread::Current());
  LOG(LS_INFO) << "Setting remote video description";

  const VideoContentDescription* video =
      static_cast<const VideoContentDescription*>(content);
  ASSERT(video != NULL);
  if (!video) return false;

  bool ret = true;
  // Set remote video codecs (what the other side wants to receive).
  if (action != CA_UPDATE || video->has_codecs()) {
    ret &= media_channel()->SetSendCodecs(video->codecs());
  }

  ret &= SetBaseRemoteContent_w(content, action);

  if (action != CA_UPDATE) {
    // Tweak our video processing settings, if needed.
    VideoOptions video_options;
    media_channel()->GetOptions(&video_options);
    video_options.conference_mode.Set(video->conference_mode());
    video_options.buffered_mode_latency.Set(video->buffered_mode_latency());

    if (!media_channel()->SetOptions(video_options)) {
      // Log an error on failure, but don't abort the call.
      LOG(LS_ERROR) << "Failed to set video channel options";
    }
  }

  // If everything worked, see if we can start sending.
  if (ret) {
    ChangeState();
  } else {
    LOG(LS_WARNING) << "Failed to set remote video description";
  }
  return ret;
}

bool VideoChannel::ApplyViewRequest_w(const ViewRequest& request) {
  bool ret = true;
  // Set the send format for each of the local streams. If the view request
  // does not contain a local stream, set its send format to 0x0, which will
  // drop all frames.
  for (std::vector<StreamParams>::const_iterator it = local_streams().begin();
      it != local_streams().end(); ++it) {
    VideoFormat format(0, 0, 0, cricket::FOURCC_I420);
    StaticVideoViews::const_iterator view;
    for (view = request.static_video_views.begin();
         view != request.static_video_views.end(); ++view) {
      if (view->selector.Matches(*it)) {
        format.width = view->width;
        format.height = view->height;
        format.interval = cricket::VideoFormat::FpsToInterval(view->framerate);
        break;
      }
    }

    ret &= media_channel()->SetSendStreamFormat(it->first_ssrc(), format);
  }

  // Check if the view request has invalid streams.
  for (StaticVideoViews::const_iterator it = request.static_video_views.begin();
      it != request.static_video_views.end(); ++it) {
    if (!GetStream(local_streams(), it->selector, NULL)) {
      LOG(LS_WARNING) << "View request for ("
                      << it->selector.ssrc << ", '"
                      << it->selector.groupid << "', '"
                      << it->selector.streamid << "'"
                      << ") is not in the local streams.";
    }
  }

  return ret;
}

void VideoChannel::SetRenderer_w(uint32 ssrc, VideoRenderer* renderer) {
  media_channel()->SetRenderer(ssrc, renderer);
}

VideoCapturer* VideoChannel::AddScreencast_w(
    uint32 ssrc, const ScreencastId& id) {
  if (screencast_capturers_.find(ssrc) != screencast_capturers_.end()) {
    return NULL;
  }
  VideoCapturer* screen_capturer =
      screencapture_factory_->CreateScreenCapturer(id);
  if (!screen_capturer) {
    return NULL;
  }
  screen_capturer->SignalStateChange.connect(this,
                                             &VideoChannel::OnStateChange);
  screencast_capturers_[ssrc] = screen_capturer;
  return screen_capturer;
}

bool VideoChannel::SetCapturer_w(uint32 ssrc, VideoCapturer* capturer) {
  return media_channel()->SetCapturer(ssrc, capturer);
}

bool VideoChannel::RemoveScreencast_w(uint32 ssrc) {
  ScreencastMap::iterator iter = screencast_capturers_.find(ssrc);
  if (iter  == screencast_capturers_.end()) {
    return false;
  }
  // Clean up VideoCapturer.
  delete iter->second;
  screencast_capturers_.erase(iter);
  return true;
}

bool VideoChannel::IsScreencasting_w() const {
  return !screencast_capturers_.empty();
}

int VideoChannel::ScreencastFps_w(uint32 ssrc) const {
  ScreencastMap::const_iterator iter = screencast_capturers_.find(ssrc);
  if (iter == screencast_capturers_.end()) {
    return 0;
  }
  VideoCapturer* capturer = iter->second;
  const VideoFormat* video_format = capturer->GetCaptureFormat();
  return VideoFormat::IntervalToFps(video_format->interval);
}

void VideoChannel::SetScreenCaptureFactory_w(
    ScreenCapturerFactory* screencapture_factory) {
  if (screencapture_factory == NULL) {
    screencapture_factory_.reset(CreateScreenCapturerFactory());
  } else {
    screencapture_factory_.reset(screencapture_factory);
  }
}

bool VideoChannel::GetStats_w(VideoMediaInfo* stats) {
  return media_channel()->GetStats(stats);
}

void VideoChannel::OnScreencastWindowEvent_s(uint32 ssrc,
                                             talk_base::WindowEvent we) {
  ASSERT(signaling_thread() == talk_base::Thread::Current());
  SignalScreencastWindowEvent(ssrc, we);
}

bool VideoChannel::SetChannelOptions(const VideoOptions &options) {
  VideoOptionsMessageData data(options);
  Send(MSG_SETCHANNELOPTIONS, &data);
  return data.result;
}

bool VideoChannel::SetChannelOptions_w(const VideoOptions &options) {
  return media_channel()->SetOptions(options);
}

void VideoChannel::OnMessage(talk_base::Message *pmsg) {
  switch (pmsg->message_id) {
    case MSG_SETRENDERER: {
      const VideoRenderMessageData* data =
          static_cast<VideoRenderMessageData*>(pmsg->pdata);
      SetRenderer_w(data->ssrc, data->renderer);
      break;
    }
    case MSG_ADDSCREENCAST: {
      AddScreencastMessageData* data =
          static_cast<AddScreencastMessageData*>(pmsg->pdata);
      data->result = AddScreencast_w(data->ssrc, data->window_id);
      break;
    }
    case MSG_SETCAPTURER: {
      SetCapturerMessageData* data =
          static_cast<SetCapturerMessageData*>(pmsg->pdata);
      data->result = SetCapturer_w(data->ssrc, data->capturer);
      break;
    }
    case MSG_REMOVESCREENCAST: {
      RemoveScreencastMessageData* data =
          static_cast<RemoveScreencastMessageData*>(pmsg->pdata);
      data->result = RemoveScreencast_w(data->ssrc);
      break;
    }
    case MSG_SCREENCASTWINDOWEVENT: {
      const ScreencastEventMessageData* data =
          static_cast<ScreencastEventMessageData*>(pmsg->pdata);
      OnScreencastWindowEvent_s(data->ssrc, data->event);
      delete data;
      break;
    }
    case  MSG_ISSCREENCASTING: {
      IsScreencastingMessageData* data =
          static_cast<IsScreencastingMessageData*>(pmsg->pdata);
      data->result = IsScreencasting_w();
      break;
    }
    case MSG_SCREENCASTFPS: {
      ScreencastFpsMessageData* data =
          static_cast<ScreencastFpsMessageData*>(pmsg->pdata);
      data->result = ScreencastFps_w(data->ssrc);
      break;
    }
    case MSG_SENDINTRAFRAME: {
      SendIntraFrame_w();
      break;
    }
    case MSG_REQUESTINTRAFRAME: {
      RequestIntraFrame_w();
      break;
    }
    case MSG_SETCHANNELOPTIONS: {
      VideoOptionsMessageData* data =
         static_cast<VideoOptionsMessageData*>(pmsg->pdata);
      data->result = SetChannelOptions_w(data->options);
      break;
    }
    case MSG_CHANNEL_ERROR: {
      const VideoChannelErrorMessageData* data =
          static_cast<VideoChannelErrorMessageData*>(pmsg->pdata);
      SignalMediaError(this, data->ssrc, data->error);
      delete data;
      break;
    }
    case MSG_HANDLEVIEWREQUEST: {
      ViewRequestMessageData* data =
          static_cast<ViewRequestMessageData*>(pmsg->pdata);
      data->result = ApplyViewRequest_w(data->request);
      break;
    }
    case MSG_SETSCREENCASTFACTORY: {
      SetScreenCaptureFactoryMessageData* data =
          static_cast<SetScreenCaptureFactoryMessageData*>(pmsg->pdata);
      SetScreenCaptureFactory_w(data->screencapture_factory);
    }
    case MSG_GETSTATS: {
      VideoStatsMessageData* data =
          static_cast<VideoStatsMessageData*>(pmsg->pdata);
      data->result = GetStats_w(data->stats);
      break;
    }
    default:
      BaseChannel::OnMessage(pmsg);
      break;
  }
}

void VideoChannel::OnConnectionMonitorUpdate(
    SocketMonitor *monitor, const std::vector<ConnectionInfo> &infos) {
  SignalConnectionMonitor(this, infos);
}

// TODO(pthatcher): Look into removing duplicate code between
// audio, video, and data, perhaps by using templates.
void VideoChannel::OnMediaMonitorUpdate(
    VideoMediaChannel* media_channel, const VideoMediaInfo &info) {
  ASSERT(media_channel == this->media_channel());
  SignalMediaMonitor(this, info);
}

void VideoChannel::OnScreencastWindowEvent(uint32 ssrc,
                                           talk_base::WindowEvent event) {
  ScreencastEventMessageData* pdata =
      new ScreencastEventMessageData(ssrc, event);
  signaling_thread()->Post(this, MSG_SCREENCASTWINDOWEVENT, pdata);
}

void VideoChannel::OnStateChange(VideoCapturer* capturer, CaptureState ev) {
  // Map capturer events to window events. In the future we may want to simply
  // pass these events up directly.
  talk_base::WindowEvent we;
  if (ev == CS_STOPPED) {
    we = talk_base::WE_CLOSE;
  } else if (ev == CS_PAUSED) {
    we = talk_base::WE_MINIMIZE;
  } else if (ev == CS_RUNNING && previous_we_ == talk_base::WE_MINIMIZE) {
    we = talk_base::WE_RESTORE;
  } else {
    return;
  }
  previous_we_ = we;

  uint32 ssrc = 0;
  if (!GetLocalSsrc(capturer, &ssrc)) {
    return;
  }
  ScreencastEventMessageData* pdata =
      new ScreencastEventMessageData(ssrc, we);
  signaling_thread()->Post(this, MSG_SCREENCASTWINDOWEVENT, pdata);
}

bool VideoChannel::GetLocalSsrc(const VideoCapturer* capturer, uint32* ssrc) {
  *ssrc = 0;
  for (ScreencastMap::iterator iter = screencast_capturers_.begin();
       iter != screencast_capturers_.end(); ++iter) {
    if (iter->second == capturer) {
      *ssrc = iter->first;
      return true;
    }
  }
  return false;
}

void VideoChannel::OnVideoChannelError(uint32 ssrc,
                                       VideoMediaChannel::Error error) {
  VideoChannelErrorMessageData* data = new VideoChannelErrorMessageData(
      ssrc, error);
  signaling_thread()->Post(this, MSG_CHANNEL_ERROR, data);
}

void VideoChannel::OnSrtpError(uint32 ssrc, SrtpFilter::Mode mode,
                               SrtpFilter::Error error) {
  switch (error) {
    case SrtpFilter::ERROR_FAIL:
      OnVideoChannelError(ssrc, (mode == SrtpFilter::PROTECT) ?
                          VideoMediaChannel::ERROR_REC_SRTP_ERROR :
                          VideoMediaChannel::ERROR_PLAY_SRTP_ERROR);
      break;
    case SrtpFilter::ERROR_AUTH:
      OnVideoChannelError(ssrc, (mode == SrtpFilter::PROTECT) ?
                          VideoMediaChannel::ERROR_REC_SRTP_AUTH_FAILED :
                          VideoMediaChannel::ERROR_PLAY_SRTP_AUTH_FAILED);
      break;
    case SrtpFilter::ERROR_REPLAY:
      // Only receving channel should have this error.
      ASSERT(mode == SrtpFilter::UNPROTECT);
      // TODO(gangji): Turn on the signaling of replay error once we have
      // switched to the new mechanism for doing video retransmissions.
      // OnVideoChannelError(ssrc, VideoMediaChannel::ERROR_PLAY_SRTP_REPLAY);
      break;
    default:
      break;
  }
}


void VideoChannel::GetSrtpCiphers(std::vector<std::string>* ciphers) const {
  GetSupportedVideoCryptoSuites(ciphers);
}

DataChannel::DataChannel(talk_base::Thread* thread,
                         DataMediaChannel* media_channel,
                         BaseSession* session,
                         const std::string& content_name,
                         bool rtcp)
    // MediaEngine is NULL
    : BaseChannel(thread, NULL, media_channel, session, content_name, rtcp),
      data_channel_type_(cricket::DCT_NONE) {
}

DataChannel::~DataChannel() {
  StopMediaMonitor();
  // this can't be done in the base class, since it calls a virtual
  DisableMedia_w();
}

bool DataChannel::Init() {
  TransportChannel* rtcp_channel = rtcp() ? session()->CreateChannel(
      content_name(), "data_rtcp", ICE_CANDIDATE_COMPONENT_RTCP) : NULL;
  if (!BaseChannel::Init(session()->CreateChannel(
          content_name(), "data_rtp", ICE_CANDIDATE_COMPONENT_RTP),
          rtcp_channel)) {
    return false;
  }
  media_channel()->SignalDataReceived.connect(
      this, &DataChannel::OnDataReceived);
  media_channel()->SignalMediaError.connect(
      this, &DataChannel::OnDataChannelError);
  srtp_filter()->SignalSrtpError.connect(
      this, &DataChannel::OnSrtpError);
  return true;
}

bool DataChannel::SendData(const SendDataParams& params,
                           const talk_base::Buffer& payload,
                           SendDataResult* result) {
  SendDataMessageData message_data(params, &payload, result);
  Send(MSG_SENDDATA, &message_data);
  return message_data.succeeded;
}

const ContentInfo* DataChannel::GetFirstContent(
    const SessionDescription* sdesc) {
  return GetFirstDataContent(sdesc);
}


static bool IsRtpPacket(const talk_base::Buffer* packet) {
  int version;
  if (!GetRtpVersion(packet->data(), packet->length(), &version)) {
    return false;
  }

  return version == 2;
}

bool DataChannel::WantsPacket(bool rtcp, talk_base::Buffer* packet) {
  if (data_channel_type_ == DCT_SCTP) {
    // TODO(pthatcher): Do this in a more robust way by checking for
    // SCTP or DTLS.
    return !IsRtpPacket(packet);
  } else if (data_channel_type_ == DCT_RTP) {
    return BaseChannel::WantsPacket(rtcp, packet);
  }
  return false;
}

// Sets the maximum bandwidth.  Anything over this will be dropped.
bool DataChannel::SetMaxSendBandwidth_w(int max_bps) {
  LOG(LS_INFO) << "DataChannel: Setting max bandwidth to " << max_bps;
  return media_channel()->SetSendBandwidth(false, max_bps);
}

bool DataChannel::SetDataChannelType(DataChannelType new_data_channel_type) {
  // It hasn't been set before, so set it now.
  if (data_channel_type_ == DCT_NONE) {
    data_channel_type_ = new_data_channel_type;
    return true;
  }

  // It's been set before, but doesn't match.  That's bad.
  if (data_channel_type_ != new_data_channel_type) {
    LOG(LS_WARNING) << "Data channel type mismatch."
                    << " Expected " << data_channel_type_
                    << " Got " << new_data_channel_type;
    return false;
  }

  // It's hasn't changed.  Nothing to do.
  return true;
}

bool DataChannel::SetDataChannelTypeFromContent(
    const DataContentDescription* content) {
  bool is_sctp = ((content->protocol() == kMediaProtocolSctp) ||
                  (content->protocol() == kMediaProtocolDtlsSctp));
  DataChannelType data_channel_type = is_sctp ? DCT_SCTP : DCT_RTP;
  return SetDataChannelType(data_channel_type);
}

bool DataChannel::SetLocalContent_w(const MediaContentDescription* content,
                                    ContentAction action) {
  ASSERT(worker_thread() == talk_base::Thread::Current());
  LOG(LS_INFO) << "Setting local data description";

  const DataContentDescription* data =
      static_cast<const DataContentDescription*>(content);
  ASSERT(data != NULL);
  if (!data) return false;

  bool ret = false;
  if (!SetDataChannelTypeFromContent(data)) {
    return false;
  }

  if (data_channel_type_ == DCT_SCTP) {
    // SCTP data channels don't need the rest of the stuff.
    ret = UpdateLocalStreams_w(data->streams(), action);
    if (ret) {
      set_local_content_direction(content->direction());
    }
  } else {
    ret = SetBaseLocalContent_w(content, action);

    if (action != CA_UPDATE || data->has_codecs()) {
      ret &= media_channel()->SetRecvCodecs(data->codecs());
    }
  }

  // If everything worked, see if we can start receiving.
  if (ret) {
    ChangeState();
  } else {
    LOG(LS_WARNING) << "Failed to set local data description";
  }
  return ret;
}

bool DataChannel::SetRemoteContent_w(const MediaContentDescription* content,
                                     ContentAction action) {
  ASSERT(worker_thread() == talk_base::Thread::Current());

  const DataContentDescription* data =
      static_cast<const DataContentDescription*>(content);
  ASSERT(data != NULL);
  if (!data) return false;

  bool ret = true;
  if (!SetDataChannelTypeFromContent(data)) {
    return false;
  }

  if (data_channel_type_ == DCT_SCTP) {
    LOG(LS_INFO) << "Setting SCTP remote data description";
    // SCTP data channels don't need the rest of the stuff.
    ret = UpdateRemoteStreams_w(content->streams(), action);
    if (ret) {
      set_remote_content_direction(content->direction());
    }
  } else {
    // If the remote data doesn't have codecs and isn't an update, it
    // must be empty, so ignore it.
    if (action != CA_UPDATE && !data->has_codecs()) {
      return true;
    }
    LOG(LS_INFO) << "Setting remote data description";

    // Set remote video codecs (what the other side wants to receive).
    if (action != CA_UPDATE || data->has_codecs()) {
      ret &= media_channel()->SetSendCodecs(data->codecs());
    }

    if (ret) {
      ret &= SetBaseRemoteContent_w(content, action);
    }

    if (action != CA_UPDATE) {
      int bandwidth_bps = data->bandwidth();
      bool auto_bandwidth = (bandwidth_bps == kAutoBandwidth);
      ret &= media_channel()->SetSendBandwidth(auto_bandwidth, bandwidth_bps);
    }
  }

  // If everything worked, see if we can start sending.
  if (ret) {
    ChangeState();
  } else {
    LOG(LS_WARNING) << "Failed to set remote data description";
  }
  return ret;
}

void DataChannel::ChangeState() {
  // Render incoming data if we're the active call, and we have the local
  // content. We receive data on the default channel and multiplexed streams.
  bool recv = IsReadyToReceive();
  if (!media_channel()->SetReceive(recv)) {
    LOG(LS_ERROR) << "Failed to SetReceive on data channel";
  }

  // Send outgoing data if we're the active call, we have the remote content,
  // and we have had some form of connectivity.
  bool send = IsReadyToSend();
  if (!media_channel()->SetSend(send)) {
    LOG(LS_ERROR) << "Failed to SetSend on data channel";
  }

  // Post to trigger SignalReadyToSendData.
  signaling_thread()->Post(this, MSG_READYTOSENDDATA,
                           new BoolMessageData(send));

  LOG(LS_INFO) << "Changing data state, recv=" << recv << " send=" << send;
}

void DataChannel::OnMessage(talk_base::Message *pmsg) {
  switch (pmsg->message_id) {
    case MSG_READYTOSENDDATA: {
      BoolMessageData* data = static_cast<BoolMessageData*>(pmsg->pdata);
      SignalReadyToSendData(data->data());
      delete data;
      break;
    }
    case MSG_SENDDATA: {
      SendDataMessageData* msg =
          static_cast<SendDataMessageData*>(pmsg->pdata);
      msg->succeeded = media_channel()->SendData(
          msg->params, *(msg->payload), msg->result);
      break;
    }
    case MSG_DATARECEIVED: {
      DataReceivedMessageData* data =
          static_cast<DataReceivedMessageData*>(pmsg->pdata);
      SignalDataReceived(this, data->params, data->payload);
      delete data;
      break;
    }
    case MSG_CHANNEL_ERROR: {
      const DataChannelErrorMessageData* data =
          static_cast<DataChannelErrorMessageData*>(pmsg->pdata);
      SignalMediaError(this, data->ssrc, data->error);
      delete data;
      break;
    }
    default:
      BaseChannel::OnMessage(pmsg);
      break;
  }
}

void DataChannel::OnConnectionMonitorUpdate(
    SocketMonitor* monitor, const std::vector<ConnectionInfo>& infos) {
  SignalConnectionMonitor(this, infos);
}

void DataChannel::StartMediaMonitor(int cms) {
  media_monitor_.reset(new DataMediaMonitor(media_channel(), worker_thread(),
      talk_base::Thread::Current()));
  media_monitor_->SignalUpdate.connect(
      this, &DataChannel::OnMediaMonitorUpdate);
  media_monitor_->Start(cms);
}

void DataChannel::StopMediaMonitor() {
  if (media_monitor_) {
    media_monitor_->Stop();
    media_monitor_->SignalUpdate.disconnect(this);
    media_monitor_.reset();
  }
}

void DataChannel::OnMediaMonitorUpdate(
    DataMediaChannel* media_channel, const DataMediaInfo& info) {
  ASSERT(media_channel == this->media_channel());
  SignalMediaMonitor(this, info);
}

void DataChannel::OnDataReceived(
    const ReceiveDataParams& params, const char* data, size_t len) {
  DataReceivedMessageData* msg = new DataReceivedMessageData(
      params, data, len);
  signaling_thread()->Post(this, MSG_DATARECEIVED, msg);
}

void DataChannel::OnDataChannelError(
    uint32 ssrc, DataMediaChannel::Error err) {
  DataChannelErrorMessageData* data = new DataChannelErrorMessageData(
      ssrc, err);
  signaling_thread()->Post(this, MSG_CHANNEL_ERROR, data);
}

void DataChannel::OnSrtpError(uint32 ssrc, SrtpFilter::Mode mode,
                              SrtpFilter::Error error) {
  switch (error) {
    case SrtpFilter::ERROR_FAIL:
      OnDataChannelError(ssrc, (mode == SrtpFilter::PROTECT) ?
                         DataMediaChannel::ERROR_SEND_SRTP_ERROR :
                         DataMediaChannel::ERROR_RECV_SRTP_ERROR);
      break;
    case SrtpFilter::ERROR_AUTH:
      OnDataChannelError(ssrc, (mode == SrtpFilter::PROTECT) ?
                         DataMediaChannel::ERROR_SEND_SRTP_AUTH_FAILED :
                         DataMediaChannel::ERROR_RECV_SRTP_AUTH_FAILED);
      break;
    case SrtpFilter::ERROR_REPLAY:
      // Only receving channel should have this error.
      ASSERT(mode == SrtpFilter::UNPROTECT);
      OnDataChannelError(ssrc, DataMediaChannel::ERROR_RECV_SRTP_REPLAY);
      break;
    default:
      break;
  }
}

void DataChannel::GetSrtpCiphers(std::vector<std::string>* ciphers) const {
  GetSupportedDataCryptoSuites(ciphers);
}

bool DataChannel::ShouldSetupDtlsSrtp() const {
  return (data_channel_type_ == DCT_RTP);
}

}  // namespace cricket
