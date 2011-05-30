/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "common_types.h"
#include "rtp_rtcp_impl.h"
#include "trace.h"

#ifdef MATLAB
#include "../test/BWEStandAlone/MatlabPlot.h"
extern MatlabEngine eng; // global variable defined elsewhere
#endif

#include <string.h> //memcpy
#include <cassert> //assert

// local for this file
namespace
{
    const float FracMS = 4.294967296E6f;
}

#ifdef _WIN32
    // disable warning C4355: 'this' : used in base member initializer list
    #pragma warning(disable : 4355)
#endif

namespace webrtc {
using namespace RTCPUtility;

RtpRtcp*
RtpRtcp::CreateRtpRtcp(const WebRtc_Word32 id,
                       const bool audio)
{
    if(audio)
    {
        WEBRTC_TRACE(kTraceModuleCall, kTraceRtpRtcp, id, "CreateRtpRtcp(audio)");
    } else
    {
        WEBRTC_TRACE(kTraceModuleCall, kTraceRtpRtcp, id, "CreateRtpRtcp(video)");
    }
    return new ModuleRtpRtcpImpl(id, audio);
}

void RtpRtcp::DestroyRtpRtcp(RtpRtcp* module)
{
    if(module)
    {
        WEBRTC_TRACE(kTraceModuleCall, kTraceRtpRtcp, static_cast<ModuleRtpRtcpImpl*>(module)->Id(), "DestroyRtpRtcp()");
        delete static_cast<ModuleRtpRtcpImpl*>(module);
    }
}

ModuleRtpRtcpImpl::ModuleRtpRtcpImpl(const WebRtc_Word32 id,
                                     const bool audio):
    TMMBRHelp(audio),
    _id(id),
    _audio(audio),
    _collisionDetected(false),
    _lastProcessTime(ModuleRTPUtility::GetTimeInMS()),

    _packetOverHead(28), // IPV4 UDP
    _criticalSectionModulePtrs(*CriticalSectionWrapper::CreateCriticalSection()),
    _criticalSectionModulePtrsFeedback(*CriticalSectionWrapper::CreateCriticalSection()),
    _defaultModule(NULL),
    _audioModule(NULL),
    _videoModule(NULL),
    _childModules(),
    _deadOrAliveActive(false),
    _deadOrAliveTimeoutMS(0),
    _deadOrAliveLastTimer(0),
    _rtpReceiver(id, audio, *this),
    _rtcpReceiver(id,*this),
    _bandwidthManagement(id),
    _receivedNTPsecsAudio(0),
    _receivedNTPfracAudio(0),
    _RTCPArrivalTimeSecsAudio(0),
    _RTCPArrivalTimeFracAudio(0),
    _rtpSender(id, audio),
    _rtcpSender(id, audio, *this),
    _nackMethod(kNackOff),
    _nackLastTimeSent(0),
    _nackLastSeqNumberSent(0),
    _keyFrameReqMethod(kKeyFrameReqFirRtp),
    _lastChildBitrateUpdate(0)
#ifdef MATLAB
    ,_plot1(NULL)
#endif
{
    // make sure that RTCP objects are aware of our SSRC
    WebRtc_UWord32 SSRC = _rtpSender.SSRC();
    _rtcpSender.SetSSRC(SSRC);

    WEBRTC_TRACE(kTraceMemory, kTraceRtpRtcp, id, "%s created", __FUNCTION__);
}

ModuleRtpRtcpImpl::~ModuleRtpRtcpImpl()
{
    WEBRTC_TRACE(kTraceMemory, kTraceRtpRtcp, _id, "%s deleted", __FUNCTION__);

    // make sure to unregister this module from other modules

    const bool defaultInstance(_childModules.Empty()?false:true);

    if(defaultInstance)
    {
        // deregister for the default module
        // will go in to the child modules and remove it self
        ListItem* item = _childModules.First();
        while (item)
        {
            RtpRtcp* module = (RtpRtcp*)item->GetItem();
            _childModules.Erase(item);
            if(module)
            {
                module->DeRegisterDefaultModule();
            }
            item = _childModules.First();
        }
    } else
    {
        // deregister for the child modules
        // will go in to the default and remove it self
        DeRegisterDefaultModule();
    }

    if(_audio)
    {
        DeRegisterVideoModule();
    } else
    {
        DeRegisterSyncModule();
    }

#ifdef MATLAB
    if (_plot1)
    {
        eng.DeletePlot(_plot1);
        _plot1 = NULL;
    }
#endif

    delete &_criticalSectionModulePtrs;
    delete &_criticalSectionModulePtrsFeedback;
}

WebRtc_Word32
ModuleRtpRtcpImpl::Version(WebRtc_Word8*   version,
                           WebRtc_UWord32& remainingBufferInBytes,
                           WebRtc_UWord32& position) const
{
    WEBRTC_TRACE(kTraceModuleCall, kTraceRtpRtcp, _id, "Version(bufferLength:%d)", remainingBufferInBytes);
    return GetVersion(version, remainingBufferInBytes, position);
}

WebRtc_Word32
RtpRtcp::GetVersion(WebRtc_Word8*   version,
                              WebRtc_UWord32& remainingBufferInBytes,
                              WebRtc_UWord32& position)
{
    if(version == NULL)
    {
        WEBRTC_TRACE(kTraceWarning, kTraceRtpRtcp, -1, "Invalid in argument to Version()");
        return -1;
    }
    WebRtc_Word8 ourVersion[] = "Module RTP RTCP 1.3.0";
    WebRtc_UWord32 ourLength = (WebRtc_UWord32)strlen(ourVersion);
    if(remainingBufferInBytes < ourLength +1)
    {
        return -1;
    }
    memcpy(version, ourVersion, ourLength);
    version[ourLength] = '\0'; // null terminaion
    remainingBufferInBytes -= (ourLength + 1);
    position += (ourLength + 1);
    return 0;
}

WebRtc_Word32
ModuleRtpRtcpImpl::ChangeUniqueId(const WebRtc_Word32 id)
{
    WEBRTC_TRACE(kTraceModuleCall, kTraceRtpRtcp, _id, "ChangeUniqueId(new id:%d)", id);

    _id = id;

    _rtpReceiver.ChangeUniqueId(id);
    _rtcpReceiver.ChangeUniqueId(id);
    _rtpSender.ChangeUniqueId(id);
    _rtcpSender.ChangeUniqueId(id);
    return 0;
}

// default encoder that we need to multiplex out
WebRtc_Word32
ModuleRtpRtcpImpl::RegisterDefaultModule(RtpRtcp* module)
{
    WEBRTC_TRACE(kTraceModuleCall, kTraceRtpRtcp, _id, "RegisterDefaultModule(module:0x%x)", module);

    if(module == NULL)
    {
        return -1;
    }
    CriticalSectionScoped lock(_criticalSectionModulePtrs);

    if(_defaultModule)
    {
        _defaultModule->DeRegisterChildModule(this);
    }
    _defaultModule = (ModuleRtpRtcpPrivate*)module;
    _defaultModule->RegisterChildModule(this);
    return 0;
}

WebRtc_Word32
ModuleRtpRtcpImpl::DeRegisterDefaultModule()
{
    WEBRTC_TRACE(kTraceModuleCall, kTraceRtpRtcp, _id, "DeRegisterDefaultModule()");

    CriticalSectionScoped lock(_criticalSectionModulePtrs);
    if(_defaultModule)
    {
        _defaultModule->DeRegisterChildModule(this);
        _defaultModule = NULL;
    }
    return 0;
}

bool ModuleRtpRtcpImpl::DefaultModuleRegistered()
{
    WEBRTC_TRACE(kTraceModuleCall, kTraceRtpRtcp, _id, "DefaultModuleRegistered()");

    CriticalSectionScoped lock(_criticalSectionModulePtrs);
    if(_defaultModule)
    {
        return true;
    }
    else
    {
        return false;
    }
}

WebRtc_UWord32
ModuleRtpRtcpImpl::NumberChildModules()
{
    WEBRTC_TRACE(kTraceModuleCall, kTraceRtpRtcp, _id, "NumberChildModules");

    CriticalSectionScoped lock(_criticalSectionModulePtrs);
    CriticalSectionScoped doubleLock(_criticalSectionModulePtrsFeedback);
    // we use two locks for protecting _childModules one (_criticalSectionModulePtrsFeedback) for incoming
    // messages (BitrateSent and UpdateTMMBR) and _criticalSectionModulePtrs for all outgoing messages sending packets etc

    return _childModules.GetSize();
}

void
ModuleRtpRtcpImpl::RegisterChildModule(RtpRtcp* module)
{
    WEBRTC_TRACE(kTraceModuleCall, kTraceRtpRtcp, _id, "RegisterChildModule(module:0x%x)", module);

    CriticalSectionScoped lock(_criticalSectionModulePtrs);

    CriticalSectionScoped doubleLock(_criticalSectionModulePtrsFeedback);
    // we use two locks for protecting _childModules one (_criticalSectionModulePtrsFeedback) for incoming
    // messages (BitrateSent and UpdateTMMBR) and _criticalSectionModulePtrs for all outgoing messages sending packets etc

    _childModules.PushFront(module);
}

void
ModuleRtpRtcpImpl::DeRegisterChildModule(RtpRtcp* removeModule)
{
    WEBRTC_TRACE(kTraceModuleCall, kTraceRtpRtcp, _id, "DeRegisterChildModule(module:0x%x)", removeModule);

    CriticalSectionScoped lock(_criticalSectionModulePtrs);

    CriticalSectionScoped doubleLock(_criticalSectionModulePtrsFeedback);

    ListItem* item = _childModules.First();
    while (item)
    {
        RtpRtcp* module = (RtpRtcp*)item->GetItem();
        if(module == removeModule)
        {
            _childModules.Erase(item);
            return;
        }
        item = _childModules.Next(item);
    }
}

// Lip-sync between voice-video engine,
WebRtc_Word32
ModuleRtpRtcpImpl::RegisterSyncModule(RtpRtcp* audioModule)
{
    WEBRTC_TRACE(kTraceModuleCall, kTraceRtpRtcp, _id, "RegisterSyncModule(module:0x%x)", audioModule);

    if(audioModule == NULL)
    {
        return -1;
    }
    if(_audio)
    {
        return -1;
    }
    CriticalSectionScoped lock(_criticalSectionModulePtrs);
    _audioModule = (ModuleRtpRtcpPrivate*)audioModule;
    return _audioModule->RegisterVideoModule(this);
}

WebRtc_Word32
ModuleRtpRtcpImpl::DeRegisterSyncModule()
{
    WEBRTC_TRACE(kTraceModuleCall, kTraceRtpRtcp, _id, "DeRegisterSyncModule()");

    CriticalSectionScoped lock(_criticalSectionModulePtrs);
    if(_audioModule)
    {
        ModuleRtpRtcpPrivate* audioModule=_audioModule;
        _audioModule = NULL;
        _receivedNTPsecsAudio = 0;
        _receivedNTPfracAudio = 0;
        _RTCPArrivalTimeSecsAudio = 0;
        _RTCPArrivalTimeFracAudio = 0;
        audioModule->DeRegisterVideoModule();
    }
    return 0;
}

WebRtc_Word32
ModuleRtpRtcpImpl::RegisterVideoModule(RtpRtcp* videoModule)
{
    WEBRTC_TRACE(kTraceModuleCall, kTraceRtpRtcp, _id, "RegisterVideoModule(module:0x%x)", videoModule);

    if(videoModule == NULL)
    {
        return -1;
    }
    if(!_audio)
    {
        return -1;
    }
    CriticalSectionScoped lock(_criticalSectionModulePtrs);
    _videoModule = (ModuleRtpRtcpPrivate*)videoModule;
    return 0;
}

void
ModuleRtpRtcpImpl::DeRegisterVideoModule()
{
    WEBRTC_TRACE(kTraceModuleCall, kTraceRtpRtcp, _id, "DeRegisterVideoModule()");

    CriticalSectionScoped lock(_criticalSectionModulePtrs);
    if(_videoModule)
    {
        ModuleRtpRtcpPrivate* videoModule=_videoModule;
        _videoModule=NULL;
        videoModule->DeRegisterSyncModule();
    }
}

// returns the number of milliseconds until the module want a worker thread to call Process
WebRtc_Word32
ModuleRtpRtcpImpl::TimeUntilNextProcess()
{
    const WebRtc_UWord32 now = ModuleRTPUtility::GetTimeInMS();
    return kRtpRtcpMaxIdleTimeProcess - (now -_lastProcessTime);
}

// Process any pending tasks such as timeouts
// non time critical events
WebRtc_Word32
ModuleRtpRtcpImpl::Process()
{
    _lastProcessTime = ModuleRTPUtility::GetTimeInMS();

    _rtpReceiver.PacketTimeout();
    _rtcpReceiver.PacketTimeout();

    _rtpSender.ProcessBitrate();
    _rtpReceiver.ProcessBitrate();

    ProcessDeadOrAliveTimer();

    if(_rtcpSender.TimeToSendRTCPReport())
    {
        WebRtc_UWord16 RTT = 0;
        _rtcpReceiver.RTT(_rtpReceiver.SSRC(), &RTT, NULL,NULL,NULL);
        _rtcpSender.SendRTCP(kRtcpReport, 0, 0, RTT);
    }
    if(_rtpSender.RTPKeepalive())
    {
        // check time to send RTP keep alive
        if( _rtpSender.TimeToSendRTPKeepalive())
        {
            _rtpSender.SendRTPKeepalivePacket();
        }
    }
    if(UpdateRTCPReceiveInformationTimers())
    {
        // a receiver has timed out
        UpdateTMMBR();
    }
    return 0;
}

