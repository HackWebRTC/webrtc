/*
 * libjingle
 * Copyright 2004 Google Inc.
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

#ifndef TALK_BASE_THREAD_H_
#define TALK_BASE_THREAD_H_

#include <algorithm>
#include <list>
#include <string>
#include <vector>

#ifdef POSIX
#include <pthread.h>
#endif
#include "talk/base/constructormagic.h"
#include "talk/base/messagequeue.h"

#ifdef WIN32
#include "talk/base/win32.h"
#endif

namespace talk_base {

class Thread;

class ThreadManager {
 public:
  ThreadManager();
  ~ThreadManager();

  static ThreadManager* Instance();

  Thread* CurrentThread();
  void SetCurrentThread(Thread* thread);

  // Returns a thread object with its thread_ ivar set
  // to whatever the OS uses to represent the thread.
  // If there already *is* a Thread object corresponding to this thread,
  // this method will return that.  Otherwise it creates a new Thread
  // object whose wrapped() method will return true, and whose
  // handle will, on Win32, be opened with only synchronization privileges -
  // if you need more privilegs, rather than changing this method, please
  // write additional code to adjust the privileges, or call a different
  // factory method of your own devising, because this one gets used in
  // unexpected contexts (like inside browser plugins) and it would be a
  // shame to break it.  It is also conceivable on Win32 that we won't even
  // be able to get synchronization privileges, in which case the result
  // will have a NULL handle.
  Thread *WrapCurrentThread();
  void UnwrapCurrentThread();

 private:
#ifdef POSIX
  pthread_key_t key_;
#endif

#ifdef WIN32
  DWORD key_;
#endif

  DISALLOW_COPY_AND_ASSIGN(ThreadManager);
};

struct _SendMessage {
  _SendMessage() {}
  Thread *thread;
  Message msg;
  bool *ready;
};

enum ThreadPriority {
  PRIORITY_IDLE = -1,
  PRIORITY_NORMAL = 0,
  PRIORITY_ABOVE_NORMAL = 1,
  PRIORITY_HIGH = 2,
};

class Runnable {
 public:
  virtual ~Runnable() {}
  virtual void Run(Thread* thread) = 0;

 protected:
  Runnable() {}

 private:
  DISALLOW_COPY_AND_ASSIGN(Runnable);
};

// WARNING! SUBCLASSES MUST CALL Stop() IN THEIR DESTRUCTORS!  See ~Thread().

class Thread : public MessageQueue {
 public:
  explicit Thread(SocketServer* ss = NULL);
  // NOTE: ALL SUBCLASSES OF Thread MUST CALL Stop() IN THEIR DESTRUCTORS (or
  // guarantee Stop() is explicitly called before the subclass is destroyed).
  // This is required to avoid a data race between the destructor modifying the
  // vtable, and the Thread::PreRun calling the virtual method Run().
  virtual ~Thread();

  static Thread* Current();

  bool IsCurrent() const {
    return Current() == this;
  }

  // Sleeps the calling thread for the specified number of milliseconds, during
  // which time no processing is performed. Returns false if sleeping was
  // interrupted by a signal (POSIX only).
  static bool SleepMs(int millis);

  // Sets the thread's name, for debugging. Must be called before Start().
  // If |obj| is non-NULL, its value is appended to |name|.
  const std::string& name() const { return name_; }
  bool SetName(const std::string& name, const void* obj);

  // Sets the thread's priority. Must be called before Start().
  ThreadPriority priority() const { return priority_; }
  bool SetPriority(ThreadPriority priority);

  // Starts the execution of the thread.
  bool started() const { return started_; }
  bool Start(Runnable* runnable = NULL);

  // Used for fire-and-forget threads.  Deletes this thread object when the
  // Run method returns.
  void Release() {
    delete_self_when_complete_ = true;
  }

  // Tells the thread to stop and waits until it is joined.
  // Never call Stop on the current thread.  Instead use the inherited Quit
  // function which will exit the base MessageQueue without terminating the
  // underlying OS thread.
  virtual void Stop();

  // By default, Thread::Run() calls ProcessMessages(kForever).  To do other
  // work, override Run().  To receive and dispatch messages, call
  // ProcessMessages occasionally.
  virtual void Run();

  virtual void Send(MessageHandler *phandler, uint32 id = 0,
      MessageData *pdata = NULL);

  // Convenience method to invoke a functor on another thread.  Caller must
  // provide the |ReturnT| template argument, which cannot (easily) be deduced.
  // Uses Send() internally, which blocks the current thread until execution
  // is complete.
  // Ex: bool result = thread.Invoke<bool>(&MyFunctionReturningBool);
  template <class ReturnT, class FunctorT>
  ReturnT Invoke(const FunctorT& functor) {
    FunctorMessageHandler<ReturnT, FunctorT> handler(functor);
    Send(&handler);
    return handler.result();
  }

  // From MessageQueue
  virtual void Clear(MessageHandler *phandler, uint32 id = MQID_ANY,
                     MessageList* removed = NULL);
  virtual void ReceiveSends();

  // ProcessMessages will process I/O and dispatch messages until:
  //  1) cms milliseconds have elapsed (returns true)
  //  2) Stop() is called (returns false)
  bool ProcessMessages(int cms);

  // Returns true if this is a thread that we created using the standard
  // constructor, false if it was created by a call to
  // ThreadManager::WrapCurrentThread().  The main thread of an application
  // is generally not owned, since the OS representation of the thread
  // obviously exists before we can get to it.
  // You cannot call Start on non-owned threads.
  bool IsOwned();

#ifdef WIN32
  HANDLE GetHandle() const {
    return thread_;
  }
  DWORD GetId() const {
    return thread_id_;
  }
#elif POSIX
  pthread_t GetPThread() {
    return thread_;
  }
#endif

  // This method should be called when thread is created using non standard
  // method, like derived implementation of talk_base::Thread and it can not be
  // started by calling Start(). This will set started flag to true and
  // owned to false. This must be called from the current thread.
  // NOTE: These methods should be used by the derived classes only, added here
  // only for testing.
  bool WrapCurrent();
  void UnwrapCurrent();

 protected:
  // Blocks the calling thread until this thread has terminated.
  void Join();

 private:
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
    virtual void OnMessage(Message* msg) { functor_(); }
    void result() const {}
   private:
    FunctorT functor_;
  };

  static void *PreRun(void *pv);

  // ThreadManager calls this instead WrapCurrent() because
  // ThreadManager::Instance() cannot be used while ThreadManager is
  // being created.
  bool WrapCurrentWithThreadManager(ThreadManager* thread_manager);

  std::list<_SendMessage> sendlist_;
  std::string name_;
  ThreadPriority priority_;
  bool started_;

#ifdef POSIX
  pthread_t thread_;
#endif

#ifdef WIN32
  HANDLE thread_;
  DWORD thread_id_;
#endif

  bool owned_;
  bool delete_self_when_complete_;

  friend class ThreadManager;

  DISALLOW_COPY_AND_ASSIGN(Thread);
};

// AutoThread automatically installs itself at construction
// uninstalls at destruction, if a Thread object is
// _not already_ associated with the current OS thread.

class AutoThread : public Thread {
 public:
  explicit AutoThread(SocketServer* ss = 0);
  virtual ~AutoThread();

 private:
  DISALLOW_COPY_AND_ASSIGN(AutoThread);
};

// Win32 extension for threads that need to use COM
#ifdef WIN32
class ComThread : public Thread {
 public:
  ComThread() {}
  virtual ~ComThread() { Stop(); }

 protected:
  virtual void Run();

 private:
  DISALLOW_COPY_AND_ASSIGN(ComThread);
};
#endif

// Provides an easy way to install/uninstall a socketserver on a thread.
class SocketServerScope {
 public:
  explicit SocketServerScope(SocketServer* ss) {
    old_ss_ = Thread::Current()->socketserver();
    Thread::Current()->set_socketserver(ss);
  }
  ~SocketServerScope() {
    Thread::Current()->set_socketserver(old_ss_);
  }

 private:
  SocketServer* old_ss_;

  DISALLOW_IMPLICIT_CONSTRUCTORS(SocketServerScope);
};

}  // namespace talk_base

#endif  // TALK_BASE_THREAD_H_
