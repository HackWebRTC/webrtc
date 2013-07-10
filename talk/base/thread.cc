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

#include "talk/base/thread.h"

#ifndef __has_feature
#define __has_feature(x) 0  // Compatibility with non-clang or LLVM compilers.
#endif  // __has_feature

#if defined(WIN32)
#include <comdef.h>
#elif defined(POSIX)
#include <time.h>
#endif

#include "talk/base/common.h"
#include "talk/base/logging.h"
#include "talk/base/stringutils.h"
#include "talk/base/timeutils.h"

#if !__has_feature(objc_arc) && (defined(OSX) || defined(IOS))
#include "talk/base/maccocoathreadhelper.h"
#include "talk/base/scoped_autorelease_pool.h"
#endif

namespace talk_base {

ThreadManager* ThreadManager::Instance() {
  LIBJINGLE_DEFINE_STATIC_LOCAL(ThreadManager, thread_manager, ());
  return &thread_manager;
}

// static
Thread* Thread::Current() {
  return ThreadManager::Instance()->CurrentThread();
}

#ifdef POSIX
ThreadManager::ThreadManager() {
  pthread_key_create(&key_, NULL);
#ifndef NO_MAIN_THREAD_WRAPPING
  WrapCurrentThread();
#endif
#if !__has_feature(objc_arc) && (defined(OSX) || defined(IOS))
  // Under Automatic Reference Counting (ARC), you cannot use autorelease pools
  // directly. Instead, you use @autoreleasepool blocks instead.  Also, we are
  // maintaining thread safety using immutability within context of GCD dispatch
  // queues in this case.
  InitCocoaMultiThreading();
#endif
}

ThreadManager::~ThreadManager() {
#if __has_feature(objc_arc)
  @autoreleasepool
#elif defined(OSX) || defined(IOS)
  // This is called during exit, at which point apparently no NSAutoreleasePools
  // are available; but we might still need them to do cleanup (or we get the
  // "no autoreleasepool in place, just leaking" warning when exiting).
  ScopedAutoreleasePool pool;
#endif
  {
    UnwrapCurrentThread();
    pthread_key_delete(key_);
  }
}

Thread *ThreadManager::CurrentThread() {
  return static_cast<Thread *>(pthread_getspecific(key_));
}

void ThreadManager::SetCurrentThread(Thread *thread) {
  pthread_setspecific(key_, thread);
}
#endif

#ifdef WIN32
ThreadManager::ThreadManager() {
  key_ = TlsAlloc();
#ifndef NO_MAIN_THREAD_WRAPPING
  WrapCurrentThread();
#endif
}

ThreadManager::~ThreadManager() {
  UnwrapCurrentThread();
  TlsFree(key_);
}

Thread *ThreadManager::CurrentThread() {
  return static_cast<Thread *>(TlsGetValue(key_));
}

void ThreadManager::SetCurrentThread(Thread *thread) {
  TlsSetValue(key_, thread);
}
#endif

Thread *ThreadManager::WrapCurrentThread() {
  Thread* result = CurrentThread();
  if (NULL == result) {
    result = new Thread();
    result->WrapCurrentWithThreadManager(this);
  }
  return result;
}

void ThreadManager::UnwrapCurrentThread() {
  Thread* t = CurrentThread();
  if (t && !(t->IsOwned())) {
    t->UnwrapCurrent();
    delete t;
  }
}

struct ThreadInit {
  Thread* thread;
  Runnable* runnable;
};

Thread::Thread(SocketServer* ss)
    : MessageQueue(ss),
      priority_(PRIORITY_NORMAL),
      started_(false),
      has_sends_(false),
#if defined(WIN32)
      thread_(NULL),
      thread_id_(0),
#endif
      owned_(true),
      delete_self_when_complete_(false) {
  SetName("Thread", this);  // default name
}

Thread::~Thread() {
  Stop();
  if (active_)
    Clear(NULL);
}

bool Thread::SleepMs(int milliseconds) {
#ifdef WIN32
  ::Sleep(milliseconds);
  return true;
#else
  // POSIX has both a usleep() and a nanosleep(), but the former is deprecated,
  // so we use nanosleep() even though it has greater precision than necessary.
  struct timespec ts;
  ts.tv_sec = milliseconds / 1000;
  ts.tv_nsec = (milliseconds % 1000) * 1000000;
  int ret = nanosleep(&ts, NULL);
  if (ret != 0) {
    LOG_ERR(LS_WARNING) << "nanosleep() returning early";
    return false;
  }
  return true;
#endif
}

bool Thread::SetName(const std::string& name, const void* obj) {
  if (started_) return false;
  name_ = name;
  if (obj) {
    char buf[16];
    sprintfn(buf, sizeof(buf), " 0x%p", obj);
    name_ += buf;
  }
  return true;
}

bool Thread::SetPriority(ThreadPriority priority) {
#if defined(WIN32)
  if (started_) {
    BOOL ret = FALSE;
    if (priority == PRIORITY_NORMAL) {
      ret = ::SetThreadPriority(thread_, THREAD_PRIORITY_NORMAL);
    } else if (priority == PRIORITY_HIGH) {
      ret = ::SetThreadPriority(thread_, THREAD_PRIORITY_HIGHEST);
    } else if (priority == PRIORITY_ABOVE_NORMAL) {
      ret = ::SetThreadPriority(thread_, THREAD_PRIORITY_ABOVE_NORMAL);
    } else if (priority == PRIORITY_IDLE) {
      ret = ::SetThreadPriority(thread_, THREAD_PRIORITY_IDLE);
    }
    if (!ret) {
      return false;
    }
  }
  priority_ = priority;
  return true;
#else
  // TODO: Implement for Linux/Mac if possible.
  if (started_) return false;
  priority_ = priority;
  return true;
#endif
}

bool Thread::Start(Runnable* runnable) {
  ASSERT(owned_);
  if (!owned_) return false;
  ASSERT(!started_);
  if (started_) return false;

  Restart();  // reset fStop_ if the thread is being restarted

  // Make sure that ThreadManager is created on the main thread before
  // we start a new thread.
  ThreadManager::Instance();

  ThreadInit* init = new ThreadInit;
  init->thread = this;
  init->runnable = runnable;
#if defined(WIN32)
  DWORD flags = 0;
  if (priority_ != PRIORITY_NORMAL) {
    flags = CREATE_SUSPENDED;
  }
  thread_ = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)PreRun, init, flags,
                         &thread_id_);
  if (thread_) {
    started_ = true;
    if (priority_ != PRIORITY_NORMAL) {
      SetPriority(priority_);
      ::ResumeThread(thread_);
    }
  } else {
    return false;
  }
