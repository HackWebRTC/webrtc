/*
 * libjingle
 * Copyright 2007, Google Inc.
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

// A libjingle compatible SocketServer for OSX/iOS/Cocoa.

#ifndef TALK_BASE_MACCOCOASOCKETSERVER_H_
#define TALK_BASE_MACCOCOASOCKETSERVER_H_

#include "talk/base/macsocketserver.h"

#ifdef __OBJC__
@class NSTimer, MacCocoaSocketServerHelper;
#else
class NSTimer;
class MacCocoaSocketServerHelper;
#endif

namespace talk_base {

// A socketserver implementation that wraps the main cocoa
// application loop accessed through [NSApp run].
class MacCocoaSocketServer : public MacBaseSocketServer {
 public:
  explicit MacCocoaSocketServer();
  virtual ~MacCocoaSocketServer();

  virtual bool Wait(int cms, bool process_io);
  virtual void WakeUp();

 private:
  MacCocoaSocketServerHelper* helper_;
  NSTimer* timer_;  // Weak.

  DISALLOW_EVIL_CONSTRUCTORS(MacCocoaSocketServer);
};

}  // namespace talk_base

#endif  // TALK_BASE_MACCOCOASOCKETSERVER_H_
