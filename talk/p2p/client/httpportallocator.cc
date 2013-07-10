/*
 * libjingle
 * Copyright 2004--2008, Google Inc.
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

#include "talk/p2p/client/httpportallocator.h"

#include <algorithm>
#include <map>

#include "talk/base/asynchttprequest.h"
#include "talk/base/basicdefs.h"
#include "talk/base/common.h"
#include "talk/base/helpers.h"
#include "talk/base/logging.h"
#include "talk/base/nethelpers.h"
#include "talk/base/signalthread.h"
#include "talk/base/stringencode.h"

namespace {

const uint32 MSG_TIMEOUT = 100;  // must not conflict
  // with BasicPortAllocator.cpp

// Helper routine to remove whitespace from the ends of a string.
void Trim(std::string& str) {
  size_t first = str.find_first_not_of(" \t\r\n");
  if (first == std::string::npos) {
    str.clear();
    return;
  }

  ASSERT(str.find_last_not_of(" \t\r\n") != std::string::npos);
}

// Parses the lines in the result of the HTTP request that are of the form
// 'a=b' and returns them in a map.
typedef std::map<std::string, std::string> StringMap;
void ParseMap(const std::string& string, StringMap& map) {
  size_t start_of_line = 0;
  size_t end_of_line = 0;

  for (;;) {  // for each line
    start_of_line = string.find_first_not_of("\r\n", end_of_line);
    if (start_of_line == std::string::npos)
      break;

    end_of_line = string.find_first_of("\r\n", start_of_line);
    if (end_of_line == std::string::npos) {
      end_of_line = string.length();
    }

    size_t equals = string.find('=', start_of_line);
    if ((equals >= end_of_line) || (equals == std::string::npos))
      continue;

    std::string key(string, start_of_line, equals - start_of_line);
    std::string value(string, equals + 1, end_of_line - equals - 1);

    Trim(key);
    Trim(value);

    if ((key.size() > 0) && (value.size() > 0))
      map[key] = value;
  }
}

}  // namespace

namespace cricket {

// HttpPortAllocatorBase

const int HttpPortAllocatorBase::kNumRetries = 5;

const char HttpPortAllocatorBase::kCreateSessionURL[] = "/create_session";

HttpPortAllocatorBase::HttpPortAllocatorBase(
    talk_base::NetworkManager* network_manager,
    talk_base::PacketSocketFactory* socket_factory,
    const std::string &user_agent)
    : BasicPortAllocator(network_manager, socket_factory), agent_(user_agent) {
  relay_hosts_.push_back("relay.google.com");
  stun_hosts_.push_back(
      talk_base::SocketAddress("stun.l.google.com", 19302));
}

HttpPortAllocatorBase::HttpPortAllocatorBase(
    talk_base::NetworkManager* network_manager,
    const std::string &user_agent)
    : BasicPortAllocator(network_manager), agent_(user_agent) {
  relay_hosts_.push_back("relay.google.com");
  stun_hosts_.push_back(
      talk_base::SocketAddress("stun.l.google.com", 19302));
}

HttpPortAllocatorBase::~HttpPortAllocatorBase() {
}

// HttpPortAllocatorSessionBase

HttpPortAllocatorSessionBase::HttpPortAllocatorSessionBase(
    HttpPortAllocatorBase* allocator,
    const std::string& content_name,
    int component,
    const std::string& ice_ufrag,
    const std::string& ice_pwd,
    const std::vector<talk_base::SocketAddress>& stun_hosts,
    const std::vector<std::string>& relay_hosts,
    const std::string& relay_token,
    const std::string& user_agent)
    : BasicPortAllocatorSession(allocator, content_name, component,
                                ice_ufrag, ice_pwd),
      relay_hosts_(relay_hosts), stun_hosts_(stun_hosts),
      relay_token_(relay_token), agent_(user_agent), attempts_(0) {
}

HttpPortAllocatorSessionBase::~HttpPortAllocatorSessionBase() {}

void HttpPortAllocatorSessionBase::GetPortConfigurations() {
  // Creating relay sessions can take time and is done asynchronously.
  // Creating stun sessions could also take time and could be done aysnc also,
  // but for now is done here and added to the initial config.  Note any later
  // configs will have unresolved stun ips and will be discarded by the
  // AllocationSequence.
  PortConfiguration* config = new PortConfiguration(stun_hosts_[0],
                                                    username(),
                                                    password());
  ConfigReady(config);
  TryCreateRelaySession();
}

void HttpPortAllocatorSessionBase::TryCreateRelaySession() {
  if (allocator()->flags() & PORTALLOCATOR_DISABLE_RELAY) {
    LOG(LS_VERBOSE) << "HttpPortAllocator: Relay ports disabled, skipping.";
    return;
  }

  if (attempts_ == HttpPortAllocator::kNumRetries) {
    LOG(LS_ERROR) << "HttpPortAllocator: maximum number of requests reached; "
                  << "giving up on relay.";
    return;
  }

  if (relay_hosts_.size() == 0) {
    LOG(LS_ERROR) << "HttpPortAllocator: no relay hosts configured.";
    return;
  }

  // Choose the next host to try.
  std::string host = relay_hosts_[attempts_ % relay_hosts_.size()];
  attempts_++;
  LOG(LS_INFO) << "HTTPPortAllocator: sending to relay host " << host;
  if (relay_token_.empty()) {
    LOG(LS_WARNING) << "No relay auth token found.";
  }

  SendSessionRequest(host, talk_base::HTTP_SECURE_PORT);
}

std::string HttpPortAllocatorSessionBase::GetSessionRequestUrl() {
  std::string url = std::string(HttpPortAllocator::kCreateSessionURL);
  if (allocator()->flags() & PORTALLOCATOR_ENABLE_SHARED_UFRAG) {
    ASSERT(!username().empty());
    ASSERT(!password().empty());
    url = url + "?username=" + talk_base::s_url_encode(username()) +
        "&password=" + talk_base::s_url_encode(password());
  }
  return url;
}

void HttpPortAllocatorSessionBase::ReceiveSessionResponse(
    const std::string& response) {

  StringMap map;
  ParseMap(response, map);

  if (!username().empty() && map["username"] != username()) {
    LOG(LS_WARNING) << "Received unexpected username value from relay server.";
  }
  if (!password().empty() && map["password"] != password()) {
    LOG(LS_WARNING) << "Received unexpected password value from relay server.";
  }

  std::string relay_ip = map["relay.ip"];
  std::string relay_udp_port = map["relay.udp_port"];
  std::string relay_tcp_port = map["relay.tcp_port"];
  std::string relay_ssltcp_port = map["relay.ssltcp_port"];

  PortConfiguration* config = new PortConfiguration(stun_hosts_[0],
                                                    map["username"],
                                                    map["password"]);

  RelayServerConfig relay_config(RELAY_GTURN);
  if (!relay_udp_port.empty()) {
    talk_base::SocketAddress address(relay_ip, atoi(relay_udp_port.c_str()));
    relay_config.ports.push_back(ProtocolAddress(address, PROTO_UDP));
  }
  if (!relay_tcp_port.empty()) {
    talk_base::SocketAddress address(relay_ip, atoi(relay_tcp_port.c_str()));
    relay_config.ports.push_back(ProtocolAddress(address, PROTO_TCP));
  }
  if (!relay_ssltcp_port.empty()) {
    talk_base::SocketAddress address(relay_ip, atoi(relay_ssltcp_port.c_str()));
    relay_config.ports.push_back(ProtocolAddress(address, PROTO_SSLTCP));
  }
  config->AddRelay(relay_config);
  ConfigReady(config);
}

// HttpPortAllocator

HttpPortAllocator::HttpPortAllocator(
    talk_base::NetworkManager* network_manager,
    talk_base::PacketSocketFactory* socket_factory,
    const std::string &user_agent)
    : HttpPortAllocatorBase(network_manager, socket_factory, user_agent) {
}

HttpPortAllocator::HttpPortAllocator(
    talk_base::NetworkManager* network_manager,
    const std::string &user_agent)
    : HttpPortAllocatorBase(network_manager, user_agent) {
}
HttpPortAllocator::~HttpPortAllocator() {}

PortAllocatorSession* HttpPortAllocator::CreateSessionInternal(
    const std::string& content_name,
    int component,
    const std::string& ice_ufrag, const std::string& ice_pwd) {
  return new HttpPortAllocatorSession(this, content_name, component,
                                      ice_ufrag, ice_pwd, stun_hosts(),
                                      relay_hosts(), relay_token(),
                                      user_agent());
}

// HttpPortAllocatorSession

HttpPortAllocatorSession::HttpPortAllocatorSession(
    HttpPortAllocator* allocator,
    const std::string& content_name,
    int component,
    const std::string& ice_ufrag,
    const std::string& ice_pwd,
    const std::vector<talk_base::SocketAddress>& stun_hosts,
    const std::vector<std::string>& relay_hosts,
    const std::string& relay,
    const std::string& agent)
    : HttpPortAllocatorSessionBase(allocator, content_name, component,
                                   ice_ufrag, ice_pwd, stun_hosts,
                                   relay_hosts, relay, agent) {
}

HttpPortAllocatorSession::~HttpPortAllocatorSession() {
  for (std::list<talk_base::AsyncHttpRequest*>::iterator it = requests_.begin();
       it != requests_.end(); ++it) {
    (*it)->Destroy(true);
  }
}

void HttpPortAllocatorSession::SendSessionRequest(const std::string& host,
                                                  int port) {
  // Initiate an HTTP request to create a session through the chosen host.
  talk_base::AsyncHttpRequest* request =
      new talk_base::AsyncHttpRequest(user_agent());
  request->SignalWorkDone.connect(this,
      &HttpPortAllocatorSession::OnRequestDone);

  request->set_secure(port == talk_base::HTTP_SECURE_PORT);
  request->set_proxy(allocator()->proxy());
  request->response().document.reset(new talk_base::MemoryStream);
  request->request().verb = talk_base::HV_GET;
  request->request().path = GetSessionRequestUrl();
  request->request().addHeader("X-Talk-Google-Relay-Auth", relay_token(), true);
  request->request().addHeader("X-Stream-Type", "video_rtp", true);
  request->set_host(host);
  request->set_port(port);
  request->Start();
  request->Release();

  requests_.push_back(request);
}

void HttpPortAllocatorSession::OnRequestDone(talk_base::SignalThread* data) {
  talk_base::AsyncHttpRequest* request =
      static_cast<talk_base::AsyncHttpRequest*>(data);

  // Remove the request from the list of active requests.
  std::list<talk_base::AsyncHttpRequest*>::iterator it =
      std::find(requests_.begin(), requests_.end(), request);
  if (it != requests_.end()) {
    requests_.erase(it);
  }

  if (request->response().scode != 200) {
    LOG(LS_WARNING) << "HTTPPortAllocator: request "
                    << " received error " << request->response().scode;
    TryCreateRelaySession();
    return;
  }
  LOG(LS_INFO) << "HTTPPortAllocator: request succeeded";

  talk_base::MemoryStream* stream =
      static_cast<talk_base::MemoryStream*>(request->response().document.get());
  stream->Rewind();
  size_t length;
  stream->GetSize(&length);
  std::string resp = std::string(stream->GetBuffer(), length);
  ReceiveSessionResponse(resp);
}

}  // namespace cricket
