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

#include <algorithm>

#include "talk/base/httpcommon-inl.h"

#include "talk/base/asyncsocket.h"
#include "talk/base/common.h"
#include "talk/base/httpserver.h"
#include "talk/base/logging.h"
#include "talk/base/socketstream.h"
#include "talk/base/thread.h"

namespace talk_base {

///////////////////////////////////////////////////////////////////////////////
// HttpServer
///////////////////////////////////////////////////////////////////////////////

HttpServer::HttpServer() : next_connection_id_(1), closing_(false) {
}

HttpServer::~HttpServer() {
  if (closing_) {
    LOG(LS_WARNING) << "HttpServer::CloseAll has not completed";
  }
  for (ConnectionMap::iterator it = connections_.begin();
       it != connections_.end();
       ++it) {
    StreamInterface* stream = it->second->EndProcess();
    delete stream;
    delete it->second;
  }
}

int
HttpServer::HandleConnection(StreamInterface* stream) {
  int connection_id = next_connection_id_++;
  ASSERT(connection_id != HTTP_INVALID_CONNECTION_ID);
  Connection* connection = new Connection(connection_id, this);
  connections_.insert(ConnectionMap::value_type(connection_id, connection));
  connection->BeginProcess(stream);
  return connection_id;
}

void
HttpServer::Respond(HttpServerTransaction* transaction) {
  int connection_id = transaction->connection_id();
  if (Connection* connection = Find(connection_id)) {
    connection->Respond(transaction);
  } else {
    delete transaction;
    // We may be tempted to SignalHttpComplete, but that implies that a
    // connection still exists.
  }
}

void
HttpServer::Close(int connection_id, bool force) {
  if (Connection* connection = Find(connection_id)) {
    connection->InitiateClose(force);
  }
}

void
HttpServer::CloseAll(bool force) {
  if (connections_.empty()) {
    SignalCloseAllComplete(this);
    return;
  }
  closing_ = true;
  std::list<Connection*> connections;
  for (ConnectionMap::const_iterator it = connections_.begin();
       it != connections_.end(); ++it) {
    connections.push_back(it->second);
  }
  for (std::list<Connection*>::const_iterator it = connections.begin();
      it != connections.end(); ++it) {
    (*it)->InitiateClose(force);
  }
}

HttpServer::Connection*
HttpServer::Find(int connection_id) {
  ConnectionMap::iterator it = connections_.find(connection_id);
  if (it == connections_.end())
    return NULL;
  return it->second;
}

void
HttpServer::Remove(int connection_id) {
  ConnectionMap::iterator it = connections_.find(connection_id);
  if (it == connections_.end()) {
    ASSERT(false);
    return;
  }
  Connection* connection = it->second;
  connections_.erase(it);
  SignalConnectionClosed(this, connection_id, connection->EndProcess());
  delete connection;
  if (closing_ && connections_.empty()) {
    closing_ = false;
    SignalCloseAllComplete(this);
  }
}

///////////////////////////////////////////////////////////////////////////////
// HttpServer::Connection
///////////////////////////////////////////////////////////////////////////////

HttpServer::Connection::Connection(int connection_id, HttpServer* server)
  : connection_id_(connection_id), server_(server),
    current_(NULL), signalling_(false), close_(false) {
}

HttpServer::Connection::~Connection() {
  // It's possible that an object hosted inside this transaction signalled
  // an event which caused the connection to close.
  Thread::Current()->Dispose(current_);
}

void
HttpServer::Connection::BeginProcess(StreamInterface* stream) {
  base_.notify(this);
  base_.attach(stream);
  current_ = new HttpServerTransaction(connection_id_);
  if (base_.mode() != HM_CONNECT)
    base_.recv(&current_->request);
}

StreamInterface*
HttpServer::Connection::EndProcess() {
  base_.notify(NULL);
  base_.abort(HE_DISCONNECTED);
  return base_.detach();
}

void
HttpServer::Connection::Respond(HttpServerTransaction* transaction) {
  ASSERT(current_ == NULL);
  current_ = transaction;
  if (current_->response.begin() == current_->response.end()) {
    current_->response.set_error(HC_INTERNAL_SERVER_ERROR);
  }
  bool keep_alive = HttpShouldKeepAlive(current_->request);
  current_->response.setHeader(HH_CONNECTION,
                               keep_alive ? "Keep-Alive" : "Close",
                               false);
  close_ = !HttpShouldKeepAlive(current_->response);
  base_.send(&current_->response);
}

void
HttpServer::Connection::InitiateClose(bool force) {
  bool request_in_progress = (HM_SEND == base_.mode()) || (NULL == current_);
  if (!signalling_ && (force || !request_in_progress)) {
    server_->Remove(connection_id_);
  } else {
    close_ = true;
  }
}

//
// IHttpNotify Implementation
//

HttpError
HttpServer::Connection::onHttpHeaderComplete(bool chunked, size_t& data_size) {
  if (data_size == SIZE_UNKNOWN) {
    data_size = 0;
  }
  ASSERT(current_ != NULL);
  bool custom_document = false;
  server_->SignalHttpRequestHeader(server_, current_, &custom_document);
  if (!custom_document) {
    current_->request.document.reset(new MemoryStream);
  }
  return HE_NONE;
}

void
HttpServer::Connection::onHttpComplete(HttpMode mode, HttpError err) {
  if (mode == HM_SEND) {
    ASSERT(current_ != NULL);
    signalling_ = true;
    server_->SignalHttpRequestComplete(server_, current_, err);
    signalling_ = false;
    if (close_) {
      // Force a close
      err = HE_DISCONNECTED;
    }
  }
  if (err != HE_NONE) {
    server_->Remove(connection_id_);
  } else if (mode == HM_CONNECT) {
    base_.recv(&current_->request);
  } else if (mode == HM_RECV) {
    ASSERT(current_ != NULL);
    // TODO: do we need this?
    //request_.document_->rewind();
    HttpServerTransaction* transaction = current_;
    current_ = NULL;
    server_->SignalHttpRequest(server_, transaction);
  } else if (mode == HM_SEND) {
    Thread::Current()->Dispose(current_->response.document.release());
    current_->request.clear(true);
    current_->response.clear(true);
    base_.recv(&current_->request);
  } else {
    ASSERT(false);
  }
}

void
HttpServer::Connection::onHttpClosed(HttpError err) {
  UNUSED(err);
  server_->Remove(connection_id_);
}

///////////////////////////////////////////////////////////////////////////////
// HttpListenServer
///////////////////////////////////////////////////////////////////////////////

HttpListenServer::HttpListenServer() {
  SignalConnectionClosed.connect(this, &HttpListenServer::OnConnectionClosed);
}

HttpListenServer::~HttpListenServer() {
}

int HttpListenServer::Listen(const SocketAddress& address) {
  AsyncSocket* sock =
      Thread::Current()->socketserver()->CreateAsyncSocket(address.family(),
                                                           SOCK_STREAM);
  if (!sock) {
    return SOCKET_ERROR;
  }
  listener_.reset(sock);
  listener_->SignalReadEvent.connect(this, &HttpListenServer::OnReadEvent);
  if ((listener_->Bind(address) != SOCKET_ERROR) &&
      (listener_->Listen(5) != SOCKET_ERROR))
    return 0;
  return listener_->GetError();
}

bool HttpListenServer::GetAddress(SocketAddress* address) const {
  if (!listener_) {
    return false;
  }
  *address = listener_->GetLocalAddress();
  return !address->IsNil();
}

void HttpListenServer::StopListening() {
  if (listener_) {
    listener_->Close();
  }
}

void HttpListenServer::OnReadEvent(AsyncSocket* socket) {
  ASSERT(socket == listener_.get());
  ASSERT(listener_);
  AsyncSocket* incoming = listener_->Accept(NULL);
  if (incoming) {
    StreamInterface* stream = new SocketStream(incoming);
    //stream = new LoggingAdapter(stream, LS_VERBOSE, "HttpServer", false);
    HandleConnection(stream);
  }
}

void HttpListenServer::OnConnectionClosed(HttpServer* server,
                                          int connection_id,
                                          StreamInterface* stream) {
  Thread::Current()->Dispose(stream);
}

///////////////////////////////////////////////////////////////////////////////

}  // namespace talk_base
