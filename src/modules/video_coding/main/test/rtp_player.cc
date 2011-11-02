/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "rtp_player.h"
#include "../source/internal_defines.h"
#include "rtp_rtcp.h"
#include "tick_time.h"

#include <cstdlib>
#ifdef WIN32
#include <windows.h>
#include <Winsock2.h>
#else
#include <arpa/inet.h>
#endif

using namespace webrtc;

RawRtpPacket::RawRtpPacket(WebRtc_UWord8* data, WebRtc_UWord16 len)
:
rtpData(), rtpLen(len), resendTimeMs(-1)
{
    rtpData = new WebRtc_UWord8[rtpLen];
    memcpy(rtpData, data, rtpLen);
}

RawRtpPacket::~RawRtpPacket()
{
    delete [] rtpData;
}

LostPackets::LostPackets()
:
_critSect(*CriticalSectionWrapper::CreateCriticalSection()),
_lossCount(0),
_debugFile(NULL)
{
    _debugFile = fopen("PacketLossDebug.txt", "w");
}

LostPackets::~LostPackets()
{
    if (_debugFile)
    {
        fclose(_debugFile);
    }
    ListItem* item = First();
    while (item != NULL)
    {
        RawRtpPacket* packet = static_cast<RawRtpPacket*>(item->GetItem());
        if (packet != NULL)
        {
            delete packet;
        }
        Erase(item);
        item = First();
    }
    delete &_critSect;
}

WebRtc_UWord32 LostPackets::AddPacket(WebRtc_UWord8* rtpData, WebRtc_UWord16 rtpLen)
{
    CriticalSectionScoped cs(_critSect);
    RawRtpPacket* packet = new RawRtpPacket(rtpData, rtpLen);
    ListItem* newItem = new ListItem(packet);
    Insert(Last(), newItem);
    const WebRtc_UWord16 seqNo = (rtpData[2] << 8) + rtpData[3];
    if (_debugFile != NULL)
    {
        fprintf(_debugFile, "%u Lost packet: %u\n", _lossCount, seqNo);
    }
    _lossCount++;
    return 0;
}

WebRtc_UWord32 LostPackets::SetResendTime(WebRtc_UWord16 sequenceNumber, WebRtc_Word64 resendTime)
{
    CriticalSectionScoped cs(_critSect);
    ListItem* item = First();
    while (item != NULL)
    {
        RawRtpPacket* packet = static_cast<RawRtpPacket*>(item->GetItem());
        const WebRtc_UWord16 seqNo = (packet->rtpData[2] << 8) + packet->rtpData[3];
        const WebRtc_Word64 nowMs = VCMTickTime::MillisecondTimestamp();
        if (sequenceNumber == seqNo && packet->resendTimeMs + 10 < nowMs)
        {
            if (_debugFile != NULL)
            {
                fprintf(_debugFile, "Resend %u at %u\n", seqNo, MaskWord64ToUWord32(resendTime));
            }
            packet->resendTimeMs = resendTime;
            return 0;
        }
        item = Next(item);
    }
    fprintf(_debugFile, "Packet not lost %u\n", sequenceNumber);
    return -1;
}

WebRtc_UWord32 LostPackets::NumberOfPacketsToResend() const
{
    CriticalSectionScoped cs(_critSect);
    WebRtc_UWord32 count = 0;
    ListItem* item = First();
    while (item != NULL)
    {
        RawRtpPacket* packet = static_cast<RawRtpPacket*>(item->GetItem());
        if (packet->resendTimeMs >= 0)
        {
            count++;
        }
        item = Next(item);
    }
    return count;
}

void LostPackets::ResentPacket(WebRtc_UWord16 seqNo)
{
    CriticalSectionScoped cs(_critSect);
    if (_debugFile != NULL)
    {
        fprintf(_debugFile, "Resent %u at %u\n", seqNo,
                MaskWord64ToUWord32(VCMTickTime::MillisecondTimestamp()));
    }
}

RTPPlayer::RTPPlayer(const char* filename, RtpData* callback)
:
_rtpModule(*RtpRtcp::CreateRtpRtcp(1, false)),
_nextRtpTime(0),
_dataCallback(callback),
_firstPacket(true),
_lossRate(0.0f),
_nackEnabled(false),
_resendPacketCount(0),
_noLossStartup(100),
_endOfFile(false),
_rttMs(0),
_firstPacketRtpTime(0),
_firstPacketTimeMs(0),
_reorderBuffer(NULL),
_reordering(false),
_nextPacket(),
_nextPacketLength(0),
_randVec(),
_randVecPos(0)
{
    _rtpFile = fopen(filename, "rb");
    memset(_nextPacket, 0, sizeof(_nextPacket));
}

RTPPlayer::~RTPPlayer()
{
    RtpRtcp::DestroyRtpRtcp(&_rtpModule);
    if (_rtpFile != NULL)
    {
        fclose(_rtpFile);
    }
    if (_reorderBuffer != NULL)
    {
        delete _reorderBuffer;
        _reorderBuffer = NULL;
    }
}

