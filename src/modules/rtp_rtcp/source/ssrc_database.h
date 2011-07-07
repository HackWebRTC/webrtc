/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MODULES_RTP_RTCP_SOURCE_SSRC_DATABASE_H_
#define WEBRTC_MODULES_RTP_RTCP_SOURCE_SSRC_DATABASE_H_

#include "typedefs.h"

#ifndef WEBRTC_NO_STL
    #include <map>
#endif

namespace webrtc {
class CriticalSectionWrapper;

class SSRCDatabase
{
    enum SsrcDatabaseCount
    {
        kSsrcDbDec    = 0,
        kSsrcDbInc    = 1
    };

    enum SsrcDatabaseCreate
    {
        kSsrcDbExist    = 0,
        kSsrcDbCreate    = 1,
        kSsrcDbDestroy   = 2
    };

public:
    static SSRCDatabase* GetSSRCDatabase();
    static void ReturnSSRCDatabase();

    WebRtc_UWord32 CreateSSRC();
    WebRtc_Word32 RegisterSSRC(const WebRtc_UWord32 ssrc);
    WebRtc_Word32 ReturnSSRC(const WebRtc_UWord32 ssrc);

private:
    static SSRCDatabase*& StaticInstance(SsrcDatabaseCount inc);

private:
    SSRCDatabase();
    virtual ~SSRCDatabase();

    WebRtc_UWord32 GenerateRandom();

#ifdef WEBRTC_NO_STL
    int _numberOfSSRC;
    int _sizeOfSSRC;

    WebRtc_UWord32* _ssrcVector;
#else
    std::map<WebRtc_UWord32, WebRtc_UWord32>    _ssrcMap;
#endif

    CriticalSectionWrapper* _critSect;
};
} // namespace webrtc

#endif // WEBRTC_MODULES_RTP_RTCP_SOURCE_SSRC_DATABASE_H_
