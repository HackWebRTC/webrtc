/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

// testAPI.cpp : Defines the entry point for the console application.
//
#define _CRTDBG_MAP_ALLOC
#include <stdlib.h>
#include <crtdbg.h>

#include <cassert>
#include <windows.h>
#include <iostream>
#include <tchar.h>

#include "rtp_rtcp.h"
#include "common_types.h"
#include "process_thread.h"
#include "trace.h"

#include "../source/ModuleRtpRtcpImpl.h"

#define TEST_AUDIO
#define TEST_VIDEO

WebRtc_UWord8  _payloadDataFile[65000];
WebRtc_UWord16 _payloadDataFileLength;
#define VIDEO_NACK_LIST_SIZE 30

class LoopBackTransport : public webrtc::Transport
{
public:
    LoopBackTransport(RtpRtcp* rtpRtcpModule)  :
      _rtpRtcpModule(rtpRtcpModule)
    {
        _sendCount = 0;
    }
    virtual int SendPacket(int channel, const void *data, int len)
    {
        _sendCount++;
        if(_sendCount > 500 && _sendCount <= 510)
        {
            // drop 10 packets
            printf("\tDrop packet\n");
            return len;
        }
        if(_rtpRtcpModule->IncomingPacket((const WebRtc_UWord8*)data, len) == 0)
        {
            return len;
        }
        return -1;
    }
    virtual int SendRTCPPacket(int channel, const void *data, int len)
    {
        if(_rtpRtcpModule->IncomingPacket((const WebRtc_UWord8*)data, len) == 0)
        {
            return len;
        }
        return -1;
    }
    WebRtc_UWord32        _sendCount;
    RtpRtcp*  _rtpRtcpModule;
};

class DataRelayReceiverVideo : public RtpData
{
public:
    DataRelayReceiverVideo(RtpRtcp* rtpRtcpModule) : _rtpRtcpModule(rtpRtcpModule)
    {}
    virtual WebRtc_Word32 OnReceivedPayloadData(const WebRtc_UWord8* payloadData,
                                              const WebRtc_UWord16 payloadSize,
                                              const webrtc::WebRtcRTPHeader* rtpHeader,
                                              const WebRtc_UWord8* rtpPacket,
                                              const WebRtc_UWord16 rtpPacketSize)
    {
        if(rtpPacketSize == 0)
        {
            // we relay only one packet once, but this function in called for each NALU
            return 0;
        }
        if(_rtpRtcpModule->SendRTPPacket(rtpHeader, rtpPacket, rtpPacketSize) == 0)
        {
            return 0;
        }
        return -1;
    }
    RtpRtcp* _rtpRtcpModule;
};

// Dummy comment, shall be removed
class LoopBackTransportVideo : public webrtc::Transport
{
public:
    LoopBackTransportVideo(RtpRtcp* rtpRtcpModule)  :
      _count(0),
      _packetLoss(0),
      _rtpRtcpModule(rtpRtcpModule)
    {
    }
    virtual int SendPacket(int channel, const void *data, int len)
    {
        if(static_cast<const WebRtc_UWord8*>(data)[0] == 0)
        {
//            printf("\t\tReceived pad data length:%d\n", len);
            return len;
        }
        _count++;
        if(_packetLoss > 0)
        {
            if(_count%_packetLoss == 0)
            {
//                printf("Drop video packet: %u\n", static_cast<const unsigned char*>(data)[3]);
                return len;
            }
//            printf("video packet: %u\n", static_cast<const unsigned char*>(data)[3]);
        } else
        {
//             printf("video packet: %u\n", static_cast<const unsigned char*>(data)[3]);
        }
        if(_rtpRtcpModule->IncomingPacket((const WebRtc_UWord8*)data, len) == 0)
        {
            return len;
        }
        return -1;
    }
    virtual int SendRTCPPacket(int channel, const void *data, int len)
    {
        if(_rtpRtcpModule->IncomingPacket((const WebRtc_UWord8*)data, len) == 0)
        {
            return len;
        }
        return -1;
    }
    WebRtc_UWord32        _packetLoss;
    WebRtc_UWord32        _count;
    WebRtc_UWord32        _time;
    RtpRtcp*  _rtpRtcpModule;
};


class DataReceiver : public RtpData
{
public:
    DataReceiver(RtpRtcp* rtpRtcpModule) :
        _rtpRtcpModule(rtpRtcpModule)
    {
    }

    virtual WebRtc_Word32 OnReceivedPayloadData(const WebRtc_UWord8* payloadData,
                                              const WebRtc_UWord16 payloadSize,
                                              const webrtc::WebRtcRTPHeader* rtpHeader,
                                              const WebRtc_UWord8* rtpPacket,
                                              const WebRtc_UWord16 rtpPacketSize)
    {
//        printf("\tReceived packet:%d payload type:%d length:%d\n", rtpHeader->header.sequenceNumber, rtpHeader->header.payloadType, payloadSize);

        if(rtpHeader->header.payloadType == 98 ||
           rtpHeader->header.payloadType == 99)
        {
            if(strncmp("test", (const char*)payloadData, 4) == 0)
            {
                return 0;
            }
            assert(false);
            return -1;
        }
        if(rtpHeader->header.payloadType == 100 ||
           rtpHeader->header.payloadType == 101 ||
           rtpHeader->header.payloadType == 102)
        {
            if(rtpHeader->type.Audio.channel == 1)
            {
                if(payloadData[0] == 0xff)
                {
                    return 0;
                }
            }else if(rtpHeader->type.Audio.channel == 2)
            {
                if(payloadData[0] == 0x0)
                {
                    return 0;
                }
            }else if(rtpHeader->type.Audio.channel == 3)
            {
                if(payloadData[0] == 0xaa)
                {
                    return 0;
                }
            }
            assert(false);
            return -1;
        }
        if(payloadSize == 10)
        {
            if(strncmp("testEnergy", (const char*)payloadData, 10) == 0)
            {
                if(rtpHeader->type.Audio.numEnergy == 2)
                {
                    if( rtpHeader->type.Audio.arrOfEnergy[0] == 7 &&
                        rtpHeader->type.Audio.arrOfEnergy[1] == 9)
                    {
                        return 0;
                    }
                }
                assert(false);
                return -1;
            }
        }
        return 0;
    }

    RtpRtcp* _rtpRtcpModule;
};

class DataReceiverVideo : public RtpData
{
public:
    DataReceiverVideo() :
        _packetLoss(false),
        _curLength(0)
    {
    }
    void CheckRecivedFrame(bool nack)
    {
         printf("\t\tCheckRecivedFrame\n");
        {
            assert(_curLength == _payloadDataFileLength);
            _curLength = 0;
            if(!nack)
            {
                for (int i = 0; i < _payloadDataFileLength; i++)
                {
                        assert(_receiveBuffer[i] == _payloadDataFile[i]);
                }
            }
        }
    }

