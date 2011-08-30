
#include "talk/app/webrtc_dev/peerconnectiontransport.h"

#include "talk/base/common.h"
#include "talk/base/logging.h"
#include "talk/base/thread.h"
#include "talk/p2p/base/p2ptransport.h"
#include "talk/p2p/base/portallocator.h"
#include "talk/p2p/base/transportchannelimpl.h"

namespace {
static const int MSG_TRANSPORT_TIMEOUT = 1;
// TODO(mallinath) - This value is not finalized yet. For now 30 seconds
// timeout value is taken from magicflute.
static const int kCallSetupTimeout = 30 * 1000;
}


namespace webrtc {

PeerConnectionTransport::PeerConnectionTransport(
    talk_base::Thread* signaling_thread,
    talk_base::Thread* worker_thread,
    cricket::PortAllocator* port_allocator)
    : state_(INIT),
      all_writable_(false),
      signaling_thread_(signaling_thread),
      transport_(new cricket::P2PTransport(
          signaling_thread, worker_thread, port_allocator)) {
}

PeerConnectionTransport::~PeerConnectionTransport() {
}

bool PeerConnectionTransport::Initialize() {
  ASSERT(transport_.get());
  transport_->SignalCandidatesReady.connect(
      this, &PeerConnectionTransport::OnCandidatesReady);
  transport_->SignalRequestSignaling.connect(
        this, &PeerConnectionTransport::OnRequestSignaling);
  transport_->SignalWritableState.connect(
        this, &PeerConnectionTransport::OnWritableState);
  transport_->SignalRouteChange.connect(
      this, &PeerConnectionTransport::OnRouteChange);
  transport_->SignalConnecting.connect(
      this, &PeerConnectionTransport::OnConnecting);
  return true;
}

void PeerConnectionTransport::ConnectChannels() {
  transport_->ConnectChannels();
  state_ = CONNECTING;
}

cricket::TransportChannel* PeerConnectionTransport::CreateChannel(
    const std::string& channel_name,
    const std::string& content_type) {
  cricket::TransportChannel* channel = FindChannel(channel_name);
  if (channel) {
    LOG(LS_INFO) << "Channel alreary exists";
    return channel;
  } else {
    channel = transport_->CreateChannel(channel_name, content_type);
    channels_[channel_name] = channel;
    return channel;
  }
}

cricket::TransportChannel* PeerConnectionTransport::FindChannel(
    const std::string& name) const {
  TransportChannelMap::const_iterator iter = channels_.find(name);
  return (iter != channels_.end()) ? iter->second : NULL;
}

cricket::TransportChannel* PeerConnectionTransport::GetChannel(
    const std::string& channel_name,
    const std::string& content_type) {
  return FindChannel(channel_name);
}

void PeerConnectionTransport::DestroyChannel(const std::string& channel_name,
                                             const std::string& content_type) {
  TransportChannelMap::iterator iter = channels_.find(channel_name);
  if (iter != channels_.end()) {
    channels_.erase(iter);
  }
  transport_->DestroyChannel(channel_name);
  return;
}

void PeerConnectionTransport::OnRequestSignaling(
    cricket::Transport* transport) {
  transport_->OnSignalingReady();
}

void PeerConnectionTransport::OnCandidatesReady(
    cricket::Transport* transport,
    const std::vector<cricket::Candidate>& candidates) {
  std::vector<cricket::Candidate>::const_iterator iter;
  for (iter = candidates.begin(); iter != candidates.end(); ++iter) {
    local_candidates_.push_back(*iter);
  }
}

void PeerConnectionTransport::OnWritableState(cricket::Transport* transport) {
  bool all_writable = transport->writable();
  if (all_writable != all_writable_) {
    if (all_writable) {
      signaling_thread_->Clear(this, MSG_TRANSPORT_TIMEOUT);
    } else {
      signaling_thread_->PostDelayed(
          kCallSetupTimeout, this, MSG_TRANSPORT_TIMEOUT);
    }
    all_writable_ = all_writable;
  }
}

void PeerConnectionTransport::OnRouteChange(
    cricket::Transport* transport,
    const std::string& name,
    const cricket::Candidate& remote_candidate) {
  channel_best_remote_candidate_[name] = remote_candidate;
}

void PeerConnectionTransport::OnConnecting(cricket::Transport* transport) {
  ASSERT(signaling_thread_->IsCurrent());
  if (transport->HasChannels() && !transport->writable()) {
    signaling_thread_->PostDelayed(
        kCallSetupTimeout, this, MSG_TRANSPORT_TIMEOUT);
  }
}

void PeerConnectionTransport::OnRemoteCandidates(const Candidates candidates) {
  transport_->OnRemoteCandidates(candidates);
  state_ = CONNECTED;
}

void PeerConnectionTransport::OnMessage(talk_base::Message* message) {
  switch(message->message_id) {
    case MSG_TRANSPORT_TIMEOUT: {
      LOG(LS_ERROR) << "Transport Timeout";
      SignalTransportTimeout();
      break;
    }
  }
}

}