    /**
    *   Receiver
    */

WebRtc_Word32
ModuleRtpRtcpImpl::InitReceiver()
{
    WEBRTC_TRACE(kTraceModuleCall, kTraceRtpRtcp, _id, "InitReceiver()");

    _packetOverHead = 28; // default is IPV4 UDP
    _receivedNTPsecsAudio = 0;
    _receivedNTPfracAudio = 0;
    _RTCPArrivalTimeSecsAudio = 0;
    _RTCPArrivalTimeFracAudio = 0;

    WebRtc_Word32 ret = _rtpReceiver.Init();
    if (ret < 0)
    {
        return ret;
    }
    _rtpReceiver.SetPacketOverHead(_packetOverHead);
    return ret;
}

void
ModuleRtpRtcpImpl::ProcessDeadOrAliveTimer()
{
    if(_deadOrAliveActive)
    {
        const WebRtc_UWord32 now = ModuleRTPUtility::GetTimeInMS();
        if(now > _deadOrAliveTimeoutMS +_deadOrAliveLastTimer)
        {
            _deadOrAliveLastTimer += _deadOrAliveTimeoutMS;

            bool RTCPalive = false;  // RTCP is alive if we have received a report the last 12 seconds
            if(_rtcpReceiver.LastReceived() + 12000 > now)
            {
                RTCPalive = true;
            }
            _rtpReceiver.ProcessDeadOrAlive(RTCPalive, now);
        }
    }
}

    // Set periodic dead or alive notification
WebRtc_Word32
ModuleRtpRtcpImpl::SetPeriodicDeadOrAliveStatus(const bool enable,
                                                const WebRtc_UWord8 sampleTimeSeconds)
{
    if(enable)
    {
        WEBRTC_TRACE(kTraceModuleCall, kTraceRtpRtcp, _id, "SetPeriodicDeadOrAliveStatus(enable, %d)", sampleTimeSeconds);
    }else
    {
        WEBRTC_TRACE(kTraceModuleCall, kTraceRtpRtcp, _id, "SetPeriodicDeadOrAliveStatus(disable)");
    }

    if(sampleTimeSeconds == 0)
    {
        return -1;
    }
    _deadOrAliveActive = enable;
    _deadOrAliveTimeoutMS = sampleTimeSeconds*1000;
    _deadOrAliveLastTimer = ModuleRTPUtility::GetTimeInMS();  // trigger the first after one period
    return 0;
}

WebRtc_Word32
ModuleRtpRtcpImpl::PeriodicDeadOrAliveStatus(bool &enable,
                                             WebRtc_UWord8 &sampleTimeSeconds)
{
    WEBRTC_TRACE(kTraceModuleCall, kTraceRtpRtcp, _id, "PeriodicDeadOrAliveStatus()");

    enable = _deadOrAliveActive;
    sampleTimeSeconds = (WebRtc_UWord8)(_deadOrAliveTimeoutMS/1000);
    return 0;
}

WebRtc_Word32
ModuleRtpRtcpImpl::SetPacketTimeout(const WebRtc_UWord32 RTPtimeoutMS,
                                    const WebRtc_UWord32 RTCPtimeoutMS)
{
    WEBRTC_TRACE(kTraceModuleCall, kTraceRtpRtcp, _id, "SetPacketTimeout(%u,%u)",RTPtimeoutMS, RTCPtimeoutMS);

    if(_rtpReceiver.SetPacketTimeout(RTPtimeoutMS) == 0)
    {
        return _rtcpReceiver.SetPacketTimeout(RTCPtimeoutMS);
    }
    return -1;
}

// set codec name and payload type
WebRtc_Word32
ModuleRtpRtcpImpl::RegisterReceivePayload(const WebRtc_Word8 payloadName[RTP_PAYLOAD_NAME_SIZE],
                                          const WebRtc_Word8 payloadType,
                                          const WebRtc_UWord32 frequency,
                                          const WebRtc_UWord8 channels,
                                          const WebRtc_UWord32 rate)
{
    WEBRTC_TRACE(kTraceModuleCall, kTraceRtpRtcp, _id, "RegisterReceivePayload()");

    return _rtpReceiver.RegisterReceivePayload(payloadName, payloadType, frequency, channels, rate);
}

WebRtc_Word32
ModuleRtpRtcpImpl::DeRegisterReceivePayload(const WebRtc_Word8 payloadType)
{
    WEBRTC_TRACE(kTraceModuleCall, kTraceRtpRtcp, _id, "DeRegisterReceivePayload(%d)", payloadType);

    return _rtpReceiver.DeRegisterReceivePayload(payloadType);
}

    // get configured payload type
WebRtc_Word32
ModuleRtpRtcpImpl::ReceivePayloadType(const WebRtc_Word8 payloadName[RTP_PAYLOAD_NAME_SIZE],
                                      const WebRtc_UWord32 frequency,
                                      const WebRtc_UWord8 channels,
                                      WebRtc_Word8* payloadType,
                                      const WebRtc_UWord32 rate) const
{
    WEBRTC_TRACE(kTraceModuleCall, kTraceRtpRtcp, _id, "ReceivePayloadType()");

    return _rtpReceiver.ReceivePayloadType(payloadName, frequency, channels, payloadType, rate);
}

WebRtc_Word32
ModuleRtpRtcpImpl::ReceivePayload(const WebRtc_Word8 payloadType,
                                  WebRtc_Word8 payloadName[RTP_PAYLOAD_NAME_SIZE],
                                  WebRtc_UWord32* frequency,
                                  WebRtc_UWord8* channels,
                                  WebRtc_UWord32* rate) const
{
    WEBRTC_TRACE(kTraceModuleCall, kTraceRtpRtcp, _id, "ReceivePayload()");

    return _rtpReceiver.ReceivePayload(payloadType, payloadName, frequency, channels, rate);
}

WebRtc_Word32
ModuleRtpRtcpImpl::RemotePayload(WebRtc_Word8 payloadName[RTP_PAYLOAD_NAME_SIZE],
                                 WebRtc_Word8* payloadType,
                                 WebRtc_UWord32* frequency,
                                 WebRtc_UWord8* channels) const
{
    WEBRTC_TRACE(kTraceModuleCall, kTraceRtpRtcp, _id, "RemotePayload()");

    return _rtpReceiver.RemotePayload(payloadName, payloadType, frequency, channels);
}

    // get the currently configured SSRC filter
WebRtc_Word32
ModuleRtpRtcpImpl::SSRCFilter(WebRtc_UWord32& allowedSSRC) const
{
    WEBRTC_TRACE(kTraceModuleCall, kTraceRtpRtcp, _id, "SSRCFilter()");

    return _rtpReceiver.SSRCFilter(allowedSSRC);
}

    // set a SSRC to be used as a filter for incoming RTP streams
WebRtc_Word32
ModuleRtpRtcpImpl::SetSSRCFilter(const bool enable, const WebRtc_UWord32 allowedSSRC)
{
    if(enable)
    {
        WEBRTC_TRACE(kTraceModuleCall, kTraceRtpRtcp, _id, "SetSSRCFilter(enable, 0x%x)", allowedSSRC);
    }else
    {
        WEBRTC_TRACE(kTraceModuleCall, kTraceRtpRtcp, _id, "SetSSRCFilter(disable)");
    }

    return _rtpReceiver.SetSSRCFilter(enable, allowedSSRC);
}

// Get last received remote timestamp
WebRtc_UWord32
ModuleRtpRtcpImpl::RemoteTimestamp() const
{
    WEBRTC_TRACE(kTraceModuleCall, kTraceRtpRtcp, _id, "RemoteTimestamp()");

    return _rtpReceiver.TimeStamp();
}

// Get the current estimated remote timestamp
WebRtc_Word32
ModuleRtpRtcpImpl::EstimatedRemoteTimeStamp(WebRtc_UWord32& timestamp) const
{
    WEBRTC_TRACE(kTraceModuleCall, kTraceRtpRtcp, _id, "EstimatedRemoteTimeStamp()");

    return _rtpReceiver.EstimatedRemoteTimeStamp(timestamp);
}

// Get incoming SSRC
WebRtc_UWord32
ModuleRtpRtcpImpl::RemoteSSRC() const
{
    WEBRTC_TRACE(kTraceModuleCall, kTraceRtpRtcp, _id, "RemoteSSRC()");

    return _rtpReceiver.SSRC();
}

// Get remote CSRC
WebRtc_Word32
ModuleRtpRtcpImpl::RemoteCSRCs( WebRtc_UWord32 arrOfCSRC[kRtpCsrcSize]) const
{
    WEBRTC_TRACE(kTraceModuleCall, kTraceRtpRtcp, _id, "RemoteCSRCs()");

    return _rtpReceiver.CSRCs(arrOfCSRC);
}

// called by the network module when we receive a packet
WebRtc_Word32
ModuleRtpRtcpImpl::IncomingPacket(const WebRtc_UWord8* incomingPacket,
                                  const WebRtc_UWord16 incomingPacketLength)
{
    WEBRTC_TRACE(kTraceStream, kTraceRtpRtcp, _id, "IncomingPacket(packetLength:%u)", incomingPacketLength);

    // minimum RTP is 12 bytes
    // minimum RTCP is 8 bytes (RTCP BYE)
    if(incomingPacketLength < 8 || incomingPacket == NULL)
    {
        WEBRTC_TRACE(kTraceDebug, kTraceRtpRtcp, _id, "IncomingPacket invalid buffer or length");
        return -1;
    }
    // check RTP version
    const WebRtc_UWord8  version  = incomingPacket[0] >> 6 ;
    if(version != 2)
    {
        WEBRTC_TRACE(kTraceDebug, kTraceRtpRtcp, _id, "IncomingPacket invalid RTP version");
        return -1;
    }

    ModuleRTPUtility::RTPHeaderParser rtpParser(incomingPacket, incomingPacketLength);

    if(rtpParser.RTCP())
    {
        RTCPUtility::RTCPParserV2 rtcpParser(incomingPacket,
                                            incomingPacketLength,
                                            true); // Allow receive of non-compound RTCP packets.

        const bool validRTCPHeader = rtcpParser.IsValid();
        if(!validRTCPHeader)
        {
            WEBRTC_TRACE(kTraceDebug, kTraceRtpRtcp, _id, "IncomingPacket invalid RTCP packet");
            return -1;
        }
        RTCPHelp::RTCPPacketInformation rtcpPacketInformation;
        WebRtc_Word32 retVal = _rtcpReceiver.IncomingRTCPPacket(rtcpPacketInformation,
                                                              &rtcpParser);
        if(retVal == 0)
        {
            _rtcpReceiver.TriggerCallbacksFromRTCPPacket(rtcpPacketInformation);
        }
        return retVal;

    } else
    {
        WebRtcRTPHeader rtpHeader;
        memset(&rtpHeader, 0, sizeof(rtpHeader));

        const bool validRTPHeader = rtpParser.Parse(rtpHeader);
        if(!validRTPHeader)
        {
            WEBRTC_TRACE(kTraceDebug, kTraceRtpRtcp, _id, "IncomingPacket invalid RTP header");
            return -1;
        }
        return _rtpReceiver.IncomingRTPPacket(&rtpHeader,
                                              incomingPacket,
                                              incomingPacketLength);
    }
}

WebRtc_Word32
ModuleRtpRtcpImpl::IncomingAudioNTP(const WebRtc_UWord32 audioReceivedNTPsecs,
                                    const WebRtc_UWord32 audioReceivedNTPfrac,
                                    const WebRtc_UWord32 audioRTCPArrivalTimeSecs,
                                    const WebRtc_UWord32 audioRTCPArrivalTimeFrac)
{
    _receivedNTPsecsAudio = audioReceivedNTPsecs;
    _receivedNTPfracAudio = audioReceivedNTPfrac;
    _RTCPArrivalTimeSecsAudio = audioRTCPArrivalTimeSecs;
    _RTCPArrivalTimeFracAudio = audioRTCPArrivalTimeFrac;
    return 0;
}

WebRtc_Word32
ModuleRtpRtcpImpl::RegisterIncomingDataCallback(RtpData* incomingDataCallback)
{
    WEBRTC_TRACE(kTraceModuleCall, kTraceRtpRtcp, _id, "RegisterIncomingDataCallback(incomingDataCallback:0x%x)", incomingDataCallback);

    return _rtpReceiver.RegisterIncomingDataCallback(incomingDataCallback);
}

WebRtc_Word32
ModuleRtpRtcpImpl::RegisterIncomingRTPCallback(RtpFeedback* incomingMessagesCallback)
{
    WEBRTC_TRACE(kTraceModuleCall, kTraceRtpRtcp, _id, "RegisterIncomingRTPCallback(incomingMessagesCallback:0x%x)",incomingMessagesCallback);

    return _rtpReceiver.RegisterIncomingRTPCallback(incomingMessagesCallback);
}

WebRtc_Word32
ModuleRtpRtcpImpl::RegisterIncomingRTCPCallback(RtcpFeedback* incomingMessagesCallback)
{
    WEBRTC_TRACE(kTraceModuleCall, kTraceRtpRtcp, _id, "RegisterIncomingRTCPCallback(incomingMessagesCallback:0x%x)",incomingMessagesCallback);

    return _rtcpReceiver.RegisterIncomingRTCPCallback(incomingMessagesCallback);
}

WebRtc_Word32
ModuleRtpRtcpImpl::RegisterIncomingVideoCallback(RtpVideoFeedback* incomingMessagesCallback)
{
    WEBRTC_TRACE(kTraceModuleCall, kTraceRtpRtcp, _id, "RegisterIncomingVideoCallback(incomingMessagesCallback:0x%x)",incomingMessagesCallback);

    if(_rtcpReceiver.RegisterIncomingVideoCallback(incomingMessagesCallback) == 0)
    {
        return _rtpReceiver.RegisterIncomingVideoCallback(incomingMessagesCallback);
    }
    return -1;
}

WebRtc_Word32
ModuleRtpRtcpImpl::RegisterAudioCallback(RtpAudioFeedback* messagesCallback)
{
    WEBRTC_TRACE(kTraceModuleCall, kTraceRtpRtcp, _id, "RegisterAudioCallback(messagesCallback:0x%x)",messagesCallback);

    if(_rtpSender.RegisterAudioCallback(messagesCallback) == 0)
    {
        return _rtpReceiver.RegisterIncomingAudioCallback(messagesCallback);
    }
    return -1;
}

