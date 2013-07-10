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

#ifndef TALK_BASE_NATSOCKETFACTORY_H_
#define TALK_BASE_NATSOCKETFACTORY_H_

#include <string>
#include <map>
#include <set>

#include "talk/base/natserver.h"
#include "talk/base/socketaddress.h"
#include "talk/base/socketserver.h"

namespace talk_base {

const size_t kNATEncodedIPv4AddressSize = 8U;
const size_t kNATEncodedIPv6AddressSize = 20U;

// Used by the NAT socket implementation.
class NATInternalSocketFactory {
 public:
  virtual ~NATInternalSocketFactory() {}
  virtual AsyncSocket* CreateInternalSocket(int family, int type,
      const SocketAddress& local_addr, SocketAddress* nat_addr) = 0;
};

// Creates sockets that will send all traffic through a NAT, using an existing
// NATServer instance running at nat_addr. The actual data is sent using sockets
// from a socket factory, given to the constructor.
class NATSocketFactory : public SocketFactory, public NATInternalSocketFactory {
 public:
  NATSocketFactory(SocketFactory* factory, const SocketAddress& nat_addr);

  // SocketFactory implementation
  virtual Socket* CreateSocket(int type);
  virtual Socket* CreateSocket(int family, int type);
  virtual AsyncSocket* CreateAsyncSocket(int type);
  virtual AsyncSocket* CreateAsyncSocket(int family, int type);

  // NATInternalSocketFactory implementation
  virtual AsyncSocket* CreateInternalSocket(int family, int type,
      const SocketAddress& local_addr, SocketAddress* nat_addr);

 private:
  SocketFactory* factory_;
  SocketAddress nat_addr_;
  DISALLOW_EVIL_CONSTRUCTORS(NATSocketFactory);
};

// Creates sockets that will send traffic through a NAT depending on what
// address they bind to. This can be used to simulate a client on a NAT sending
// to a client that is not behind a NAT.
// Note that the internal addresses of clients must be unique. This is because
// there is only one socketserver per thread, and the Bind() address is used to
// figure out which NAT (if any) the socket should talk to.
//
// Example with 3 NATs (2 cascaded), and 3 clients.
// ss->AddTranslator("1.2.3.4", "192.168.0.1", NAT_ADDR_RESTRICTED);
// ss->AddTranslator("99.99.99.99", "10.0.0.1", NAT_SYMMETRIC)->
//     AddTranslator("10.0.0.2", "192.168.1.1", NAT_OPEN_CONE);
// ss->GetTranslator("1.2.3.4")->AddClient("1.2.3.4", "192.168.0.2");
// ss->GetTranslator("99.99.99.99")->AddClient("10.0.0.3");
// ss->GetTranslator("99.99.99.99")->GetTranslator("10.0.0.2")->
//     AddClient("192.168.1.2");
class NATSocketServer : public SocketServer, public NATInternalSocketFactory {
 public:
  class Translator;
  // holds a list of NATs
  class TranslatorMap : private std::map<SocketAddress, Translator*> {
   public:
    ~TranslatorMap();
    Translator* Get(const SocketAddress& ext_ip);
    Translator* Add(const SocketAddress& ext_ip, Translator*);
    void Remove(const SocketAddress& ext_ip);
    Translator* FindClient(const SocketAddress& int_ip);
  };

  // a specific NAT
  class Translator {
   public:
    Translator(NATSocketServer* server, NATType type,
               const SocketAddress& int_addr, SocketFactory* ext_factory,
               const SocketAddress& ext_addr);

    SocketFactory* internal_factory() { return internal_factory_.get(); }
    SocketAddress internal_address() const {
      return nat_server_->internal_address();
    }
    SocketAddress internal_tcp_address() const {
      return SocketAddress();  // nat_server_->internal_tcp_address();
    }

    Translator* GetTranslator(const SocketAddress& ext_ip);
    Translator* AddTranslator(const SocketAddress& ext_ip,
                              const SocketAddress& int_ip, NATType type);
    void RemoveTranslator(const SocketAddress& ext_ip);

    bool AddClient(const SocketAddress& int_ip);
    void RemoveClient(const SocketAddress& int_ip);

    // Looks for the specified client in this or a child NAT.
    Translator* FindClient(const SocketAddress& int_ip);

   private:
    NATSocketServer* server_;
    scoped_ptr<SocketFactory> internal_factory_;
    scoped_ptr<NATServer> nat_server_;
    TranslatorMap nats_;
    std::set<SocketAddress> clients_;
  };

  explicit NATSocketServer(SocketServer* ss);

  SocketServer* socketserver() { return server_; }
  MessageQueue* queue() { return msg_queue_; }

  Translator* GetTranslator(const SocketAddress& ext_ip);
  Translator* AddTranslator(const SocketAddress& ext_ip,
                            const SocketAddress& int_ip, NATType type);
  void RemoveTranslator(const SocketAddress& ext_ip);

  // SocketServer implementation
  virtual Socket* CreateSocket(int type);
  virtual Socket* CreateSocket(int family, int type);

  virtual AsyncSocket* CreateAsyncSocket(int type);
  virtual AsyncSocket* CreateAsyncSocket(int family, int type);

  virtual void SetMessageQueue(MessageQueue* queue) {
    msg_queue_ = queue;
    server_->SetMessageQueue(queue);
  }
  virtual bool Wait(int cms, bool process_io) {
    return server_->Wait(cms, process_io);
  }
  virtual void WakeUp() {
    server_->WakeUp();
  }

  // NATInternalSocketFactory implementation
  virtual AsyncSocket* CreateInternalSocket(int family, int type,
      const SocketAddress& local_addr, SocketAddress* nat_addr);

 private:
  SocketServer* server_;
  MessageQueue* msg_queue_;
  TranslatorMap nats_;
  DISALLOW_EVIL_CONSTRUCTORS(NATSocketServer);
};

// Free-standing NAT helper functions.
size_t PackAddressForNAT(char* buf, size_t buf_size,
                         const SocketAddress& remote_addr);
size_t UnpackAddressFromNAT(const char* buf, size_t buf_size,
                            SocketAddress* remote_addr);
}  // namespace talk_base

#endif  // TALK_BASE_NATSOCKETFACTORY_H_
