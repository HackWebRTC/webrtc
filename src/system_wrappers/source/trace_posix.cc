/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "trace_posix.h"

#include <cassert>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#ifdef WEBRTC_ANDROID
    #include <pthread.h>
#else
    #include <iostream>
#endif

#if defined(_DEBUG)
    #define BUILDMODE "d"
#elif defined(DEBUG)
    #define BUILDMODE "d"
#elif defined(NDEBUG)
    #define BUILDMODE "r"
#else
    #define BUILDMODE "?"
#endif
#define BUILDTIME __TIME__
#define BUILDDATE __DATE__
// example: "Oct 10 2002 12:05:30 r"
#define BUILDINFO BUILDDATE " " BUILDTIME " " BUILDMODE

namespace webrtc {
TracePosix::TracePosix()
{
    _prevAPITickCount = time(NULL);
    _prevTickCount = _prevAPITickCount;
}

TracePosix::~TracePosix()
{
    StopThread();
}

WebRtc_Word32 TracePosix::AddThreadId(char* traceMessage) const
{
    WebRtc_UWord64 threadId = (WebRtc_UWord64)pthread_self();
    sprintf(traceMessage, "%10llu; ", threadId);
    // 12 bytes are written.
    return 12;
}

WebRtc_Word32 TracePosix::AddTime(char* traceMessage,
                                  const TraceLevel level) const
{
    time_t dwCurrentTimeInSeconds = time(NULL);
    struct tm systemTime;
    gmtime_r(&dwCurrentTimeInSeconds, &systemTime);

    if(level == kTraceApiCall)
    {
        WebRtc_UWord32 dwDeltaTime = dwCurrentTimeInSeconds - _prevTickCount;
        _prevTickCount = dwCurrentTimeInSeconds;

        if(_prevTickCount == 0)
        {
            dwDeltaTime = 0;
        }
        if(dwDeltaTime > 0x0fffffff)
        {
            // Either wraparound or data race.
            dwDeltaTime = 0;
        }
        if(dwDeltaTime > 99999)
        {
            dwDeltaTime = 99999;
        }

        sprintf(traceMessage, "(%2u:%2u:%2u:%3u |%5lu) ", systemTime.tm_hour,
                systemTime.tm_min, systemTime.tm_sec, 0,
                static_cast<unsigned long>(dwDeltaTime));
    } else {
        WebRtc_UWord32 dwDeltaTime = dwCurrentTimeInSeconds - _prevAPITickCount;
        _prevAPITickCount = dwCurrentTimeInSeconds;
        if(_prevAPITickCount == 0)
        {
            dwDeltaTime = 0;
        }
        if(dwDeltaTime > 0x0fffffff)
        {
            // Either wraparound or data race.
            dwDeltaTime = 0;
        }
        if(dwDeltaTime > 99999)
        {
            dwDeltaTime = 99999;
        }
        sprintf(traceMessage, "(%2u:%2u:%2u:%3u |%5lu) ", systemTime.tm_hour,
                systemTime.tm_min, systemTime.tm_sec, 0,
                static_cast<unsigned long>(dwDeltaTime));
    }
    // Messages is 22 characters.
    return 22;
}

WebRtc_Word32 TracePosix::AddBuildInfo(char* traceMessage) const
{
    sprintf(traceMessage, "Build info: %s", BUILDINFO);
    // Include NULL termination (hence + 1).
    return strlen(traceMessage) + 1;
}

WebRtc_Word32 TracePosix::AddDateTimeInfo(char* traceMessage) const
{
    time_t t;
    time(&t);
    sprintf(traceMessage, "Local Date: %s", ctime(&t));
    WebRtc_Word32 len = static_cast<WebRtc_Word32>(strlen(traceMessage));

    if ('\n' == traceMessage[len - 1])
    {
        traceMessage[len - 1] = '\0';
        --len;
    }

    // Messages is 12 characters.
    return len + 1;
}
} // namespace webrtc