    /**
    *   Sender
    */

WebRtc_Word32
ModuleRtpRtcpImpl::InitSender()
{
    WEBRTC_TRACE(kTraceModuleCall, kTraceRtpRtcp, _id, "InitSender()");

    _collisionDetected = false;

    // if we are already receiving inform our sender to avoid collision
    if(_rtpSender.Init(_rtpReceiver.SSRC()) != 0)
    {
        return -1;
    }
    WebRtc_Word32 retVal = _rtcpSender.Init();

    // make sure that RTCP objects are aware of our SSRC (it could have changed due to collision)
    WebRtc_UWord32 SSRC = _rtpSender.SSRC();
    _rtcpReceiver.SetSSRC(SSRC);
    _rtcpSender.SetSSRC(SSRC);
    return retVal;
}

bool
ModuleRtpRtcpImpl::RTPKeepalive() const
{
    WEBRTC_TRACE(kTraceStream, kTraceRtpRtcp, _id, "RTPKeepalive()");

    return _rtpSender.RTPKeepalive();
}

WebRtc_Word32
ModuleRtpRtcpImpl::RTPKeepaliveStatus(bool* enable,
                                      WebRtc_Word8* unknownPayloadType,
                                      WebRtc_UWord16* deltaTransmitTimeMS) const
{
    WEBRTC_TRACE(kTraceModuleCall, kTraceRtpRtcp, _id, "RTPKeepaliveStatus()");

    return _rtpSender.RTPKeepaliveStatus(enable, unknownPayloadType, deltaTransmitTimeMS);
}

WebRtc_Word32
ModuleRtpRtcpImpl::SetRTPKeepaliveStatus(bool enable, WebRtc_Word8 unknownPayloadType, WebRtc_UWord16 deltaTransmitTimeMS)
{
    if (enable)
    {
        WEBRTC_TRACE(kTraceModuleCall, kTraceRtpRtcp, _id, "SetRTPKeepaliveStatus(enable, payloadType:%d deltaTransmitTimeMS:%u)",unknownPayloadType,deltaTransmitTimeMS);

        // check the transmit keepalive delta time [1,60]
        if (deltaTransmitTimeMS < 1000 || deltaTransmitTimeMS > 60000)
        {
            WEBRTC_TRACE(kTraceError, kTraceRtpRtcp, _id,  "\tinvalid deltaTransmitTimeSeconds (%d)", deltaTransmitTimeMS);
            return (-1);
        }

        // check the payload time [0,127]
        if (unknownPayloadType < 0 )
        {
            WEBRTC_TRACE(kTraceError, kTraceRtpRtcp, _id, "\tinvalid unknownPayloadType (%d)", unknownPayloadType);
            return (-1);
        }

        // enable RTP keepalive mechanism
        return _rtpSender.EnableRTPKeepalive(unknownPayloadType, deltaTransmitTimeMS);

    }else
    {
        WEBRTC_TRACE(kTraceModuleCall, kTraceRtpRtcp, _id, "SetRTPKeepaliveStatus(disable)");
        return _rtpSender.DisableRTPKeepalive();
    }
}

// set codec name and payload type
WebRtc_Word32
ModuleRtpRtcpImpl::RegisterSendPayload(const WebRtc_Word8 payloadName[RTP_PAYLOAD_NAME_SIZE],
                                       const WebRtc_Word8 payloadType,
                                       const WebRtc_UWord32 frequency,
                                       const WebRtc_UWord8 channels,
                                       const WebRtc_UWord32 rate)
{
    WEBRTC_TRACE(kTraceModuleCall, kTraceRtpRtcp, _id, "RegisterSendPayload(payloadName:%s payloadType:%d frequency:%u)", payloadName, payloadType, frequency);

    return _rtpSender.RegisterPayload(payloadName, payloadType, frequency, channels, rate);
}

WebRtc_Word32
ModuleRtpRtcpImpl::DeRegisterSendPayload(const WebRtc_Word8 payloadType)
{
    WEBRTC_TRACE(kTraceModuleCall, kTraceRtpRtcp, _id, "DeRegisterSendPayload(%d)", payloadType);

    return _rtpSender.DeRegisterSendPayload(payloadType);
}

WebRtc_Word8
ModuleRtpRtcpImpl::SendPayloadType() const
{
    return _rtpSender.SendPayloadType();
}

WebRtc_UWord32
ModuleRtpRtcpImpl::StartTimestamp() const
{
    WEBRTC_TRACE(kTraceModuleCall, kTraceRtpRtcp, _id, "StartTimestamp()");

    return _rtpSender.StartTimestamp();
}

    // configure start timestamp, default is a random number
WebRtc_Word32
ModuleRtpRtcpImpl::SetStartTimestamp(const WebRtc_UWord32 timestamp)
{
    WEBRTC_TRACE(kTraceModuleCall, kTraceRtpRtcp, _id, "SetStartTimestamp(%d)", timestamp);

    return _rtpSender.SetStartTimestamp(timestamp, true);
}

WebRtc_UWord16
ModuleRtpRtcpImpl::SequenceNumber() const
{
    WEBRTC_TRACE(kTraceModuleCall, kTraceRtpRtcp, _id, "SequenceNumber()");

    return _rtpSender.SequenceNumber();
}

    // Set SequenceNumber, default is a random number
WebRtc_Word32
ModuleRtpRtcpImpl::SetSequenceNumber(const WebRtc_UWord16 seqNum)
{
    WEBRTC_TRACE(kTraceModuleCall, kTraceRtpRtcp, _id, "SetSequenceNumber(%d)",seqNum);

    return _rtpSender.SetSequenceNumber(seqNum);
}

WebRtc_UWord32
ModuleRtpRtcpImpl::SSRC() const
{
    WEBRTC_TRACE(kTraceModuleCall, kTraceRtpRtcp, _id, "SSRC()");

    return _rtpSender.SSRC();
}

