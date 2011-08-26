

#ifndef TALK_APP_WEBRTC_PEERCONNECTIONTRANSPORT_H_
#define TALK_APP_WEBRTC_PEERCONNECTIONTRANSPORT_H_

#include <string>
#include <map>
#include <vector>

#include "talk/base/messagequeue.h"
#include "talk/base/sigslot.h"
#include "talk/p2p/base/candidate.h"

namespace talk_base {
class Thread;
class Message;
}

namespace cricket {
class PortAllocator;
class Transport;
class TransportChannel;
}

namespace webrtc {
typedef std::vector<cricket::Candidate> Candidates;

class PeerConnectionTransport : public talk_base::MessageHandler,
                                public sigslot::has_slots<> {
 public:
  PeerConnectionTransport(talk_base::Thread* signaling_thread,
                          talk_base::Thread* worker_thread,
                          cricket::PortAllocator* port_allocator);
  ~PeerConnectionTransport();

  enum TransportState {
    INIT,
    CONNECTING,
    CONNECTED,
  };

  bool Initialize();

  // methods signaled by the cricket::Transport
  void OnRequestSignaling(cricket::Transport* transport);
  void OnCandidatesReady(cricket::Transport* transport,
                         const std::vector<cricket::Candidate>& candidates);
  void OnWritableState(cricket::Transport* transport);
  void OnRouteChange(cricket::Transport* transport,
                     const std::string& name,
                     const cricket::Candidate& remote_candidate);
  void OnConnecting(cricket::Transport* transport);
  void ConnectChannels();

  // methods to handle transport channels. These methods are relayed from
  // WebRtcSession which implements cricket::BaseSession methods
  cricket::TransportChannel* CreateChannel(const std::string& channel_name,
                                           const std::string& content_type);
  cricket::TransportChannel* GetChannel(const std::string& channel_name,
                                        const std::string& content_type);
  void DestroyChannel(const std::string& channel_name,
                      const std::string& content_type);

  Candidates local_candidates() {
    return local_candidates_;
  }
  Candidates remote_candidates() {
    return remote_candidates_;
  }

  void OnRemoteCandidates(const Candidates candidates);
  sigslot::signal0<> SignalTransportTimeout;

 private:
  typedef std::map<std::string, cricket::TransportChannel*> TransportChannelMap;

  virtual void OnMessage(talk_base::Message* message);
  void StartTransportTimeout();
  void ClearTransportTimeout();
  cricket::TransportChannel* FindChannel(const std::string& name) const;

  TransportState state_;
  bool all_writable_;
  TransportChannelMap channels_;
  talk_base::scoped_ptr<cricket::Transport> transport_;
  talk_base::Thread* signaling_thread_;
  Candidates local_candidates_;
  Candidates remote_candidates_;
  std::map<std::string, cricket::Candidate> channel_best_remote_candidate_;
};

} // namespace webrtc

#endif // TALK_APP_WEBRTC_PEERCONNECTIONTRANSPORT_H_
