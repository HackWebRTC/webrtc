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

#include "talk/base/autodetectproxy.h"
#include "talk/base/httpcommon.h"
#include "talk/base/httpcommon-inl.h"
#include "talk/base/nethelpers.h"

namespace talk_base {

static const ProxyType TEST_ORDER[] = {
  PROXY_HTTPS, PROXY_SOCKS5, PROXY_UNKNOWN
};

static const int kSavedStringLimit = 128;

static void SaveStringToStack(char *dst,
                              const std::string &src,
                              size_t dst_size) {
  strncpy(dst, src.c_str(), dst_size - 1);
  dst[dst_size - 1] = '\0';
}

AutoDetectProxy::AutoDetectProxy(const std::string& user_agent)
    : agent_(user_agent), resolver_(NULL), socket_(NULL), next_(0) {
}

AutoDetectProxy::~AutoDetectProxy() {
  if (resolver_) {
    resolver_->Destroy(false);
  }
}

void AutoDetectProxy::DoWork() {
  // TODO: Try connecting to server_url without proxy first here?
  if (!server_url_.empty()) {
    LOG(LS_INFO) << "GetProxySettingsForUrl(" << server_url_ << ") - start";
    GetProxyForUrl(agent_.c_str(), server_url_.c_str(), &proxy_);
    LOG(LS_INFO) << "GetProxySettingsForUrl - stop";
  }
  Url<char> url(proxy_.address.HostAsURIString());
  if (url.valid()) {
    LOG(LS_WARNING) << "AutoDetectProxy removing http prefix on proxy host";
    proxy_.address.SetIP(url.host());
  }
  LOG(LS_INFO) << "AutoDetectProxy found proxy at " << proxy_.address;
  if (proxy_.type == PROXY_UNKNOWN) {
    LOG(LS_INFO) << "AutoDetectProxy initiating proxy classification";
    Next();
    // Process I/O until Stop()
    Thread::Current()->ProcessMessages(kForever);
    // Clean up the autodetect socket, from the thread that created it
    delete socket_;
  }
  // TODO: If we found a proxy, try to use it to verify that it
  // works by sending a request to server_url. This could either be
  // done here or by the HttpPortAllocator.
}

void AutoDetectProxy::OnMessage(Message *msg) {
  if (MSG_TIMEOUT == msg->message_id) {
    OnCloseEvent(socket_, ETIMEDOUT);
  } else {
    // This must be the ST_MSG_WORKER_DONE message that deletes the
    // AutoDetectProxy object. We have observed crashes within this stack that
    // seem to be highly reproducible for a small subset of users and thus are
    // probably correlated with a specific proxy setting, so copy potentially
    // relevant information onto the stack to make it available in Windows
    // minidumps.

    // Save the user agent and the number of auto-detection passes that we
    // needed.
    char agent[kSavedStringLimit];
    SaveStringToStack(agent, agent_, sizeof agent);

    int next = next_;

    // Now the detected proxy config (minus the password field, which could be
    // sensitive).
    ProxyType type = proxy().type;

    char address_hostname[kSavedStringLimit];
    SaveStringToStack(address_hostname,
                      proxy().address.hostname(),
                      sizeof address_hostname);

    IPAddress address_ip = proxy().address.ipaddr();

    uint16 address_port = proxy().address.port();

    char autoconfig_url[kSavedStringLimit];
    SaveStringToStack(autoconfig_url,
                      proxy().autoconfig_url,
                      sizeof autoconfig_url);

    bool autodetect = proxy().autodetect;

    char bypass_list[kSavedStringLimit];
    SaveStringToStack(bypass_list, proxy().bypass_list, sizeof bypass_list);

    char username[kSavedStringLimit];
    SaveStringToStack(username, proxy().username, sizeof username);

    SignalThread::OnMessage(msg);

    // Log the gathered data at a log level that will never actually be enabled
    // so that the compiler is forced to retain the data on the stack.
    LOG(LS_SENSITIVE) << agent << " " << next << " " << type << " "
                      << address_hostname << " " << address_ip << " "
                      << address_port << " " << autoconfig_url << " "
                      << autodetect << " " << bypass_list << " " << username;
  }
}

void AutoDetectProxy::OnResolveResult(SignalThread* thread) {
  if (thread != resolver_) {
    return;
  }
  int error = resolver_->error();
  if (error == 0) {
    LOG(LS_VERBOSE) << "Resolved " << proxy_.address << " to "
                    << resolver_->address();
    proxy_.address = resolver_->address();
    DoConnect();
  } else {
    LOG(LS_INFO) << "Failed to resolve " << resolver_->address();
    resolver_->Destroy(false);
    resolver_ = NULL;
    proxy_.address = SocketAddress();
    Thread::Current()->Post(this, MSG_TIMEOUT);
  }
}

void AutoDetectProxy::Next() {
  if (TEST_ORDER[next_] >= PROXY_UNKNOWN) {
    Complete(PROXY_UNKNOWN);
    return;
  }

  LOG(LS_VERBOSE) << "AutoDetectProxy connecting to "
                  << proxy_.address.ToSensitiveString();

  if (socket_) {
    Thread::Current()->Clear(this, MSG_TIMEOUT);
    socket_->Close();
    Thread::Current()->Dispose(socket_);
    socket_ = NULL;
  }
  int timeout = 2000;
  if (proxy_.address.IsUnresolvedIP()) {
    // Launch an asyncresolver. This thread will spin waiting for it.
    timeout += 2000;
    if (!resolver_) {
      resolver_ = new AsyncResolver();
    }
    resolver_->set_address(proxy_.address);
    resolver_->SignalWorkDone.connect(this,
                                      &AutoDetectProxy::OnResolveResult);
    resolver_->Start();
  } else {
    DoConnect();
  }
  Thread::Current()->PostDelayed(timeout, this, MSG_TIMEOUT);
}

void AutoDetectProxy::DoConnect() {
  if (resolver_) {
    resolver_->Destroy(false);
    resolver_ = NULL;
  }
  socket_ =
      Thread::Current()->socketserver()->CreateAsyncSocket(
          proxy_.address.family(), SOCK_STREAM);
  if (!socket_) {
    LOG(LS_VERBOSE) << "Unable to create socket for " << proxy_.address;
    return;
  }
  socket_->SignalConnectEvent.connect(this, &AutoDetectProxy::OnConnectEvent);
  socket_->SignalReadEvent.connect(this, &AutoDetectProxy::OnReadEvent);
  socket_->SignalCloseEvent.connect(this, &AutoDetectProxy::OnCloseEvent);
  socket_->Connect(proxy_.address);
}

void AutoDetectProxy::Complete(ProxyType type) {
  Thread::Current()->Clear(this, MSG_TIMEOUT);
  if (socket_) {
    socket_->Close();
  }

  proxy_.type = type;
  LoggingSeverity sev = (proxy_.type == PROXY_UNKNOWN) ? LS_ERROR : LS_INFO;
  LOG_V(sev) << "AutoDetectProxy detected "
             << proxy_.address.ToSensitiveString()
             << " as type " << proxy_.type;

  Thread::Current()->Quit();
}

void AutoDetectProxy::OnConnectEvent(AsyncSocket * socket) {
  std::string probe;

  switch (TEST_ORDER[next_]) {
    case PROXY_HTTPS:
      probe.assign("CONNECT www.google.com:443 HTTP/1.0\r\n"
                   "User-Agent: ");
      probe.append(agent_);
      probe.append("\r\n"
                   "Host: www.google.com\r\n"
                   "Content-Length: 0\r\n"
                   "Proxy-Connection: Keep-Alive\r\n"
                   "\r\n");
      break;
    case PROXY_SOCKS5:
      probe.assign("\005\001\000", 3);
      break;
    default:
      ASSERT(false);
      return;
  }

  LOG(LS_VERBOSE) << "AutoDetectProxy probing type " << TEST_ORDER[next_]
                  << " sending " << probe.size() << " bytes";
  socket_->Send(probe.data(), probe.size());
}

void AutoDetectProxy::OnReadEvent(AsyncSocket * socket) {
  char data[257];
  int len = socket_->Recv(data, 256);
  if (len > 0) {
    data[len] = 0;
    LOG(LS_VERBOSE) << "AutoDetectProxy read " << len << " bytes";
  }

  switch (TEST_ORDER[next_]) {
    case PROXY_HTTPS:
      if ((len >= 2) && (data[0] == '\x05')) {
        Complete(PROXY_SOCKS5);
        return;
      }
      if ((len >= 5) && (strncmp(data, "HTTP/", 5) == 0)) {
        Complete(PROXY_HTTPS);
        return;
      }
      break;
    case PROXY_SOCKS5:
      if ((len >= 2) && (data[0] == '\x05')) {
        Complete(PROXY_SOCKS5);
        return;
      }
      break;
    default:
      ASSERT(false);
      return;
  }

  ++next_;
  Next();
}

void AutoDetectProxy::OnCloseEvent(AsyncSocket * socket, int error) {
  LOG(LS_VERBOSE) << "AutoDetectProxy closed with error: " << error;
  ++next_;
  Next();
}

}  // namespace talk_base
