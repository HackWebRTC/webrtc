/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "ssrc_database.h"

#include "critical_section_wrapper.h"
#include "trace.h"

#include <stdlib.h>
#include <cassert>

#ifdef _WIN32
    #include <windows.h>
    #include <MMSystem.h> //timeGetTime

    #pragma warning(disable:4311)
    #pragma warning(disable:4312)

    // Platform SDK fixes when building with /Wp64 for a 32 bits target.
    #if !defined(_WIN64) && defined(_Wp64)
        #ifdef InterlockedExchangePointer
            #undef InterlockedExchangePointer
            // The problem is that the macro provided for InterlockedExchangePointer() is
            // doing a (LONG) C-style cast that triggers invariably the warning C4312 when
            // building on 32 bits.
            inline void* InterlockedExchangePointer(void* volatile* target, void* value)
            {
                return reinterpret_cast<void*>(static_cast<LONG_PTR>(InterlockedExchange(
                    reinterpret_cast<volatile LONG*>(target),
                    static_cast<LONG>(reinterpret_cast<LONG_PTR>(value)))));
            }
        #endif  // #ifdef InterlockedExchangePointer
    #endif  //!defined(_WIN64) && defined(_Wp64)

#else
    #include <stdio.h>
    #include <string.h>
    #include <time.h>
    #include <sys/time.h>
    #ifndef WEBRTC_NO_AUTO_PTR    // are we allowed to use auto_ptrs?
        #include <memory>       // definition of auto_ptr
    #endif
#endif

