/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MODULES_VIDEO_CODING_TICK_TIME_H_
#define WEBRTC_MODULES_VIDEO_CODING_TICK_TIME_H_

#include "tick_util.h"

#include <assert.h>

namespace webrtc
{

//#define TICK_TIME_DEBUG

class VCMTickTime : public TickTime
{
#ifdef TICK_TIME_DEBUG
public:
    /*
    *   Get current time
    */
    static TickTime Now() { assert(false); };

    /*
    *   Get time in milli seconds
    */
    static WebRtc_Word64 MillisecondTimestamp() { return _timeNowDebug; };

    /*
    *   Get time in micro seconds
    */
    static WebRtc_Word64 MicrosecondTimestamp() { return _timeNowDebug * 1000LL; };

    static void IncrementDebugClock() { _timeNowDebug++; };

private:
    static WebRtc_Word64     _timeNowDebug;

#else
public:
    static void IncrementDebugClock() { assert(false); };
#endif
};

} // namespace webrtc

#endif // WEBRTC_MODULES_VIDEO_CODING_TICK_TIME_H_
