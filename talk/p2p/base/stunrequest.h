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

#ifndef TALK_P2P_BASE_STUNREQUEST_H_
#define TALK_P2P_BASE_STUNREQUEST_H_

#include "talk/base/sigslot.h"
#include "talk/base/thread.h"
#include "talk/p2p/base/stun.h"
#include <map>
#include <string>

namespace cricket {

class StunRequest;

// Manages a set of STUN requests, sending and resending until we receive a
// response or determine that the request has timed out.
class StunRequestManager {
public:
  StunRequestManager(talk_base::Thread* thread);
  ~StunRequestManager();

  // Starts sending the given request (perhaps after a delay).
  void Send(StunRequest* request);
  void SendDelayed(StunRequest* request, int delay);

  // Removes a stun request that was added previously.  This will happen
  // automatically when a request succeeds, fails, or times out.
  void Remove(StunRequest* request);

  // Removes all stun requests that were added previously.
  void Clear();

  // Determines whether the given message is a response to one of the
  // outstanding requests, and if so, processes it appropriately.
  bool CheckResponse(StunMessage* msg);
  bool CheckResponse(const char* data, size_t size);

  bool empty() { return requests_.empty(); }

  // Raised when there are bytes to be sent.
  sigslot::signal3<const void*, size_t, StunRequest*> SignalSendPacket;

private:
  typedef std::map<std::string, StunRequest*> RequestMap;

  talk_base::Thread* thread_;
  RequestMap requests_;

  friend class StunRequest;
};

// Represents an individual request to be sent.  The STUN message can either be
// constructed beforehand or built on demand.
class StunRequest : public talk_base::MessageHandler {
public:
  StunRequest();
  StunRequest(StunMessage* request);
  virtual ~StunRequest();

  // Causes our wrapped StunMessage to be Prepared
  void Construct();

  // The manager handling this request (if it has been scheduled for sending).
  StunRequestManager* manager() { return manager_; }

  // Returns the transaction ID of this request.
  const std::string& id() { return msg_->transaction_id(); }

  // Returns the STUN type of the request message.
  int type();

  // Returns a const pointer to |msg_|.
  const StunMessage* msg() const;

  // Time elapsed since last send (in ms)
  uint32 Elapsed() const;

protected:
  int count_;
  bool timeout_;

  // Fills in a request object to be sent.  Note that request's transaction ID
  // will already be set and cannot be changed.
  virtual void Prepare(StunMessage* request) {}

  // Called when the message receives a response or times out.
  virtual void OnResponse(StunMessage* response) {}
  virtual void OnErrorResponse(StunMessage* response) {}
  virtual void OnTimeout() {}
  virtual int GetNextDelay();

private:
  void set_manager(StunRequestManager* manager);

  // Handles messages for sending and timeout.
  void OnMessage(talk_base::Message* pmsg);

  StunRequestManager* manager_;
  StunMessage* msg_;
  uint32 tstamp_;

  friend class StunRequestManager;
};

}  // namespace cricket

#endif  // TALK_P2P_BASE_STUNREQUEST_H_