#elif defined(POSIX)
  pthread_attr_t attr;
  pthread_attr_init(&attr);
  if (priority_ != PRIORITY_NORMAL) {
    if (priority_ == PRIORITY_IDLE) {
      // There is no POSIX-standard way to set a below-normal priority for an
      // individual thread (only whole process), so let's not support it.
      LOG(LS_WARNING) << "PRIORITY_IDLE not supported";
    } else {
      // Set real-time round-robin policy.
      if (pthread_attr_setschedpolicy(&attr, SCHED_RR) != 0) {
        LOG(LS_ERROR) << "pthread_attr_setschedpolicy";
      }
      struct sched_param param;
      if (pthread_attr_getschedparam(&attr, &param) != 0) {
        LOG(LS_ERROR) << "pthread_attr_getschedparam";
      } else {
        // The numbers here are arbitrary.
        if (priority_ == PRIORITY_HIGH) {
          param.sched_priority = 6;           // 6 = HIGH
        } else {
          ASSERT(priority_ == PRIORITY_ABOVE_NORMAL);
          param.sched_priority = 4;           // 4 = ABOVE_NORMAL
        }
        if (pthread_attr_setschedparam(&attr, &param) != 0) {
          LOG(LS_ERROR) << "pthread_attr_setschedparam";
        }
      }
    }
  }
  int error_code = pthread_create(&thread_, &attr, PreRun, init);
  if (0 != error_code) {
    LOG(LS_ERROR) << "Unable to create pthread, error " << error_code;
    return false;
  }
  started_ = true;
#endif
  return true;
}

void Thread::Join() {
  if (started_) {
    ASSERT(!IsCurrent());
#if defined(WIN32)
    WaitForSingleObject(thread_, INFINITE);
    CloseHandle(thread_);
    thread_ = NULL;
    thread_id_ = 0;
#elif defined(POSIX)
    void *pv;
    pthread_join(thread_, &pv);
#endif
    started_ = false;
  }
}

