/*
 * libjingle
 * Copyright 2012, Google Inc
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
#import "talk/base/maccocoasocketserver.h"

#import <Foundation/Foundation.h>
#import <AppKit/AppKit.h>
#include <assert.h>

#include "talk/base/scoped_autorelease_pool.h"

// MacCocoaSocketServerHelper serves as a delegate to NSMachPort or a target for
// a timeout.
@interface MacCocoaSocketServerHelper : NSObject {
  // This is a weak reference. This works fine since the
  // talk_base::MacCocoaSocketServer owns this object.
  talk_base::MacCocoaSocketServer* socketServer_;  // Weak.
}
@end

@implementation MacCocoaSocketServerHelper
- (id)initWithSocketServer:(talk_base::MacCocoaSocketServer*)ss {
  self = [super init];
  if (self) {
    socketServer_ = ss;
  }
  return self;
}

- (void)timerFired:(NSTimer*)timer {
  socketServer_->WakeUp();
}

- (void)breakMainloop {
  [NSApp stop:self];
  // NSApp stop only exits after finishing processing of the
  // current event.  Since we're potentially in a timer callback
  // and not an NSEvent handler, we need to trigger a dummy one
  // and turn the loop over.  We may be able to skip this if we're
  // on the ss' thread and not inside the app loop already.
  NSEvent* event = [NSEvent otherEventWithType:NSApplicationDefined
                                      location:NSMakePoint(0,0)
                                 modifierFlags:0
                                     timestamp:0
                                  windowNumber:0
                                       context:nil
                                       subtype:0
                                         data1:0
                                         data2:0];
  [NSApp postEvent:event atStart:NO];
}
@end

namespace talk_base {

MacCocoaSocketServer::MacCocoaSocketServer() {
  helper_ = [[MacCocoaSocketServerHelper alloc] initWithSocketServer:this];
  timer_ = nil;
  run_count_ = 0;

  // Initialize the shared NSApplication
  [NSApplication sharedApplication];
}

MacCocoaSocketServer::~MacCocoaSocketServer() {
  [timer_ invalidate];
  [timer_ release];
  [helper_ release];
}

// ::Wait is reentrant, for example when blocking on another thread while
// responding to I/O. Calls to [NSApp] MUST be made from the main thread
// only!
bool MacCocoaSocketServer::Wait(int cms, bool process_io) {
  talk_base::ScopedAutoreleasePool pool;
  if (!process_io && cms == 0) {
    // No op.
    return true;
  }
  if ([NSApp isRunning]) {
    // Only allow reentrant waiting if we're in a blocking send.
    ASSERT(!process_io && cms == kForever);
  }

  if (!process_io) {
    // No way to listen to common modes and not get socket events, unless
    // we disable each one's callbacks.
    EnableSocketCallbacks(false);
  }

  if (kForever != cms) {
    // Install a timer that fires wakeup after cms has elapsed.
    timer_ =
        [NSTimer scheduledTimerWithTimeInterval:cms / 1000.0
                                         target:helper_
                                       selector:@selector(timerFired:)
                                       userInfo:nil
                                        repeats:NO];
    [timer_ retain];
  }

  // Run until WakeUp is called, which will call stop and exit this loop.
  run_count_++;
  [NSApp run];
  run_count_--;

  if (!process_io) {
    // Reenable them.  Hopefully this won't cause spurious callbacks or
    // missing ones while they were disabled.
    EnableSocketCallbacks(true);
  }

  return true;
}

// Can be called from any thread.  Post a message back to the main thread to
// break out of the NSApp loop.
void MacCocoaSocketServer::WakeUp() {
  if (timer_ != nil) {
    [timer_ invalidate];
    [timer_ release];
    timer_ = nil;
  }

  // [NSApp isRunning] returns unexpected results when called from another
  // thread.  Maintain our own count of how many times to break the main loop.
  if (run_count_ > 0) {
    [helper_ performSelectorOnMainThread:@selector(breakMainloop)
                              withObject:nil
                           waitUntilDone:false];
  }
}

}  // namespace talk_base
