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

#include "talk/p2p/base/stunrequest.h"

#include "talk/base/common.h"
#include "talk/base/helpers.h"
#include "talk/base/logging.h"

namespace cricket {

const uint32 MSG_STUN_SEND = 1;

const int MAX_SENDS = 9;
const int DELAY_UNIT = 100;  // 100 milliseconds
const int DELAY_MAX_FACTOR = 16;

StunRequestManager::StunRequestManager(talk_base::Thread* thread)
    : thread_(thread) {
}

StunRequestManager::~StunRequestManager() {
  while (requests_.begin() != requests_.end()) {
    StunRequest *request = requests_.begin()->second;
    requests_.erase(requests_.begin());
    delete request;
  }
}

void StunRequestManager::Send(StunRequest* request) {
  SendDelayed(request, 0);
}

void StunRequestManager::SendDelayed(StunRequest* request, int delay) {
  request->set_manager(this);
  ASSERT(requests_.find(request->id()) == requests_.end());
  request->Construct();
  requests_[request->id()] = request;
  thread_->PostDelayed(delay, request, MSG_STUN_SEND, NULL);
}

void StunRequestManager::Remove(StunRequest* request) {
  ASSERT(request->manager() == this);
  RequestMap::iterator iter = requests_.find(request->id());
  if (iter != requests_.end()) {
    ASSERT(iter->second == request);
    requests_.erase(iter);
    thread_->Clear(request);
  }
}

void StunRequestManager::Clear() {
  std::vector<StunRequest*> requests;
  for (RequestMap::iterator i = requests_.begin(); i != requests_.end(); ++i)
    requests.push_back(i->second);

  for (uint32 i = 0; i < requests.size(); ++i) {
    // StunRequest destructor calls Remove() which deletes requests
    // from |requests_|.
    delete requests[i];
  }
}

bool StunRequestManager::CheckResponse(StunMessage* msg) {
  RequestMap::iterator iter = requests_.find(msg->transaction_id());
  if (iter == requests_.end())
    return false;

  StunRequest* request = iter->second;
  if (msg->type() == GetStunSuccessResponseType(request->type())) {
    request->OnResponse(msg);
  } else if (msg->type() == GetStunErrorResponseType(request->type())) {
    request->OnErrorResponse(msg);
  } else {
    LOG(LERROR) << "Received response with wrong type: " << msg->type()
                << " (expecting "
                << GetStunSuccessResponseType(request->type()) << ")";
    return false;
  }

  delete request;
  return true;
}

bool StunRequestManager::CheckResponse(const char* data, size_t size) {
  // Check the appropriate bytes of the stream to see if they match the
  // transaction ID of a response we are expecting.

  if (size < 20)
    return false;

  std::string id;
  id.append(data + kStunTransactionIdOffset, kStunTransactionIdLength);

  RequestMap::iterator iter = requests_.find(id);
  if (iter == requests_.end())
    return false;

  // Parse the STUN message and continue processing as usual.

  talk_base::ByteBuffer buf(data, size);
  talk_base::scoped_ptr<StunMessage> response(iter->second->msg_->CreateNew());
  if (!response->Read(&buf))
    return false;

  return CheckResponse(response.get());
}

StunRequest::StunRequest()
    : count_(0), timeout_(false), manager_(0),
      msg_(new StunMessage()), tstamp_(0) {
  msg_->SetTransactionID(
      talk_base::CreateRandomString(kStunTransactionIdLength));
}

StunRequest::StunRequest(StunMessage* request)
    : count_(0), timeout_(false), manager_(0),
      msg_(request), tstamp_(0) {
  msg_->SetTransactionID(
      talk_base::CreateRandomString(kStunTransactionIdLength));
}

StunRequest::~StunRequest() {
  ASSERT(manager_ != NULL);
  if (manager_) {
    manager_->Remove(this);
    manager_->thread_->Clear(this);
  }
  delete msg_;
}

void StunRequest::Construct() {
  if (msg_->type() == 0) {
    Prepare(msg_);
    ASSERT(msg_->type() != 0);
  }
}

int StunRequest::type() {
  ASSERT(msg_ != NULL);
  return msg_->type();
}

const StunMessage* StunRequest::msg() const {
  return msg_;
}

uint32 StunRequest::Elapsed() const {
  return talk_base::TimeSince(tstamp_);
}


void StunRequest::set_manager(StunRequestManager* manager) {
  ASSERT(!manager_);
  manager_ = manager;
}

void StunRequest::OnMessage(talk_base::Message* pmsg) {
  ASSERT(manager_ != NULL);
  ASSERT(pmsg->message_id == MSG_STUN_SEND);

  if (timeout_) {
    OnTimeout();
    delete this;
    return;
  }

  tstamp_ = talk_base::Time();

  talk_base::ByteBuffer buf;
  msg_->Write(&buf);
  manager_->SignalSendPacket(buf.Data(), buf.Length(), this);

  int delay = GetNextDelay();
  manager_->thread_->PostDelayed(delay, this, MSG_STUN_SEND, NULL);
}

int StunRequest::GetNextDelay() {
  int delay = DELAY_UNIT * talk_base::_min(1 << count_, DELAY_MAX_FACTOR);
  count_ += 1;
  if (count_ == MAX_SENDS)
    timeout_ = true;
  return delay;
}

}  // namespace cricket
