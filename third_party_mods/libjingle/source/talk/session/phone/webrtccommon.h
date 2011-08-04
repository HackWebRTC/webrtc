/*
 * libjingle
 * Copyright 2004--2011, Google Inc.
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


#ifndef TALK_SESSION_PHONE_WEBRTCCOMMON_H_
#define TALK_SESSION_PHONE_WEBRTCCOMMON_H_

#ifdef WEBRTC_RELATIVE_PATH
#include "common_types.h"
#include "video_engine/main/interface/vie_base.h"
#include "voice_engine/main/interface/voe_base.h"
#else
#include "third_party/webrtc/files/include/common_types.h"
#include "third_party/webrtc/files/include/voe_base.h"
#include "third_party/webrtc/files/include/vie_base.h"
#endif  // WEBRTC_RELATIVE_PATH

namespace cricket {

// Tracing helpers, for easy logging when WebRTC calls fail.
// Example: "LOG_RTCERR1(StartSend, channel);" produces the trace
//          "StartSend(1) failed, err=XXXX"
// The method GetLastEngineError must be defined in the calling scope.
#define LOG_RTCERR0(func) \
    LOG_RTCERR0_EX(func, GetLastEngineError())
#define LOG_RTCERR1(func, a1) \
    LOG_RTCERR1_EX(func, a1, GetLastEngineError())
#define LOG_RTCERR2(func, a1, a2) \
    LOG_RTCERR2_EX(func, a1, a2, GetLastEngineError())
#define LOG_RTCERR3(func, a1, a2, a3) \
    LOG_RTCERR3_EX(func, a1, a2, a3, GetLastEngineError())
#define LOG_RTCERR4(func, a1, a2, a3, a4) \
    LOG_RTCERR4_EX(func, a1, a2, a3, a4, GetLastEngineError())
#define LOG_RTCERR5(func, a1, a2, a3, a4, a5) \
    LOG_RTCERR5_EX(func, a1, a2, a3, a4, a5, GetLastEngineError())
#define LOG_RTCERR6(func, a1, a2, a3, a4, a5, a6) \
    LOG_RTCERR6_EX(func, a1, a2, a3, a4, a5, a6, GetLastEngineError())
#define LOG_RTCERR0_EX(func, err) LOG(LS_WARNING) \
    << "" << #func << "() failed, err=" << err
#define LOG_RTCERR1_EX(func, a1, err) LOG(LS_WARNING) \
    << "" << #func << "(" << a1 << ") failed, err=" << err
#define LOG_RTCERR2_EX(func, a1, a2, err) LOG(LS_WARNING) \
    << "" << #func << "(" << a1 << ", " << a2 << ") failed, err=" \
    << err
#define LOG_RTCERR3_EX(func, a1, a2, a3, err) LOG(LS_WARNING) \
    << "" << #func << "(" << a1 << ", " << a2 << ", " << a3 \
    << ") failed, err=" << err
#define LOG_RTCERR4_EX(func, a1, a2, a3, a4, err) LOG(LS_WARNING) \
    << "" << #func << "(" << a1 << ", " << a2 << ", " << a3 \
    << ", " << a4 << ") failed, err=" << err
#define LOG_RTCERR5_EX(func, a1, a2, a3, a4, a5, err) LOG(LS_WARNING) \
    << "" << #func << "(" << a1 << ", " << a2 << ", " << a3 \
    << ", " << a4 << ", " << a5 << ") failed, err=" << err
#define LOG_RTCERR6_EX(func, a1, a2, a3, a4, a5, a6, err) LOG(LS_WARNING) \
    << "" << #func << "(" << a1 << ", " << a2 << ", " << a3 \
    << ", " << a4 << ", " << a5 << ", " << a6 << ") failed, err=" << err

}  // namespace cricket

#endif  // TALK_SESSION_PHONE_WEBRTCCOMMON_H_
