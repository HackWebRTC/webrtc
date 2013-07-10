// Copyright 2007, Google Inc.


#ifndef TALK_BASE_MACSOCKETSERVER_H__
#define TALK_BASE_MACSOCKETSERVER_H__

#include <set>
#ifdef OSX // Invalid on IOS
#include <Carbon/Carbon.h>
#endif
#include "talk/base/physicalsocketserver.h"

namespace talk_base {

///////////////////////////////////////////////////////////////////////////////
// MacBaseSocketServer
///////////////////////////////////////////////////////////////////////////////
class MacAsyncSocket;

class MacBaseSocketServer : public PhysicalSocketServer {
 public:
  MacBaseSocketServer();
  virtual ~MacBaseSocketServer();

  // SocketServer Interface
  virtual Socket* CreateSocket(int type) { return NULL; }
  virtual Socket* CreateSocket(int family, int type) { return NULL; }

  virtual AsyncSocket* CreateAsyncSocket(int type);
  virtual AsyncSocket* CreateAsyncSocket(int family, int type);

  virtual bool Wait(int cms, bool process_io) = 0;
  virtual void WakeUp() = 0;

  void RegisterSocket(MacAsyncSocket* socket);
  void UnregisterSocket(MacAsyncSocket* socket);

  // PhysicalSocketServer Overrides
  virtual bool SetPosixSignalHandler(int signum, void (*handler)(int));

 protected:
  void EnableSocketCallbacks(bool enable);
  const std::set<MacAsyncSocket*>& sockets() {
    return sockets_;
  }

 private:
  static void FileDescriptorCallback(CFFileDescriptorRef ref,
                                     CFOptionFlags flags,
                                     void* context);

  std::set<MacAsyncSocket*> sockets_;
};

// Core Foundation implementation of the socket server. While idle it
// will run the current CF run loop. When the socket server has work
// to do the run loop will be paused. Does not support Carbon or Cocoa
// UI interaction.
class MacCFSocketServer : public MacBaseSocketServer {
 public:
  MacCFSocketServer();
  virtual ~MacCFSocketServer();

  // SocketServer Interface
  virtual bool Wait(int cms, bool process_io);
  virtual void WakeUp();
  void OnWakeUpCallback();

 private:
  CFRunLoopRef run_loop_;
  CFRunLoopSourceRef wake_up_;
};

#ifndef CARBON_DEPRECATED

///////////////////////////////////////////////////////////////////////////////
// MacCarbonSocketServer
///////////////////////////////////////////////////////////////////////////////

// Interacts with the Carbon event queue. While idle it will block,
// waiting for events. When the socket server has work to do, it will
// post a 'wake up' event to the queue, causing the thread to exit the
// event loop until the next call to Wait. Other events are dispatched
// to their target. Supports Carbon and Cocoa UI interaction.
class MacCarbonSocketServer : public MacBaseSocketServer {
 public:
  MacCarbonSocketServer();
  virtual ~MacCarbonSocketServer();

  // SocketServer Interface
  virtual bool Wait(int cms, bool process_io);
  virtual void WakeUp();

 private:
  EventQueueRef event_queue_;
  EventRef wake_up_;
};

///////////////////////////////////////////////////////////////////////////////
// MacCarbonAppSocketServer
///////////////////////////////////////////////////////////////////////////////

// Runs the Carbon application event loop on the current thread while
// idle. When the socket server has work to do, it will post an event
// to the queue, causing the thread to exit the event loop until the
// next call to Wait. Other events are automatically dispatched to
// their target.
class MacCarbonAppSocketServer : public MacBaseSocketServer {
 public:
  MacCarbonAppSocketServer();
  virtual ~MacCarbonAppSocketServer();

  // SocketServer Interface
  virtual bool Wait(int cms, bool process_io);
  virtual void WakeUp();

 private:
  static OSStatus WakeUpEventHandler(EventHandlerCallRef next, EventRef event,
                                     void *data);
  static void TimerHandler(EventLoopTimerRef timer, void *data);

  EventQueueRef event_queue_;
  EventHandlerRef event_handler_;
  EventLoopTimerRef timer_;
};

#endif
} // namespace talk_base

#endif  // TALK_BASE_MACSOCKETSERVER_H__
