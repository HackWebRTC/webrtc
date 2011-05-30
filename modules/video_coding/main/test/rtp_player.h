/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MODULES_VIDEO_CODING_TEST_RTP_PLAYER_H_
#define WEBRTC_MODULES_VIDEO_CODING_TEST_RTP_PLAYER_H_

#include "typedefs.h"
#include "rtp_rtcp.h"
#include "list_wrapper.h"
#include "critical_section_wrapper.h"
#include "video_coding_defines.h"

#include <stdio.h>
#include <string>

#define HDR_SIZE 8 // rtpplay packet header size in bytes
#define FIRSTLINELEN 40
#define RAND_VEC_LENGTH 4096

struct RawRtpPacket
{
public:
    RawRtpPacket(WebRtc_UWord8* data, WebRtc_UWord16 len);
    ~RawRtpPacket();

    WebRtc_UWord8* rtpData;
    WebRtc_UWord16 rtpLen;
    WebRtc_Word64 resendTimeMs;
};

class LostPackets : public webrtc::ListWrapper
{
public:
    LostPackets();
    ~LostPackets();

    WebRtc_UWord32 AddPacket(WebRtc_UWord8* rtpData, WebRtc_UWord16 rtpLen);
    WebRtc_UWord32 SetResendTime(WebRtc_UWord16 sequenceNumber, WebRtc_Word64 resendTime);
    WebRtc_UWord32 TotalNumberOfLosses() const { return _lossCount; };
    WebRtc_UWord32 NumberOfPacketsToResend() const;
    void ResentPacket(WebRtc_UWord16 seqNo);
    void Lock()     {_critSect.Enter();};
    void Unlock()   {_critSect.Leave();};
private:
    webrtc::CriticalSectionWrapper& _critSect;
    WebRtc_UWord32 _lossCount;
    FILE*        _debugFile;
};

struct PayloadCodecTuple
{
    PayloadCodecTuple(WebRtc_UWord8 plType, std::string codecName, webrtc::VideoCodecType type) :
        name(codecName), payloadType(plType), codecType(type) {};
    const std::string name;
    const WebRtc_UWord8 payloadType;
    const webrtc::VideoCodecType codecType;
};

class RTPPlayer : public webrtc::VCMPacketRequestCallback
{
public:
    RTPPlayer(const char* filename, webrtc::RtpData* callback);
    virtual ~RTPPlayer();

    WebRtc_Word32 Initialize(const webrtc::ListWrapper& payloadList);
    WebRtc_Word32 NextPacket(const WebRtc_Word64 timeNow);
    WebRtc_UWord32 TimeUntilNextPacket() const;
    WebRtc_Word32 SimulatePacketLoss(float lossRate, bool enableNack = false, WebRtc_UWord32 rttMs = 0);
    WebRtc_Word32 SetReordering(bool enabled);
    WebRtc_Word32 ResendPackets(const WebRtc_UWord16* sequenceNumbers, WebRtc_UWord16 length);
    void Print() const;

private:
    WebRtc_Word32 SendPacket(WebRtc_UWord8* rtpData, WebRtc_UWord16 rtpLen);
    WebRtc_Word32 ReadPacket(WebRtc_Word16* rtpdata, WebRtc_UWord32* offset);
    WebRtc_Word32 ReadHeader();
    FILE*              _rtpFile;
    webrtc::RtpRtcp&   _rtpModule;
    WebRtc_UWord32     _nextRtpTime;
    webrtc::RtpData*   _dataCallback;
    bool               _firstPacket;
    float              _lossRate;
    bool               _nackEnabled;
    LostPackets        _lostPackets;
    WebRtc_UWord32     _resendPacketCount;
    WebRtc_Word32      _noLossStartup;
    bool               _endOfFile;
    WebRtc_UWord32     _rttMs;
    WebRtc_Word64      _firstPacketRtpTime;
    WebRtc_Word64      _firstPacketTimeMs;
    RawRtpPacket*      _reorderBuffer;
    bool               _reordering;
    WebRtc_Word16      _nextPacket[8000];
    WebRtc_Word32      _nextPacketLength;
    int                _randVec[RAND_VEC_LENGTH];
    int                _randVecPos;
};

#endif // WEBRTC_MODULES_VIDEO_CODING_TEST_RTP_PLAYER_H_