#ifdef WIN32
// As seen on MSDN.
// http://msdn.microsoft.com/en-us/library/xcb2z8hs(VS.71).aspx
#define MSDEV_SET_THREAD_NAME  0x406D1388
typedef struct tagTHREADNAME_INFO {
  DWORD dwType;
  LPCSTR szName;
  DWORD dwThreadID;
  DWORD dwFlags;
} THREADNAME_INFO;

void SetThreadName(DWORD dwThreadID, LPCSTR szThreadName) {
  THREADNAME_INFO info;
  info.dwType = 0x1000;
  info.szName = szThreadName;
  info.dwThreadID = dwThreadID;
  info.dwFlags = 0;

  __try {
    RaiseException(MSDEV_SET_THREAD_NAME, 0, sizeof(info) / sizeof(DWORD),
                   reinterpret_cast<ULONG_PTR*>(&info));
  }
  __except(EXCEPTION_CONTINUE_EXECUTION) {
  }
}
#endif  // WIN32

void* Thread::PreRun(void* pv) {
  ThreadInit* init = static_cast<ThreadInit*>(pv);
  ThreadManager::Instance()->SetCurrentThread(init->thread);
#if defined(WIN32)
  SetThreadName(GetCurrentThreadId(), init->thread->name_.c_str());
#elif defined(POSIX)
  // TODO: See if naming exists for pthreads.
#endif
#if __has_feature(objc_arc)
  @autoreleasepool
#elif defined(OSX) || defined(IOS)
  // Make sure the new thread has an autoreleasepool
  ScopedAutoreleasePool pool;
#endif
  {
    if (init->runnable) {
      init->runnable->Run(init->thread);
    } else {
      init->thread->Run();
    }
    if (init->thread->delete_self_when_complete_) {
      init->thread->started_ = false;
      delete init->thread;
    }
    delete init;
    return NULL;
  }
}

void Thread::Run() {
  ProcessMessages(kForever);
}

bool Thread::IsOwned() {
  return owned_;
}

void Thread::Stop() {
  MessageQueue::Quit();
  Join();
}

void Thread::Send(MessageHandler *phandler, uint32 id, MessageData *pdata) {
  if (fStop_)
    return;

  // Sent messages are sent to the MessageHandler directly, in the context
  // of "thread", like Win32 SendMessage. If in the right context,
  // call the handler directly.

  Message msg;
  msg.phandler = phandler;
  msg.message_id = id;
  msg.pdata = pdata;
  if (IsCurrent()) {
    phandler->OnMessage(&msg);
    return;
  }

  AutoThread thread;
  Thread *current_thread = Thread::Current();
  ASSERT(current_thread != NULL);  // AutoThread ensures this

  bool ready = false;
  {
    CritScope cs(&crit_);
    EnsureActive();
    _SendMessage smsg;
    smsg.thread = current_thread;
    smsg.msg = msg;
    smsg.ready = &ready;
    sendlist_.push_back(smsg);
    has_sends_ = true;
  }

  // Wait for a reply

  ss_->WakeUp();

  bool waited = false;
  while (!ready) {
    current_thread->ReceiveSends();
    current_thread->socketserver()->Wait(kForever, false);
    waited = true;
  }

  // Our Wait loop above may have consumed some WakeUp events for this
  // MessageQueue, that weren't relevant to this Send.  Losing these WakeUps can
  // cause problems for some SocketServers.
  //
  // Concrete example:
  // Win32SocketServer on thread A calls Send on thread B.  While processing the
  // message, thread B Posts a message to A.  We consume the wakeup for that
  // Post while waiting for the Send to complete, which means that when we exit
  // this loop, we need to issue another WakeUp, or else the Posted message
  // won't be processed in a timely manner.

  if (waited) {
    current_thread->socketserver()->WakeUp();
  }
}