    virtual WebRtc_Word32 OnReceivedPayloadData(const WebRtc_UWord8* payloadData,
                                              const WebRtc_UWord16 payloadSize,
                                              const webrtc::WebRtcRTPHeader* rtpHeader,
                                              const WebRtc_UWord8* rtpPacket,
                                              const WebRtc_UWord16 rtpPacketSize)
    {
        if(rtpHeader->frameType == webrtc::kFrameEmpty && payloadSize == 0)
        {
            return 0;
        }
        // store received payload data
        int sByte = 0;
        if (rtpHeader->type.Video.codec == VideoH263 && rtpHeader->type.Video.codecHeader.H263.bits)
        {
            // "or" the first bits
            assert(_curLength > 0);
            _receiveBuffer[_curLength - 1] |= payloadData[0];
            sByte = 1;
        }
        memcpy(&_receiveBuffer[_curLength], &payloadData[sByte], payloadSize - sByte);
        _curLength += payloadSize - sByte;

        if(!_packetLoss)
        {
            if (rtpHeader->header.markerBit && payloadSize)
            {
                 // last packet, compare send and received data stream
                 CheckRecivedFrame(false);
            }
        } else
        {
            for(int i = 0; i < VIDEO_NACK_LIST_SIZE; i++)
            {
                if(_nackList[i] == rtpHeader->header.sequenceNumber)
                {
                    _nackList[i] = -1;
                    break;
                }
            }
        }
        return 0;
    }

    bool                _packetLoss;
    WebRtc_Word32         _nackList[VIDEO_NACK_LIST_SIZE];
    WebRtc_UWord8        _receiveBuffer[100000];
    WebRtc_UWord32       _curLength;
};

class VideoFeedback : public RtpVideoFeedback
{
    virtual void OnReceivedIntraFrameRequest(const WebRtc_Word32 id,
                                             const WebRtc_UWord8 message)
    {
        printf("\tReceived video IntraFrameRequest message:%d \n", message);
    };

    virtual void OnNetworkChanged(const WebRtc_Word32 id,
                                  const WebRtc_UWord32 bitrateTarget,
                                  const WebRtc_UWord8 fractionLost,
                                  const WebRtc_UWord16 roundTripTimeMs,
                                  const WebRtc_UWord32 jitterMS,
                                  const WebRtc_UWord16 bwEstimateKbitMin,
                                  const WebRtc_UWord16 bwEstimateKbitMax)
    {
        static int count = 0;
        count++;
        const WebRtc_UWord32 bitrateTargetKbit = bitrateTarget/1000;

        // todo jitter is not valid due to send rate
        if(count == 1)
        {
            assert(3667 >= bwEstimateKbitMax);
            assert(fractionLost >= 80 && fractionLost < 150);
            assert(300 == bitrateTargetKbit); // no inc due to no fraction loss

        } else if(count == 2)
        {
            assert(1517 == bwEstimateKbitMax);
            assert(0 == fractionLost);
            assert(300 == bitrateTargetKbit); // no inc due to no actual bitrate
        } else if(count == 3)
        {
            assert(1517 == bwEstimateKbitMax);
            assert(0 == fractionLost);
            assert(220 == bitrateTargetKbit);
        } else if(count == 4)
        {
            assert(0 == fractionLost);
            assert(243 == bitrateTargetKbit);
        } else
        {
            assert(10 == jitterMS);
            assert(4 == fractionLost);
        }

        printf("\tReceived video OnNetworkChanged bitrateTargetKbit:%d RTT:%d Loss:%d\n", bitrateTargetKbit, roundTripTimeMs, fractionLost);
    };
};

class AudioFeedback : public RtpAudioFeedback
{
    virtual void OnReceivedTelephoneEvent(const WebRtc_Word32 id,
                                          const WebRtc_UWord8 event,
                                          const bool end)
    {
        static WebRtc_UWord8 expectedEvent = 0;

        if(end)
        {
            WebRtc_UWord8 oldEvent = expectedEvent-1;
            if(expectedEvent == 32)
            {
                oldEvent = 15;
            }
#if 0 // test of multiple events
            else if(expectedEvent == 34)
            {
                oldEvent = 32;
                expectedEvent = 33;
            }else if(expectedEvent == 33)
            {
                oldEvent = 33;
                expectedEvent = 34;
            }
#endif
            assert(oldEvent == event);
        }else
        {
            assert(expectedEvent == event);
            expectedEvent++;
        }
        if(expectedEvent == 16)
        {
            expectedEvent = 32;
        }

        if(end)
        {
            printf("\tReceived End of DTMF event:%d with id:%d\n", event, id);
        }else
        {
            printf("\tReceived Start of DTMF event:%d with id:%d\n", event, id);
        }
    }
    virtual void OnPlayTelephoneEvent(const WebRtc_Word32 id,
                            const WebRtc_UWord8 event,
                            const WebRtc_UWord16 lengthMs,
                            const WebRtc_UWord8 volume)
    {
        printf("\tPlayout DTMF event:%d time:%d ms volume:%d with id:%d\n", event, lengthMs,volume, id);
    };
};

class RtcpFeedback : public RtcpFeedback
{
public:
    RtcpFeedback()
    {
        _rtpRtcpModule = NULL;
        _rtpRtcpModuleRelay = NULL;
    };
    virtual void OnRTCPPacketTimeout(const WebRtc_Word32 id)
    {
        printf("\tReceived OnPacketTimeout for RTCP id:%d\n", id);
    }

    // if audioVideoOffset > 0 video is behind audio
    virtual void OnLipSyncUpdate(const WebRtc_Word32 id,
                                 const WebRtc_Word32 audioVideoOffset)
    {
//        printf("\tReceived OnLipSyncUpdate:%d with id:%d\n", audioVideoOffset, id);
    };
    virtual void OnTMMBRReceived(const WebRtc_Word32 id,
                                 const WebRtc_UWord16 bwEstimateKbit)
    {
        printf("\tReceived OnTMMBRReceived:%d with id:%d\n", bwEstimateKbit, id);
    };

    virtual void OnXRVoIPMetricReceived(const WebRtc_Word32 id,
                                        const RTCPVoIPMetric* metric,
                                        const WebRtc_Word8 VoIPmetricBuffer[28])
    {
        printf("\tOnXRVoIPMetricReceived:%d with id:%d\n", metric->burstDensity, id);
    };
    virtual void OnSLIReceived(const WebRtc_Word32 id,
                               const WebRtc_UWord8 pictureId)
    {
        printf("\tReceived OnSLIReceived:%d with id:%d\n", pictureId, id);
        assert(pictureId == 28);
    };

    virtual void OnRPSIReceived(const WebRtc_Word32 id,
                                const WebRtc_UWord64 pictureId)
    {
        printf("\tReceived OnRPSIReceived:%d with id:%d\n", pictureId, id);
        assert(pictureId == 12345678);
    };

