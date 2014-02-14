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

#ifndef TALK_BASE_MESSAGEHANDLER_H_
#define TALK_BASE_MESSAGEHANDLER_H_

#include "talk/base/constructormagic.h"

namespace talk_base {

struct Message;

// Messages get dispatched to a MessageHandler

class MessageHandler {
 public:
  virtual ~MessageHandler();
  virtual void OnMessage(Message* msg) = 0;

 protected:
  MessageHandler() {}

 private:
  DISALLOW_COPY_AND_ASSIGN(MessageHandler);
};

// Helper class to facilitate executing a functor on a thread.
template <class ReturnT, class FunctorT>
class FunctorMessageHandler : public MessageHandler {
 public:
  explicit FunctorMessageHandler(const FunctorT& functor)
      : functor_(functor) {}
  virtual void OnMessage(Message* msg) {
    result_ = functor_();
  }
  const ReturnT& result() const { return result_; }

 private:
  FunctorT functor_;
  ReturnT result_;
};

// Specialization for ReturnT of void.
template <class FunctorT>
class FunctorMessageHandler<void, FunctorT> : public MessageHandler {
 public:
  explicit FunctorMessageHandler(const FunctorT& functor)
      : functor_(functor) {}
  virtual void OnMessage(Message* msg) {
    functor_();
  }
  void result() const {}

 private:
  FunctorT functor_;
};


} // namespace talk_base

#endif // TALK_BASE_MESSAGEHANDLER_H_
