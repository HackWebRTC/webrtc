/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

//
// tb_external_transport.cc
//

#include "tb_external_transport.h"

#include <stdlib.h> // rand
#if defined(WEBRTC_LINUX) || defined(__linux__)
#include <stdlib.h>
#include <string.h>
#endif
#if defined(WEBRTC_MAC)
#include <cstring>
#endif

#include "critical_section_wrapper.h"
#include "event_wrapper.h"
#include "thread_wrapper.h"
#include "tick_util.h"
#include "vie_network.h"

#if defined(_WIN32)
#pragma warning(disable: 4355) // 'this' : used in base member initializer list
#endif

tbExternalTransport::tbExternalTransport(webrtc::ViENetwork& vieNetwork) :
        _vieNetwork(vieNetwork),
        _thread(*webrtc::ThreadWrapper::CreateThread(
            ViEExternalTransportRun, this, webrtc::kHighPriority,
            "AutotestTransport")),
        _event(*webrtc::EventWrapper::Create()),
        _crit(*webrtc::CriticalSectionWrapper::CreateCriticalSection()),
        _statCrit(*webrtc::CriticalSectionWrapper::CreateCriticalSection()),
        _lossRate(0),
        _networkDelayMs(0),
        _rtpCount(0),
        _rtcpCount(0),
        _dropCount(0),
        _rtpPackets(),
        _rtcpPackets(),
        _checkSSRC(false),
        _lastSSRC(0),
        _filterSSRC(false),
        _SSRC(0),
        _checkSequenceNumber(0),
        _firstSequenceNumber(0)
{
    srand((int) webrtc::TickTime::MicrosecondTimestamp());
    unsigned int tId = 0;
    _thread.Start(tId);
}

tbExternalTransport::~tbExternalTransport()
{
    _thread.SetNotAlive();
    _event.Set();
    if (_thread.Stop())
    {
        delete &_thread;
        delete &_event;
    }
    delete &_crit;
    delete &_statCrit;
}

int tbExternalTransport::SendPacket(int channel, const void *data, int len)
{
    if (_filterSSRC)
    {
        WebRtc_UWord8* ptr = (WebRtc_UWord8*)data;
        WebRtc_UWord32 ssrc = ptr[8] << 24;
        ssrc += ptr[9] << 16;
        ssrc += ptr[10] << 8;
        ssrc += ptr[11];
        if (ssrc != _SSRC)
        {  
            return len; // return len to avoif error in trace file
        }
    }
    _statCrit.Enter();
    _rtpCount++;
    _statCrit.Leave();

    // Packet loss
    int dropThis = rand() % 100;
    if (dropThis < _lossRate)
    {
        _statCrit.Enter();
        _dropCount++;
        _statCrit.Leave();
        return 0;
    }

    VideoPacket* newPacket = new VideoPacket();
    memcpy(newPacket->packetBuffer, data, len);
    newPacket->length = len;
    newPacket->channel = channel;

    _crit.Enter();
    newPacket->receiveTime = NowMs() + _networkDelayMs;
    _rtpPackets.PushBack(newPacket);
    _event.Set();
    _crit.Leave();
    return len;
}

int tbExternalTransport::SendRTCPPacket(int channel, const void *data, int len)
{
    _statCrit.Enter();
    _rtcpCount++;
    _statCrit.Leave();

    VideoPacket* newPacket = new VideoPacket();
    memcpy(newPacket->packetBuffer, data, len);
    newPacket->length = len;
    newPacket->channel = channel;

    _crit.Enter();
    newPacket->receiveTime = NowMs() + _networkDelayMs;
    _rtcpPackets.PushBack(newPacket);
    _event.Set();
    _crit.Leave();
    return len;
}

WebRtc_Word32 tbExternalTransport::SetPacketLoss(WebRtc_Word32 lossRate)
{
    webrtc::CriticalSectionScoped cs(_statCrit);
    _lossRate = lossRate;
    return 0;
}

void tbExternalTransport::SetNetworkDelay(WebRtc_Word64 delayMs)
{
    webrtc::CriticalSectionScoped cs(_crit);
    _networkDelayMs = delayMs;
}

void tbExternalTransport::SetSSRCFilter(WebRtc_UWord32 ssrc)
{
    webrtc::CriticalSectionScoped cs(_crit);
    _filterSSRC = true;
    _SSRC = ssrc;
}

