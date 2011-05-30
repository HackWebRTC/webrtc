/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "thread_linux.h"

#include <errno.h>
#include <string.h> // strncpy
#include <time.h>   // nanosleep
#include <unistd.h>
#ifdef WEBRTC_LINUX
#include <sys/types.h>
#include <sched.h>
#include <sys/syscall.h>
#include <linux/unistd.h>
#include <sys/prctl.h>
#endif

#include "event_wrapper.h"
#include "trace.h"

namespace webrtc {
extern "C"
{
    static void* StartThread(void* lpParameter)
    {
        static_cast<ThreadLinux*>(lpParameter)->Run();
        return 0;
    }
}

#if (defined(WEBRTC_LINUX) && !defined(ANDROID))
static pid_t gettid()
{
#if defined(__NR_gettid)
    return  syscall(__NR_gettid);
#else
    return -1;
#endif
}
#endif

ThreadWrapper* ThreadLinux::Create(ThreadRunFunction func, ThreadObj obj,
                                   ThreadPriority prio, const char* threadName)
{
    ThreadLinux* ptr = new ThreadLinux(func, obj, prio, threadName);
    if (!ptr)
    {
        return NULL;
    }
    const int error = ptr->Construct();
    if (error)
    {
        delete ptr;
        return NULL;
    }
    return ptr;
}

ThreadLinux::ThreadLinux(ThreadRunFunction func, ThreadObj obj,
                         ThreadPriority prio, const char* threadName)
    : _runFunction(func),
      _obj(obj),
      _alive(false),
      _dead(true),
      _prio(prio),
      _event(EventWrapper::Create()),
      _setThreadName(false)
{
#ifdef WEBRTC_LINUX
    _linuxPid = -1;
#endif
    if (threadName != NULL)
    {
        _setThreadName = true;
        strncpy(_name, threadName, kThreadMaxNameLength);
    }
}

int ThreadLinux::Construct()
{
    int result = 0;
#if !defined(ANDROID)
    // Enable immediate cancellation if requested, see Shutdown()
    result = pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL);
    if (result != 0)
    {
        return -1;
    }
    result = pthread_setcanceltype(PTHREAD_CANCEL_ASYNCHRONOUS, NULL);
    if (result != 0)
    {
        return -1;
    }
#endif
    result = pthread_attr_init(&_attr);
    if (result != 0)
    {
        return -1;
    }

    return 0;
}

ThreadLinux::~ThreadLinux()
{
    pthread_attr_destroy(&_attr);
    delete _event;
}

#define HAS_THREAD_ID !defined(MAC_IPHONE) && !defined(MAC_IPHONE_SIM)  &&  \
                      !defined(WEBRTC_MAC) && !defined(WEBRTC_MAC_INTEL) && \
                      !defined(MAC_DYLIB)  && !defined(MAC_INTEL_DYLIB)
#if HAS_THREAD_ID
bool ThreadLinux::Start(unsigned int& threadID)
#else
bool ThreadLinux::Start(unsigned int& /*threadID*/)
#endif
{
    if (!_runFunction)
    {
        return false;
    }
    int result = pthread_attr_setdetachstate(&_attr, PTHREAD_CREATE_DETACHED);
    // Set the stack stack size to 1M.
    result |= pthread_attr_setstacksize(&_attr, 1024*1024);
#ifdef WEBRTC_THREAD_RR
    const int policy = SCHED_RR;
#else
    const int policy = SCHED_FIFO;
#endif
    _event->Reset();
    result |= pthread_create(&_thread, &_attr, &StartThread, this);
    if (result != 0)
    {
        return false;
    }

    // Wait up to 10 seconds for the OS to call the callback function. Prevents
    // race condition if Stop() is called too quickly after start.
    if (kEventSignaled != _event->Wait(WEBRTC_EVENT_10_SEC))
    {
        // Timed out. Something went wrong.
        _runFunction = NULL;
        return false;
    }

#if HAS_THREAD_ID
    threadID = static_cast<unsigned int>(_thread);
#endif
    sched_param param;

    const int minPrio = sched_get_priority_min(policy);
    const int maxPrio = sched_get_priority_max(policy);
    if ((minPrio == EINVAL) || (maxPrio == EINVAL))
    {
        return false;
    }

    switch (_prio)
    {
    case kLowPriority:
        param.sched_priority = minPrio + 1;
        break;
    case kNormalPriority:
        param.sched_priority = (minPrio + maxPrio) / 2;
        break;
    case kHighPriority:
        param.sched_priority = maxPrio - 3;
        break;
    case kHighestPriority:
        param.sched_priority = maxPrio - 2;
        break;
    case kRealtimePriority:
        param.sched_priority = maxPrio - 1;
        break;
    default:
        return false;
    }
    result = pthread_setschedparam(_thread, policy, &param);
    if (result == EINVAL)
    {
        return false;
    }
    return true;
}

