/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

/*
 * vie_sender.h
 */

#ifndef WEBRTC_VIDEO_ENGINE_MAIN_SOURCE_VIE_SENDER_H_
#define WEBRTC_VIDEO_ENGINE_MAIN_SOURCE_VIE_SENDER_H_

// Defines
#include "engine_configurations.h"
#include "vie_defines.h"
#include "typedefs.h"
#include "common_types.h"

// Forward declarations

#ifdef WEBRTC_SRTP
  class SrtpModule;
#endif

namespace webrtc {
class CriticalSectionWrapper;
class RtpDump;
class RtpRtcp;
class Transport;
class VideoCodingModule;

class ViESender: public Transport
{
public:
    ViESender(int engineId, int channelId, RtpRtcp& rtpRtcpModule);
    ~ViESender();

    int RegisterExternalEncryption(Encryption* encryption);
    int DeregisterExternalEncryption();

    int RegisterSendTransport(Transport* transport);
    int DeregisterSendTransport();

#ifdef WEBRTC_SRTP
    int RegisterSRTPModule(SrtpModule* srtpModule);
    int DeregisterSRTPModule();

    int RegisterSRTCPModule(SrtpModule* srtpModule);
    int DeregisterSRTCPModule();
#endif

    int StartRTPDump(const char fileNameUTF8[1024]);
    int StopRTPDump();

    // Implements Transport
    virtual int SendPacket(int vieId, const void *data, int len);
    virtual int SendRTCPPacket(int vieId, const void *data, int len);

private:
    int _engineId;
    int _channelId;
    CriticalSectionWrapper& _sendCritsect;
    RtpRtcp& _rtpRtcp;

#ifdef WEBRTC_SRTP
    SrtpModule* _ptrSrtp;
    SrtpModule* _ptrSrtcp;
#endif

    Encryption* _ptrExternalEncryption;
    WebRtc_UWord8* _ptrSrtpBuffer;
    WebRtc_UWord8* _ptrSrtcpBuffer;
    WebRtc_UWord8* _ptrEncryptionBuffer;
    Transport* _ptrTransport;
    RtpDump* _rtpDump;
};

} // namespace webrtc
#endif    // WEBRTC_VIDEO_ENGINE_MAIN_SOURCE_VIE_SENDER_H_
