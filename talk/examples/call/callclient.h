/*
 * libjingle
 * Copyright 2004--2005, Google Inc.
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

#ifndef TALK_EXAMPLES_CALL_CALLCLIENT_H_
#define TALK_EXAMPLES_CALL_CALLCLIENT_H_

#include <map>
#include <string>
#include <vector>

#include "talk/base/scoped_ptr.h"
#include "talk/base/sslidentity.h"
#include "talk/examples/call/console.h"
#include "talk/media/base/mediachannel.h"
#include "talk/p2p/base/session.h"
#include "talk/session/media/mediamessages.h"
#include "talk/session/media/mediasessionclient.h"
#include "talk/xmpp/hangoutpubsubclient.h"
#include "talk/xmpp/presencestatus.h"
#include "talk/xmpp/xmppclient.h"

namespace buzz {
class PresencePushTask;
class PresenceOutTask;
class MucInviteRecvTask;
class MucInviteSendTask;
class FriendInviteSendTask;
class DiscoInfoQueryTask;
class Muc;
class PresenceStatus;
class IqTask;
class MucRoomConfigTask;
class MucRoomLookupTask;
class MucPresenceStatus;
class XmlElement;
class HangoutPubSubClient;
struct AvailableMediaEntry;
struct MucRoomInfo;
}  // namespace buzz

namespace talk_base {
class Thread;
class NetworkManager;
}  // namespace talk_base

namespace cricket {
class PortAllocator;
class MediaEngineInterface;
class MediaSessionClient;
class Call;
class SessionManagerTask;
struct CallOptions;
struct MediaStreams;
struct StreamParams;
}  // namespace cricket

struct RosterItem {
  buzz::Jid jid;
  buzz::PresenceStatus::Show show;
  std::string status;
};

struct StaticRenderedView {
  StaticRenderedView(const cricket::StaticVideoView& view,
                     cricket::VideoRenderer* renderer) :
      view(view),
      renderer(renderer) {
  }

  cricket::StaticVideoView view;
  cricket::VideoRenderer* renderer;
};

// Maintain a mapping of (session, ssrc) to rendered view.
typedef std::map<std::pair<cricket::Session*, uint32>,
                 StaticRenderedView> StaticRenderedViews;

class CallClient: public sigslot::has_slots<> {
 public:
  CallClient(buzz::XmppClient* xmpp_client,
             const std::string& caps_node,
             const std::string& version);
  ~CallClient();

  cricket::MediaSessionClient* media_client() const { return media_client_; }
  void SetMediaEngine(cricket::MediaEngineInterface* media_engine) {
    media_engine_ = media_engine;
  }
  void SetAutoAccept(bool auto_accept) {
    auto_accept_ = auto_accept;
  }
  void SetPmucDomain(const std::string &pmuc_domain) {
    pmuc_domain_ = pmuc_domain;
  }
  void SetRender(bool render) {
    render_ = render;
  }
  void SetDataChannelType(cricket::DataChannelType data_channel_type) {
    data_channel_type_ = data_channel_type;
  }
  void SetMultiSessionEnabled(bool multisession_enabled) {
    multisession_enabled_ = multisession_enabled;
  }
  void SetConsole(Console *console) {
    console_ = console;
  }
  void SetPriority(int priority) {
    my_status_.set_priority(priority);
  }
  void SendStatus() {
    SendStatus(my_status_);
  }
  void SendStatus(const buzz::PresenceStatus& status);

  void ParseLine(const std::string &str);

  void SendChat(const std::string& to, const std::string msg);
  void SendData(const std::string& stream_name,
                const std::string& text);
  void InviteFriend(const std::string& user);
  void JoinMuc(const buzz::Jid& room_jid);
  void JoinMuc(const std::string& room_jid_str);
  void LookupAndJoinMuc(const std::string& room_name);
  void InviteToMuc(const std::string& user, const std::string& room);
  bool InMuc();
  const buzz::Jid* FirstMucJid();
  void LeaveMuc(const std::string& room);
  void SetNick(const std::string& muc_nick);
  void SetPortAllocatorFlags(uint32 flags) { portallocator_flags_ = flags; }
  void SetAllowLocalIps(bool allow_local_ips) {
    allow_local_ips_ = allow_local_ips;
  }

  void SetSignalingProtocol(cricket::SignalingProtocol protocol) {
    signaling_protocol_ = protocol;
  }
  void SetTransportProtocol(cricket::TransportProtocol protocol) {
    transport_protocol_ = protocol;
  }
  void SetSecurePolicy(cricket::SecurePolicy sdes_policy,
                       cricket::SecurePolicy dtls_policy) {
    sdes_policy_ = sdes_policy;
    dtls_policy_ = dtls_policy;
  }
  void SetSslIdentity(talk_base::SSLIdentity* identity) {
    ssl_identity_.reset(identity);
  }

  typedef std::map<buzz::Jid, buzz::Muc*> MucMap;

  const MucMap& mucs() const {
    return mucs_;
  }

  void SetShowRosterMessages(bool show_roster_messages) {
    show_roster_messages_ = show_roster_messages;
  }

 private:
  void AddStream(uint32 audio_src_id, uint32 video_src_id);
  void RemoveStream(uint32 audio_src_id, uint32 video_src_id);
  void OnStateChange(buzz::XmppEngine::State state);

  void InitMedia();
  void InitPresence();
  void StartXmppPing();
  void OnPingTimeout();
  void OnRequestSignaling();
  void OnSessionCreate(cricket::Session* session, bool initiate);
  void OnCallCreate(cricket::Call* call);
  void OnCallDestroy(cricket::Call* call);
  void OnSessionState(cricket::Call* call,
                      cricket::Session* session,
                      cricket::Session::State state);
  void OnStatusUpdate(const buzz::PresenceStatus& status);
  void OnMucInviteReceived(const buzz::Jid& inviter, const buzz::Jid& room,
      const std::vector<buzz::AvailableMediaEntry>& avail);
  void OnMucJoined(const buzz::Jid& endpoint);
  void OnMucStatusUpdate(const buzz::Jid& jid,
                         const buzz::MucPresenceStatus& status);
  void OnMucLeft(const buzz::Jid& endpoint, int error);
  void OnPresenterStateChange(const std::string& nick,
                              bool was_presenting, bool is_presenting);
  void OnAudioMuteStateChange(const std::string& nick,
                              bool was_muted, bool is_muted);
  void OnRecordingStateChange(const std::string& nick,
                              bool was_recording, bool is_recording);
  void OnRemoteMuted(const std::string& mutee_nick,
                     const std::string& muter_nick,
                     bool should_mute_locally);
  void OnMediaBlocked(const std::string& blockee_nick,
                      const std::string& blocker_nick);
  void OnHangoutRequestError(const std::string& node,
                             const buzz::XmlElement* stanza);
  void OnHangoutPublishAudioMuteError(const std::string& task_id,
                                      const buzz::XmlElement* stanza);
  void OnHangoutPublishPresenterError(const std::string& task_id,
                                      const buzz::XmlElement* stanza);
  void OnHangoutPublishRecordingError(const std::string& task_id,
                                      const buzz::XmlElement* stanza);
  void OnHangoutRemoteMuteError(const std::string& task_id,
                                const std::string& mutee_nick,
                                const buzz::XmlElement* stanza);
  void OnDevicesChange();
  void OnMediaStreamsUpdate(cricket::Call* call,
                            cricket::Session* session,
                            const cricket::MediaStreams& added,
                            const cricket::MediaStreams& removed);
  void OnSpeakerChanged(cricket::Call* call,
                        cricket::Session* session,
                        const cricket::StreamParams& speaker_stream);
  void OnRoomLookupResponse(buzz::MucRoomLookupTask* task,
                            const buzz::MucRoomInfo& room_info);
  void OnRoomLookupError(buzz::IqTask* task,
                         const buzz::XmlElement* stanza);
  void OnRoomConfigResult(buzz::MucRoomConfigTask* task);
  void OnRoomConfigError(buzz::IqTask* task,
                         const buzz::XmlElement* stanza);
  void OnDataReceived(cricket::Call*,
                      const cricket::ReceiveDataParams& params,
                      const talk_base::Buffer& payload);
  buzz::Jid GenerateRandomMucJid();

  // Depending on |enable|, render (or don't) all the streams in |session|.
  void RenderAllStreams(cricket::Call* call,
                        cricket::Session* session,
                        bool enable);

  // Depending on |enable|, render (or don't) the streams in |video_streams|.
  void RenderStreams(cricket::Call* call,
                     cricket::Session* session,
                     const std::vector<cricket::StreamParams>& video_streams,
                     bool enable);

  // Depending on |enable|, render (or don't) the supplied |stream|.
  void RenderStream(cricket::Call* call,
                    cricket::Session* session,
                    const cricket::StreamParams& stream,
                    bool enable);
  void AddStaticRenderedView(
      cricket::Session* session,
      uint32 ssrc, int width, int height, int framerate,
      int x_offset, int y_offset);
  bool RemoveStaticRenderedView(uint32 ssrc);
  void RemoveCallsStaticRenderedViews(cricket::Call* call);
  void SendViewRequest(cricket::Call* call, cricket::Session* session);
  bool SelectFirstDesktopScreencastId(cricket::ScreencastId* screencastid);

  static const std::string strerror(buzz::XmppEngine::Error err);

  void PrintRoster();
  bool FindJid(const std::string& name,
               buzz::Jid* found_jid,
               cricket::CallOptions* options);
  bool PlaceCall(const std::string& name, cricket::CallOptions options);
  bool InitiateAdditionalSession(const std::string& name,
                                 cricket::CallOptions options);
  void TerminateAndRemoveSession(cricket::Call* call, const std::string& id);
  void PrintCalls();
  void SwitchToCall(uint32 call_id);
  void Accept(const cricket::CallOptions& options);
  void Reject();
  void Quit();

  void GetDevices();
  void PrintDevices(const std::vector<std::string>& names);

  void SetVolume(const std::string& level);

  cricket::Session* GetFirstSession() { return sessions_[call_->id()][0]; }
  void AddSession(cricket::Session* session) {
    sessions_[call_->id()].push_back(session);
  }

  void PrintStats() const;
  void SetupAcceptedCall();

  typedef std::map<std::string, RosterItem> RosterMap;

  Console *console_;
  buzz::XmppClient* xmpp_client_;
  talk_base::Thread* worker_thread_;
  talk_base::NetworkManager* network_manager_;
  cricket::PortAllocator* port_allocator_;
  cricket::SessionManager* session_manager_;
  cricket::SessionManagerTask* session_manager_task_;
  cricket::MediaEngineInterface* media_engine_;
  cricket::DataEngineInterface* data_engine_;
  cricket::MediaSessionClient* media_client_;
  MucMap mucs_;

  cricket::Call* call_;
  typedef std::map<uint32, std::vector<cricket::Session *> > SessionMap;
  SessionMap sessions_;

  buzz::HangoutPubSubClient* hangout_pubsub_client_;
  bool incoming_call_;
  bool auto_accept_;
  std::string pmuc_domain_;
  bool render_;
  cricket::DataChannelType data_channel_type_;
  bool multisession_enabled_;
  cricket::VideoRenderer* local_renderer_;
  StaticRenderedViews static_rendered_views_;
  uint32 static_views_accumulated_count_;
  uint32 screencast_ssrc_;

  buzz::PresenceStatus my_status_;
  buzz::PresencePushTask* presence_push_;
  buzz::PresenceOutTask* presence_out_;
  buzz::MucInviteRecvTask* muc_invite_recv_;
  buzz::MucInviteSendTask* muc_invite_send_;
  buzz::FriendInviteSendTask* friend_invite_send_;
  RosterMap* roster_;
  uint32 portallocator_flags_;

  bool allow_local_ips_;
  cricket::SignalingProtocol signaling_protocol_;
  cricket::TransportProtocol transport_protocol_;
  cricket::SecurePolicy sdes_policy_;
  cricket::SecurePolicy dtls_policy_;
  talk_base::scoped_ptr<talk_base::SSLIdentity> ssl_identity_;
  std::string last_sent_to_;

  bool show_roster_messages_;
};

#endif  // TALK_EXAMPLES_CALL_CALLCLIENT_H_
