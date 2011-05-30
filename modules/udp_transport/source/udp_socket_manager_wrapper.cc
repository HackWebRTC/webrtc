/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "udp_socket_manager_wrapper.h"

#include <cassert>

#ifdef _WIN32
#include "fix_interlocked_exchange_pointer_windows.h"
#include "udp_socket_manager_windows.h"
#include "udp_socket2_manager_windows.h"
#else
#include "udp_socket_manager_linux.h"
#endif

#ifndef _WIN32
#ifndef WEBRTC_NO_AUTO_PTR
#include <memory>
#endif
#endif

namespace webrtc {
UdpSocketManager* UdpSocketManager::CreateSocketManager(
    const WebRtc_Word32 id,
    WebRtc_UWord8& numOfWorkThreads)
{
#if defined(_WIN32)
    #if (defined(USE_WINSOCK2))
        return static_cast<UdpSocketManager*>(
            new UdpSocket2ManagerWindows(id, numOfWorkThreads));
    #else
        numOfWorkThreads = 1;
        return static_cast<UdpSocketManager*>(
            new UdpSocketManagerWindows(id, numOfWorkThreads));
    #endif
#else
    return new UdpSocketManagerLinux(id, numOfWorkThreads);
#endif
}

// TODO (hellner): more or less the same code is used in trace_impl.cc.
// Should be possible to avoid duplication here.
// Construct On First Use idiom. Avoids "static initialization order fiasco".
UdpSocketManager* UdpSocketManager::StaticInstance(
    const UdpSocketManagerCount inc,
    const WebRtc_Word32 id,
    WebRtc_UWord8& numOfWorkThreads)
{
    // TODO (hellner): use atomic wrapper instead.
    static volatile long theUdpSocketManagerCount = 0;
    static UdpSocketManager* volatile theUdpSocketManager = NULL;

    UdpSocketManagerState state = kUdpSocketManagerExist;

#ifndef _WIN32

#ifdef WEBRTC_NO_AUTO_PTR
    // TODO (pwestin): crtiSect is never reclaimed. Fix memory leak.
    static CriticalSectionWrapper* crtiSect(
        CriticalSectionWrapper::CreateCriticalSection());
#else
    static std::auto_ptr<CriticalSectionWrapper> crtiSect =
        std::auto_ptr<CriticalSectionWrapper>(
            CriticalSectionWrapper::CreateCriticalSection());
#endif
    CriticalSectionScoped lock(*crtiSect);

    if(inc == kUdpSocketManagerInc)
    {
        theUdpSocketManagerCount++;
        if(theUdpSocketManagerCount == 1)
        {
            state = kUdpSocketManagerCreate;
        }
    } else
    {
        theUdpSocketManagerCount--;
        if(theUdpSocketManagerCount == 0)
        {
            state = kUdpSocketManagerDestroy;
        }
    }
    if(state == kUdpSocketManagerCreate)
    {
        theUdpSocketManager =
            UdpSocketManager::CreateSocketManager(id, numOfWorkThreads);
        theUdpSocketManager->Start();
        assert(theUdpSocketManager);
        return theUdpSocketManager;

    }else if(state == kUdpSocketManagerDestroy)
    {
        UdpSocketManager* oldValue = theUdpSocketManager;
        theUdpSocketManager = NULL;
        if(oldValue)
        {
            if(oldValue->Stop())
            {
                delete static_cast<UdpSocketManager*>(oldValue);
            }
        }
        return NULL;
    } else
    {
        if(theUdpSocketManager)
        {
            numOfWorkThreads = theUdpSocketManager->WorkThreads();
        }
    }
#else // _WIN32
    if(inc == kUdpSocketManagerInc)
    {
        if(theUdpSocketManagerCount == 0)
        {
            state = kUdpSocketManagerCreate;
        }else {
            if(1 == InterlockedIncrement(&theUdpSocketManagerCount))
            {
                // The instance has been destroyed by some other thread.
                // Rollback.
                InterlockedDecrement(&theUdpSocketManagerCount);
                state = kUdpSocketManagerCreate;
            }
        }
    } else
    {
        WebRtc_Word32 newValue = InterlockedDecrement(
            &theUdpSocketManagerCount);
        if(newValue == 0)
        {
            state = kUdpSocketManagerDestroy;
        }
    }
    if(state == kUdpSocketManagerCreate)
    {
        // Create instance and let whichever thread finishes first assign its
        // local copy to the global instance. All other threads reclaim their
        // local copy.
        UdpSocketManager* newSocketMgr=
            UdpSocketManager::CreateSocketManager(id, numOfWorkThreads);
        if(1 == InterlockedIncrement(&theUdpSocketManagerCount))
        {
            UdpSocketManager* oldValue = (UdpSocketManager*)
                InterlockedExchangePointer(
                    reinterpret_cast<void* volatile*>(&theUdpSocketManager),
                    newSocketMgr);
            newSocketMgr->Start();
            assert(oldValue == NULL);
            assert(theUdpSocketManager);
            return newSocketMgr;

        }
        InterlockedDecrement(&theUdpSocketManagerCount);
        if(newSocketMgr)
        {
            delete static_cast<UdpSocketManager*>(newSocketMgr);
        }
        return NULL;
    } else if(state == kUdpSocketManagerDestroy)
    {
        UdpSocketManager* oldValue = (UdpSocketManager*)
            InterlockedExchangePointer(
                reinterpret_cast<void* volatile*>(&theUdpSocketManager),
                NULL);
        if(oldValue)
        {
            if(oldValue->Stop())
            {
                delete static_cast<UdpSocketManager*>(oldValue);
            }
        }
        return NULL;
    } else
    {
        if(theUdpSocketManager)
        {
            numOfWorkThreads = theUdpSocketManager->WorkThreads();
        }
    }
#endif // #ifndef _WIN32
    return theUdpSocketManager;
}

UdpSocketManager* UdpSocketManager::Create(const WebRtc_Word32 id,
                                           WebRtc_UWord8& numOfWorkThreads)
{
    return UdpSocketManager::StaticInstance(kUdpSocketManagerInc, id,
                                            numOfWorkThreads);
}

void UdpSocketManager::Return()
{
    WebRtc_UWord8 numOfWorkThreads = 0;
    UdpSocketManager::StaticInstance(kUdpSocketManagerDec, -1,
                                     numOfWorkThreads);
}

UdpSocketManager::UdpSocketManager(const WebRtc_Word32 /*id*/,
                                   WebRtc_UWord8& numOfWorkThreads)
    : _numOfWorkThreads(numOfWorkThreads)
{
}

WebRtc_UWord8 UdpSocketManager::WorkThreads() const
{
    return _numOfWorkThreads;
}
} // namespace webrtc