    // configure SSRC, default is a random number
WebRtc_Word32
ModuleRtpRtcpImpl::SetSSRC(const WebRtc_UWord32 ssrc)
{
    WEBRTC_TRACE(kTraceModuleCall, kTraceRtpRtcp, _id, "SetSSRC(%d)", ssrc);

    if(_rtpSender.SetSSRC(ssrc) == 0)
    {
        _rtcpReceiver.SetSSRC(ssrc);
        _rtcpSender.SetSSRC(ssrc);
        return 0;
    }
    return -1;
}

WebRtc_Word32
ModuleRtpRtcpImpl::SetCSRCStatus(const bool include)
{
    _rtcpSender.SetCSRCStatus(include);
    return _rtpSender.SetCSRCStatus(include);
}

WebRtc_Word32
ModuleRtpRtcpImpl::CSRCs( WebRtc_UWord32 arrOfCSRC[kRtpCsrcSize]) const
{
    WEBRTC_TRACE(kTraceModuleCall, kTraceRtpRtcp, _id, "CSRCs()");

    return _rtpSender.CSRCs(arrOfCSRC);
}

WebRtc_Word32
ModuleRtpRtcpImpl::SetCSRCs(const WebRtc_UWord32 arrOfCSRC[kRtpCsrcSize],
                            const WebRtc_UWord8 arrLength)
{
    WEBRTC_TRACE(kTraceModuleCall, kTraceRtpRtcp, _id, "SetCSRCs(arrLength:%d)", arrLength);

    const bool defaultInstance(_childModules.Empty()?false:true);

    if(defaultInstance)
    {
        // for default we need to update all child modules too
        CriticalSectionScoped lock(_criticalSectionModulePtrs);

        ListItem* item = _childModules.First();
        while (item)
        {
            RtpRtcp* module = (RtpRtcp*)item->GetItem();
            if(module)
            {
                module->SetCSRCs(arrOfCSRC, arrLength);
            }
            item = _childModules.Next(item);
        }
        return 0;

    } else
    {
        for(int i = 0;i < arrLength;i++)
        {
            WEBRTC_TRACE(kTraceModuleCall, kTraceRtpRtcp, _id, "\tidx:%d CSRC:%u", i, arrOfCSRC[i]);
        }
        _rtcpSender.SetCSRCs(arrOfCSRC, arrLength);
        return _rtpSender.SetCSRCs(arrOfCSRC, arrLength);
    }
}

WebRtc_UWord32
ModuleRtpRtcpImpl::PacketCountSent() const
{
    WEBRTC_TRACE(kTraceModuleCall, kTraceRtpRtcp, _id, "PacketCountSent()");

    return _rtpSender.Packets();
}

WebRtc_UWord32
ModuleRtpRtcpImpl::ByteCountSent() const
{
    WEBRTC_TRACE(kTraceModuleCall, kTraceRtpRtcp, _id, "ByteCountSent()");

    return _rtpSender.Bytes();
}

int ModuleRtpRtcpImpl::CurrentSendFrequencyHz() const 
{
    WEBRTC_TRACE(kTraceModuleCall, kTraceRtpRtcp, _id, "CurrentSendFrequencyHz()");

    return _rtpSender.SendPayloadFrequency();
}

WebRtc_Word32
ModuleRtpRtcpImpl::SetSendingStatus(const bool sending)
{
    if(sending)
    {
        WEBRTC_TRACE(kTraceModuleCall, kTraceRtpRtcp, _id, "SetSendingStatus(sending)");
    }else
    {
        if(_rtpSender.RTPKeepalive())
        {
            WEBRTC_TRACE(kTraceWarning, kTraceRtpRtcp, _id, "Can't SetSendingStatus(stopped) when RTP Keepalive is active");
            return -1;
        }
        WEBRTC_TRACE(kTraceModuleCall, kTraceRtpRtcp, _id, "SetSendingStatus(stopped)");
    }
    if(_rtcpSender.Sending() != sending)
    {
        // sends RTCP BYE when going from true to false
        WebRtc_Word32 retVal = _rtcpSender.SetSendingStatus(sending);

        _collisionDetected = false;

        // generate a new timeStamp if true and not configured via API
        // generate a new SSRC for the next "call" if false
        _rtpSender.SetSendingStatus(sending);

        // make sure that RTCP objects are aware of our SSRC (it could have changed due to collision)
        WebRtc_UWord32 SSRC = _rtpSender.SSRC();
        _rtcpReceiver.SetSSRC(SSRC);
        _rtcpSender.SetSSRC(SSRC);
        return retVal;
    }
    return 0;
}

bool
ModuleRtpRtcpImpl::Sending() const
{
    WEBRTC_TRACE(kTraceModuleCall, kTraceRtpRtcp, _id, "Sending()");

    return _rtcpSender.Sending();
}

WebRtc_Word32
ModuleRtpRtcpImpl::SetSendingMediaStatus(const bool sending)
{
    if(sending)
    {
        WEBRTC_TRACE(kTraceModuleCall, kTraceRtpRtcp, _id, "SetSendingMediaStatus(sending)");
    }else
    {
        WEBRTC_TRACE(kTraceModuleCall, kTraceRtpRtcp, _id, "SetSendingMediaStatus(stopped)");
    }
    _rtpSender.SetSendingMediaStatus(sending);
    return 0;
}

bool
ModuleRtpRtcpImpl::SendingMedia() const
{
    WEBRTC_TRACE(kTraceModuleCall, kTraceRtpRtcp, _id, "Sending()");

    const bool haveChildModules(_childModules.Empty()?false:true);

    if(!haveChildModules)
    {
        return _rtpSender.SendingMedia();
    }
    else
    {
        CriticalSectionScoped lock(_criticalSectionModulePtrs);
        ListItem* item = _childModules.First();
        if(item)
        {
            RTPSender& rtpSender = static_cast<ModuleRtpRtcpImpl*>(item->GetItem())->_rtpSender;
            if (rtpSender.SendingMedia())
            {
                return true;
            }
            item = _childModules.Next(item);
        }
    }
    return false;
}

WebRtc_Word32
ModuleRtpRtcpImpl::RegisterSendTransport(Transport* outgoingTransport)
{
    WEBRTC_TRACE(kTraceModuleCall, kTraceRtpRtcp, _id, "RegisterSendTransport(0x%x)", outgoingTransport);

    if(_rtpSender.RegisterSendTransport(outgoingTransport) == 0)
    {
        return _rtcpSender.RegisterSendTransport(outgoingTransport);
    }
    return -1;
}

WebRtc_Word32
ModuleRtpRtcpImpl::SendOutgoingData(const FrameType frameType,
                                    const WebRtc_Word8 payloadType,
                                    const WebRtc_UWord32 timeStamp,
                                    const WebRtc_UWord8* payloadData,
                                    const WebRtc_UWord32 payloadSize,
                                    const RTPFragmentationHeader* fragmentation)
{
    WEBRTC_TRACE(kTraceStream, kTraceRtpRtcp, _id,
               "SendOutgoingData(frameType:%d payloadType:%d timeStamp:%u payloadSize:%u)",
               frameType, payloadType, timeStamp, payloadSize);

    if(_rtcpSender.TimeToSendRTCPReport(kVideoFrameKey == frameType))
    {
        WebRtc_UWord16 RTT = 0;
        _rtcpReceiver.RTT(_rtpReceiver.SSRC(), &RTT, NULL,NULL,NULL);
        _rtcpSender.SendRTCP(kRtcpReport, 0, 0, RTT);
    }

    const bool haveChildModules(_childModules.Empty()?false:true);

    WebRtc_Word32 retVal = -1;
    if(!haveChildModules)
    {
        retVal = _rtpSender.SendOutgoingData(frameType,
                                              payloadType,
                                             timeStamp,
                                             payloadData,
                                             payloadSize,
                                             fragmentation);
    } else
    {
        CriticalSectionScoped lock(_criticalSectionModulePtrs);

        VideoCodecInformation* codecInfo = NULL;

        ListItem* item = _childModules.First();
        if(item)
        {
            RTPSender& rtpSender = static_cast<ModuleRtpRtcpImpl*>(item->GetItem())->_rtpSender;
            retVal = rtpSender.SendOutgoingData(frameType,
                                                payloadType,
                                                timeStamp,
                                                payloadData,
                                                payloadSize,
                                                fragmentation);

            item = _childModules.Next(item);
        }

        // send to all remaining "child" modules
        while(item)
        {
            RTPSender& rtpSender = static_cast<ModuleRtpRtcpImpl*>(item->GetItem())->_rtpSender;
            retVal = rtpSender.SendOutgoingData(frameType,
                                                payloadType,
                                                timeStamp,
                                                payloadData,
                                                payloadSize,
                                                fragmentation,
                                                codecInfo);

            item = _childModules.Next(item);
        }
    }
    return retVal;
}

WebRtc_UWord16
ModuleRtpRtcpImpl::MaxPayloadLength() const
{
    WEBRTC_TRACE(kTraceModuleCall, kTraceRtpRtcp, _id, "MaxPayloadLength()");

    return _rtpSender.MaxPayloadLength();
}

WebRtc_UWord16
ModuleRtpRtcpImpl::MaxDataPayloadLength() const
{
    WEBRTC_TRACE(kTraceModuleCall, kTraceRtpRtcp, _id, "MaxDataPayloadLength()");

    WebRtc_UWord16 minDataPayloadLength = IP_PACKET_SIZE-28; // Assuming IP/UDP

    const bool defaultInstance(_childModules.Empty() ? false : true);
    if (defaultInstance)
    {
        // for default we need to update all child modules too
        CriticalSectionScoped lock(_criticalSectionModulePtrs);
        ListItem* item = _childModules.First();
        while(item)
        {
            RtpRtcp* module = static_cast<RtpRtcp*>(item->GetItem());
            if (module)
            {
                WebRtc_UWord16 dataPayloadLength = module->MaxDataPayloadLength();
                if (dataPayloadLength < minDataPayloadLength)
                {
                    minDataPayloadLength = dataPayloadLength;
                }
            }
            item = _childModules.Next(item);
        }
    }

    WebRtc_UWord16 dataPayloadLength = _rtpSender.MaxDataPayloadLength();
    if (dataPayloadLength < minDataPayloadLength)
    {
        minDataPayloadLength = dataPayloadLength;
    }

    return minDataPayloadLength;
}

WebRtc_Word32
ModuleRtpRtcpImpl::SetTransportOverhead(const bool TCP,
                                        const bool IPV6,
                                        const WebRtc_UWord8 authenticationOverhead)
{
    WEBRTC_TRACE(kTraceModuleCall, kTraceRtpRtcp, _id, "SetTransportOverhead(TCP:%d, IPV6:%d authenticationOverhead:%u)",TCP,IPV6,authenticationOverhead);

    WebRtc_UWord16 packetOverHead = 0;
    if(IPV6)
    {
        packetOverHead = 40;
    } else
    {
        packetOverHead = 20;
    }
    if(TCP)
    {
        // TCP
        packetOverHead += 20;
    } else
    {
        // UDP
        packetOverHead += 8;
    }
    packetOverHead += authenticationOverhead;

    if(packetOverHead == _packetOverHead)
    {
        // ok same as before
        return 0;
    }
    // calc diff
    WebRtc_Word16 packetOverHeadDiff = packetOverHead - _packetOverHead;

    // store new
    _packetOverHead = packetOverHead;

    _rtpReceiver.SetPacketOverHead(_packetOverHead);
    WebRtc_UWord16 length = _rtpSender.MaxPayloadLength() - packetOverHeadDiff;
    return _rtpSender.SetMaxPayloadLength(length, _packetOverHead);
}

WebRtc_Word32
ModuleRtpRtcpImpl::SetMaxTransferUnit(const WebRtc_UWord16 MTU)
{
    WEBRTC_TRACE(kTraceModuleCall, kTraceRtpRtcp, _id, "SetMaxTransferUnit(%u)",MTU);

    if(MTU > IP_PACKET_SIZE)
    {
        WEBRTC_TRACE(kTraceWarning, kTraceRtpRtcp, _id, "Invalid in argument to SetMaxTransferUnit(%u)",MTU);
        return -1;
    }
    return _rtpSender.SetMaxPayloadLength(MTU - _packetOverHead, _packetOverHead);
}

    /*
    *   RTCP
    */
RTCPMethod
ModuleRtpRtcpImpl::RTCP() const
{
    WEBRTC_TRACE(kTraceModuleCall, kTraceRtpRtcp, _id, "RTCP()");

    if(_rtcpSender.Status() != kRtcpOff)
    {
        return _rtcpReceiver.Status();
    }
    return kRtcpOff;
}

    // configure RTCP status i.e on/off
WebRtc_Word32
ModuleRtpRtcpImpl::SetRTCPStatus(const RTCPMethod method)
{
    WEBRTC_TRACE(kTraceModuleCall, kTraceRtpRtcp, _id, "SetRTCPStatus(%d)",method);

    if(_rtcpSender.SetRTCPStatus(method) == 0)
    {
        return _rtcpReceiver.SetRTCPStatus(method);
    }
    return -1;
}
// only for internal test
WebRtc_UWord32
ModuleRtpRtcpImpl::LastSendReport(WebRtc_UWord32& lastRTCPTime)
{
    return _rtcpSender.LastSendReport(lastRTCPTime);
}

WebRtc_Word32
ModuleRtpRtcpImpl::SetCNAME(const WebRtc_Word8 cName[RTCP_CNAME_SIZE])
{
    WEBRTC_TRACE(kTraceModuleCall, kTraceRtpRtcp, _id, "SetCNAME(%s)", cName);

    return _rtcpSender.SetCNAME(cName);
}

WebRtc_Word32
ModuleRtpRtcpImpl::CNAME(WebRtc_Word8 cName[RTCP_CNAME_SIZE])
{
    WEBRTC_TRACE(kTraceModuleCall, kTraceRtpRtcp, _id, "CNAME()");

    return _rtcpSender.CNAME(cName);
}

WebRtc_Word32
ModuleRtpRtcpImpl::AddMixedCNAME(const WebRtc_UWord32 SSRC,
                                 const WebRtc_Word8 cName[RTCP_CNAME_SIZE])
{
    WEBRTC_TRACE(kTraceModuleCall, kTraceRtpRtcp, _id, "AddMixedCNAME(SSRC:%u)", SSRC);

    return _rtcpSender.AddMixedCNAME(SSRC, cName);
}

WebRtc_Word32
ModuleRtpRtcpImpl::RemoveMixedCNAME(const WebRtc_UWord32 SSRC)
{
    WEBRTC_TRACE(kTraceModuleCall, kTraceRtpRtcp, _id, "RemoveMixedCNAME(SSRC:%u)", SSRC);

    return _rtcpSender.RemoveMixedCNAME(SSRC);
}

WebRtc_Word32
ModuleRtpRtcpImpl::RemoteCNAME(const WebRtc_UWord32 remoteSSRC,
                               WebRtc_Word8 cName[RTCP_CNAME_SIZE]) const
{
    WEBRTC_TRACE(kTraceModuleCall, kTraceRtpRtcp, _id, "RemoteCNAME(SSRC:%u)", remoteSSRC);

    return _rtcpReceiver.CNAME(remoteSSRC, cName);
}

WebRtc_UWord16 ModuleRtpRtcpImpl::RemoteSequenceNumber() const
{
    WEBRTC_TRACE(kTraceModuleCall, kTraceRtpRtcp, _id, "RemoteSequenceNumber()");

    return _rtpReceiver.SequenceNumber();
}

WebRtc_Word32
ModuleRtpRtcpImpl::RemoteNTP(WebRtc_UWord32 *receivedNTPsecs,
                             WebRtc_UWord32 *receivedNTPfrac,
                             WebRtc_UWord32 *RTCPArrivalTimeSecs,
                             WebRtc_UWord32 *RTCPArrivalTimeFrac) const
{
    WEBRTC_TRACE(kTraceModuleCall, kTraceRtpRtcp, _id, "RemoteNTP()");

    return _rtcpReceiver.NTP(receivedNTPsecs,
                             receivedNTPfrac,
                             RTCPArrivalTimeSecs,
                             RTCPArrivalTimeFrac);
}

// Get RoundTripTime
WebRtc_Word32
ModuleRtpRtcpImpl::RTT(const WebRtc_UWord32 remoteSSRC,
                       WebRtc_UWord16* RTT,
                       WebRtc_UWord16* avgRTT,
                       WebRtc_UWord16* minRTT,
                       WebRtc_UWord16* maxRTT) const
{
    WEBRTC_TRACE(kTraceModuleCall, kTraceRtpRtcp, _id, "RTT()");

    return _rtcpReceiver.RTT(remoteSSRC, RTT, avgRTT, minRTT, maxRTT);
}