namespace webrtc {
// Construct On First Use idiom. Avoids "static initialization order fiasco" (JFGI).
SSRCDatabase*&
SSRCDatabase::StaticInstance(SsrcDatabaseCount inc)
{
    static volatile long theSSRCDatabaseCount = 0; // this needs to be long due to Windows, not an issue due to its usage
    static SSRCDatabase* theSSRCDatabase = NULL;

    SsrcDatabaseCreate state = kSsrcDbExist;

#ifndef _WIN32
#ifdef WEBRTC_NO_AUTO_PTR
    // since we only have InterlockedExchange on windows and no auto_ptrs, this will result in a memory leak but we accept it for now
    static CriticalSectionWrapper* crtiSect(CriticalSectionWrapper::CreateCriticalSection());
    CriticalSectionScoped lock(*crtiSect);
#else
    static std::auto_ptr<CriticalSectionWrapper> crtiSect = std::auto_ptr<CriticalSectionWrapper>(CriticalSectionWrapper::CreateCriticalSection());
    CriticalSectionScoped lock(*crtiSect);
#endif

    if(inc == kSsrcDbInc)
    {
        theSSRCDatabaseCount++;
        if(theSSRCDatabaseCount == 1)
        {
            state = kSsrcDbCreate;
        }
    } else
    {
        theSSRCDatabaseCount--;
        if(theSSRCDatabaseCount == 0)
        {
            state = kSsrcDbDestroy;
        }
    }
    if(state == kSsrcDbCreate)
    {
        theSSRCDatabase = new SSRCDatabase();

    }else if(state == kSsrcDbDestroy)
    {
        SSRCDatabase* oldValue = theSSRCDatabase;
        theSSRCDatabase = NULL;
        if(oldValue)
        {
            delete oldValue;
        }
        return theSSRCDatabase;
    }
#else
    // Windows
    if(inc == kSsrcDbInc)
    {
        if(1 == InterlockedIncrement(&theSSRCDatabaseCount))
        {
            state = kSsrcDbCreate;
        }
    } else
    {
        int newValue = InterlockedDecrement(&theSSRCDatabaseCount);
        if(newValue == 0)
        {
            state = kSsrcDbDestroy;
        }
    }
    if(state == kSsrcDbCreate)
    {
        SSRCDatabase* newValue = new SSRCDatabase();
        SSRCDatabase* oldValue = (SSRCDatabase*)InterlockedExchangePointer(reinterpret_cast<void* volatile*>(&theSSRCDatabase), newValue);
        assert(oldValue == NULL);

    }else if(state == kSsrcDbDestroy)
    {
        SSRCDatabase* oldValue = (SSRCDatabase*)InterlockedExchangePointer(reinterpret_cast<void* volatile*>(&theSSRCDatabase), NULL);
        if(oldValue)
        {
            delete oldValue;
        }
        return theSSRCDatabase;
    }
#endif
    assert(theSSRCDatabase);
    return theSSRCDatabase;
}

SSRCDatabase*
SSRCDatabase::GetSSRCDatabase()
{
    return StaticInstance(kSsrcDbInc);
}

void
SSRCDatabase::ReturnSSRCDatabase()
{
    StaticInstance(kSsrcDbDec);
}

WebRtc_UWord32
SSRCDatabase::CreateSSRC()
{
    CriticalSectionScoped lock(*_critSect);

    WebRtc_UWord32 ssrc = GenerateRandom();

#ifndef WEBRTC_NO_STL

    while(_ssrcMap.find(ssrc) != _ssrcMap.end())
    {
        ssrc = GenerateRandom();
    }
    _ssrcMap[ssrc] = 0;

#else
    if(_sizeOfSSRC <= _numberOfSSRC)
    {
        // allocate more space
        const int newSize = _sizeOfSSRC + 10;
        WebRtc_UWord32* tempSSRCVector = new WebRtc_UWord32[newSize];
        memcpy(tempSSRCVector, _ssrcVector, _sizeOfSSRC*sizeof(WebRtc_UWord32));
        delete [] _ssrcVector;

        _ssrcVector = tempSSRCVector;
        _sizeOfSSRC = newSize;
    }

    // check if in DB
    if(_ssrcVector)
    {
        for (int i=0; i<_numberOfSSRC; i++)
        {
            if (_ssrcVector[i] == ssrc)
            {
                // we have a match
                i = 0; // start over with a new ssrc
                ssrc = GenerateRandom();
            }

        }
        //  add to database
        _ssrcVector[_numberOfSSRC] = ssrc;
        _numberOfSSRC++;
    }
#endif
    return ssrc;
}

WebRtc_Word32
SSRCDatabase::RegisterSSRC(const WebRtc_UWord32 ssrc)
{
    CriticalSectionScoped lock(*_critSect);

#ifndef WEBRTC_NO_STL

    _ssrcMap[ssrc] = 0;

#else
    if(_sizeOfSSRC <= _numberOfSSRC)
    {
        // allocate more space
        const int newSize = _sizeOfSSRC + 10;
        WebRtc_UWord32* tempSSRCVector = new WebRtc_UWord32[newSize];
        memcpy(tempSSRCVector, _ssrcVector, _sizeOfSSRC*sizeof(WebRtc_UWord32));
        delete [] _ssrcVector;

        _ssrcVector = tempSSRCVector;
        _sizeOfSSRC = newSize;
    }
    // check if in DB
    if(_ssrcVector)
    {
        for (int i=0; i<_numberOfSSRC; i++)
        {
            if (_ssrcVector[i] == ssrc)
            {
                // we have a match
                return -1;
            }
        }
        //  add to database
        _ssrcVector[_numberOfSSRC] = ssrc;
        _numberOfSSRC++;
    }
#endif
    return 0;
}

WebRtc_Word32
SSRCDatabase::ReturnSSRC(const WebRtc_UWord32 ssrc)
{
    CriticalSectionScoped lock(*_critSect);

#ifndef WEBRTC_NO_STL
    _ssrcMap.erase(ssrc);

#else
    if(_ssrcVector)
    {
        for (int i=0; i<_numberOfSSRC; i++)
        {
            if (_ssrcVector[i] == ssrc)
            {
                // we have a match
                // remove from database
                _ssrcVector[i] = _ssrcVector[_numberOfSSRC-1];
                _numberOfSSRC--;
                break;
            }
        }
    }
#endif
    return 0;
}

SSRCDatabase::SSRCDatabase()
{
    // we need to seed the random generator, otherwise we get 26500 each time, hardly a random value :)
#ifdef _WIN32
    srand(timeGetTime());
#else
    struct timeval tv;
    struct timezone tz;
    gettimeofday(&tv, &tz);
    srand(tv.tv_usec);
#endif

#ifdef WEBRTC_NO_STL
    _sizeOfSSRC = 10;
    _numberOfSSRC = 0;
    _ssrcVector = new WebRtc_UWord32[10];
#endif
    _critSect = CriticalSectionWrapper::CreateCriticalSection();

    WEBRTC_TRACE(kTraceMemory, kTraceRtpRtcp, -1, "%s created", __FUNCTION__);
}

SSRCDatabase::~SSRCDatabase()
{
#ifdef WEBRTC_NO_STL
    delete [] _ssrcVector;
#else
    _ssrcMap.clear();
#endif
    delete _critSect;

    WEBRTC_TRACE(kTraceMemory, kTraceRtpRtcp, -1, "%s deleted", __FUNCTION__);
}

WebRtc_UWord32 SSRCDatabase::GenerateRandom()
{
    WebRtc_UWord32 ssrc = 0;
    do
    {
        ssrc = rand();
        ssrc = ssrc <<16;
        ssrc += rand();

    } while (ssrc == 0 || ssrc == 0xffffffff);

    return ssrc;
}
} // namespace webrtc