    virtual void OnApplicationDataReceived(const WebRtc_Word32 id,
                                           const WebRtc_UWord8 subType,
                                           const WebRtc_UWord32 name,
                                           const WebRtc_UWord16 length,
                                           const WebRtc_UWord8* data)
    {
        WebRtc_Word8 printName[5];
        printName[0] = (WebRtc_Word8)(name >> 24);
        printName[1] = (WebRtc_Word8)(name >> 16);
        printName[2] = (WebRtc_Word8)(name >> 8);
        printName[3] = (WebRtc_Word8)name;
        printName[4] = 0;

        WebRtc_Word8* printData = new WebRtc_Word8[length+1];
        memcpy(printData, data, length);
        printData[length] = 0;

        printf("\tOnApplicationDataReceived subtype:%d name:%s data:%s with id:%d\n", subType, printName, printData, id);

        assert(strncmp("test",printName, 5)  == 0);
        delete [] printData;
    };

    virtual void OnSendReportReceived(const WebRtc_Word32 id,
                                      const WebRtc_UWord32 senderSSRC,
                                      const WebRtc_UWord8* incomingPacket,
                                      const WebRtc_UWord16 packetLength)
    {
        printf("\tOnSendReportReceived RTCP id:%d\n", id);

        if(_rtpRtcpModule)
        {
            RTCPSenderInfo senderInfo;
            assert(_rtpRtcpModule->RemoteRTCPStat(&senderInfo) == 0);
            senderInfo.sendOctetCount;
            senderInfo.sendPacketCount;
        }
        if(_rtpRtcpModuleRelay)
        {
            // relay packet
            _rtpRtcpModuleRelay->SendRTCPPacket(incomingPacket, packetLength);
        }
    };

    // for relay conferencing
    virtual void OnReceiveReportReceived(const WebRtc_Word32 id,
                                         const WebRtc_UWord32 senderSSRC,
                                         const WebRtc_UWord8* incomingPacket,
                                         const WebRtc_UWord16 packetLength)
    {
        WebRtc_UWord16 RTT = 0;
        WebRtc_UWord32 remoteSSRC;
        switch(id)
        {
        case 123:
            remoteSSRC = 124;
            break;
        case 124:
            remoteSSRC = 123;
            break;
        case 125:
            remoteSSRC = 126;
            break;
        case 126:
            remoteSSRC = 125;
            break;
        default:
            assert(false);
        }

            _rtpRtcpModule->RTT(remoteSSRC, &RTT,NULL,NULL,NULL);

            printf("\tOnReceiveReportReceived RTT:%d RTCP id:%d\n", RTT, id);
        if(_rtpRtcpModuleRelay)
        {
            // relay packet
            _rtpRtcpModuleRelay->SendRTCPPacket(incomingPacket, packetLength);
        }
    };
    RtpRtcp* _rtpRtcpModule;
    RtpRtcp* _rtpRtcpModuleRelay;
};

class RTPCallback : public RtpFeedback
{
public:
    virtual WebRtc_Word32 OnInitializeDecoder(const WebRtc_Word32 id,
                                            const WebRtc_Word8 payloadType,
                                            const WebRtc_Word8 payloadName[RTP_PAYLOAD_NAME_SIZE],
                                            const WebRtc_UWord32 frequency,
                                            const WebRtc_UWord8 channels,
                                            const WebRtc_UWord32 rate)
    {
        if(payloadType == 96)
        {
            assert(rate == 64000);
        }
        printf("\tReceived OnInitializeDecoder \n\t\tpayloadName:%s \n\t\tpayloadType:%d \n\t\tfrequency:%d \n\t\tchannels:%d \n\t\trate:%d  \n\t\twith id:%d\n", payloadName,payloadType,frequency, channels, rate, id);
        return 0;
    }

    virtual void OnPacketTimeout(const WebRtc_Word32 id)
    {
        printf("\tReceived OnPacketTimeout\n");
    }

    virtual void OnReceivedPacket(const WebRtc_Word32 id,
                                  const RtpRtcpPacketType packetType)
    {
        printf("\tReceived OnReceivedPacket\n");
    }

    virtual void OnPeriodicDeadOrAlive(const WebRtc_Word32 id,
                                       const RTPAliveType alive)
    {
        printf("\tReceived OnPeriodicDeadOrAlive\n");
    }

    virtual void OnIncomingSSRCChanged( const WebRtc_Word32 id,
                                        const WebRtc_UWord32 SSRC)
    {
        printf("\tReceived OnIncomingSSRCChanged\n");
    }

    virtual void OnIncomingCSRCChanged( const WebRtc_Word32 id,
                                        const WebRtc_UWord32 CSRC,
                                        const bool added)
    {
        printf("\tReceived OnIncomingCSRCChanged\n");
    }
};