    // Reset RoundTripTime statistics
WebRtc_Word32
ModuleRtpRtcpImpl::ResetRTT(const WebRtc_UWord32 remoteSSRC)
{
    WEBRTC_TRACE(kTraceModuleCall, kTraceRtpRtcp, _id, "ResetRTT(SSRC:%u)", remoteSSRC);

    return _rtcpReceiver.ResetRTT(remoteSSRC);
}

    // Reset RTP statistics
WebRtc_Word32
ModuleRtpRtcpImpl::ResetStatisticsRTP()
{
    WEBRTC_TRACE(kTraceModuleCall, kTraceRtpRtcp, _id, "ResetStatisticsRTP()");

    return _rtpReceiver.ResetStatistics();
}

    // Reset RTP data counters for the receiving side
WebRtc_Word32
ModuleRtpRtcpImpl::ResetReceiveDataCountersRTP()
{
    WEBRTC_TRACE(kTraceModuleCall, kTraceRtpRtcp, _id, "ResetReceiveDataCountersRTP()");

    return _rtpReceiver.ResetDataCounters();
}

    // Reset RTP data counters for the sending side
WebRtc_Word32
ModuleRtpRtcpImpl::ResetSendDataCountersRTP()
{
    WEBRTC_TRACE(kTraceModuleCall, kTraceRtpRtcp, _id, "ResetSendDataCountersRTP()");

    return _rtpSender.ResetDataCounters();
}

    // Force a send of an RTCP packet
    // normal SR and RR are triggered via the process function
WebRtc_Word32
ModuleRtpRtcpImpl::SendRTCP(WebRtc_UWord32 rtcpPacketType)
{
    WEBRTC_TRACE(kTraceModuleCall, kTraceRtpRtcp, _id, "SendRTCP(0x%x)", rtcpPacketType);

    return  _rtcpSender.SendRTCP(rtcpPacketType);
}

WebRtc_Word32
ModuleRtpRtcpImpl::SetRTCPApplicationSpecificData(const WebRtc_UWord8 subType,
                                                  const WebRtc_UWord32 name,
                                                  const WebRtc_UWord8* data,
                                                  const WebRtc_UWord16 length)
{
    WEBRTC_TRACE(kTraceModuleCall, kTraceRtpRtcp, _id, "SetRTCPApplicationSpecificData(subType:%d name:0x%x)", subType, name);

    return  _rtcpSender.SetApplicationSpecificData(subType, name, data, length);
}

    /*
    *   (XR) VOIP metric
    */
WebRtc_Word32
ModuleRtpRtcpImpl::SetRTCPVoIPMetrics(const RTCPVoIPMetric* VoIPMetric)
{
    WEBRTC_TRACE(kTraceModuleCall, kTraceRtpRtcp, _id, "SetRTCPVoIPMetrics()");

    return  _rtcpSender.SetRTCPVoIPMetrics(VoIPMetric);
}

    // our localy created statistics of the received RTP stream
WebRtc_Word32
ModuleRtpRtcpImpl::StatisticsRTP(WebRtc_UWord8  *fraction_lost,
                                 WebRtc_UWord32 *cum_lost,
                                 WebRtc_UWord32 *ext_max,
                                 WebRtc_UWord32 *jitter,
                                 WebRtc_UWord32 *max_jitter) const
{
    WEBRTC_TRACE(kTraceModuleCall, kTraceRtpRtcp, _id, "StatisticsRTP()");

    WebRtc_Word32 retVal =_rtpReceiver.Statistics(fraction_lost,cum_lost,ext_max,jitter, max_jitter,(_rtcpSender.Status() == kRtcpOff));
    if(retVal == -1)
    {
        WEBRTC_TRACE(kTraceWarning, kTraceRtpRtcp, _id, "StatisticsRTP() no statisitics availble");
    }
    return retVal;
}

WebRtc_Word32
ModuleRtpRtcpImpl::DataCountersRTP(WebRtc_UWord32 *bytesSent,
                                   WebRtc_UWord32 *packetsSent,
                                   WebRtc_UWord32 *bytesReceived,
                                   WebRtc_UWord32 *packetsReceived) const
{
    WEBRTC_TRACE(kTraceStream, kTraceRtpRtcp, _id, "DataCountersRTP()");

    if(bytesSent)
    {
        *bytesSent = _rtpSender.Bytes();
    }
    if(packetsSent)
    {
        *packetsSent= _rtpSender.Packets();
    }
    return _rtpReceiver.DataCounters(bytesReceived, packetsReceived);
}

WebRtc_Word32
ModuleRtpRtcpImpl::ReportBlockStatistics(WebRtc_UWord8 *fraction_lost,
                                         WebRtc_UWord32 *cum_lost,
                                         WebRtc_UWord32 *ext_max,
                                         WebRtc_UWord32 *jitter)
{
    WEBRTC_TRACE(kTraceModuleCall, kTraceRtpRtcp, _id, "ReportBlockStatistics()");
    WebRtc_Word32 missing = 0;
    WebRtc_Word32 ret = _rtpReceiver.Statistics(fraction_lost,cum_lost,ext_max,jitter, NULL, &missing, true);

#ifdef MATLAB
    if (_plot1 == NULL)
    {
        _plot1 = eng.NewPlot(new MatlabPlot());
        _plot1->AddTimeLine(30, "b", "lost", TickTime::MillisecondTimestamp());
    }
    _plot1->Append("lost", missing);
    _plot1->Plot();
#endif

    return ret;
}

WebRtc_Word32
ModuleRtpRtcpImpl::RemoteRTCPStat( RTCPSenderInfo* senderInfo)
{
    WEBRTC_TRACE(kTraceModuleCall, kTraceRtpRtcp, _id, "RemoteRTCPStat()");

    return _rtcpReceiver.SenderInfoReceived(senderInfo);
}

    // received RTCP report
WebRtc_Word32
ModuleRtpRtcpImpl::RemoteRTCPStat(const WebRtc_UWord32 remoteSSRC,
                                  RTCPReportBlock* receiveBlock)
{
    WEBRTC_TRACE(kTraceModuleCall, kTraceRtpRtcp, _id, "RemoteRTCPStat()");

    return _rtcpReceiver.StatisticsReceived(remoteSSRC, receiveBlock);
}

WebRtc_Word32
ModuleRtpRtcpImpl::AddRTCPReportBlock(const WebRtc_UWord32 SSRC,
                                      const RTCPReportBlock* reportBlock)
{
    WEBRTC_TRACE(kTraceModuleCall, kTraceRtpRtcp, _id, "AddRTCPReportBlock()");

    return _rtcpSender.AddReportBlock(SSRC, reportBlock);
}

WebRtc_Word32
ModuleRtpRtcpImpl::RemoveRTCPReportBlock(const WebRtc_UWord32 SSRC)
{
    WEBRTC_TRACE(kTraceModuleCall, kTraceRtpRtcp, _id, "RemoveRTCPReportBlock()");

    return _rtcpSender.RemoveReportBlock(SSRC);
}

    /*
    *   (TMMBR) Temporary Max Media Bit Rate
    */
bool
ModuleRtpRtcpImpl::TMMBR() const
{
    WEBRTC_TRACE(kTraceModuleCall, kTraceRtpRtcp, _id, "TMMBR()");

    return _rtcpSender.TMMBR();
}

WebRtc_Word32
ModuleRtpRtcpImpl::SetTMMBRStatus(const bool enable)
{
    if(enable)
    {
        WEBRTC_TRACE(kTraceModuleCall, kTraceRtpRtcp, _id, "SetTMMBRStatus(enable)");
    }else
    {
        WEBRTC_TRACE(kTraceModuleCall, kTraceRtpRtcp, _id, "SetTMMBRStatus(disable)");
    }

    return _rtcpSender.SetTMMBRStatus(enable);
}

WebRtc_Word32
ModuleRtpRtcpImpl::TMMBRReceived(const WebRtc_UWord32 size,
                                 const WebRtc_UWord32 accNumCandidates,
                                 TMMBRSet* candidateSet) const
{
    WEBRTC_TRACE(kTraceModuleCall, kTraceRtpRtcp, _id, "TMMBRReceived()");

    return _rtcpReceiver.TMMBRReceived(size, accNumCandidates, candidateSet);
}

WebRtc_Word32
ModuleRtpRtcpImpl::SetTMMBN(const TMMBRSet* boundingSet,
                            const WebRtc_UWord32 maxBitrateKbit)
{
    WEBRTC_TRACE(kTraceModuleCall, kTraceRtpRtcp, _id, "SetTMMBN()");

    return _rtcpSender.SetTMMBN(boundingSet, maxBitrateKbit);
}

WebRtc_Word32
ModuleRtpRtcpImpl::RequestTMMBR(const WebRtc_UWord32 estimatedBW,
                                const WebRtc_UWord32 packetOH)
{
    WEBRTC_TRACE(kTraceModuleCall, kTraceRtpRtcp, _id, "RequestTMMBR()");

    return _rtcpSender.RequestTMMBR(estimatedBW, packetOH);
}

    /*
    *   (NACK) Negative acknowledgement
    */

    // Is Negative acknowledgement requests on/off?
NACKMethod
ModuleRtpRtcpImpl::NACK() const
{
    WEBRTC_TRACE(kTraceModuleCall, kTraceRtpRtcp, _id, "NACK()");

    NACKMethod childMethod = kNackOff;
    const bool defaultInstance(_childModules.Empty() ? false : true);
    if (defaultInstance)
    {
        // for default we need to check all child modules too
        CriticalSectionScoped lock(_criticalSectionModulePtrs);
        ListItem* item = _childModules.First();
        while(item)
        {
            RtpRtcp* module = static_cast<RtpRtcp*>(item->GetItem());
            if (module)
            {
                NACKMethod nackMethod = module->NACK();
                if (nackMethod != kNackOff)
                {
                    childMethod = nackMethod;
                    break;
                }
            }
            item = _childModules.Next(item);
        }
    }

    NACKMethod method = _nackMethod;
    if (childMethod != kNackOff)
    {
        method = childMethod;
    }
    return method;
}

    // Turn negative acknowledgement requests on/off
WebRtc_Word32
ModuleRtpRtcpImpl::SetNACKStatus(NACKMethod method)
{
    WEBRTC_TRACE(kTraceModuleCall, kTraceRtpRtcp, _id, "SetNACKStatus(%u)",method);

    _nackMethod = method;
    _rtpReceiver.SetNACKStatus(method);
    return 0;
}

