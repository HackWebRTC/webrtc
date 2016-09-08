/*
 *  Copyright 2007 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */


#include "webrtc/base/macsocketserver.h"

#include "webrtc/base/common.h"
#include "webrtc/base/logging.h"
#include "webrtc/base/macasyncsocket.h"
#include "webrtc/base/macutils.h"
#include "webrtc/base/thread.h"

namespace rtc {

///////////////////////////////////////////////////////////////////////////////
// MacBaseSocketServer
///////////////////////////////////////////////////////////////////////////////

MacBaseSocketServer::MacBaseSocketServer() {
}

MacBaseSocketServer::~MacBaseSocketServer() {
}

Socket* MacBaseSocketServer::CreateSocket(int type) {
  return NULL;
}

Socket* MacBaseSocketServer::CreateSocket(int family, int type) {
  return NULL;
}

AsyncSocket* MacBaseSocketServer::CreateAsyncSocket(int type) {
  return CreateAsyncSocket(AF_INET, type);
}

AsyncSocket* MacBaseSocketServer::CreateAsyncSocket(int family, int type) {
  if (SOCK_STREAM != type)
    return NULL;

  MacAsyncSocket* socket = new MacAsyncSocket(this, family);
  if (!socket->valid()) {
    delete socket;
    return NULL;
  }
  return socket;
}

void MacBaseSocketServer::RegisterSocket(MacAsyncSocket* s) {
  sockets_.insert(s);
}

void MacBaseSocketServer::UnregisterSocket(MacAsyncSocket* s) {
  VERIFY(1 == sockets_.erase(s));   // found 1
}

bool MacBaseSocketServer::SetPosixSignalHandler(int signum,
                                                void (*handler)(int)) {
  Dispatcher* dispatcher = signal_dispatcher();
  if (!PhysicalSocketServer::SetPosixSignalHandler(signum, handler)) {
    return false;
  }

  // Only register the FD once, when the first custom handler is installed.
  if (!dispatcher && (dispatcher = signal_dispatcher())) {
    CFFileDescriptorContext ctx = { 0 };
    ctx.info = this;

    CFFileDescriptorRef desc = CFFileDescriptorCreate(
        kCFAllocatorDefault,
        dispatcher->GetDescriptor(),
        false,
        &MacBaseSocketServer::FileDescriptorCallback,
        &ctx);
    if (!desc) {
      return false;
    }

    CFFileDescriptorEnableCallBacks(desc, kCFFileDescriptorReadCallBack);
    CFRunLoopSourceRef ref =
        CFFileDescriptorCreateRunLoopSource(kCFAllocatorDefault, desc, 0);

    if (!ref) {
      CFRelease(desc);
      return false;
    }

    CFRunLoopAddSource(CFRunLoopGetCurrent(), ref, kCFRunLoopCommonModes);
    CFRelease(desc);
    CFRelease(ref);
  }

  return true;
}

// Used to disable socket events from waking our message queue when
// process_io is false.  Does not disable signal event handling though.
void MacBaseSocketServer::EnableSocketCallbacks(bool enable) {
  for (std::set<MacAsyncSocket*>::iterator it = sockets().begin();
       it != sockets().end(); ++it) {
    if (enable) {
      (*it)->EnableCallbacks();
    } else {
      (*it)->DisableCallbacks();
    }
  }
}

void MacBaseSocketServer::FileDescriptorCallback(CFFileDescriptorRef fd,
                                                 CFOptionFlags flags,
                                                 void* context) {
  MacBaseSocketServer* this_ss =
      reinterpret_cast<MacBaseSocketServer*>(context);
  ASSERT(this_ss);
  Dispatcher* signal_dispatcher = this_ss->signal_dispatcher();
  ASSERT(signal_dispatcher);

  signal_dispatcher->OnPreEvent(DE_READ);
  signal_dispatcher->OnEvent(DE_READ, 0);
  CFFileDescriptorEnableCallBacks(fd, kCFFileDescriptorReadCallBack);
}


///////////////////////////////////////////////////////////////////////////////
// MacCFSocketServer
///////////////////////////////////////////////////////////////////////////////

void WakeUpCallback(void* info) {
  MacCFSocketServer* server = static_cast<MacCFSocketServer*>(info);
  ASSERT(NULL != server);
  server->OnWakeUpCallback();
}

MacCFSocketServer::MacCFSocketServer()
    : run_loop_(CFRunLoopGetCurrent()),
      wake_up_(NULL) {
  CFRunLoopSourceContext ctx;
  memset(&ctx, 0, sizeof(ctx));
  ctx.info = this;
  ctx.perform = &WakeUpCallback;
  wake_up_ = CFRunLoopSourceCreate(NULL, 0, &ctx);
  ASSERT(NULL != wake_up_);
  if (wake_up_) {
    CFRunLoopAddSource(run_loop_, wake_up_, kCFRunLoopCommonModes);
  }
}

MacCFSocketServer::~MacCFSocketServer() {
  if (wake_up_) {
    CFRunLoopSourceInvalidate(wake_up_);
    CFRelease(wake_up_);
  }
}

bool MacCFSocketServer::Wait(int cms, bool process_io) {
  ASSERT(CFRunLoopGetCurrent() == run_loop_);

  if (!process_io && cms == 0) {
    // No op.
    return true;
  }

  if (!process_io) {
    // No way to listen to common modes and not get socket events, unless
    // we disable each one's callbacks.
    EnableSocketCallbacks(false);
  }

  SInt32 result;
  if (kForever == cms) {
    do {
      // Would prefer to run in a custom mode that only listens to wake_up,
      // but we have qtkit sending work to the main thread which is effectively
      // blocked here, causing deadlock.  Thus listen to the common modes.
      // TODO: If QTKit becomes thread safe, do the above.
      result = CFRunLoopRunInMode(kCFRunLoopDefaultMode, 10000000, false);
    } while (result != kCFRunLoopRunFinished && result != kCFRunLoopRunStopped);
  } else {
    // TODO: In the case of 0ms wait, this will only process one event, so we
    // may want to loop until it returns TimedOut.
    CFTimeInterval seconds = cms / 1000.0;
    result = CFRunLoopRunInMode(kCFRunLoopDefaultMode, seconds, false);
  }

  if (!process_io) {
    // Reenable them.  Hopefully this won't cause spurious callbacks or
    // missing ones while they were disabled.
    EnableSocketCallbacks(true);
  }

  if (kCFRunLoopRunFinished == result) {
    return false;
  }
  return true;
}

void MacCFSocketServer::WakeUp() {
  if (wake_up_) {
    CFRunLoopSourceSignal(wake_up_);
    CFRunLoopWakeUp(run_loop_);
  }
}

void MacCFSocketServer::OnWakeUpCallback() {
  ASSERT(run_loop_ == CFRunLoopGetCurrent());
  CFRunLoopStop(run_loop_);
}
} // namespace rtc