void Thread::ReceiveSends() {
  // Before entering critical section, check boolean.

  if (!has_sends_)
    return;

  // Receive a sent message. Cleanup scenarios:
  // - thread sending exits: We don't allow this, since thread can exit
  //   only via Join, so Send must complete.
  // - thread receiving exits: Wakeup/set ready in Thread::Clear()
  // - object target cleared: Wakeup/set ready in Thread::Clear()
  crit_.Enter();
  while (!sendlist_.empty()) {
    _SendMessage smsg = sendlist_.front();
    sendlist_.pop_front();
    crit_.Leave();
    smsg.msg.phandler->OnMessage(&smsg.msg);
    crit_.Enter();
    *smsg.ready = true;
    smsg.thread->socketserver()->WakeUp();
  }
  has_sends_ = false;
  crit_.Leave();
}

void Thread::Clear(MessageHandler *phandler, uint32 id,
                   MessageList* removed) {
  CritScope cs(&crit_);

  // Remove messages on sendlist_ with phandler
  // Object target cleared: remove from send list, wakeup/set ready
  // if sender not NULL.

  std::list<_SendMessage>::iterator iter = sendlist_.begin();
  while (iter != sendlist_.end()) {
    _SendMessage smsg = *iter;
    if (smsg.msg.Match(phandler, id)) {
      if (removed) {
        removed->push_back(smsg.msg);
      } else {
        delete smsg.msg.pdata;
      }
      iter = sendlist_.erase(iter);
      *smsg.ready = true;
      smsg.thread->socketserver()->WakeUp();
      continue;
    }
    ++iter;
  }

  MessageQueue::Clear(phandler, id, removed);
}

bool Thread::ProcessMessages(int cmsLoop) {
  uint32 msEnd = (kForever == cmsLoop) ? 0 : TimeAfter(cmsLoop);
  int cmsNext = cmsLoop;

  while (true) {
#if __has_feature(objc_arc)
    @autoreleasepool
#elif defined(OSX) || defined(IOS)
    // see: http://developer.apple.com/library/mac/#documentation/Cocoa/Reference/Foundation/Classes/NSAutoreleasePool_Class/Reference/Reference.html
    // Each thread is supposed to have an autorelease pool. Also for event loops
    // like this, autorelease pool needs to be created and drained/released
    // for each cycle.
    ScopedAutoreleasePool pool;
#endif
    {
      Message msg;
      if (!Get(&msg, cmsNext))
        return !IsQuitting();
      Dispatch(&msg);

      if (cmsLoop != kForever) {
        cmsNext = TimeUntil(msEnd);
        if (cmsNext < 0)
          return true;
      }
    }
  }
}

bool Thread::WrapCurrent() {
  return WrapCurrentWithThreadManager(ThreadManager::Instance());
}

bool Thread::WrapCurrentWithThreadManager(ThreadManager* thread_manager) {
  if (started_)
    return false;
#if defined(WIN32)
  // We explicitly ask for no rights other than synchronization.
  // This gives us the best chance of succeeding.
  thread_ = OpenThread(SYNCHRONIZE, FALSE, GetCurrentThreadId());
  if (!thread_) {
    LOG_GLE(LS_ERROR) << "Unable to get handle to thread.";
    return false;
  }
  thread_id_ = GetCurrentThreadId();
#elif defined(POSIX)
  thread_ = pthread_self();
#endif
  owned_ = false;
  started_ = true;
  thread_manager->SetCurrentThread(this);
  return true;
}

void Thread::UnwrapCurrent() {
  // Clears the platform-specific thread-specific storage.
  ThreadManager::Instance()->SetCurrentThread(NULL);
#ifdef WIN32
  if (!CloseHandle(thread_)) {
    LOG_GLE(LS_ERROR) << "When unwrapping thread, failed to close handle.";
  }
#endif
  started_ = false;
}


AutoThread::AutoThread(SocketServer* ss) : Thread(ss) {
  if (!ThreadManager::Instance()->CurrentThread()) {
    ThreadManager::Instance()->SetCurrentThread(this);
  }
}

AutoThread::~AutoThread() {
  if (ThreadManager::Instance()->CurrentThread() == this) {
    ThreadManager::Instance()->SetCurrentThread(NULL);
  }
}

#ifdef WIN32
void ComThread::Run() {
  HRESULT hr = CoInitializeEx(NULL, COINIT_MULTITHREADED);
  ASSERT(SUCCEEDED(hr));
  if (SUCCEEDED(hr)) {
    Thread::Run();
    CoUninitialize();
  } else {
    LOG(LS_ERROR) << "CoInitialize failed, hr=" << hr;
  }
}
#endif

}  // namespace talk_base