#if (defined(WEBRTC_LINUX) && !defined(ANDROID))
bool ThreadLinux::SetAffinity(const int* processorNumbers,
                              const unsigned int amountOfProcessors)
{
    if (!processorNumbers || (amountOfProcessors == 0))
    {
        return false;
    }

    cpu_set_t mask;
    CPU_ZERO(&mask);

    for(unsigned int processor = 0;
        processor < amountOfProcessors;
        processor++)
    {
        CPU_SET(processorNumbers[processor], &mask);
    }
    const int result = sched_setaffinity(_linuxPid, (unsigned int)sizeof(mask),
                                         &mask);
    if (result != 0)
    {
        return false;

    }
    return true;
}
#else
// NOTE: On Mac OS X, use the Thread affinity API in
// /usr/include/mach/thread_policy.h: thread_policy_set and mach_thread_self()
// instead of Linux gettid() syscall.
bool ThreadLinux::SetAffinity(const int* , const unsigned int)
{
    return false;
}
#endif

void ThreadLinux::SetNotAlive()
{
    _alive = false;
}

bool ThreadLinux::Shutdown()
{
#if !defined(ANDROID)
    if (_thread && (0 != pthread_cancel(_thread)))
    {
        return false;
    }

    return true;
#else
    return false;
#endif
}

bool ThreadLinux::Stop()
{
    _alive = false;

    // TODO (hellner) why not use an event here?
    // Wait up to 10 seconds for the thread to terminate
    for (int i = 0; i < 1000 && !_dead; i++)
    {
        timespec t;
        t.tv_sec = 0;
        t.tv_nsec = 10*1000*1000;
        nanosleep(&t, NULL);
    }
    if (_dead)
    {
        return true;
    }
    else
    {
        return false;
    }
}

void ThreadLinux::Run()
{
    _alive = true;
    _dead  = false;
#ifdef WEBRTC_LINUX
    if(_linuxPid == -1)
    {
        _linuxPid = gettid();
    }
#endif
    // The event the Start() is waiting for.
    _event->Set();

    if (_setThreadName)
    {
#ifdef WEBRTC_LINUX
        WEBRTC_TRACE(kTraceStateInfo, kTraceUtility,-1,
                     "Thread with id:%d name:%s started ", _linuxPid, _name);
        prctl(PR_SET_NAME, (unsigned long)_name, 0, 0, 0);
#else
        WEBRTC_TRACE(kTraceStateInfo, kTraceUtility,-1,
                     "Thread with name:%s started ", _name);
#endif
    }else
    {
#ifdef WEBRTC_LINUX
        WEBRTC_TRACE(kTraceStateInfo, kTraceUtility, -1,
                     "Thread with id:%d without name started", _linuxPid);
#else
        WEBRTC_TRACE(kTraceStateInfo, kTraceUtility, -1,
                     "Thread without name started");
#endif
    }
    do
    {
        if (_runFunction)
        {
            if (!_runFunction(_obj))
            {
                _alive = false;
            }
        }
        else
        {
            _alive = false;
        }
    }
    while (_alive);

    if (_setThreadName)
    {
        // Don't set the name for the trace thread because it may cause a
        // deadlock. TODO (hellner) there should be a better solution than
        // coupling the thread and the trace class like this.
        if (strcmp(_name, "Trace"))
        {
            WEBRTC_TRACE(kTraceStateInfo, kTraceUtility,-1,
                         "Thread with name:%s stopped", _name);
        }
    }
    else
    {
        WEBRTC_TRACE(kTraceStateInfo, kTraceUtility,-1,
                     "Thread without name stopped");
    }
    _dead = true;
}
} // namespace webrtc
