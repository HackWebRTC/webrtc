/*
 * peerconnection_transport_impl.h
 *
 *  Created on: May 2, 2011
 *      Author: mallinath
 */

#ifndef TALK_APP_PEERCONNECTION_TRANSPORT_IMPL_H_
#define TALK_APP_PEERCONNECTION_TRANSPORT_IMPL_H_

#include <vector>

#include "talk/base/thread.h"
#include "talk/base/event.h"
#include "talk/base/messagehandler.h"
#include "talk/base/scoped_ptr.h"

#ifdef PLATFORM_CHROMIUM
#include "net/base/completion_callback.h"
#include "webkit/glue/p2p_transport.h"
class P2PTransportImpl;
#else
#include "talk/app/p2p_transport_manager.h"
#endif

#ifdef PLATFORM_CHROMIUM
typedef P2PTransportImpl TransportImplClass;
typedef webkit_glue::P2PTransport::EventHandler TransportEventHandler;
typedef webkit_glue::P2PTransport P2PTransportClass;
#else
typedef webrtc::P2PTransportManager TransportImplClass;
typedef webrtc::P2PTransportManager::EventHandler TransportEventHandler;
typedef webrtc::P2PTransportManager P2PTransportClass;
#endif

namespace cricket {
class TransportChannel;
class Candidate;
}

namespace webrtc {

const int kMaxRtpRtcpPacketLen = 1500;

class WebRTCSessionImpl;
// PC - PeerConnection
class PC_Transport_Impl : public talk_base::MessageHandler,
                          public TransportEventHandler {
 public:
  PC_Transport_Impl(WebRTCSessionImpl* session);
  virtual ~PC_Transport_Impl();

  bool Init(const std::string& name);
#ifdef PLATFORM_CHROMIUM
  virtual void OnCandidateReady(const std::string& address);
#else
  virtual void OnCandidateReady(const cricket::Candidate& candidate);
#endif
  virtual void OnStateChange(P2PTransportClass::State state);
  virtual void OnError(int error);

#ifdef PLATFORM_CHROMIUM
  void OnRead(int result);
  void OnWrite(int result);
  net::Socket* GetChannel();
#endif

  void OnMessage(talk_base::Message* message);
  cricket::TransportChannel* GetP2PChannel();
  bool AddRemoteCandidate(const cricket::Candidate& candidate);
  WebRTCSessionImpl* session() { return session_; }
  P2PTransportClass* p2p_transport() { return p2p_transport_.get(); }
  const std::string& name() { return name_; }
  std::vector<cricket::Candidate>& local_candidates() {
    return local_candidates_;
  }

 private:
  void MsgSend(uint32 id);
  P2PTransportClass* CreateP2PTransport();
#ifdef PLATFORM_CHROMIUM
  void OnReadPacket_w(
       cricket::TransportChannel* channel, const char* data, size_t len);
  int32 DoRecv();
  void StreamRead();
  std::string SerializeCandidate(const cricket::Candidate& candidate);
  bool DeserializeCandidate(const std::string& address,
                            cricket::Candidate* candidate);
#endif

  std::string name_;
  WebRTCSessionImpl* session_;
  talk_base::scoped_ptr<P2PTransportClass> p2p_transport_;
  std::vector<cricket::Candidate> local_candidates_;

#ifdef PLATFORM_CHROMIUM
  net::CompletionCallbackImpl<PC_Transport_Impl> channel_read_callback_;
  net::CompletionCallbackImpl<PC_Transport_Impl> channel_write_callback_;
  talk_base::Thread* network_thread_chromium_;
#endif
  bool writable_;
  char recv_buffer_[kMaxRtpRtcpPacketLen];
  talk_base::Event event_;
  talk_base::Thread* network_thread_jingle_;
};

} // namespace webrtc

#endif /* TALK_APP_PEERCONNECTION_TRANSPORT_IMPL_H_ */