void tbExternalTransport::ClearStats()
{
    webrtc::CriticalSectionScoped cs(_statCrit);
    _rtpCount = 0;
    _dropCount = 0;
    _rtcpCount = 0;
}

void tbExternalTransport::GetStats(WebRtc_Word32& numRtpPackets,
                                   WebRtc_Word32& numDroppedPackets,
                                   WebRtc_Word32& numRtcpPackets)
{
    webrtc::CriticalSectionScoped cs(_statCrit);
    numRtpPackets = _rtpCount;
    numDroppedPackets = _dropCount;
    numRtcpPackets = _rtcpCount;
}

void tbExternalTransport::EnableSSRCCheck()
{
    webrtc::CriticalSectionScoped cs(_statCrit);
    _checkSSRC = true;
}

unsigned int tbExternalTransport::ReceivedSSRC()
{
    webrtc::CriticalSectionScoped cs(_statCrit);
    return _lastSSRC;
}

void tbExternalTransport::EnableSequenceNumberCheck()
{
    webrtc::CriticalSectionScoped cs(_statCrit);
    _checkSequenceNumber = true;
}

unsigned short tbExternalTransport::GetFirstSequenceNumber()
{
    webrtc::CriticalSectionScoped cs(_statCrit);
    return _firstSequenceNumber;
}

bool tbExternalTransport::ViEExternalTransportRun(void* object)
{
    return static_cast<tbExternalTransport*>
        (object)->ViEExternalTransportProcess();
}
bool tbExternalTransport::ViEExternalTransportProcess()
{
    unsigned int waitTime = KMaxWaitTimeMs;

    VideoPacket* packet = NULL;

    while (!_rtpPackets.Empty())
    {
        // Take first packet in queue
        _crit.Enter();
        packet = static_cast<VideoPacket*> ((_rtpPackets.First())->GetItem());
        WebRtc_Word64 timeToReceive = packet->receiveTime - NowMs();
        if (timeToReceive > 0)
        {
            // No packets to receive yet
            if (timeToReceive < waitTime && timeToReceive > 0)
            {
                waitTime = (unsigned int) timeToReceive;
            }
            _crit.Leave();
            break;
        }
        _rtpPackets.PopFront();
        _crit.Leave();

        // Send to ViE
        if (packet)
        {
            {
                webrtc::CriticalSectionScoped cs(_statCrit);
                if (_checkSSRC)
                {
                    _lastSSRC = ((packet->packetBuffer[8]) << 24);
                    _lastSSRC += (packet->packetBuffer[9] << 16);
                    _lastSSRC += (packet->packetBuffer[10] << 8);
                    _lastSSRC += packet->packetBuffer[11];
                    _checkSSRC = false;
                }
                if (_checkSequenceNumber)
                {
                    _firstSequenceNumber
                        = (unsigned char) packet->packetBuffer[2] << 8;
                    _firstSequenceNumber
                        += (unsigned char) packet->packetBuffer[3];
                    _checkSequenceNumber = false;
                }
            }
            _vieNetwork.ReceivedRTPPacket(packet->channel,
                                          packet->packetBuffer, packet->length);
            delete packet;
            packet = NULL;
        }
    }
    while (!_rtcpPackets.Empty())
    {
        // Take first packet in queue
        _crit.Enter();
        packet = static_cast<VideoPacket*> ((_rtcpPackets.First())->GetItem());
        WebRtc_Word64 timeToReceive = packet->receiveTime - NowMs();
        if (timeToReceive > 0)
        {
            // No packets to receive yet
            if (timeToReceive < waitTime && timeToReceive > 0)
            {
                waitTime = (unsigned int) timeToReceive;
            }
            _crit.Leave();
            break;
        }
        _rtcpPackets.PopFront();
        _crit.Leave();

        // Send to ViE
        if (packet)
        {
            _vieNetwork.ReceivedRTCPPacket(
                 packet->channel,
                 packet->packetBuffer, packet->length);
            delete packet;
            packet = NULL;
        }
    }
    _event.Wait(waitTime + 1); // Add 1 ms to not call to early...
    return true;
}

WebRtc_Word64 tbExternalTransport::NowMs()
{
    return webrtc::TickTime::MillisecondTimestamp();
}
