/*
 * libjingle
 * Copyright 2012, Google Inc.
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

#include "talk/p2p/base/turnserver.h"

#include "talk/base/bytebuffer.h"
#include "talk/base/helpers.h"
#include "talk/base/logging.h"
#include "talk/base/messagedigest.h"
#include "talk/base/socketadapters.h"
#include "talk/base/stringencode.h"
#include "talk/base/thread.h"
#include "talk/p2p/base/asyncstuntcpsocket.h"
#include "talk/p2p/base/common.h"
#include "talk/p2p/base/packetsocketfactory.h"
#include "talk/p2p/base/stun.h"

namespace cricket {

// TODO(juberti): Move this all to a future turnmessage.h
//static const int IPPROTO_UDP = 17;
static const int kNonceTimeout = 60 * 60 * 1000;              // 60 minutes
static const int kDefaultAllocationTimeout = 10 * 60 * 1000;  // 10 minutes
static const int kPermissionTimeout = 5 * 60 * 1000;          //  5 minutes
static const int kChannelTimeout = 10 * 60 * 1000;            // 10 minutes

static const int kMinChannelNumber = 0x4000;
static const int kMaxChannelNumber = 0x7FFF;

static const size_t kNonceKeySize = 16;
static const size_t kNonceSize = 40;

static const size_t TURN_CHANNEL_HEADER_SIZE = 4U;

// TODO(mallinath) - Move these to a common place.
static const size_t kMaxPacketSize = 64 * 1024;

inline bool IsTurnChannelData(uint16 msg_type) {
  // The first two bits of a channel data message are 0b01.
  return ((msg_type & 0xC000) == 0x4000);
}

// IDs used for posted messages.
enum {
  MSG_TIMEOUT,
};

// Encapsulates a TURN allocation.
// The object is created when an allocation request is received, and then
// handles TURN messages (via HandleTurnMessage) and channel data messages
// (via HandleChannelData) for this allocation when received by the server.
// The object self-deletes and informs the server if its lifetime timer expires.
class TurnServer::Allocation : public talk_base::MessageHandler,
                               public sigslot::has_slots<> {
 public:
  Allocation(TurnServer* server_,
             talk_base::Thread* thread, const Connection& conn,
             talk_base::AsyncPacketSocket* server_socket,
             const std::string& key);
  virtual ~Allocation();

  Connection* conn() { return &conn_; }
  const std::string& key() const { return key_; }
  const std::string& transaction_id() const { return transaction_id_; }
  const std::string& username() const { return username_; }
  const std::string& last_nonce() const { return last_nonce_; }
  void set_last_nonce(const std::string& nonce) { last_nonce_ = nonce; }

  std::string ToString() const;

  void HandleTurnMessage(const TurnMessage* msg);
  void HandleChannelData(const char* data, size_t size);

  sigslot::signal1<Allocation*> SignalDestroyed;

 private:
  typedef std::list<Permission*> PermissionList;
  typedef std::list<Channel*> ChannelList;

  void HandleAllocateRequest(const TurnMessage* msg);
  void HandleRefreshRequest(const TurnMessage* msg);
  void HandleSendIndication(const TurnMessage* msg);
  void HandleCreatePermissionRequest(const TurnMessage* msg);
  void HandleChannelBindRequest(const TurnMessage* msg);

  void OnExternalPacket(talk_base::AsyncPacketSocket* socket,
                        const char* data, size_t size,
                        const talk_base::SocketAddress& addr,
                        const talk_base::PacketTime& packet_time);

  static int ComputeLifetime(const TurnMessage* msg);
  bool HasPermission(const talk_base::IPAddress& addr);
  void AddPermission(const talk_base::IPAddress& addr);
  Permission* FindPermission(const talk_base::IPAddress& addr) const;
  Channel* FindChannel(int channel_id) const;
  Channel* FindChannel(const talk_base::SocketAddress& addr) const;

  void SendResponse(TurnMessage* msg);
  void SendBadRequestResponse(const TurnMessage* req);
  void SendErrorResponse(const TurnMessage* req, int code,
                         const std::string& reason);
  void SendExternal(const void* data, size_t size,
                    const talk_base::SocketAddress& peer);

  void OnPermissionDestroyed(Permission* perm);
  void OnChannelDestroyed(Channel* channel);
  virtual void OnMessage(talk_base::Message* msg);

  TurnServer* server_;
  talk_base::Thread* thread_;
  Connection conn_;
  talk_base::scoped_ptr<talk_base::AsyncPacketSocket> external_socket_;
  std::string key_;
  std::string transaction_id_;
  std::string username_;
  std::string last_nonce_;
  PermissionList perms_;
  ChannelList channels_;
};

// Encapsulates a TURN permission.
// The object is created when a create permission request is received by an
// allocation, and self-deletes when its lifetime timer expires.
class TurnServer::Permission : public talk_base::MessageHandler {
 public:
  Permission(talk_base::Thread* thread, const talk_base::IPAddress& peer);
  ~Permission();

  const talk_base::IPAddress& peer() const { return peer_; }
  void Refresh();

  sigslot::signal1<Permission*> SignalDestroyed;

 private:
  virtual void OnMessage(talk_base::Message* msg);

  talk_base::Thread* thread_;
  talk_base::IPAddress peer_;
};

// Encapsulates a TURN channel binding.
// The object is created when a channel bind request is received by an
// allocation, and self-deletes when its lifetime timer expires.
class TurnServer::Channel : public talk_base::MessageHandler {
 public:
  Channel(talk_base::Thread* thread, int id,
                     const talk_base::SocketAddress& peer);
  ~Channel();

  int id() const { return id_; }
  const talk_base::SocketAddress& peer() const { return peer_; }
  void Refresh();

  sigslot::signal1<Channel*> SignalDestroyed;

 private:
  virtual void OnMessage(talk_base::Message* msg);

  talk_base::Thread* thread_;
  int id_;
  talk_base::SocketAddress peer_;
};

static bool InitResponse(const StunMessage* req, StunMessage* resp) {
  int resp_type = (req) ? GetStunSuccessResponseType(req->type()) : -1;
  if (resp_type == -1)
    return false;
  resp->SetType(resp_type);
  resp->SetTransactionID(req->transaction_id());
  return true;
}

static bool InitErrorResponse(const StunMessage* req, int code,
                              const std::string& reason, StunMessage* resp) {
  int resp_type = (req) ? GetStunErrorResponseType(req->type()) : -1;
  if (resp_type == -1)
    return false;
  resp->SetType(resp_type);
  resp->SetTransactionID(req->transaction_id());
  VERIFY(resp->AddAttribute(new cricket::StunErrorCodeAttribute(
      STUN_ATTR_ERROR_CODE, code, reason)));
  return true;
}

TurnServer::TurnServer(talk_base::Thread* thread)
    : thread_(thread),
      nonce_key_(talk_base::CreateRandomString(kNonceKeySize)),
      auth_hook_(NULL),
      enable_otu_nonce_(false) {
}

TurnServer::~TurnServer() {
  for (AllocationMap::iterator it = allocations_.begin();
       it != allocations_.end(); ++it) {
    delete it->second;
  }

  for (InternalSocketMap::iterator it = server_sockets_.begin();
       it != server_sockets_.end(); ++it) {
    talk_base::AsyncPacketSocket* socket = it->first;
    delete socket;
  }

  for (ServerSocketMap::iterator it = server_listen_sockets_.begin();
       it != server_listen_sockets_.end(); ++it) {
    talk_base::AsyncSocket* socket = it->first;
    delete socket;
  }
}

void TurnServer::AddInternalSocket(talk_base::AsyncPacketSocket* socket,
                                   ProtocolType proto) {
  ASSERT(server_sockets_.end() == server_sockets_.find(socket));
  server_sockets_[socket] = proto;
  socket->SignalReadPacket.connect(this, &TurnServer::OnInternalPacket);
}

void TurnServer::AddInternalServerSocket(talk_base::AsyncSocket* socket,
                                         ProtocolType proto) {
  ASSERT(server_listen_sockets_.end() ==
      server_listen_sockets_.find(socket));
  server_listen_sockets_[socket] = proto;
  socket->SignalReadEvent.connect(this, &TurnServer::OnNewInternalConnection);
}

void TurnServer::SetExternalSocketFactory(
    talk_base::PacketSocketFactory* factory,
    const talk_base::SocketAddress& external_addr) {
  external_socket_factory_.reset(factory);
  external_addr_ = external_addr;
}

void TurnServer::OnNewInternalConnection(talk_base::AsyncSocket* socket) {
  ASSERT(server_listen_sockets_.find(socket) != server_listen_sockets_.end());
  AcceptConnection(socket);
}

void TurnServer::AcceptConnection(talk_base::AsyncSocket* server_socket) {
  // Check if someone is trying to connect to us.
  talk_base::SocketAddress accept_addr;
  talk_base::AsyncSocket* accepted_socket = server_socket->Accept(&accept_addr);
  if (accepted_socket != NULL) {
    ProtocolType proto = server_listen_sockets_[server_socket];
    cricket::AsyncStunTCPSocket* tcp_socket =
        new cricket::AsyncStunTCPSocket(accepted_socket, false);

    tcp_socket->SignalClose.connect(this, &TurnServer::OnInternalSocketClose);
    // Finally add the socket so it can start communicating with the client.
    AddInternalSocket(tcp_socket, proto);
  }
}

void TurnServer::OnInternalSocketClose(talk_base::AsyncPacketSocket* socket,
                                       int err) {
  DestroyInternalSocket(socket);
}

void TurnServer::OnInternalPacket(talk_base::AsyncPacketSocket* socket,
                                  const char* data, size_t size,
                                  const talk_base::SocketAddress& addr,
                                  const talk_base::PacketTime& packet_time) {
  // Fail if the packet is too small to even contain a channel header.
  if (size < TURN_CHANNEL_HEADER_SIZE) {
   return;
  }
  InternalSocketMap::iterator iter = server_sockets_.find(socket);
  ASSERT(iter != server_sockets_.end());
  Connection conn(addr, iter->second, socket);
  uint16 msg_type = talk_base::GetBE16(data);
  if (!IsTurnChannelData(msg_type)) {
    // This is a STUN message.
    HandleStunMessage(&conn, data, size);
  } else {
    // This is a channel message; let the allocation handle it.
    Allocation* allocation = FindAllocation(&conn);
    if (allocation) {
      allocation->HandleChannelData(data, size);
    }
  }
}

void TurnServer::HandleStunMessage(Connection* conn, const char* data,
                                   size_t size) {
  TurnMessage msg;
  talk_base::ByteBuffer buf(data, size);
  if (!msg.Read(&buf) || (buf.Length() > 0)) {
    LOG(LS_WARNING) << "Received invalid STUN message";
    return;
  }

  // If it's a STUN binding request, handle that specially.
  if (msg.type() == STUN_BINDING_REQUEST) {
    HandleBindingRequest(conn, &msg);
    return;
  }

  // Look up the key that we'll use to validate the M-I. If we have an
  // existing allocation, the key will already be cached.
  Allocation* allocation = FindAllocation(conn);
  std::string key;
  if (!allocation) {
    GetKey(&msg, &key);
  } else {
    key = allocation->key();
  }

  // Ensure the message is authorized; only needed for requests.
  if (IsStunRequestType(msg.type())) {
    if (!CheckAuthorization(conn, &msg, data, size, key)) {
      return;
    }
  }

  if (!allocation && msg.type() == STUN_ALLOCATE_REQUEST) {
    // This is a new allocate request.
    HandleAllocateRequest(conn, &msg, key);
  } else if (allocation &&
             (msg.type() != STUN_ALLOCATE_REQUEST ||
              msg.transaction_id() == allocation->transaction_id())) {
    // This is a non-allocate request, or a retransmit of an allocate.
    // Check that the username matches the previous username used.
    if (IsStunRequestType(msg.type()) &&
        msg.GetByteString(STUN_ATTR_USERNAME)->GetString() !=
            allocation->username()) {
      SendErrorResponse(conn, &msg, STUN_ERROR_WRONG_CREDENTIALS,
                        STUN_ERROR_REASON_WRONG_CREDENTIALS);
      return;
    }
    allocation->HandleTurnMessage(&msg);
  } else {
    // Allocation mismatch.
    SendErrorResponse(conn, &msg, STUN_ERROR_ALLOCATION_MISMATCH,
                      STUN_ERROR_REASON_ALLOCATION_MISMATCH);
  }
}

bool TurnServer::GetKey(const StunMessage* msg, std::string* key) {
  const StunByteStringAttribute* username_attr =
      msg->GetByteString(STUN_ATTR_USERNAME);
  if (!username_attr) {
    return false;
  }

  std::string username = username_attr->GetString();
  return (auth_hook_ != NULL && auth_hook_->GetKey(username, realm_, key));
}

bool TurnServer::CheckAuthorization(Connection* conn,
                                    const StunMessage* msg,
                                    const char* data, size_t size,
                                    const std::string& key) {
  // RFC 5389, 10.2.2.
  ASSERT(IsStunRequestType(msg->type()));
  const StunByteStringAttribute* mi_attr =
      msg->GetByteString(STUN_ATTR_MESSAGE_INTEGRITY);
  const StunByteStringAttribute* username_attr =
      msg->GetByteString(STUN_ATTR_USERNAME);
  const StunByteStringAttribute* realm_attr =
      msg->GetByteString(STUN_ATTR_REALM);
  const StunByteStringAttribute* nonce_attr =
      msg->GetByteString(STUN_ATTR_NONCE);

  // Fail if no M-I.
  if (!mi_attr) {
    SendErrorResponseWithRealmAndNonce(conn, msg, STUN_ERROR_UNAUTHORIZED,
                                       STUN_ERROR_REASON_UNAUTHORIZED);
    return false;
  }

  // Fail if there is M-I but no username, nonce, or realm.
  if (!username_attr || !realm_attr || !nonce_attr) {
    SendErrorResponse(conn, msg, STUN_ERROR_BAD_REQUEST,
                      STUN_ERROR_REASON_BAD_REQUEST);
    return false;
  }

  // Fail if bad nonce.
  if (!ValidateNonce(nonce_attr->GetString())) {
    SendErrorResponseWithRealmAndNonce(conn, msg, STUN_ERROR_STALE_NONCE,
                                       STUN_ERROR_REASON_STALE_NONCE);
    return false;
  }

  // Fail if bad username or M-I.
  // We need |data| and |size| for the call to ValidateMessageIntegrity.
  if (key.empty() || !StunMessage::ValidateMessageIntegrity(data, size, key)) {
    SendErrorResponseWithRealmAndNonce(conn, msg, STUN_ERROR_UNAUTHORIZED,
                                       STUN_ERROR_REASON_UNAUTHORIZED);
    return false;
  }

  // Fail if one-time-use nonce feature is enabled.
  Allocation* allocation = FindAllocation(conn);
  if (enable_otu_nonce_ && allocation &&
      allocation->last_nonce() == nonce_attr->GetString()) {
    SendErrorResponseWithRealmAndNonce(conn, msg, STUN_ERROR_STALE_NONCE,
                                       STUN_ERROR_REASON_STALE_NONCE);
    return false;
  }

  if (allocation) {
    allocation->set_last_nonce(nonce_attr->GetString());
  }
  // Success.
  return true;
}

void TurnServer::HandleBindingRequest(Connection* conn,
                                      const StunMessage* req) {
  StunMessage response;
  InitResponse(req, &response);

  // Tell the user the address that we received their request from.
  StunAddressAttribute* mapped_addr_attr;
  mapped_addr_attr = new StunXorAddressAttribute(
      STUN_ATTR_XOR_MAPPED_ADDRESS, conn->src());
  VERIFY(response.AddAttribute(mapped_addr_attr));

  SendStun(conn, &response);
}

void TurnServer::HandleAllocateRequest(Connection* conn,
                                       const TurnMessage* msg,
                                       const std::string& key) {
  // Check the parameters in the request.
  const StunUInt32Attribute* transport_attr =
      msg->GetUInt32(STUN_ATTR_REQUESTED_TRANSPORT);
  if (!transport_attr) {
    SendErrorResponse(conn, msg, STUN_ERROR_BAD_REQUEST,
                      STUN_ERROR_REASON_BAD_REQUEST);
    return;
  }

  // Only UDP is supported right now.
  int proto = transport_attr->value() >> 24;
  if (proto != IPPROTO_UDP) {
    SendErrorResponse(conn, msg, STUN_ERROR_UNSUPPORTED_PROTOCOL,
                      STUN_ERROR_REASON_UNSUPPORTED_PROTOCOL);
    return;
  }

  // Create the allocation and let it send the success response.
  // If the actual socket allocation fails, send an internal error.
  Allocation* alloc = CreateAllocation(conn, proto, key);
  if (alloc) {
    alloc->HandleTurnMessage(msg);
  } else {
    SendErrorResponse(conn, msg, STUN_ERROR_SERVER_ERROR,
                      "Failed to allocate socket");
  }
}

std::string TurnServer::GenerateNonce() const {
  // Generate a nonce of the form hex(now + HMAC-MD5(nonce_key_, now))
  uint32 now = talk_base::Time();
  std::string input(reinterpret_cast<const char*>(&now), sizeof(now));
  std::string nonce = talk_base::hex_encode(input.c_str(), input.size());
  nonce += talk_base::ComputeHmac(talk_base::DIGEST_MD5, nonce_key_, input);
  ASSERT(nonce.size() == kNonceSize);
  return nonce;
}

bool TurnServer::ValidateNonce(const std::string& nonce) const {
  // Check the size.
  if (nonce.size() != kNonceSize) {
    return false;
  }

  // Decode the timestamp.
  uint32 then;
  char* p = reinterpret_cast<char*>(&then);
  size_t len = talk_base::hex_decode(p, sizeof(then),
      nonce.substr(0, sizeof(then) * 2));
  if (len != sizeof(then)) {
    return false;
  }

  // Verify the HMAC.
  if (nonce.substr(sizeof(then) * 2) != talk_base::ComputeHmac(
      talk_base::DIGEST_MD5, nonce_key_, std::string(p, sizeof(then)))) {
    return false;
  }

  // Validate the timestamp.
  return talk_base::TimeSince(then) < kNonceTimeout;
}

TurnServer::Allocation* TurnServer::FindAllocation(Connection* conn) {
  AllocationMap::const_iterator it = allocations_.find(*conn);
  return (it != allocations_.end()) ? it->second : NULL;
}

TurnServer::Allocation* TurnServer::CreateAllocation(Connection* conn,
                                                     int proto,
                                                     const std::string& key) {
  talk_base::AsyncPacketSocket* external_socket = (external_socket_factory_) ?
      external_socket_factory_->CreateUdpSocket(external_addr_, 0, 0) : NULL;
  if (!external_socket) {
    return NULL;
  }

  // The Allocation takes ownership of the socket.
  Allocation* allocation = new Allocation(this,
      thread_, *conn, external_socket, key);
  allocation->SignalDestroyed.connect(this, &TurnServer::OnAllocationDestroyed);
  allocations_[*conn] = allocation;
  return allocation;
}

void TurnServer::SendErrorResponse(Connection* conn,
                                   const StunMessage* req,
                                   int code, const std::string& reason) {
  TurnMessage resp;
  InitErrorResponse(req, code, reason, &resp);
  LOG(LS_INFO) << "Sending error response, type=" << resp.type()
               << ", code=" << code << ", reason=" << reason;
  SendStun(conn, &resp);
}

void TurnServer::SendErrorResponseWithRealmAndNonce(
    Connection* conn, const StunMessage* msg,
    int code, const std::string& reason) {
  TurnMessage resp;
  InitErrorResponse(msg, code, reason, &resp);
  VERIFY(resp.AddAttribute(new StunByteStringAttribute(
      STUN_ATTR_NONCE, GenerateNonce())));
  VERIFY(resp.AddAttribute(new StunByteStringAttribute(
      STUN_ATTR_REALM, realm_)));
  SendStun(conn, &resp);
}

void TurnServer::SendStun(Connection* conn, StunMessage* msg) {
  talk_base::ByteBuffer buf;
  // Add a SOFTWARE attribute if one is set.
  if (!software_.empty()) {
    VERIFY(msg->AddAttribute(
        new StunByteStringAttribute(STUN_ATTR_SOFTWARE, software_)));
  }
  msg->Write(&buf);
  Send(conn, buf);
}

void TurnServer::Send(Connection* conn,
                      const talk_base::ByteBuffer& buf) {
  conn->socket()->SendTo(buf.Data(), buf.Length(), conn->src(),
                         talk_base::DSCP_NO_CHANGE);
}

void TurnServer::OnAllocationDestroyed(Allocation* allocation) {
  // Removing the internal socket if the connection is not udp.
  talk_base::AsyncPacketSocket* socket = allocation->conn()->socket();
  InternalSocketMap::iterator iter = server_sockets_.find(socket);
  ASSERT(iter != server_sockets_.end());
  // Skip if the socket serving this allocation is UDP, as this will be shared
  // by all allocations.
  if (iter->second != cricket::PROTO_UDP) {
    DestroyInternalSocket(socket);
  }

  AllocationMap::iterator it = allocations_.find(*(allocation->conn()));
  if (it != allocations_.end())
    allocations_.erase(it);
}

void TurnServer::DestroyInternalSocket(talk_base::AsyncPacketSocket* socket) {
  InternalSocketMap::iterator iter = server_sockets_.find(socket);
  if (iter != server_sockets_.end()) {
    talk_base::AsyncPacketSocket* socket = iter->first;
    delete socket;
    server_sockets_.erase(iter);
  }
}

TurnServer::Connection::Connection(const talk_base::SocketAddress& src,
                                   ProtocolType proto,
                                   talk_base::AsyncPacketSocket* socket)
    : src_(src),
      dst_(socket->GetRemoteAddress()),
      proto_(proto),
      socket_(socket) {
}

bool TurnServer::Connection::operator==(const Connection& c) const {
  return src_ == c.src_ && dst_ == c.dst_ && proto_ == c.proto_;
}

bool TurnServer::Connection::operator<(const Connection& c) const {
  return src_ < c.src_ || dst_ < c.dst_ || proto_ < c.proto_;
}

std::string TurnServer::Connection::ToString() const {
  const char* const kProtos[] = {
      "unknown", "udp", "tcp", "ssltcp"
  };
  std::ostringstream ost;
  ost << src_.ToString() << "-" << dst_.ToString() << ":"<< kProtos[proto_];
  return ost.str();
}

TurnServer::Allocation::Allocation(TurnServer* server,
                                   talk_base::Thread* thread,
                                   const Connection& conn,
                                   talk_base::AsyncPacketSocket* socket,
                                   const std::string& key)
    : server_(server),
      thread_(thread),
      conn_(conn),
      external_socket_(socket),
      key_(key) {
  external_socket_->SignalReadPacket.connect(
      this, &TurnServer::Allocation::OnExternalPacket);
}

TurnServer::Allocation::~Allocation() {
  for (ChannelList::iterator it = channels_.begin();
       it != channels_.end(); ++it) {
    delete *it;
  }
  for (PermissionList::iterator it = perms_.begin();
       it != perms_.end(); ++it) {
    delete *it;
  }
  thread_->Clear(this, MSG_TIMEOUT);
  LOG_J(LS_INFO, this) << "Allocation destroyed";
}

std::string TurnServer::Allocation::ToString() const {
  std::ostringstream ost;
  ost << "Alloc[" << conn_.ToString() << "]";
  return ost.str();
}

void TurnServer::Allocation::HandleTurnMessage(const TurnMessage* msg) {
  ASSERT(msg != NULL);
  switch (msg->type()) {
    case STUN_ALLOCATE_REQUEST:
      HandleAllocateRequest(msg);
      break;
    case TURN_REFRESH_REQUEST:
      HandleRefreshRequest(msg);
      break;
    case TURN_SEND_INDICATION:
      HandleSendIndication(msg);
      break;
    case TURN_CREATE_PERMISSION_REQUEST:
      HandleCreatePermissionRequest(msg);
      break;
    case TURN_CHANNEL_BIND_REQUEST:
      HandleChannelBindRequest(msg);
      break;
    default:
      // Not sure what to do with this, just eat it.
      LOG_J(LS_WARNING, this) << "Invalid TURN message type received: "
                              << msg->type();
  }
}

void TurnServer::Allocation::HandleAllocateRequest(const TurnMessage* msg) {
  // Copy the important info from the allocate request.
  transaction_id_ = msg->transaction_id();
  const StunByteStringAttribute* username_attr =
      msg->GetByteString(STUN_ATTR_USERNAME);
  ASSERT(username_attr != NULL);
  username_ = username_attr->GetString();

  // Figure out the lifetime and start the allocation timer.
  int lifetime_secs = ComputeLifetime(msg);
  thread_->PostDelayed(lifetime_secs * 1000, this, MSG_TIMEOUT);

  LOG_J(LS_INFO, this) << "Created allocation, lifetime=" << lifetime_secs;

  // We've already validated all the important bits; just send a response here.
  TurnMessage response;
  InitResponse(msg, &response);

  StunAddressAttribute* mapped_addr_attr =
      new StunXorAddressAttribute(STUN_ATTR_XOR_MAPPED_ADDRESS, conn_.src());
  StunAddressAttribute* relayed_addr_attr =
      new StunXorAddressAttribute(STUN_ATTR_XOR_RELAYED_ADDRESS,
          external_socket_->GetLocalAddress());
  StunUInt32Attribute* lifetime_attr =
      new StunUInt32Attribute(STUN_ATTR_LIFETIME, lifetime_secs);
  VERIFY(response.AddAttribute(mapped_addr_attr));
  VERIFY(response.AddAttribute(relayed_addr_attr));
  VERIFY(response.AddAttribute(lifetime_attr));

  SendResponse(&response);
}

void TurnServer::Allocation::HandleRefreshRequest(const TurnMessage* msg) {
  // Figure out the new lifetime.
  int lifetime_secs = ComputeLifetime(msg);

  // Reset the expiration timer.
  thread_->Clear(this, MSG_TIMEOUT);
  thread_->PostDelayed(lifetime_secs * 1000, this, MSG_TIMEOUT);

  LOG_J(LS_INFO, this) << "Refreshed allocation, lifetime=" << lifetime_secs;

  // Send a success response with a LIFETIME attribute.
  TurnMessage response;
  InitResponse(msg, &response);

  StunUInt32Attribute* lifetime_attr =
      new StunUInt32Attribute(STUN_ATTR_LIFETIME, lifetime_secs);
  VERIFY(response.AddAttribute(lifetime_attr));

  SendResponse(&response);
}

void TurnServer::Allocation::HandleSendIndication(const TurnMessage* msg) {
  // Check mandatory attributes.
  const StunByteStringAttribute* data_attr = msg->GetByteString(STUN_ATTR_DATA);
  const StunAddressAttribute* peer_attr =
      msg->GetAddress(STUN_ATTR_XOR_PEER_ADDRESS);
  if (!data_attr || !peer_attr) {
    LOG_J(LS_WARNING, this) << "Received invalid send indication";
    return;
  }

  // If a permission exists, send the data on to the peer.
  if (HasPermission(peer_attr->GetAddress().ipaddr())) {
    SendExternal(data_attr->bytes(), data_attr->length(),
                 peer_attr->GetAddress());
  } else {
    LOG_J(LS_WARNING, this) << "Received send indication without permission"
                            << "peer=" << peer_attr->GetAddress();
  }
}

void TurnServer::Allocation::HandleCreatePermissionRequest(
    const TurnMessage* msg) {
  // Check mandatory attributes.
  const StunAddressAttribute* peer_attr =
      msg->GetAddress(STUN_ATTR_XOR_PEER_ADDRESS);
  if (!peer_attr) {
    SendBadRequestResponse(msg);
    return;
  }

  // Add this permission.
  AddPermission(peer_attr->GetAddress().ipaddr());

  LOG_J(LS_INFO, this) << "Created permission, peer="
                       << peer_attr->GetAddress();

  // Send a success response.
  TurnMessage response;
  InitResponse(msg, &response);
  SendResponse(&response);
}

void TurnServer::Allocation::HandleChannelBindRequest(const TurnMessage* msg) {
  // Check mandatory attributes.
  const StunUInt32Attribute* channel_attr =
      msg->GetUInt32(STUN_ATTR_CHANNEL_NUMBER);
  const StunAddressAttribute* peer_attr =
      msg->GetAddress(STUN_ATTR_XOR_PEER_ADDRESS);
  if (!channel_attr || !peer_attr) {
    SendBadRequestResponse(msg);
    return;
  }

  // Check that channel id is valid.
  int channel_id = channel_attr->value() >> 16;
  if (channel_id < kMinChannelNumber || channel_id > kMaxChannelNumber) {
    SendBadRequestResponse(msg);
    return;
  }

  // Check that this channel id isn't bound to another transport address, and
  // that this transport address isn't bound to another channel id.
  Channel* channel1 = FindChannel(channel_id);
  Channel* channel2 = FindChannel(peer_attr->GetAddress());
  if (channel1 != channel2) {
    SendBadRequestResponse(msg);
    return;
  }

  // Add or refresh this channel.
  if (!channel1) {
    channel1 = new Channel(thread_, channel_id, peer_attr->GetAddress());
    channel1->SignalDestroyed.connect(this,
        &TurnServer::Allocation::OnChannelDestroyed);
    channels_.push_back(channel1);
  } else {
    channel1->Refresh();
  }

  // Channel binds also refresh permissions.
  AddPermission(peer_attr->GetAddress().ipaddr());

  LOG_J(LS_INFO, this) << "Bound channel, id=" << channel_id
                       << ", peer=" << peer_attr->GetAddress();

  // Send a success response.
  TurnMessage response;
  InitResponse(msg, &response);
  SendResponse(&response);
}

void TurnServer::Allocation::HandleChannelData(const char* data, size_t size) {
  // Extract the channel number from the data.
  uint16 channel_id = talk_base::GetBE16(data);
  Channel* channel = FindChannel(channel_id);
  if (channel) {
    // Send the data to the peer address.
    SendExternal(data + TURN_CHANNEL_HEADER_SIZE,
                 size - TURN_CHANNEL_HEADER_SIZE, channel->peer());
  } else {
    LOG_J(LS_WARNING, this) << "Received channel data for invalid channel, id="
                            << channel_id;
  }
}

void TurnServer::Allocation::OnExternalPacket(
    talk_base::AsyncPacketSocket* socket,
    const char* data, size_t size,
    const talk_base::SocketAddress& addr,
    const talk_base::PacketTime& packet_time) {
  ASSERT(external_socket_.get() == socket);
  Channel* channel = FindChannel(addr);
  if (channel) {
    // There is a channel bound to this address. Send as a channel message.
    talk_base::ByteBuffer buf;
    buf.WriteUInt16(channel->id());
    buf.WriteUInt16(static_cast<uint16>(size));
    buf.WriteBytes(data, size);
    server_->Send(&conn_, buf);
  } else if (HasPermission(addr.ipaddr())) {
    // No channel, but a permission exists. Send as a data indication.
    TurnMessage msg;
    msg.SetType(TURN_DATA_INDICATION);
    msg.SetTransactionID(
        talk_base::CreateRandomString(kStunTransactionIdLength));
    VERIFY(msg.AddAttribute(new StunXorAddressAttribute(
        STUN_ATTR_XOR_PEER_ADDRESS, addr)));
    VERIFY(msg.AddAttribute(new StunByteStringAttribute(
        STUN_ATTR_DATA, data, size)));
    server_->SendStun(&conn_, &msg);
  } else {
    LOG_J(LS_WARNING, this) << "Received external packet without permission, "
                            << "peer=" << addr;
  }
}

int TurnServer::Allocation::ComputeLifetime(const TurnMessage* msg) {
  // Return the smaller of our default lifetime and the requested lifetime.
  uint32 lifetime = kDefaultAllocationTimeout / 1000;  // convert to seconds
  const StunUInt32Attribute* lifetime_attr = msg->GetUInt32(STUN_ATTR_LIFETIME);
  if (lifetime_attr && lifetime_attr->value() < lifetime) {
    lifetime = lifetime_attr->value();
  }
  return lifetime;
}

bool TurnServer::Allocation::HasPermission(const talk_base::IPAddress& addr) {
  return (FindPermission(addr) != NULL);
}

void TurnServer::Allocation::AddPermission(const talk_base::IPAddress& addr) {
  Permission* perm = FindPermission(addr);
  if (!perm) {
    perm = new Permission(thread_, addr);
    perm->SignalDestroyed.connect(
        this, &TurnServer::Allocation::OnPermissionDestroyed);
    perms_.push_back(perm);
  } else {
    perm->Refresh();
  }
}

TurnServer::Permission* TurnServer::Allocation::FindPermission(
    const talk_base::IPAddress& addr) const {
  for (PermissionList::const_iterator it = perms_.begin();
       it != perms_.end(); ++it) {
    if ((*it)->peer() == addr)
      return *it;
  }
  return NULL;
}

TurnServer::Channel* TurnServer::Allocation::FindChannel(int channel_id) const {
  for (ChannelList::const_iterator it = channels_.begin();
       it != channels_.end(); ++it) {
    if ((*it)->id() == channel_id)
      return *it;
  }
  return NULL;
}

TurnServer::Channel* TurnServer::Allocation::FindChannel(
    const talk_base::SocketAddress& addr) const {
  for (ChannelList::const_iterator it = channels_.begin();
       it != channels_.end(); ++it) {
    if ((*it)->peer() == addr)
      return *it;
  }
  return NULL;
}

void TurnServer::Allocation::SendResponse(TurnMessage* msg) {
  // Success responses always have M-I.
  msg->AddMessageIntegrity(key_);
  server_->SendStun(&conn_, msg);
}

void TurnServer::Allocation::SendBadRequestResponse(const TurnMessage* req) {
  SendErrorResponse(req, STUN_ERROR_BAD_REQUEST, STUN_ERROR_REASON_BAD_REQUEST);
}

void TurnServer::Allocation::SendErrorResponse(const TurnMessage* req, int code,
                                       const std::string& reason) {
  server_->SendErrorResponse(&conn_, req, code, reason);
}

void TurnServer::Allocation::SendExternal(const void* data, size_t size,
                                  const talk_base::SocketAddress& peer) {
  external_socket_->SendTo(data, size, peer, talk_base::DSCP_NO_CHANGE);
}

void TurnServer::Allocation::OnMessage(talk_base::Message* msg) {
  ASSERT(msg->message_id == MSG_TIMEOUT);
  SignalDestroyed(this);
  delete this;
}

void TurnServer::Allocation::OnPermissionDestroyed(Permission* perm) {
  PermissionList::iterator it = std::find(perms_.begin(), perms_.end(), perm);
  ASSERT(it != perms_.end());
  perms_.erase(it);
}

void TurnServer::Allocation::OnChannelDestroyed(Channel* channel) {
  ChannelList::iterator it =
      std::find(channels_.begin(), channels_.end(), channel);
  ASSERT(it != channels_.end());
  channels_.erase(it);
}

TurnServer::Permission::Permission(talk_base::Thread* thread,
                                   const talk_base::IPAddress& peer)
    : thread_(thread), peer_(peer) {
  Refresh();
}

TurnServer::Permission::~Permission() {
  thread_->Clear(this, MSG_TIMEOUT);
}

void TurnServer::Permission::Refresh() {
  thread_->Clear(this, MSG_TIMEOUT);
  thread_->PostDelayed(kPermissionTimeout, this, MSG_TIMEOUT);
}

void TurnServer::Permission::OnMessage(talk_base::Message* msg) {
  ASSERT(msg->message_id == MSG_TIMEOUT);
  SignalDestroyed(this);
  delete this;
}

TurnServer::Channel::Channel(talk_base::Thread* thread, int id,
                             const talk_base::SocketAddress& peer)
    : thread_(thread), id_(id), peer_(peer) {
  Refresh();
}

TurnServer::Channel::~Channel() {
  thread_->Clear(this, MSG_TIMEOUT);
}

void TurnServer::Channel::Refresh() {
  thread_->Clear(this, MSG_TIMEOUT);
  thread_->PostDelayed(kChannelTimeout, this, MSG_TIMEOUT);
}

void TurnServer::Channel::OnMessage(talk_base::Message* msg) {
  ASSERT(msg->message_id == MSG_TIMEOUT);
  SignalDestroyed(this);
  delete this;
}

}  // namespace cricket