    // Send a Negative acknowledgement packet
WebRtc_Word32
ModuleRtpRtcpImpl::SendNACK(const WebRtc_UWord16* nackList,
                            const WebRtc_UWord16 size)
{
    WEBRTC_TRACE(kTraceModuleCall, kTraceRtpRtcp, _id, "SendNACK(size:%u)", size);

    if(size > NACK_PACKETS_MAX_SIZE)
    {
        RequestKeyFrame(kVideoFrameKey);
        return -1;
    }
    WebRtc_UWord16 avgRTT = 0;
    _rtcpReceiver.RTT(_rtpReceiver.SSRC(),NULL, &avgRTT, NULL, NULL);

    WebRtc_UWord32 waitTime = 5 +((avgRTT*3)>>1); // 5 + RTT*1.5
    if(waitTime==5)
    {
        waitTime = 100; //During startup we don't have an RTT
    }
      const WebRtc_UWord32 now = ModuleRTPUtility::GetTimeInMS();
    const WebRtc_UWord32 timeLimit = now - waitTime;

    if(_nackLastTimeSent < timeLimit)
    {
        // send list
    } else
    {
        // only send if extended list
        if(_nackLastSeqNumberSent == nackList[size-1])
        {
            // last seq num is the same don't send list
            return 0;
        }else
        {
            // send list
        }
    }
    _nackLastTimeSent =  now;
    _nackLastSeqNumberSent = nackList[size-1];

    switch(_nackMethod)
    {
    case kNackRtcp:
        return _rtcpSender.SendRTCP(kRtcpNack, size, nackList);
    case kNackOff:
        return -1;
    default:
        assert(false);
    };
    return -1;
}

    // Store the sent packets, needed to answer to a Negative acknowledgement requests
WebRtc_Word32
ModuleRtpRtcpImpl::SetStorePacketsStatus(const bool enable, const WebRtc_UWord16 numberToStore)
{
    if(enable)
    {
        WEBRTC_TRACE(kTraceModuleCall, kTraceRtpRtcp, _id, "SetStorePacketsStatus(enable, numberToStore:%d)", numberToStore);
    }else
    {
        WEBRTC_TRACE(kTraceModuleCall, kTraceRtpRtcp, _id, "SetStorePacketsStatus(disable)");
    }

    return _rtpSender.SetStorePacketsStatus(enable, numberToStore);
}

    /*
    *   Audio
    */

    // Outband TelephoneEvent detection
WebRtc_Word32 ModuleRtpRtcpImpl::SetTelephoneEventStatus(const bool enable,
                                                       const bool forwardToDecoder,
                                                       const bool detectEndOfTone)
{
    WEBRTC_TRACE(kTraceModuleCall, kTraceRtpRtcp, _id, "SetTelephoneEventStatus(enable:%d forwardToDecoder:%d detectEndOfTone:%d)", enable, forwardToDecoder, detectEndOfTone);

    return _rtpReceiver.SetTelephoneEventStatus(enable, forwardToDecoder, detectEndOfTone);
}

    // Is outband TelephoneEvent turned on/off?
bool ModuleRtpRtcpImpl::TelephoneEvent() const
{
    WEBRTC_TRACE(kTraceModuleCall, kTraceRtpRtcp, _id, "TelephoneEvent()");

    return _rtpReceiver.TelephoneEvent();
}

    // Is forwarding of outband telephone events turned on/off?
bool ModuleRtpRtcpImpl::TelephoneEventForwardToDecoder() const
{
    WEBRTC_TRACE(kTraceModuleCall, kTraceRtpRtcp, _id, "TelephoneEventForwardToDecoder()");

    return _rtpReceiver.TelephoneEventForwardToDecoder();
}

    // Send a TelephoneEvent tone using RFC 2833 (4733)
WebRtc_Word32
ModuleRtpRtcpImpl::SendTelephoneEventOutband(const WebRtc_UWord8 key,
                                             const WebRtc_UWord16 timeMs,
                                             const WebRtc_UWord8 level)
{
    WEBRTC_TRACE(kTraceModuleCall, kTraceRtpRtcp, _id, "SendTelephoneEventOutband(key:%u, timeMs:%u, level:%u)", key, timeMs, level);

    return _rtpSender.SendTelephoneEvent(key, timeMs, level);
}

bool
ModuleRtpRtcpImpl::SendTelephoneEventActive(WebRtc_Word8& telephoneEvent) const
{
    WEBRTC_TRACE(kTraceModuleCall, kTraceRtpRtcp, _id, "SendTelephoneEventActive()");

    return _rtpSender.SendTelephoneEventActive(telephoneEvent);
}

    // set audio packet size, used to determine when it's time to send a DTMF packet in silence (CNG)
WebRtc_Word32
ModuleRtpRtcpImpl::SetAudioPacketSize(const WebRtc_UWord16 packetSizeSamples)
{
    WEBRTC_TRACE(kTraceModuleCall, kTraceRtpRtcp, _id, "SetAudioPacketSize(%u)", packetSizeSamples);

    return _rtpSender.SetAudioPacketSize(packetSizeSamples);
}

WebRtc_Word32
ModuleRtpRtcpImpl::SetRTPAudioLevelIndicationStatus(const bool enable,
                                                    const WebRtc_UWord8 ID)
{
    WEBRTC_TRACE(kTraceModuleCall, kTraceRtpRtcp, _id,
        "SetRTPAudioLevelIndicationStatus(enable=%d, ID=%u)", enable, ID);
    return _rtpSender.SetAudioLevelIndicationStatus(enable, ID);
}

WebRtc_Word32
ModuleRtpRtcpImpl::GetRTPAudioLevelIndicationStatus(bool& enable,
                                                    WebRtc_UWord8& ID) const
{
    WEBRTC_TRACE(kTraceModuleCall, kTraceRtpRtcp, _id, "GetRTPAudioLevelIndicationStatus()");
    return _rtpSender.AudioLevelIndicationStatus(enable, ID);
}

WebRtc_Word32
ModuleRtpRtcpImpl::SetAudioLevel(const WebRtc_UWord8 level_dBov)
{
    WEBRTC_TRACE(kTraceModuleCall, kTraceRtpRtcp, _id, "SetAudioLevel(level_dBov:%u)", level_dBov);
    return _rtpSender.SetAudioLevel(level_dBov);
}

    // Set payload type for Redundant Audio Data RFC 2198
WebRtc_Word32
ModuleRtpRtcpImpl::SetSendREDPayloadType(const WebRtc_Word8 payloadType)
{
    WEBRTC_TRACE(kTraceModuleCall, kTraceRtpRtcp, _id, "SetSendREDPayloadType(%d)", payloadType);

    return _rtpSender.SetRED(payloadType);
}

    // Get payload type for Redundant Audio Data RFC 2198
WebRtc_Word32
ModuleRtpRtcpImpl::SendREDPayloadType(WebRtc_Word8& payloadType) const
{
    WEBRTC_TRACE(kTraceModuleCall, kTraceRtpRtcp, _id, "SendREDPayloadType()");

    return _rtpSender.RED(payloadType);
}


    /*
    *   Video
    */
RtpVideoCodecTypes
ModuleRtpRtcpImpl::ReceivedVideoCodec() const
{
    return _rtpReceiver.VideoCodecType();
}

RtpVideoCodecTypes
ModuleRtpRtcpImpl::SendVideoCodec() const
{
    return _rtpSender.VideoCodecType();
}

WebRtc_Word32
ModuleRtpRtcpImpl::SetSendBitrate(const WebRtc_UWord32 startBitrate,
                                  const WebRtc_UWord16 minBitrateKbit,
                                  const WebRtc_UWord16 maxBitrateKbit)
{
    WEBRTC_TRACE(kTraceModuleCall, kTraceRtpRtcp, _id, "SetSendBitrate start:%ubit/s min:%uKbit/s max:%uKbit/s", startBitrate, minBitrateKbit, maxBitrateKbit);

    const bool defaultInstance(_childModules.Empty()?false:true);

    if(defaultInstance)
    {
        // for default we need to update all child modules too
        CriticalSectionScoped lock(_criticalSectionModulePtrs);

        ListItem* item = _childModules.First();
        while (item)
        {
            RtpRtcp* module = (RtpRtcp*)item->GetItem();
            if(module)
            {
                module->SetSendBitrate(startBitrate, minBitrateKbit, maxBitrateKbit);
            }
            item = _childModules.Next(item);
        }
    }
    _rtpSender.SetTargetSendBitrate(startBitrate);

    return _bandwidthManagement.SetSendBitrate(startBitrate, minBitrateKbit, maxBitrateKbit);
}

WebRtc_Word32
ModuleRtpRtcpImpl::SetKeyFrameRequestMethod(const KeyFrameRequestMethod method)
{
    WEBRTC_TRACE(kTraceModuleCall, kTraceRtpRtcp, _id, "SetKeyFrameRequestMethod(method:%u)",method);

    _keyFrameReqMethod = method;
    return 0;
}

WebRtc_Word32
ModuleRtpRtcpImpl::RequestKeyFrame(const FrameType frameType)
{
    WEBRTC_TRACE(kTraceModuleCall, kTraceRtpRtcp, _id, "RequestKeyFrame(frameType:%d)",frameType);

    switch(_keyFrameReqMethod)
    {
    case kKeyFrameReqFirRtp:
        return _rtpSender.SendRTPIntraRequest();

    case kKeyFrameReqPliRtcp:
        return _rtcpSender.SendRTCP(kRtcpPli);

    case kKeyFrameReqFirRtcp:
        {
            // conference scenario
            WebRtc_UWord16 RTT = 0;
            _rtcpReceiver.RTT(_rtpReceiver.SSRC(), &RTT, NULL,NULL,NULL);
            return _rtcpSender.SendRTCP(kRtcpFir, 0,NULL, RTT);
        }
    default:
        assert(false);
        return -1;
    }
}

WebRtc_Word32
ModuleRtpRtcpImpl::SendRTCPSliceLossIndication(const WebRtc_UWord8 pictureID)
{
    WEBRTC_TRACE(kTraceModuleCall, kTraceRtpRtcp, _id, "SendRTCPSliceLossIndication (pictureID:%d)", pictureID);
    return _rtcpSender.SendRTCP(kRtcpSli, 0,0,0, pictureID);
}

WebRtc_Word32
ModuleRtpRtcpImpl::SetCameraDelay(const WebRtc_Word32 delayMS)
{
    WEBRTC_TRACE(kTraceModuleCall, kTraceRtpRtcp, _id, "SetCameraDelay(%d)",delayMS);
    const bool defaultInstance(_childModules.Empty()?false:true);

    if(defaultInstance)
    {
        CriticalSectionScoped lock(_criticalSectionModulePtrs);

        ListItem* item = _childModules.First();
        while (item)
        {
            RtpRtcp* module = (RtpRtcp*)item->GetItem();
            if(module)
            {
                module->SetCameraDelay(delayMS);
            }
            item = _childModules.Next(item);
        }
        return 0;
    } else
    {
        return _rtcpSender.SetCameraDelay(delayMS);
    }
}

WebRtc_Word32
ModuleRtpRtcpImpl::SetGenericFECStatus(const bool enable,
                                       const WebRtc_UWord8 payloadTypeRED,
                                       const WebRtc_UWord8 payloadTypeFEC)
{
    if(enable)
    {
        WEBRTC_TRACE(kTraceModuleCall, kTraceRtpRtcp, _id, "SetGenericFECStatus(enable, %u)", payloadTypeRED);
    } else
    {
        WEBRTC_TRACE(kTraceModuleCall, kTraceRtpRtcp, _id, "SetGenericFECStatus(disable)");
    }
    return _rtpSender.SetGenericFECStatus(enable, payloadTypeRED, payloadTypeFEC);
}

WebRtc_Word32
ModuleRtpRtcpImpl::GenericFECStatus(bool& enable, WebRtc_UWord8& payloadTypeRED, WebRtc_UWord8& payloadTypeFEC)
{
    WEBRTC_TRACE(kTraceModuleCall, kTraceRtpRtcp, _id, "GenericFECStatus()");

    bool childEnabled = false;
    const bool defaultInstance(_childModules.Empty() ? false : true);
    if (defaultInstance)
    {
        // for default we need to check all child modules too
        CriticalSectionScoped lock(_criticalSectionModulePtrs);
        ListItem* item = _childModules.First();
        while(item)
        {
            RtpRtcp* module = static_cast<RtpRtcp*>(item->GetItem());
            if (module)
            {
                bool enabled = false;
                WebRtc_UWord8 dummyPTypeRED = 0;
                WebRtc_UWord8 dummyPTypeFEC = 0;
                if (module->GenericFECStatus(enabled, dummyPTypeRED, dummyPTypeFEC) == 0 && enabled)
                {
                    childEnabled = true;
                    break;
                }
            }
            item = _childModules.Next(item);
        }
    }

    WebRtc_Word32 retVal = _rtpSender.GenericFECStatus(enable, payloadTypeRED, payloadTypeFEC);
    if (childEnabled)
    {
        // returns true if enabled for any child module
        enable = childEnabled;
    }
    return retVal;
}

WebRtc_Word32
ModuleRtpRtcpImpl::SetFECCodeRate(const WebRtc_UWord8 keyFrameCodeRate,
                                  const WebRtc_UWord8 deltaFrameCodeRate)
{
    WEBRTC_TRACE(kTraceModuleCall, kTraceRtpRtcp, _id, "SetFECCodeRate(%u, %u)", keyFrameCodeRate, deltaFrameCodeRate);

    const bool defaultInstance(_childModules.Empty()?false:true);
    if (defaultInstance)
    {
        // for default we need to update all child modules too
        CriticalSectionScoped lock(_criticalSectionModulePtrs);

        ListItem* item = _childModules.First();
        while (item)
        {
            RtpRtcp* module = (RtpRtcp*)item->GetItem();
            if (module)
            {
                module->SetFECCodeRate(keyFrameCodeRate, deltaFrameCodeRate);
            }
            item = _childModules.Next(item);
        }
        return 0;

    } else
    {
        return _rtpSender.SetFECCodeRate(keyFrameCodeRate, deltaFrameCodeRate);
    }
}