WebRtc_Word32 RTPPlayer::Initialize(const ListWrapper& payloadList)
{
    std::srand(321);
    for (int i=0; i < RAND_VEC_LENGTH; i++)
    {
        _randVec[i] = rand();
    }
    _randVecPos = 0;
    WebRtc_Word32 ret = _rtpModule.SetNACKStatus(kNackOff);
    if (ret < 0)
    {
        return -1;
    }
    ret = _rtpModule.InitReceiver();
    if (ret < 0)
    {
        return -1;
    }

    _rtpModule.InitSender();
    _rtpModule.SetRTCPStatus(kRtcpNonCompound);
    _rtpModule.SetTMMBRStatus(true);

    ret = _rtpModule.RegisterIncomingDataCallback(_dataCallback);
    if (ret < 0)
    {
        return -1;
    }
    // Register payload types
    ListItem* item = payloadList.First();
    while (item != NULL)
    {
        PayloadCodecTuple* payloadType = static_cast<PayloadCodecTuple*>(item->GetItem());
        if (payloadType != NULL)
        {
            VideoCodec videoCodec;
            strncpy(videoCodec.plName, payloadType->name.c_str(), 32);
            videoCodec.plType = payloadType->payloadType;
            if (_rtpModule.RegisterReceivePayload(videoCodec) < 0)
            {
                return -1;
            }
        }
        item = payloadList.Next(item);
    }
    if (ReadHeader() < 0)
    {
        return -1;
    }
    memset(_nextPacket, 0, sizeof(_nextPacket));
    _nextPacketLength = ReadPacket(_nextPacket, &_nextRtpTime);
    return 0;
}

WebRtc_Word32 RTPPlayer::ReadHeader()
{
    char firstline[FIRSTLINELEN];
    if (_rtpFile == NULL)
    {
        return -1;
    }
    fgets(firstline, FIRSTLINELEN, _rtpFile);
    if(strncmp(firstline,"#!rtpplay",9) == 0) {
        if(strncmp(firstline,"#!rtpplay1.0",12) != 0){
            printf("ERROR: wrong rtpplay version, must be 1.0\n");
            return -1;
        }
    }
    else if (strncmp(firstline,"#!RTPencode",11) == 0) {
        if(strncmp(firstline,"#!RTPencode1.0",14) != 0){
            printf("ERROR: wrong RTPencode version, must be 1.0\n");
            return -1;
        }
    }
    else {
        printf("ERROR: wrong file format of input file\n");
        return -1;
    }

    WebRtc_UWord32 start_sec;
    WebRtc_UWord32 start_usec;
    WebRtc_UWord32 source;
    WebRtc_UWord16 port;
    WebRtc_UWord16 padding;

    fread(&start_sec, 4, 1, _rtpFile);
    start_sec=ntohl(start_sec);
    fread(&start_usec, 4, 1, _rtpFile);
    start_usec=ntohl(start_usec);
    fread(&source, 4, 1, _rtpFile);
    source=ntohl(source);
    fread(&port, 2, 1, _rtpFile);
    port=ntohs(port);
    fread(&padding, 2, 1, _rtpFile);
    padding=ntohs(padding);
    return 0;
}

WebRtc_UWord32 RTPPlayer::TimeUntilNextPacket() const
{
    WebRtc_Word64 timeLeft = (_nextRtpTime - _firstPacketRtpTime) - (VCMTickTime::MillisecondTimestamp() - _firstPacketTimeMs);
    if (timeLeft < 0)
    {
        return 0;
    }
    return static_cast<WebRtc_UWord32>(timeLeft);
}

WebRtc_Word32 RTPPlayer::NextPacket(const WebRtc_Word64 timeNow)
{
    // Send any packets ready to be resent
    _lostPackets.Lock();
    ListItem* item = _lostPackets.First();
    _lostPackets.Unlock();
    while (item != NULL)
    {
        _lostPackets.Lock();
        RawRtpPacket* packet = static_cast<RawRtpPacket*>(item->GetItem());
        _lostPackets.Unlock();
        if (timeNow >= packet->resendTimeMs && packet->resendTimeMs != -1)
        {
            const WebRtc_UWord16 seqNo = (packet->rtpData[2] << 8) + packet->rtpData[3];
            printf("Resend: %u\n", seqNo);
            WebRtc_Word32 ret = SendPacket(packet->rtpData, packet->rtpLen);
            ListItem* itemToRemove = item;
            _lostPackets.Lock();
            item = _lostPackets.Next(item);
            _lostPackets.Erase(itemToRemove);
            delete packet;
            _lostPackets.Unlock();
            _resendPacketCount++;
            if (ret > 0)
            {
                _lostPackets.ResentPacket(seqNo);
            }
            else if (ret < 0)
            {
                return ret;
            }
        }
        else
        {
            _lostPackets.Lock();
            item = _lostPackets.Next(item);
            _lostPackets.Unlock();
        }
    }

    // Send any packets from rtp file
    if (!_endOfFile && (TimeUntilNextPacket() == 0 || _firstPacket))
    {
        _rtpModule.Process();
        if (_firstPacket)
        {
            _firstPacketRtpTime = static_cast<WebRtc_Word64>(_nextRtpTime);
            _firstPacketTimeMs = VCMTickTime::MillisecondTimestamp();
        }
        if (_reordering && _reorderBuffer == NULL)
        {
            _reorderBuffer = new RawRtpPacket(reinterpret_cast<WebRtc_UWord8*>(_nextPacket), static_cast<WebRtc_UWord16>(_nextPacketLength));
            return 0;
        }
        WebRtc_Word32 ret = SendPacket(reinterpret_cast<WebRtc_UWord8*>(_nextPacket), static_cast<WebRtc_UWord16>(_nextPacketLength));
        if (_reordering && _reorderBuffer != NULL)
        {
            RawRtpPacket* rtpPacket = _reorderBuffer;
            _reorderBuffer = NULL;
            SendPacket(rtpPacket->rtpData, rtpPacket->rtpLen);
            delete rtpPacket;
        }
        _firstPacket = false;
        if (ret < 0)
        {
            return ret;
        }
        _nextPacketLength = ReadPacket(_nextPacket, &_nextRtpTime);
        if (_nextPacketLength < 0)
        {
            _endOfFile = true;
            return 0;
        }
        else if (_nextPacketLength == 0)
        {
            return 0;
        }
    }
    if (_endOfFile && _lostPackets.NumberOfPacketsToResend() == 0)
    {
        return 1;
    }
    return 0;
}

