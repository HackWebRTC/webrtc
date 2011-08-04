
#include "talk/app/webrtc/peerconnection_impl_dev.h"

#include "talk/app/webrtc/webrtcsession.h"
#include "talk/base/logging.h"
#include "talk/session/phone/channelmanager.h"
#include "talk/p2p/base/portallocator.h"

namespace webrtc {

PeerConnectionImpl::PeerConnectionImpl(
    cricket::ChannelManager* channel_manager,
    cricket::PortAllocator* port_allocator)
    : initialized_(false),
      ready_state_(NEW),
      observer_(NULL),
      session_(NULL),
      signaling_thread_(new talk_base::Thread()),
      channel_manager_(channel_manager),
      port_allocator_(port_allocator) {
}

PeerConnectionImpl::~PeerConnectionImpl() {
}

bool PeerConnectionImpl::Init() {
  ASSERT(port_allocator_ != NULL);
  ASSERT(signaling_thread_.get());
  if (!signaling_thread_->SetName("signaling_thread", this) ||
      !signaling_thread_->Start()) {
    LOG(LS_ERROR) << "Failed to start signalig thread";
    return false;
  }
  session_.reset(CreateSession());
  ASSERT(session_.get() != NULL);
  return true;
}

WebRTCSession* PeerConnectionImpl::CreateSession() {
  // TODO(ronghuawu): when we have the new WebRTCSession we don't need these
  std::string id = "";
  std::string direction = "";
  WebRTCSession* session =
      new WebRTCSession(id, direction, port_allocator_,
                            channel_manager_,
                            // TODO(ronghuawu): implement PeerConnectionImplCallbacks
                            // this,
                            NULL,
                            signaling_thread_.get());
  if (!session->Initiate()) {
    delete session;
    session = NULL;
  }
  return session;
}

void PeerConnectionImpl::RegisterObserver(PeerConnectionObserver* observer) {
  observer_ = observer;
}

void PeerConnectionImpl::AddStream(LocalStream* local_stream) {

}

void PeerConnectionImpl::RemoveStream(LocalStream* remove_stream) {

}

void PeerConnectionImpl::OnMessage(talk_base::Message* msg) {

}

} // namespace webrtc
