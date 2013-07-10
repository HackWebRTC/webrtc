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

#ifndef TALK_EXAMPLES_CALL_CONSOLE_H_
#define TALK_EXAMPLES_CALL_CONSOLE_H_

#include <cstdio>

#include "talk/base/thread.h"
#include "talk/base/messagequeue.h"
#include "talk/base/scoped_ptr.h"

class CallClient;

class Console : public talk_base::MessageHandler {
 public:
  Console(talk_base::Thread *thread, CallClient *client);
  ~Console();

  // Starts reading lines from the console and giving them to the CallClient.
  void Start();
  // Stops reading lines. Cannot be restarted.
  void Stop();

  virtual void OnMessage(talk_base::Message *msg);

  void PrintLine(const char* format, ...);

  static void SetEcho(bool on);

 private:
  enum {
    MSG_START,
    MSG_INPUT,
  };

  void RunConsole();
  void ParseLine(std::string &str);

  CallClient *client_;
  talk_base::Thread *client_thread_;
  talk_base::scoped_ptr<talk_base::Thread> console_thread_;
};

#endif // TALK_EXAMPLES_CALL_CONSOLE_H_