WebRtc_Word32 RTPPlayer::SendPacket(WebRtc_UWord8* rtpData, WebRtc_UWord16 rtpLen)
{
    if ((_randVec[(_randVecPos++) % RAND_VEC_LENGTH] + 1.0)/(RAND_MAX + 1.0) < _lossRate &&
        _noLossStartup < 0)
    {
        if (_nackEnabled)
        {
            const WebRtc_UWord16 seqNo = (rtpData[2] << 8) + rtpData[3];
            printf("Throw: %u\n", seqNo);
            _lostPackets.AddPacket(rtpData, rtpLen);
            return 0;
        }
    }
    else
    {
        WebRtc_Word32 ret = _rtpModule.IncomingPacket(rtpData, rtpLen);
        if (ret < 0)
        {
            return -1;
        }
    }
    if (_noLossStartup >= 0)
    {
        _noLossStartup--;
    }
    return 1;
}

WebRtc_Word32 RTPPlayer::ReadPacket(WebRtc_Word16* rtpdata, WebRtc_UWord32* offset)
{
    WebRtc_UWord16 length, plen;

    if (fread(&length,2,1,_rtpFile)==0)
        return(-1);
    length=ntohs(length);

    if (fread(&plen,2,1,_rtpFile)==0)
        return(-1);
    plen=ntohs(plen);

    if (fread(offset,4,1,_rtpFile)==0)
        return(-1);
    *offset=ntohl(*offset);

    // Use length here because a plen of 0 specifies rtcp
    length = (WebRtc_UWord16) (length - HDR_SIZE);
    if (fread((unsigned short *) rtpdata,1,length,_rtpFile) != length)
        return(-1);

#ifdef JUNK_DATA
    // destroy the RTP payload with random data
    if (plen > 12) { // ensure that we have more than just a header
        for ( int ix = 12; ix < plen; ix=ix+2 ) {
            rtpdata[ix>>1] = (short) (rtpdata[ix>>1] + (short) rand());
        }
    }
#endif
    return plen;
}

WebRtc_Word32 RTPPlayer::SimulatePacketLoss(float lossRate, bool enableNack, WebRtc_UWord32 rttMs)
{
    _nackEnabled = enableNack;
    _lossRate = lossRate;
    _rttMs = rttMs;
    return 0;
}

WebRtc_Word32 RTPPlayer::SetReordering(bool enabled)
{
    _reordering = enabled;
    return 0;
}

WebRtc_Word32 RTPPlayer::ResendPackets(const WebRtc_UWord16* sequenceNumbers, WebRtc_UWord16 length)
{
    if (sequenceNumbers == NULL)
    {
        return 0;
    }
    for (int i=0; i < length; i++)
    {
        _lostPackets.SetResendTime(sequenceNumbers[i], VCMTickTime::MillisecondTimestamp() + _rttMs);
    }
    return 0;
}

void RTPPlayer::Print() const
{
    printf("Lost packets: %u, resent packets: %u\n", _lostPackets.TotalNumberOfLosses(), _resendPacketCount);
    printf("Packets still lost: %u\n", _lostPackets.GetSize());
    printf("Packets waiting to be resent: %u\n", _lostPackets.NumberOfPacketsToResend());
    printf("Sequence numbers:\n");
    ListItem* item = _lostPackets.First();
    while (item != NULL)
    {
        RawRtpPacket* packet = static_cast<RawRtpPacket*>(item->GetItem());
        const WebRtc_UWord16 seqNo = (packet->rtpData[2] << 8) + packet->rtpData[3];
        printf("%u, ", seqNo);
        item = _lostPackets.Next(item);
    }
    printf("\n");
}
