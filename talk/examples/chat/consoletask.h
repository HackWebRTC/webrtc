/*
 * libjingle
 * Copyright 2004--2013, Google Inc.
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

#ifndef TALK_EXAMPLES_CHAT_CONSOLETASK_H_
#define TALK_EXAMPLES_CHAT_CONSOLETASK_H_

#include <cstdio>

#include "talk/base/thread.h"
#include "talk/base/sigslot.h"

namespace buzz {

//
// Provides properly threaded console I/O.
//
class ConsoleTask : public talk_base::MessageHandler {
 public:
  // Arguments:
  // thread The main application thread.  Input messages get posted through
  //   this.
  explicit ConsoleTask(talk_base::Thread *thread);

  // Shuts down the thread associated with this task.
  ~ConsoleTask();

  // Slot for text inputs handler.
  sigslot::signal1<const std::string&> TextInputHandler;

  // Starts reading lines from the console and passes them to the
  //  TextInputHandler.
  void Start();

  // Stops reading lines and shuts down the thread.  Cannot be restarted.
  void Stop();

  // Thread messages (especialy text-input messages) come in through here.
  virtual void OnMessage(talk_base::Message *msg);

  // printf() style output to the console.
  void Print(const char* format, ...);

  // Turns on/off the echo of input characters on the console.
  // Arguments:
  //   on If true turns echo on, off otherwise.
  static void SetEcho(bool on);

 private:
  /** Message IDs (for OnMessage()). */
  enum {
    MSG_START,
    MSG_INPUT,
  };

  // Starts up polling for console input
  void RunConsole();

  // The main application thread
  talk_base::Thread *client_thread_;

  // The tread associated with this console object
  talk_base::scoped_ptr<talk_base::Thread> console_thread_;
};

}  // namespace buzz

#endif  // TALK_EXAMPLES_CHAT_CONSOLETASK_H_