    /*
    *   Implementation of ModuleRtpRtcpPrivate
    */
void
ModuleRtpRtcpImpl::SetRemoteSSRC(const WebRtc_UWord32 SSRC)
{
    // inform about the incoming SSRC
    _rtcpSender.SetRemoteSSRC(SSRC);
    _rtcpReceiver.SetRemoteSSRC(SSRC);

    // check for a SSRC collision
    if(_rtpSender.SSRC() == SSRC && !_collisionDetected ) // loopback
    {
        // if we detect a collision change the SSRC but only once
        _collisionDetected = true;
        WebRtc_UWord32 newSSRC =_rtpSender.GenerateNewSSRC();
        if(newSSRC == 0)
        {
            // configured via API ignore
            return;
        }
        if(kRtcpOff != _rtcpSender.Status())
        {
            // send RTCP bye on the current SSRC
            _rtcpSender.SendRTCP(kRtcpBye);
        }
        // change local SSRC

        // inform all objects about the new SSRC
        _rtcpSender.SetSSRC(newSSRC);
        _rtcpReceiver.SetSSRC(newSSRC);
    }
}

WebRtc_UWord32
ModuleRtpRtcpImpl::BitrateReceivedNow() const
{
    return _rtpReceiver.BitrateNow();
}

WebRtc_UWord32
ModuleRtpRtcpImpl::BitrateSent() const
{
    const bool defaultInstance(_childModules.Empty()?false:true);

    if(defaultInstance)
    {
        // for default we need to update the send bitrate
        CriticalSectionScoped lock(_criticalSectionModulePtrsFeedback);

        ListItem* item = _childModules.First();
        WebRtc_UWord32 bitrate = 0;
        while (item)
        {
            RtpRtcp* module = (RtpRtcp*)item->GetItem();
            if(module)
            {
                bitrate = (module->BitrateSent() > bitrate) ?module->BitrateSent():bitrate;
            }
            item = _childModules.Next(item);
        }
        return bitrate;
    } else
    {
        return _rtpSender.BitrateLast();
    }
}

// for lip sync
void
ModuleRtpRtcpImpl::OnReceivedNTP()
{
    // don't do anything if we are the audio module
    // video module is responsible for sync

    if(!_audio)
    {
        WebRtc_Word32 diff = 0;
        WebRtc_UWord32 receivedNTPsecs = 0;
        WebRtc_UWord32 receivedNTPfrac= 0;
        WebRtc_UWord32 RTCPArrivalTimeSecs= 0;
        WebRtc_UWord32 RTCPArrivalTimeFrac= 0;

        if(0 == _rtcpReceiver.NTP(&receivedNTPsecs, &receivedNTPfrac, &RTCPArrivalTimeSecs, &RTCPArrivalTimeFrac))
        {
            CriticalSectionScoped lock(_criticalSectionModulePtrs);

            if(_audioModule)
            {
                if(0 != _audioModule->RemoteNTP(&_receivedNTPsecsAudio,
                                                &_receivedNTPfracAudio,
                                                &_RTCPArrivalTimeSecsAudio,
                                                &_RTCPArrivalTimeFracAudio))
                {
                    // failed ot get audio NTP
                    return;
                }
            }
            if(_receivedNTPfracAudio != 0)
            {
                // ReceivedNTPxxx is NTP at sender side when sent.
                // RTCPArrivalTimexxx is NTP at receiver side when received.
                // can't use ConvertNTPTimeToMS since calculation can be negative

                WebRtc_Word32 NTPdiff = (WebRtc_Word32)((_receivedNTPsecsAudio - receivedNTPsecs)*1000); // ms
                NTPdiff += (WebRtc_Word32)(_receivedNTPfracAudio/FracMS - receivedNTPfrac/FracMS); // ms

                WebRtc_Word32 RTCPdiff = (WebRtc_Word32)((_RTCPArrivalTimeSecsAudio - RTCPArrivalTimeSecs)*1000); // ms
                RTCPdiff += (WebRtc_Word32)((_RTCPArrivalTimeFracAudio/FracMS - RTCPArrivalTimeFrac/FracMS)); // ms

                diff = NTPdiff - RTCPdiff;
                // if diff is + video is behind
                if(diff < -1000 || diff > 1000)
                {
                    // unresonable ignore value.
                    diff = 0;
                    return;
                }
            }
        }
        // export via callback
        // after release of critsect
        _rtcpReceiver.UpdateLipSync(diff);
    }
}

// our local BW estimate is updated
void
ModuleRtpRtcpImpl::OnBandwidthEstimateUpdate(WebRtc_UWord16 bandWidthKbit)
{
    WebRtc_UWord32 maxBitrateKbit = _rtpReceiver.MaxConfiguredBitrate()/1000;
    if(maxBitrateKbit)
    {
        // the app has set a max bitrate
        if(maxBitrateKbit < bandWidthKbit)
        {
            // cap TMMBR at max configured bitrate
            bandWidthKbit = (WebRtc_UWord16)maxBitrateKbit;
        }
    }
    if(_rtcpSender.TMMBR())
    {
        /* Maximum total media bit rate:
            The upper limit on total media bit rate for a given media
            stream at a particular receiver and for its selected protocol
            layer.  Note that this value cannot be measured on the
            received media stream.  Instead, it needs to be calculated or
            determined through other means, such as quality of service
            (QoS) negotiations or local resource limitations.  Also note
            that this value is an average (on a timescale that is
            reasonable for the application) and that it may be different
            from the instantaneous bit rate seen by packets in the media
            stream.
        */
        /* Overhead:
            All protocol header information required to convey a packet
            with media data from sender to receiver, from the application
            layer down to a pre-defined protocol level (for example, down
            to, and including, the IP header).  Overhead may include, for
            example, IP, UDP, and RTP headers, any layer 2 headers, any
            Contributing Sources (CSRCs), RTP padding, and RTP header
            extensions.  Overhead excludes any RTP payload headers and the
            payload itself.
        */
        WebRtc_UWord16 RTPpacketOH = _rtpReceiver.PacketOHReceived();

        // call RequestTMMBR when our localy created estimate changes
        _rtcpSender.RequestTMMBR(bandWidthKbit, 0/*RTPpacketOH + _packetOverHead*/);
    }
}

RateControlRegion
ModuleRtpRtcpImpl::OnOverUseStateUpdate(const RateControlInput& rateControlInput)
{
    bool firstOverUse = false;
    const RateControlRegion region = _rtcpSender.UpdateOverUseState(rateControlInput, firstOverUse);
    if (firstOverUse && _rtcpSender.Status() == kRtcpNonCompound)
    {
        // Send TMMBR immediately
        WebRtc_UWord16 RTT = 0;
        _rtcpReceiver.RTT(_rtpReceiver.SSRC(), &RTT, NULL,NULL,NULL);
        _rtcpSender.SendRTCP(kRtcpTmmbr, 0, 0, RTT);
    }
    return region;
}

// bad state of RTP receiver request a keyframe
void
ModuleRtpRtcpImpl::OnRequestIntraFrame(const FrameType frameType)
{
    RequestKeyFrame(frameType);
}

void
ModuleRtpRtcpImpl::OnReceivedIntraFrameRequest(const WebRtc_UWord8 message)
{
    if(_defaultModule)
    {
        CriticalSectionScoped lock(_criticalSectionModulePtrs);
        if(_defaultModule)
        {
            // if we use a default module pass this info to the default module
            _defaultModule->OnReceivedIntraFrameRequest(message);
            return;
        }
    }
    _rtcpReceiver.OnReceivedIntraFrameRequest(message);
}

// received a request for a new SLI
void
ModuleRtpRtcpImpl::OnReceivedSliceLossIndication(const WebRtc_UWord8 pictureID)
{
    if(_defaultModule)
    {
        CriticalSectionScoped lock(_criticalSectionModulePtrs);
        if(_defaultModule)
        {
            // if we use a default module pass this info to the default module
            _defaultModule->OnReceivedSliceLossIndication(pictureID);
            return;
        }
    }
    _rtcpReceiver.OnReceivedSliceLossIndication(pictureID);
}

// received a new refereence frame
void
ModuleRtpRtcpImpl::OnReceivedReferencePictureSelectionIndication(const WebRtc_UWord64 pictureID)
{
    if(_defaultModule)
    {
        CriticalSectionScoped lock(_criticalSectionModulePtrs);
        if(_defaultModule)
        {
            // if we use a default module pass this info to the default module
            _defaultModule->OnReceivedReferencePictureSelectionIndication(pictureID);
            return;
        }
    }
    _rtcpReceiver.OnReceivedReferencePictureSelectionIndication(pictureID);
}

void
ModuleRtpRtcpImpl::OnReceivedBandwidthEstimateUpdate(const WebRtc_UWord16 bwEstimateMinKbit,
                                                     const WebRtc_UWord16 bwEstimateMaxKbit)
{
    if(_defaultModule)
    {
        CriticalSectionScoped lock(_criticalSectionModulePtrs);
        if(_defaultModule)
        {
            // if we use a default module pass this info to the default module
            _defaultModule->OnReceivedBandwidthEstimateUpdate(bwEstimateMinKbit, bwEstimateMaxKbit);
            return;
        }
    }
    if(_audio)
    {
        _rtcpReceiver.UpdateBandwidthEstimate(bwEstimateMinKbit);
    }else
    {
        WebRtc_UWord32 newBitrate = 0;
        WebRtc_UWord8 fractionLost = 0;
        WebRtc_UWord16 roundTripTime = 0;
        if(_bandwidthManagement.UpdateBandwidthEstimate(bwEstimateMinKbit, bwEstimateMaxKbit, newBitrate, fractionLost,roundTripTime) == 0)
        {
            // video callback
            _rtpReceiver.UpdateBandwidthManagement(newBitrate, newBitrate, fractionLost, roundTripTime, bwEstimateMinKbit, bwEstimateMaxKbit);
            const bool defaultInstance = !_childModules.Empty();
            if((newBitrate > 0) && !defaultInstance)
            {
                // update bitrate
                _rtpSender.SetTargetSendBitrate(newBitrate);
            }
        }
    }
}

// bw estimation
void
ModuleRtpRtcpImpl::OnPacketLossStatisticsUpdate(const WebRtc_UWord8 fractionLost,
                                                const WebRtc_UWord16 roundTripTime,
                                                const WebRtc_UWord32 lastReceivedExtendedHighSeqNum,
                                                const WebRtc_UWord32 jitter)
{
    WebRtc_UWord32 newBitrate = 0;
    WebRtc_UWord8 filteredFractionLost = fractionLost;
    WebRtc_UWord16 filteredRoundTripTime = roundTripTime;
    WebRtc_UWord16 bwEstimateKbitMin = 0;
    WebRtc_UWord16 bwEstimateKbitMax = 0;

    const bool defaultInstance(_childModules.Empty()?false:true);
    {
        if(_bandwidthManagement.UpdatePacketLoss(lastReceivedExtendedHighSeqNum,
                                                 defaultInstance,
                                                 fractionLost,
                                                 roundTripTime,
                                                 newBitrate,
                                                 bwEstimateKbitMin,
                                                 bwEstimateKbitMax) != 0)
        {
            // ignore this update
            newBitrate = 0;
        }
    }
    if(newBitrate != 0 &&
       !defaultInstance)
    {
        // We need to do update RTP sender before calling default module in
        // case we'll strip any layers.
        _rtpSender.SetTargetSendBitrate(newBitrate);

        if(_defaultModule)
        {
            // if we have a default module update it
            CriticalSectionScoped lock(_criticalSectionModulePtrs);
            if(_defaultModule) // we need to check again inside the critsect
            {
                // if we use a default module pass this info to the default module
                _defaultModule->OnPacketLossStatisticsUpdate(filteredFractionLost,
                                                             filteredRoundTripTime,
                                                             lastReceivedExtendedHighSeqNum,
                                                             jitter);
            }
            return;
        }
        // video callback
        _rtpReceiver.UpdateBandwidthManagement(newBitrate, newBitrate, filteredFractionLost, filteredRoundTripTime, bwEstimateKbitMin, bwEstimateKbitMax);
    }
    else if (defaultInstance)
    {
        // Check if it's time to update bitrate
        WebRtc_UWord32 now = ModuleRTPUtility::GetTimeInMS();
        if((now - _lastChildBitrateUpdate) > (3*RTCP_INTERVAL_VIDEO_MS/2))
        {
            WebRtc_UWord32 minBitrateBps = 0xffffffff;
            WebRtc_UWord32 maxBitrateBps = 0;
            {
                // Time to update bitrate estimate,
                // get min and max for the sending channels
                CriticalSectionScoped lock(_criticalSectionModulePtrs);

                ListItem* item = _childModules.First();
                while(item)
                {
                    // Get child RTP sender and ask for bitrate estimate
                    ModuleRtpRtcpPrivate* childModule = (ModuleRtpRtcpPrivate*)item->GetItem();
                    if (childModule->Sending())
                    {
                        RTPSender& childRtpSender = static_cast<ModuleRtpRtcpImpl*>(item->GetItem())->_rtpSender;
                        WebRtc_UWord32 childEstimateBps = 1000*childRtpSender.TargetSendBitrateKbit();
                        if (childEstimateBps < minBitrateBps)
                        {
                            minBitrateBps = childEstimateBps;
                        }
                        if (childEstimateBps > maxBitrateBps)
                        {
                            maxBitrateBps = childEstimateBps;
                        }
                    }
                    item = _childModules.Next(item);
                }
            }
            // Limit the bitrate with TMMBR.
            if(bwEstimateKbitMin && bwEstimateKbitMin<minBitrateBps/1000)
            {
                minBitrateBps=bwEstimateKbitMin*1000;
            }
            if(bwEstimateKbitMax && bwEstimateKbitMax<maxBitrateBps/1000)
            {
                maxBitrateBps=bwEstimateKbitMax*1000;
            }
            _bandwidthManagement.SetSendBitrate(minBitrateBps,0,0); // Update default module bitrate. Don't care about min max.
            if (maxBitrateBps > 0)
            {
                // video callback
                _rtpReceiver.UpdateBandwidthManagement(minBitrateBps, maxBitrateBps, filteredFractionLost, filteredRoundTripTime, bwEstimateKbitMin, bwEstimateKbitMax);
            }
            _lastChildBitrateUpdate = now;
        }

    }
}

void
ModuleRtpRtcpImpl::OnRequestSendReport()
{
    _rtcpSender.SendRTCP(kRtcpSr);
}

WebRtc_Word32
ModuleRtpRtcpImpl::SendRTCPReferencePictureSelection(const WebRtc_UWord64 pictureID)
{
    return _rtcpSender.SendRTCP(kRtcpRpsi, 0,0,0, pictureID);
}

WebRtc_UWord32
ModuleRtpRtcpImpl::SendTimeOfSendReport(const WebRtc_UWord32 sendReport)
{
    return _rtcpSender.SendTimeOfSendReport(sendReport);
}

void
ModuleRtpRtcpImpl::OnReceivedNACK(const WebRtc_UWord16 nackSequenceNumbersLength,
                                  const WebRtc_UWord16* nackSequenceNumbers)
{
    if(!_rtpSender.StorePackets() || nackSequenceNumbers == NULL || nackSequenceNumbersLength == 0)
    {
        return;
    }
    WebRtc_UWord16 avgRTT = 0;
    _rtcpReceiver.RTT(_rtpReceiver.SSRC(), NULL, &avgRTT ,NULL,NULL);
    _rtpSender.OnReceivedNACK(nackSequenceNumbersLength, nackSequenceNumbers, avgRTT);
}

WebRtc_Word32
ModuleRtpRtcpImpl::LastReceivedNTP(WebRtc_UWord32& RTCPArrivalTimeSecs, // when we received the last report
                                   WebRtc_UWord32& RTCPArrivalTimeFrac,
                                   WebRtc_UWord32& remoteSR)  // NTP inside the last received (mid 16 bits from sec and frac)
{
    WebRtc_UWord32 NTPsecs = 0;
    WebRtc_UWord32 NTPfrac = 0;

    if(-1 == _rtcpReceiver.NTP(&NTPsecs, &NTPfrac, &RTCPArrivalTimeSecs, &RTCPArrivalTimeFrac))
    {
        return -1;
    }
    remoteSR = ((NTPsecs & 0x0000ffff) << 16) + ((NTPfrac & 0xffff0000) >> 16);
    return 0;
}

void
ModuleRtpRtcpImpl::OnReceivedTMMBR()
{
    // we received a TMMBR in a RTCP packet
    // answer with a TMMBN
    UpdateTMMBR();
}

bool
ModuleRtpRtcpImpl::UpdateRTCPReceiveInformationTimers()
{
    // if this returns true this channel has timed out
    // periodically check if this is true and if so call UpdateTMMBR
    return _rtcpReceiver.UpdateRTCPReceiveInformationTimers();
}

WebRtc_Word32
ModuleRtpRtcpImpl::UpdateTMMBR()
{
    WebRtc_Word32 numBoundingSet = 0;
    WebRtc_Word32 newBitrates = 0;
    WebRtc_UWord32 minBitrateKbit = 0;
    WebRtc_UWord32 maxBitrateKbit = 0;

    if(_defaultModule)
    {
        CriticalSectionScoped lock(_criticalSectionModulePtrs);

        // no callbacks allowed inside here
        if(_defaultModule)
        {
            // let the default module do the update
            return _defaultModule->UpdateTMMBR();
        }
    }

    WebRtc_UWord32 accNumCandidates = 0;

    // Find candidate set
    if(!_childModules.Empty())
    {
        CriticalSectionScoped lock(_criticalSectionModulePtrsFeedback);

        // this module is the default module
        // loop over all modules using the default codec
        WebRtc_UWord32 size = 0;
        ListItem* item = _childModules.First();
        while(item)
        {
            ModuleRtpRtcpPrivate* module = (ModuleRtpRtcpPrivate*)item->GetItem();
            WebRtc_Word32 tmpSize = module->TMMBRReceived(0,0, NULL);
            if(tmpSize > 0)
            {
                size += tmpSize;
            }
            item = _childModules.Next(item);
        }
        TMMBRSet* candidateSet = VerifyAndAllocateCandidateSet(size);
        if(candidateSet == NULL)
        {
            return -1;
        }

        item = _childModules.First();
        while(item)
        {
            ModuleRtpRtcpPrivate* module = (ModuleRtpRtcpPrivate*)item->GetItem();
            if(size > accNumCandidates && module)
            {
                WebRtc_Word32 accSize = module->TMMBRReceived(size, accNumCandidates, candidateSet);
                if (accSize > 0)
                {
                    accNumCandidates = accSize;
                }
            }
            item = _childModules.Next(item);
        }
    } else
    {
        // this module don't use the default module and is not the default module
        WebRtc_Word32 size = _rtcpReceiver.TMMBRReceived(0,0,NULL);
        if(size > 0)
        {
            TMMBRSet* candidateSet = VerifyAndAllocateCandidateSet(size);
            // get candidate set from receiver
            accNumCandidates = _rtcpReceiver.TMMBRReceived(size, accNumCandidates, candidateSet);
        }
        else
        {
            // candidate set empty
            VerifyAndAllocateCandidateSet(0); // resets candidate set
        }
    }

    // Find bounding set
    TMMBRSet* boundingSet = NULL;
    numBoundingSet = FindTMMBRBoundingSet(boundingSet);
    if(numBoundingSet == -1)
    {
        WEBRTC_TRACE(kTraceWarning, kTraceRtpRtcp, _id, "Failed to find TMMBR bounding set.");
        return -1;
    }

    // Set bounding set
    // Inform remote clients about the new bandwidth
    if(_childModules.Empty())
    {
        // inform the remote client
        _rtcpSender.SetTMMBN(boundingSet, _rtpSender.MaxConfiguredBitrateVideo()/1000);  // might trigger a TMMBN
    } else
    {
        // inform child modules using the default codec
        CriticalSectionScoped lock(_criticalSectionModulePtrsFeedback);
        ListItem* item = _childModules.First();
        while(item)
        {
            ModuleRtpRtcpPrivate* module = (ModuleRtpRtcpPrivate*)item->GetItem();
            if( module)
            {
                module->SetTMMBN(boundingSet, _rtpSender.MaxConfiguredBitrateVideo()/1000);
            }
            item = _childModules.Next(item);
        }
    }

    if(numBoundingSet == 0)
    {
        // owner of max bitrate request has timed out
        // empty bounding set has been sent
        return 0;
    }

    // Get net bitrate from bounding set depending on sent packet rate
    newBitrates = CalcMinMaxBitRate(_rtpSender.PacketRate(),
                                    (WebRtc_UWord32)numBoundingSet,
                                    minBitrateKbit,
                                    maxBitrateKbit);

    // no critsect when calling out to "unknown" code
    if(newBitrates == 0) // we have new bitrates
    {
        // Set new max bitrate
        // we have a new bandwidth estimate on this channel
        OnReceivedBandwidthEstimateUpdate((WebRtc_UWord16)minBitrateKbit, (WebRtc_UWord16)maxBitrateKbit);
        WEBRTC_TRACE(kTraceStream, kTraceRtpRtcp, _id, "Set TMMBR request min:%d kbps max:%d kbps, channel: %d", minBitrateKbit, maxBitrateKbit, _id);
    }
    return 0;
}

// called from RTCPsender
WebRtc_Word32
ModuleRtpRtcpImpl::BoundingSet(bool &tmmbrOwner,
                               TMMBRSet*& boundingSet)
{
    return _rtcpReceiver.BoundingSet(tmmbrOwner,
                                     boundingSet);
}

WebRtc_Word32
ModuleRtpRtcpImpl::SetH263InverseLogic(const bool enable)
{
    WEBRTC_TRACE(kTraceModuleCall, kTraceRtpRtcp, _id, "SetH263InverseLogic(%s)", enable ? "true":"false");
    return _rtpReceiver.SetH263InverseLogic(enable);
}

void
ModuleRtpRtcpImpl::SendKeyFrame()
{
    WEBRTC_TRACE(kTraceStream, kTraceRtpRtcp, _id, "SendKeyFrame()");
    OnReceivedIntraFrameRequest(0);
    return;
}
} // namespace webrtc