// todo look at VE 3.0 test app
int _tmain(int argc, _TCHAR* argv[])
{
//    _crtBreakAlloc = 17967;

    WebRtc_Word8 fileName[1024] = "testTrace.txt";
    Trace::CreateTrace();
    Trace::SetTraceFile(fileName);
    memcpy(fileName, "testTraceDebug.txt", 19);
    Trace::SetEncryptedTraceFile(fileName);
    Trace::SetLevelFilter(webrtc::kTraceAll);

    int myId = 123;
    ProcessThread* processThread = ProcessThread::CreateProcessThread();
    processThread->Start();

#ifdef TEST_AUDIO
    // test all APIs in RTP/RTCP module
    RtpRtcp* rtpRtcpModule1 = RtpRtcp::CreateRtpRtcp(myId,
                                                                                  true);    // audio

    RtpRtcp* rtpRtcpModule2 = RtpRtcp::CreateRtpRtcp(myId+1,
                                                                                   true);    // audio

    processThread->RegisterModule(rtpRtcpModule1);
    processThread->RegisterModule(rtpRtcpModule2);

    printf("Welcome to API test of RTP/RTCP module\n");

    WebRtc_Word8 version[256];
    WebRtc_UWord32 remainingBufferInBytes = 256;
    WebRtc_UWord32 position = 0;
    assert( 0 == rtpRtcpModule1->Version(version, remainingBufferInBytes, position));
    assert(-1 == rtpRtcpModule1->Version(NULL, remainingBufferInBytes, position));
    printf("\nVersion\n\t%s\n\n", version);

    assert( 0 == rtpRtcpModule1->InitReceiver());
    assert( 0 == rtpRtcpModule1->InitSender());

    assert( 0 == rtpRtcpModule2->InitReceiver());
    assert( 0 == rtpRtcpModule2->InitSender());

    printf("\tInitialization done\n");

    assert(-1 == rtpRtcpModule1->SetMaxTransferUnit(10));
    assert(-1 == rtpRtcpModule1->SetMaxTransferUnit(IP_PACKET_SIZE + 1));
    assert( 0 == rtpRtcpModule1->SetMaxTransferUnit(1234));
    assert(1234-20-8 == rtpRtcpModule1->MaxPayloadLength());

    assert( 0 == rtpRtcpModule1->SetTransportOverhead(true, true, 12));
    assert(1234 - 20- 20 -20 - 12 == rtpRtcpModule1->MaxPayloadLength());

    assert( 0 == rtpRtcpModule1->SetTransportOverhead(false, false, 0));
    assert(1234 - 20 - 8== rtpRtcpModule1->MaxPayloadLength());

    assert( 0 == rtpRtcpModule1->SetSequenceNumber(2345));
    assert(2345 == rtpRtcpModule1->SequenceNumber());

    assert( 0 == rtpRtcpModule1->SetSSRC(3456));
    assert(3456 == rtpRtcpModule1->SSRC());

    assert( 0 == rtpRtcpModule1->SetStartTimestamp(4567));
    assert(4567 == rtpRtcpModule1->StartTimestamp());

    assert(0 == rtpRtcpModule1->SetAudioEnergy(NULL,0));

    WebRtc_UWord32 arrOfCSRC[webrtc::kRtpCsrcSize] = {1234,2345};
    WebRtc_UWord32 testOfCSRC[webrtc::kRtpCsrcSize] = {0,0,0};
    assert( 0 == rtpRtcpModule1->SetCSRCs(arrOfCSRC,2));
    assert( 2 == rtpRtcpModule1->CSRCs(testOfCSRC));
    assert(arrOfCSRC[0] == testOfCSRC[0]);
    assert(arrOfCSRC[1] == testOfCSRC[1]);

    assert( kRtcpOff == rtpRtcpModule1->RTCP());
    assert(0 == rtpRtcpModule1->SetRTCPStatus(kRtcpCompound));
    assert( kRtcpCompound == rtpRtcpModule1->RTCP());

    assert( kRtcpOff == rtpRtcpModule2->RTCP());
    assert(0 == rtpRtcpModule2->SetRTCPStatus(kRtcpCompound));
    assert( kRtcpCompound == rtpRtcpModule2->RTCP());

    assert( 0 == rtpRtcpModule1->SetCNAME("john.doe@test.test"));
    assert( 0 == rtpRtcpModule2->SetCNAME("jane.doe@test.test"));
    assert(-1 == rtpRtcpModule1->SetCNAME(NULL));
    WebRtc_Word8 cName[RTCP_CNAME_SIZE];
    assert(0 == rtpRtcpModule1->CNAME(cName));
    assert(0 == strncmp(cName, "john.doe@test.test", RTCP_CNAME_SIZE));
    assert(-1 == rtpRtcpModule1->CNAME(NULL));

    assert( false == rtpRtcpModule1->TMMBR());
    assert(0 == rtpRtcpModule1->SetTMMBRStatus(true));
    assert( true == rtpRtcpModule1->TMMBR());
    assert(0 == rtpRtcpModule1->SetTMMBRStatus(false));
    assert( false == rtpRtcpModule1->TMMBR());

    assert( kNackOff == rtpRtcpModule1->NACK());
    assert(0 == rtpRtcpModule1->SetNACKStatus(kNackRtcp));
    assert( kNackRtcp == rtpRtcpModule1->NACK());

    assert( false == rtpRtcpModule1->Sending());
    assert(0 == rtpRtcpModule1->SetSendingStatus(true));
    assert( true == rtpRtcpModule1->Sending());
    assert(0 == rtpRtcpModule2->SetSendingStatus(true));

    // audio specific
    assert( false == rtpRtcpModule1->TelephoneEvent());
    assert(0 == rtpRtcpModule2->SetTelephoneEventStatus(true, true, true)); // to test detection at the end of a DTMF tone
    assert( true == rtpRtcpModule2->TelephoneEvent());

    printf("Basic set/get test done\n");

    // test setup
    DataReceiver* myDataReceiver1 = new DataReceiver(rtpRtcpModule1);
    assert(0 == rtpRtcpModule1->RegisterIncomingDataCallback(myDataReceiver1));

    DataReceiver* myDataReceiver2 = new DataReceiver(rtpRtcpModule2);
    assert(0 == rtpRtcpModule2->RegisterIncomingDataCallback(myDataReceiver2));

    LoopBackTransport* myLoopBackTransport1 = new LoopBackTransport(rtpRtcpModule2);
    assert(0 == rtpRtcpModule1->RegisterSendTransport(myLoopBackTransport1));

    LoopBackTransport* myLoopBackTransport2 = new LoopBackTransport(rtpRtcpModule1);
    assert(0 == rtpRtcpModule2->RegisterSendTransport(myLoopBackTransport2));

    RTPCallback* myRTPCallback = new RTPCallback();
    assert(0 == rtpRtcpModule2->RegisterIncomingRTPCallback(myRTPCallback));

    RtcpFeedback* myRTCPFeedback1 = new RtcpFeedback();
    RtcpFeedback* myRTCPFeedback2 = new RtcpFeedback();
    myRTCPFeedback1->_rtpRtcpModule = rtpRtcpModule1;
    myRTCPFeedback2->_rtpRtcpModule = rtpRtcpModule2;
    assert(0 == rtpRtcpModule1->RegisterIncomingRTCPCallback(myRTCPFeedback1));
    assert(0 == rtpRtcpModule2->RegisterIncomingRTCPCallback(myRTCPFeedback2));

    assert(0 == rtpRtcpModule1->SetSendingStatus(true));

    // start basic RTP test
    // send an empty RTP packet, should fail since we have not registerd the payload type
    assert(-1 == rtpRtcpModule1->SendOutgoingData(webrtc::kAudioFrameSpeech, 96, 0, NULL, 0));

    WebRtc_Word8 payloadName[RTP_PAYLOAD_NAME_SIZE] = "PCMU";

    assert(0 == rtpRtcpModule1->RegisterSendPayload( payloadName, 96, 8000));
    assert(0 == rtpRtcpModule1->RegisterReceivePayload(payloadName, 96, 8000));
    assert(0 == rtpRtcpModule2->RegisterSendPayload( payloadName, 96, 8000));
    assert(0 == rtpRtcpModule2->RegisterReceivePayload( payloadName, 96, 8000, 1, 64000));

    WebRtc_Word8 testPayloadName[RTP_PAYLOAD_NAME_SIZE];
    WebRtc_UWord32 testFrequency = 0;
    WebRtc_Word8 testPayloadType= 0;
    WebRtc_UWord8 testChannels= 0;

    assert(0 == rtpRtcpModule1->ReceivePayload( 96,testPayloadName, &testFrequency, &testChannels));
    assert(0 == strncmp(testPayloadName, payloadName, 4));
    assert(1 == testChannels);

    assert(0 == rtpRtcpModule1->ReceivePayloadType( payloadName,8000,1,&testPayloadType));
    assert(testPayloadType == 96);

    // energy test
    const WebRtc_UWord8 energy[3] = {7,9,3};
    assert(-1 == rtpRtcpModule1->SetAudioEnergy(energy,3)); //should fails since we only have 2 CSRCs
    assert(0 == rtpRtcpModule1->SetAudioEnergy(energy,2));

    // send RTP packet with the data "testtest"
    const WebRtc_UWord8 test[9] = "testtest";
    const WebRtc_UWord8 testEnergy[11] = "testEnergy";
    assert(0 == rtpRtcpModule1->SendOutgoingData(webrtc::kAudioFrameSpeech,96, 0, testEnergy, 10));
    assert(0 == rtpRtcpModule2->SendOutgoingData(webrtc::kAudioFrameSpeech,96, 0, test, 8));
//    assert(-1 == rtpRtcpModule->SendOutgoingData(96, 0, NULL, 4));

    assert(3456 == rtpRtcpModule2->RemoteSSRC());
    assert(4567 == rtpRtcpModule2->RemoteTimestamp());

    assert(0 == rtpRtcpModule1->SetStorePacketsStatus(true, 100));

    assert(-1 == rtpRtcpModule1->SetTFRCStatus(true));
    assert(0 == rtpRtcpModule1->SetAudioEnergy(NULL,0));
    assert(0 == rtpRtcpModule1->SetTFRCStatus(true));

    memcpy(payloadName, "RED",4);
    // Test RED
    assert(0 == rtpRtcpModule1->SetSendREDPayloadType(127));
    WebRtc_Word8 red = 0;
    assert(0 == rtpRtcpModule1->SendREDPayloadType(red));
    assert(127 == red);
    assert(0 == rtpRtcpModule1->RegisterReceivePayload( payloadName, 127));
    assert(0 == rtpRtcpModule2->RegisterReceivePayload( payloadName, 127));

    {
        RTPFragmentationHeader fragmentation;
        fragmentation.fragmentationVectorSize = 2;
        fragmentation.fragmentationLength = new WebRtc_UWord32[2];
        fragmentation.fragmentationLength[0] = 4;
        fragmentation.fragmentationLength[1] = 4;
        fragmentation.fragmentationOffset = new WebRtc_UWord32[2];
        fragmentation.fragmentationOffset[0] = 0;
        fragmentation.fragmentationOffset[1] = 4;
        fragmentation.fragmentationTimeDiff = new WebRtc_UWord16[2];
        fragmentation.fragmentationTimeDiff[0] = 0;
        fragmentation.fragmentationTimeDiff[1] = 0;
        fragmentation.fragmentationPlType = new WebRtc_UWord8[2];
        fragmentation.fragmentationPlType[0] = 96;
        fragmentation.fragmentationPlType[1] = 96;

        // send a RTP packet
        assert(0 == rtpRtcpModule1->SendOutgoingData(webrtc::kAudioFrameSpeech,96, 160, test,8, &fragmentation));
    }
    assert(0 == rtpRtcpModule1->SetSendREDPayloadType(-1));
    assert(-1 == rtpRtcpModule1->SendREDPayloadType(red));

    assert(0 == rtpRtcpModule1->SetStorePacketsStatus(false));

    assert(0 == rtpRtcpModule1->SetTFRCStatus(false));

    printf("Basic RTP test done\n");

    // todo CNG

    AudioFeedback* audioFeedback = new AudioFeedback();
    assert(0 == rtpRtcpModule2->RegisterAudioCallback(audioFeedback));

    // prepare for DTMF
    memcpy(payloadName, "telephone-event",16);
    assert(0 == rtpRtcpModule1->RegisterSendPayload( payloadName, 97, 8000));
    assert(0 == rtpRtcpModule2->RegisterReceivePayload( payloadName, 97));

    // prepare for 3 channel audio 8 bits per sample
    memcpy(payloadName, "PCMA",5);
    assert(0 == rtpRtcpModule1->RegisterSendPayload( payloadName, 98, 8000, 3));
    assert(0 == rtpRtcpModule2->RegisterReceivePayload( payloadName, 98,8000, 3));

    // prepare for 3 channel audio 16 bits per sample
    memcpy(payloadName, "L16",4);
    assert(0 == rtpRtcpModule1->RegisterSendPayload( payloadName, 99, 8000, 3));
    assert(0 == rtpRtcpModule2->RegisterReceivePayload( payloadName, 99, 8000, 3));

    // prepare for 3 channel audio 5 bits per sample
    memcpy(payloadName, "G726-40",8);
    assert(0 == rtpRtcpModule1->RegisterSendPayload( payloadName, 100, 8000, 3));
    assert(0 == rtpRtcpModule2->RegisterReceivePayload( payloadName, 100, 8000, 3));

    // prepare for 3 channel audio 3 bits per sample
    memcpy(payloadName, "G726-24",8);
    assert(0 == rtpRtcpModule1->RegisterSendPayload( payloadName, 101, 8000, 3));
    assert(0 == rtpRtcpModule2->RegisterReceivePayload( payloadName, 101, 8000, 3));

    // prepare for 3 channel audio 2 bits per sample
    memcpy(payloadName, "G726-16",8);
    assert(0 == rtpRtcpModule1->RegisterSendPayload( payloadName, 102, 8000, 3));
    assert(0 == rtpRtcpModule2->RegisterReceivePayload( payloadName, 102, 8000, 3));

    // Start DTMF test

    // Send a DTMF tone using RFC 2833 (4733)
    for(int i = 0; i < 16; i++)
    {
        printf("\tSending tone: %d\n", i);
        assert(0 == rtpRtcpModule1->SendTelephoneEventOutband(i, 160, 10));
    }

    // send RTP packets for 16 tones a 160 ms + 100ms pause between = 2560ms + 1600ms = 4160ms
    int j = 2;
    for(;j <= 250;j++)
    {
        assert(0 == rtpRtcpModule1->SendOutgoingData(webrtc::kAudioFrameSpeech,96, 160*j, test,8));
        Sleep(20);
    }
    printf("Basic DTMF test done\n");

     assert(0 == rtpRtcpModule1->SendTelephoneEventOutband(32, 9000, 10));

    for(;j <= 740;j++)
    {
        assert(0 == rtpRtcpModule1->SendOutgoingData(webrtc::kAudioFrameSpeech,96, 160*j, test,8));
        Sleep(20);
    }

    printf("Start Stereo test\n");
    // test sample based multi channel codec, 3 channels 8 bits
    WebRtc_UWord8 test3channels[15] = "ttteeesssttt";
    assert(0 == rtpRtcpModule1->SendOutgoingData(webrtc::kAudioFrameSpeech,98, 160*j, test3channels,12));
    Sleep(20);
    j++;

    // test sample based multi channel codec, 3 channels 16 bits
    const WebRtc_UWord8 test3channels16[13] = "teteteststst";
    assert(0 == rtpRtcpModule1->SendOutgoingData(webrtc::kAudioFrameSpeech,99, 160*j, test3channels16,12));
    Sleep(20);
    j++;

    // test sample based multi channel codec, 3 channels 5 bits
    test3channels[0] = 0xf8; // 5 ones 3 zeros
    test3channels[1] = 0x2b; // 2 zeros 5 10 1 one
    test3channels[2] = 0xf0; // 4 ones 4 zeros
    test3channels[3] = 0x2b; // 1 zero 5 01 2 ones
    test3channels[4] = 0xe0; // 3 ones 5 zeros
    test3channels[5] = 0x0;
    test3channels[6] = 0x0;
    test3channels[7] = 0x0;
    test3channels[8] = 0x0;
    test3channels[9] = 0x0;
    test3channels[10] = 0x0;
    test3channels[11] = 0x0;
    test3channels[12] = 0x0;
    test3channels[13] = 0x0;
    test3channels[14] = 0x0;

    assert(0 == rtpRtcpModule1->SendOutgoingData(webrtc::kAudioFrameSpeech,100, 160*j, test3channels,15));
    Sleep(20);
    j++;

    // test sample based multi channel codec, 3 channels 3 bits
    test3channels[0] = 0xe2; // 3 ones    3 zeros     2 10
    test3channels[1] = 0xf0; // 1 1       3 ones      3 zeros     1 0
    test3channels[2] = 0xb8; // 2 10      3 ones      3 zeros
    test3channels[3] = 0xa0; // 3 101     5 zeros
    test3channels[4] = 0x0;
    assert(0 == rtpRtcpModule1->SendOutgoingData(webrtc::kAudioFrameSpeech,101, 160*j, test3channels,15));
    Sleep(20);
    j++;

    // test sample based multi channel codec, 3 channels 2 bits
    test3channels[0] = 0xcb; // 2 ones    2 zeros     2 10        2 ones
    test3channels[1] = 0x2c; // 2 zeros   2 10        2 ones      2 zeros
    test3channels[2] = 0xb2; // 2 10      2 ones      2 zeros     2 10
    test3channels[3] = 0xcb; // 2 ones    2 zeros     2 10        2 ones
    test3channels[4] = 0x2c; // 2 zeros   2 10        2 ones      2 zeros
    assert(0 == rtpRtcpModule1->SendOutgoingData(webrtc::kAudioFrameSpeech,102, 160*j, test3channels,15));
    Sleep(20);
    j++;

    for(;j <= 750;j++)
    {
        assert(0 == rtpRtcpModule1->SendOutgoingData(webrtc::kAudioFrameSpeech,96, 160*j, test,8));
        Sleep(20);
    }

    printf("Long tone DTMF test done\n");

    // start basic RTCP test
    assert(0 == rtpRtcpModule1->SendRTCPReferencePictureSelection(12345678));

    assert(0 == rtpRtcpModule1->SendRTCPSliceLossIndication(156));

    testOfCSRC[0] = 0;
    testOfCSRC[1] = 0;
    assert( 2 == rtpRtcpModule2->RemoteCSRCs(testOfCSRC));
    assert(arrOfCSRC[0] == testOfCSRC[0]);
    assert(arrOfCSRC[1] == testOfCSRC[1]);

    // set cname of mixed
    assert( 0 == rtpRtcpModule1->AddMixedCNAME(arrOfCSRC[0], "john@192.168.0.1"));
    assert( 0 == rtpRtcpModule1->AddMixedCNAME(arrOfCSRC[1], "jane@192.168.0.2"));
    assert(-1 == rtpRtcpModule1->AddMixedCNAME(arrOfCSRC[0], NULL));

    assert(-1 == rtpRtcpModule1->RemoveMixedCNAME(arrOfCSRC[0] + 1)); // not added
    assert( 0 == rtpRtcpModule1->RemoveMixedCNAME(arrOfCSRC[1]));
    assert( 0 == rtpRtcpModule1->AddMixedCNAME(arrOfCSRC[1], "jane@192.168.0.2"));

    RTCPReportBlock reportBlock;
    reportBlock.cumulativeLost = 1;
    reportBlock.delaySinceLastSR = 2;
    reportBlock.extendedHighSeqNum= 3;
    reportBlock.fractionLost= 4;
    reportBlock.jitter= 5;
    reportBlock.lastSR= 6;

    // set report blocks
    assert(-1 == rtpRtcpModule1->AddRTCPReportBlock(arrOfCSRC[0], NULL));
    assert( 0 == rtpRtcpModule1->AddRTCPReportBlock(arrOfCSRC[0], &reportBlock));

    reportBlock.lastSR= 7;
    assert(0 == rtpRtcpModule1->AddRTCPReportBlock(arrOfCSRC[1], &reportBlock));

    WebRtc_UWord32 name = 't'<<24;
    name += 'e'<<16;
    name += 's'<<8;
    name += 't';
    assert(0 == rtpRtcpModule1->SetRTCPApplicationSpecificData(3,name,(const WebRtc_UWord8 *)"test test test test test test test test test test test test test test test test test test test test test test test test test test test test test test test test test test test test test test test test test test test test test test test test test test test test test test test test test test test test ",300));

    // send RTCP packet, triggered by timer
    Sleep(8000);

    WebRtc_UWord32 receivedNTPsecs = 0;
    WebRtc_UWord32 receivedNTPfrac = 0;
    WebRtc_UWord32 RTCPArrivalTimeSecs = 0;
    WebRtc_UWord32 RTCPArrivalTimeFrac = 0;

    assert(0 == rtpRtcpModule2->RemoteNTP(&receivedNTPsecs,
                                          &receivedNTPfrac,
                                          &RTCPArrivalTimeSecs,
                                          &RTCPArrivalTimeFrac));

    assert(-1 == rtpRtcpModule2->RemoteCNAME(rtpRtcpModule2->RemoteSSRC() + 1, cName));  // not received
    assert(-1 == rtpRtcpModule2->RemoteCNAME(rtpRtcpModule2->RemoteSSRC(), NULL));

    // check multiple CNAME
    assert(0 == rtpRtcpModule2->RemoteCNAME(rtpRtcpModule2->RemoteSSRC(), cName));
    assert(0 == strncmp(cName, "john.doe@test.test", RTCP_CNAME_SIZE));

    assert(0 == rtpRtcpModule2->RemoteCNAME(arrOfCSRC[0], cName));
    assert(0 == strncmp(cName, "john@192.168.0.1", RTCP_CNAME_SIZE));

    assert(0 == rtpRtcpModule2->RemoteCNAME(arrOfCSRC[1], cName));
    assert(0 == strncmp(cName, "jane@192.168.0.2", RTCP_CNAME_SIZE));

    // get all report blocks
    RTCPReportBlock reportBlockReceived;

    assert(-1 == rtpRtcpModule1->RemoteRTCPStat(rtpRtcpModule1->RemoteSSRC() + 1, &reportBlockReceived)); // not received
    assert(-1 == rtpRtcpModule1->RemoteRTCPStat(rtpRtcpModule1->RemoteSSRC(), NULL));
    assert(0 == rtpRtcpModule1->RemoteRTCPStat(rtpRtcpModule1->RemoteSSRC(), &reportBlockReceived));
    float secSinceLastReport = (float)reportBlockReceived.delaySinceLastSR/65536.0f;
    assert( secSinceLastReport > 0.0f && secSinceLastReport < 7.5f); // audio RTCP max 7.5 sec
    // startSeqNum + number of sent + number of extra due to DTMF
    assert(2345+750+2+16 == reportBlockReceived.extendedHighSeqNum);
    assert(0 == reportBlockReceived.fractionLost);
    // we have dropped 10 packets but since we change codec it's reset
    assert(0 == reportBlockReceived.cumulativeLost);

    WebRtc_UWord8  fraction_lost = 0;  // scale 0 to 255
    WebRtc_UWord32 cum_lost = 0;       // number of lost packets
    WebRtc_UWord32 ext_max = 0;        // highest sequence number received
    WebRtc_UWord32 jitter = 0;
    WebRtc_UWord32 max_jitter = 0;
    assert(0 == rtpRtcpModule2->StatisticsRTP(&fraction_lost, &cum_lost, &ext_max, &jitter, &max_jitter));
    assert(0 == fraction_lost);
    assert(0 == cum_lost);
    assert(2345+750+16+2 == ext_max);
    assert(reportBlockReceived.jitter == jitter);

    WebRtc_UWord16 RTT;
    WebRtc_UWord16 avgRTT;
    WebRtc_UWord16 minRTT;
    WebRtc_UWord16 maxRTT;

    // Get RoundTripTime
    assert(0 == rtpRtcpModule1->RTT(rtpRtcpModule1->RemoteSSRC(),&RTT, &avgRTT, &minRTT, &maxRTT));
    assert(RTT < 10);
    assert(avgRTT < 10);
    assert(minRTT < 10);
    assert(minRTT > 0);
    assert(maxRTT < 10);

/*  since we filter out this in the receiver we can't get it

    assert(0 == rtpRtcpModule2->RemoteRTCPStat(arrOfCSRC[0], &reportBlockReceived));
    assert(reportBlock.cumulativeLost == reportBlockReceived.cumulativeLost);
    assert(reportBlock.delaySinceLastSR == reportBlockReceived.delaySinceLastSR);
    assert(reportBlock.extendedHighSeqNum == reportBlockReceived.extendedHighSeqNum);
    assert(reportBlock.fractionLost == reportBlockReceived.fractionLost);
    assert(reportBlock.jitter == reportBlockReceived.jitter);
    assert(6 == reportBlockReceived.lastSR);

    assert(0 == rtpRtcpModule2->RemoteRTCPStat(arrOfCSRC[1], &reportBlockReceived));
    assert(reportBlock.cumulativeLost == reportBlockReceived.cumulativeLost);
    assert(reportBlock.delaySinceLastSR == reportBlockReceived.delaySinceLastSR);
    assert(reportBlock.extendedHighSeqNum == reportBlockReceived.extendedHighSeqNum);
    assert(reportBlock.fractionLost == reportBlockReceived.fractionLost);
    assert(reportBlock.jitter == reportBlockReceived.jitter);
    assert(reportBlock.lastSR == reportBlockReceived.lastSR);
*/
    // set report blocks
    assert(0 == rtpRtcpModule1->AddRTCPReportBlock(arrOfCSRC[0], &reportBlock));

    // test receive report
    assert(0 == rtpRtcpModule1->SetSendingStatus(false));

    // test that BYE clears the CNAME
    assert(-1 == rtpRtcpModule2->RemoteCNAME(rtpRtcpModule2->RemoteSSRC(), cName));

    // send RTCP packet, triggered by timer
    Sleep(5000);
    printf("\tBasic RTCP test done\n");

    processThread->DeRegisterModule(rtpRtcpModule1);
    processThread->DeRegisterModule(rtpRtcpModule2);

    RtpRtcp::DestroyRtpRtcp(rtpRtcpModule1);
    RtpRtcp::DestroyRtpRtcp(rtpRtcpModule2);

#endif // TEST_AUDIO

#ifdef TEST_VIDEO

    // Test video
    RtpRtcp* rtpRtcpModuleVideo = RtpRtcp::CreateRtpRtcp(myId,
                                                         false);    // video

    assert( 0 == rtpRtcpModuleVideo->InitReceiver());
    assert( 0 == rtpRtcpModuleVideo->InitSender());

    LoopBackTransportVideo* myLoopBackTransportVideo = new LoopBackTransportVideo(rtpRtcpModuleVideo);
    assert(0 == rtpRtcpModuleVideo->RegisterSendTransport(myLoopBackTransportVideo));

    DataReceiverVideo* myDataReceiverVideo = new DataReceiverVideo();
    assert(0 == rtpRtcpModuleVideo->RegisterIncomingDataCallback(myDataReceiverVideo));

    VideoFeedback* myVideoFeedback = new VideoFeedback();
    assert(0 == rtpRtcpModuleVideo->RegisterIncomingVideoCallback(myVideoFeedback));

    printf("Start video test\n");
    WebRtc_UWord32 timestamp = 3000;
    WebRtc_Word8 payloadNameVideo[RTP_PAYLOAD_NAME_SIZE] = "I420";

    assert(0 == rtpRtcpModuleVideo->RegisterSendPayload(payloadNameVideo, 123));
    assert(0 == rtpRtcpModuleVideo->RegisterReceivePayload(payloadNameVideo, 123));

    _payloadDataFileLength = (WebRtc_UWord16)sizeof(_payloadDataFile);

    for(int n = 0; n< _payloadDataFileLength; n++)
    {
        _payloadDataFile[n] = n%10;
    }

    printf("\tSending I420 frame. Length: %d\n", _payloadDataFileLength);
    assert(0 == rtpRtcpModuleVideo->SendOutgoingData(webrtc::kVideoFrameDelta,123, timestamp, _payloadDataFile, _payloadDataFileLength));

    memcpy(payloadNameVideo, "MP4V-ES", 8);
    assert(0 == rtpRtcpModuleVideo->RegisterSendPayload(payloadNameVideo, 122));
    assert(0 == rtpRtcpModuleVideo->RegisterReceivePayload(payloadNameVideo, 122));

    // fake a MPEG-4 coded stream
    for (int m = 500; m< _payloadDataFileLength; m+= 500)
    {
        // start codes
        _payloadDataFile[m] = 0;
        _payloadDataFile[m+1] = 0;
    }
    printf("\tSending MPEG-4 frame. Length: %d\n", _payloadDataFileLength);
    assert(0 == rtpRtcpModuleVideo->SendOutgoingData(webrtc::kVideoFrameDelta,122, timestamp, _payloadDataFile, _payloadDataFileLength));

    memcpy(payloadNameVideo, "H263-1998", 10);
    assert(0 == rtpRtcpModuleVideo->RegisterSendPayload(payloadNameVideo, 124));
    assert(0 == rtpRtcpModuleVideo->RegisterReceivePayload(payloadNameVideo, 124));

    // Test send H.263 frame
    FILE* openFile = fopen("H263_CIF_IFRAME.bin", "rb");
    assert(openFile != NULL);
    fseek(openFile, 0, SEEK_END);
    _payloadDataFileLength = (WebRtc_Word16)(ftell(openFile));
    rewind(openFile);
    assert(_payloadDataFileLength > 0);
    fread(_payloadDataFile, 1, _payloadDataFileLength, openFile);
    fclose(openFile);

    // send frame (1998/2000)
    printf("\tSending H263(1998) frame. Length: %d\n", _payloadDataFileLength);
    assert(0 == rtpRtcpModuleVideo->SendOutgoingData(webrtc::kVideoFrameDelta,124, timestamp, _payloadDataFile, _payloadDataFileLength));

    memcpy(payloadNameVideo, "H263",5);
    assert(0 == rtpRtcpModuleVideo->RegisterSendPayload(payloadNameVideo, 34));
    assert(0 == rtpRtcpModuleVideo->RegisterReceivePayload(payloadNameVideo, 34));

    timestamp += 3000;

    // send frame
    printf("\tSending H263 frame. Length: %d\n", _payloadDataFileLength);
    assert(0 == rtpRtcpModuleVideo->SendOutgoingData(webrtc::kVideoFrameDelta,34, timestamp, _payloadDataFile, _payloadDataFileLength));
    timestamp += 3000;

    // lower MTU -> mode B
    printf("\tSending H263 frame (MTU 300). Length: %d\n", _payloadDataFileLength);
    assert(0 == rtpRtcpModuleVideo->SetMaxTransferUnit(300));
    assert(0 == rtpRtcpModuleVideo->SendOutgoingData(webrtc::kVideoFrameDelta,34, timestamp, _payloadDataFile, _payloadDataFileLength));

    timestamp += 3000;
    // get frame w/ non-byte aligned GOB headers
    openFile = fopen("H263_QCIF_IFRAME.bin", "rb");
    assert(openFile != NULL);
    fseek(openFile, 0, SEEK_END);
    _payloadDataFileLength = (WebRtc_Word16)(ftell(openFile));
    rewind(openFile);
    assert(_payloadDataFileLength > 0);
    fread(_payloadDataFile, 1, _payloadDataFileLength, openFile);
    fclose(openFile);

    // send frame
    printf("\tSending H263 frame (MTU 1500). Length: %d\n", _payloadDataFileLength);
    assert(0 == rtpRtcpModuleVideo->SetMaxTransferUnit(1500));
    assert(0 == rtpRtcpModuleVideo->SendOutgoingData(webrtc::kVideoFrameKey,34, timestamp, _payloadDataFile, _payloadDataFileLength));
    timestamp += 3000;

    // lower MTU -> mode B
    printf("\tSending H263 frame (MTU 300). Length: %d\n", _payloadDataFileLength);
    assert(0 == rtpRtcpModuleVideo->SetMaxTransferUnit(300));
    assert(0 == rtpRtcpModuleVideo->SendOutgoingData(webrtc::kVideoFrameKey,34, timestamp, _payloadDataFile, _payloadDataFileLength));
    timestamp += 3000;

    openFile = fopen("H263_CIF_PFRAME.bin", "rb");
    assert(openFile != NULL);
    fseek(openFile, 0, SEEK_END);
    _payloadDataFileLength = (WebRtc_Word16)(ftell(openFile));
    rewind(openFile);
    assert(_payloadDataFileLength > 0);
    fread(_payloadDataFile, 1, _payloadDataFileLength, openFile);
    fclose(openFile);

    // test H.263 without all GOBs
    assert(0 == rtpRtcpModuleVideo->SetMaxTransferUnit(1500));
    printf("\tSending H263 frame without all GOBs (MTU 1500). Length: %d\n", _payloadDataFileLength);
    assert(0 == rtpRtcpModuleVideo->SendOutgoingData(webrtc::kVideoFrameDelta,34, timestamp, _payloadDataFile, _payloadDataFileLength));
    timestamp += 3000;

    // test H.263 without all GOBs small MTU
    assert(0 == rtpRtcpModuleVideo->SetMaxTransferUnit(500));
    printf("\tSending H263 frame without all GOBs (MTU 500). Length: %d\n", _payloadDataFileLength);
    assert(0 == rtpRtcpModuleVideo->SendOutgoingData(webrtc::kVideoFrameDelta,34, timestamp, _payloadDataFile, _payloadDataFileLength));

    // test PLI with relay
    assert(0 == rtpRtcpModuleVideo->RegisterIncomingVideoCallback(myVideoFeedback));
    assert(0 == rtpRtcpModuleVideoReceiver->SetKeyFrameRequestMethod(kKeyFrameReqPliRtcp));
    assert(0 == rtpRtcpModuleVideoReceiver->RequestKeyFrame());


    processThread->DeRegisterModule(rtpRtcpModuleVideo);
    processThread->DeRegisterModule(rtpRtcpModuleVideoReceiver);
    processThread->Stop();
    ProcessThread::DestroyProcessThread(processThread);

    RtpRtcp::DestroyRtpRtcp(rtpRtcpModuleVideoReceiver);
    RtpRtcp::DestroyRtpRtcp(rtpRtcpModuleVideoRelay);
    RtpRtcp::DestroyRtpRtcp(rtpRtcpModuleVideoRelay2);
    RtpRtcp::DestroyRtpRtcp(rtpRtcpModuleVideo);
#endif // TEST_VIDEO

    printf("\nAPI test of RTP/RTCP module done\n");

#ifdef TEST_AUDIO
    delete myLoopBackTransport1;
    delete myLoopBackTransport2;
    delete myDataReceiver1;
    delete myDataReceiver2;
    delete myRTCPFeedback1;
    delete myRTCPFeedback2;
    delete audioFeedback;
    delete myRTPCallback;
#endif
#ifdef TEST_VIDEO
    delete myLoopBackTransportVideo;
    delete myVideoFeedback;
    delete myDataReceiverVideo;
    delete myRelayDataReceiver;
    delete myRelaySender;
    delete myRelayReceiver;

    delete myRelaySender2;
    delete myRelayReceiver2;
    delete myRelayDataReceiver2;
    delete myDataReceive2;

    delete myRTCPFeedbackVideo;
    delete myRTCPFeedbackRealy;
    delete myRTCPFeedbackReceiver;
    delete myRTCPFeedbackRealy2;

#endif // TEST_VIDEO

    ::Sleep(5000);
    Trace::ReturnTrace();
    return 0;
}

