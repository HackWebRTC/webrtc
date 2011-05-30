/*
 * pc_transport_impl.cc
 *
 *  Created on: May 2, 2011
 *      Author: mallinath
 */

#include "talk/app/pc_transport_impl.h"

#ifdef PLATFORM_CHROMIUM
#include "base/values.h"
#include "content/common/json_value_serializer.h"
#include "content/renderer/p2p/p2p_transport_impl.h"
#include "jingle/glue/thread_wrapper.h"
#include "net/base/io_buffer.h"
#include "net/socket/socket.h"
#else
#include "talk/app/p2p_transport_manager.h"
#endif
#include "talk/p2p/base/transportchannel.h"
#include "talk/app/webrtcsessionimpl.h"
#include "talk/app/peerconnection.h"

namespace webrtc {
enum {
  MSG_RTC_ONREADPACKET = 1,
  MSG_RTC_TRANSPORTINIT,
  MSG_RTC_ADDREMOTECANDIDATE,
  MSG_RTC_ONCANDIDATEREADY,
};

struct MediaDataMsgParams : public talk_base::MessageData {
  MediaDataMsgParams(cricket::TransportChannel* channel,
                     const char* dataPtr,
                     int len)
      : channel(channel), data(dataPtr), len(len) {}

  cricket::TransportChannel* channel;
  const char* data;
  int len;
};

PC_Transport_Impl::PC_Transport_Impl (WebRTCSessionImpl* session)
    : session_(session),
#ifdef PLATFORM_CHROMIUM
    ALLOW_THIS_IN_INITIALIZER_LIST(
        channel_read_callback_(this, &PC_Transport_Impl::OnRead)),
    ALLOW_THIS_IN_INITIALIZER_LIST(
        channel_write_callback_(this, &PC_Transport_Impl::OnWrite)),
#endif
    writable_(false),
    event_(false, false),
    network_thread_jingle_(session_->connection()->media_thread())
{
#ifdef PLATFORM_CHROMIUM
  // Before proceeding, ensure we have libjingle thread wrapper for
  // the current thread.
  jingle_glue::JingleThreadWrapper::EnsureForCurrentThread();
  network_thread_chromium_ = talk_base::Thread::Current();
#endif
  event_.Set();
}

PC_Transport_Impl::~PC_Transport_Impl() {
}

bool PC_Transport_Impl::Init(const std::string& name) {
#ifdef PLATFORM_CHROMIUM
  if(network_thread_chromium_ != talk_base::Thread::Current()) {
    network_thread_chromium_->Post(this, MSG_RTC_TRANSPORTINIT,
        new talk_base::TypedMessageData<std::string> (name));
    return true;
  }
#else
  if(network_thread_jingle_ != talk_base::Thread::Current()) {
    network_thread_jingle_->Send(this, MSG_RTC_TRANSPORTINIT,
        new talk_base::TypedMessageData<std::string> (name));
    return true;
  }
#endif

  name_ = name;
  p2p_transport_.reset(CreateP2PTransport());

#ifdef PLATFORM_CHROMIUM
  webkit_glue::P2PTransport::Protocol protocol =
      webkit_glue::P2PTransport::PROTOCOL_UDP;
#else
  webrtc::P2PTransportManager::Protocol protocol =
      webrtc::P2PTransportManager::PROTOCOL_UDP;
#endif
  p2p_transport_->Init(name_, protocol, "", this);

#ifdef PLATFORM_CHROMIUM
  StreamRead();
#endif

  return true;
}

#ifdef PLATFORM_CHROMIUM

void PC_Transport_Impl::OnCandidateReady(const std::string& address) {
  if(network_thread_chromium_ != talk_base::Thread::Current()) {
    network_thread_chromium_->Post(this, MSG_RTC_ONCANDIDATEREADY,
        new talk_base::TypedMessageData<std::string> (
            address));
    return;
  }

  // using only first candidate
  // use p2p_transport_impl.cc Deserialize method
  cricket::Candidate candidate;
  if (local_candidates_.empty()) {
    cricket::Candidate candidate;
    DeserializeCandidate(address, &candidate);
    local_candidates_.push_back(candidate);
    session_->OnCandidateReady(candidate);
  }
}

bool PC_Transport_Impl::AddRemoteCandidate(
    const cricket::Candidate& candidate) {
  if(network_thread_chromium_ != talk_base::Thread::Current()) {
    network_thread_chromium_->Post(this, MSG_RTC_ADDREMOTECANDIDATE,
        new talk_base::TypedMessageData<const cricket::Candidate*> (
            &candidate));
    // TODO: save the result
    return true;
  }

  if (!p2p_transport_.get())
    return false;

  return p2p_transport_->AddRemoteCandidate(SerializeCandidate(candidate));
}

#else

void PC_Transport_Impl::OnCandidateReady(const cricket::Candidate& candidate) {
  if(network_thread_jingle_ != talk_base::Thread::Current()) {
    network_thread_jingle_->Send(this, MSG_RTC_ONCANDIDATEREADY,
        new talk_base::TypedMessageData<const cricket::Candidate*> (
            &candidate));
    return;
  }

  if (local_candidates_.empty()) {
    local_candidates_.push_back(candidate);
    session_->OnCandidateReady(candidate);
  }
}

bool PC_Transport_Impl::AddRemoteCandidate(
    const cricket::Candidate& candidate) {
  if(network_thread_jingle_ != talk_base::Thread::Current()) {
    network_thread_jingle_->Send(this, MSG_RTC_ADDREMOTECANDIDATE,
        new talk_base::TypedMessageData<const cricket::Candidate*> (
            &candidate));
    // TODO: save the result
    return true;
  }

  if (!p2p_transport_.get())
    return false;

  return p2p_transport_->AddRemoteCandidate(candidate);
}

#endif

#ifdef PLATFORM_CHROMIUM

int32 PC_Transport_Impl::DoRecv() {
  if (!p2p_transport_.get())
    return -1;

  net::Socket* channel = p2p_transport_->GetChannel();
  if (!channel)
    return -1;

  scoped_refptr<net::IOBuffer> buffer =
      new net::WrappedIOBuffer(static_cast<const char*>(recv_buffer_));
  int result = channel->Read(
      buffer, kMaxRtpRtcpPacketLen, &channel_read_callback_);
  return result;
}

void PC_Transport_Impl::OnRead(int result) {
  network_thread_jingle_->Post(
      this, MSG_RTC_ONREADPACKET, new MediaDataMsgParams(
          GetP2PChannel(), recv_buffer_, result));
  StreamRead();
}

void PC_Transport_Impl::OnWrite(int result) {
  return;
}

net::Socket* PC_Transport_Impl::GetChannel() {
  if (!p2p_transport_.get())
    return NULL;

  return p2p_transport_->GetChannel();
}

void PC_Transport_Impl::StreamRead() {
  event_.Wait(talk_base::kForever);
  DoRecv();
}

void PC_Transport_Impl::OnReadPacket_w(cricket::TransportChannel* channel,
                                       const char* data,
                                       size_t len) {
  session()->SignalReadPacket(channel, data, len);
  event_.Set();
  return ;
}

std::string PC_Transport_Impl::SerializeCandidate(
    const cricket::Candidate& candidate) {
  // TODO(sergeyu): Use SDP to format candidates?
  DictionaryValue value;
  value.SetString("name", candidate.name());
  value.SetString("ip", candidate.address().IPAsString());
  value.SetInteger("port", candidate.address().port());
  value.SetString("type", candidate.type());
  value.SetString("protocol", candidate.protocol());
  value.SetString("username", candidate.username());
  value.SetString("password", candidate.password());
  value.SetDouble("preference", candidate.preference());
  value.SetInteger("generation", candidate.generation());

  std::string result;
  JSONStringValueSerializer serializer(&result);
  serializer.Serialize(value);
  return result;
}

bool PC_Transport_Impl::DeserializeCandidate(const std::string& address,
                                            cricket::Candidate* candidate) {
  JSONStringValueSerializer deserializer(address);
  scoped_ptr<Value> value(deserializer.Deserialize(NULL, NULL));
  if (!value.get() || !value->IsType(Value::TYPE_DICTIONARY)) {
    return false;
  }

  DictionaryValue* dic_value = static_cast<DictionaryValue*>(value.get());

  std::string name;
  std::string ip;
  int port;
  std::string type;
  std::string protocol;
  std::string username;
  std::string password;
  double preference;
  int generation;

  if (!dic_value->GetString("name", &name) ||
      !dic_value->GetString("ip", &ip) ||
      !dic_value->GetInteger("port", &port) ||
      !dic_value->GetString("type", &type) ||
      !dic_value->GetString("protocol", &protocol) ||
      !dic_value->GetString("username", &username) ||
      !dic_value->GetString("password", &password) ||
      !dic_value->GetDouble("preference", &preference) ||
      !dic_value->GetInteger("generation", &generation)) {
    return false;
  }

  candidate->set_name(name);
  candidate->set_address(talk_base::SocketAddress(ip, port));
  candidate->set_type(type);
  candidate->set_protocol(protocol);
  candidate->set_username(username);
  candidate->set_password(password);
  candidate->set_preference(static_cast<float>(preference));
  candidate->set_generation(generation);

  return true;
}
#endif

void PC_Transport_Impl::OnStateChange(P2PTransportClass::State state) {
  writable_ = (state | P2PTransportClass::STATE_WRITABLE) != 0;
  if (writable_) {
    session_->OnStateChange(state, p2p_transport()->GetP2PChannel());
  }
}

void PC_Transport_Impl::OnError(int error) {

}

cricket::TransportChannel* PC_Transport_Impl::GetP2PChannel() {
  if (!p2p_transport_.get())
    return NULL;

  return p2p_transport_->GetP2PChannel();
}

void PC_Transport_Impl::OnMessage(talk_base::Message* message) {
  talk_base::MessageData* data = message->pdata;
  switch(message->message_id) {
    case MSG_RTC_TRANSPORTINIT : {
      talk_base::TypedMessageData<std::string> *p =
                static_cast<talk_base::TypedMessageData<std::string>* >(data);
      Init(p->data());
      delete p;
      break;
    }
    case MSG_RTC_ADDREMOTECANDIDATE : {
      talk_base::TypedMessageData<const cricket::Candidate*> *p =
          static_cast<talk_base::TypedMessageData<const cricket::Candidate*>* >(data);
      AddRemoteCandidate(*p->data());
      delete p;
      break;
    }
#ifdef PLATFORM_CHROMIUM
    case MSG_RTC_ONCANDIDATEREADY : {
      talk_base::TypedMessageData<std::string> *p =
          static_cast<talk_base::TypedMessageData<std::string>* >(data);
      OnCandidateReady(p->data());
      delete p;
      break;
    }
    case MSG_RTC_ONREADPACKET : {
      MediaDataMsgParams* p = static_cast<MediaDataMsgParams*> (data);
      ASSERT (p != NULL);
      OnReadPacket_w(p->channel, p->data, p->len);
      delete data;
      break;
    }
#else
    case MSG_RTC_ONCANDIDATEREADY : {
      talk_base::TypedMessageData<const cricket::Candidate*> *p =
          static_cast<talk_base::TypedMessageData<const cricket::Candidate*>* >(data);
      OnCandidateReady(*p->data());
      delete p;
      break;
    }
#endif
    default:
      ASSERT(false);
  }
}

P2PTransportClass* PC_Transport_Impl::CreateP2PTransport() {
#ifdef PLATFORM_CHROMIUM
  return new P2PTransportImpl(
      session()->connection()->p2p_socket_dispatcher());
#else
  return new P2PTransportManager(session()->port_allocator());
#endif
}

} //namespace webrtc

