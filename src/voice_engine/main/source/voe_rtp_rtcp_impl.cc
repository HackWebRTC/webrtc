/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "voe_rtp_rtcp_impl.h"
#include "trace.h"
#include "file_wrapper.h"
#include "critical_section_wrapper.h"
#include "voice_engine_impl.h"
#include "voe_errors.h"

#include "channel.h"
#include "transmit_mixer.h"

namespace webrtc {

VoERTP_RTCP* VoERTP_RTCP::GetInterface(VoiceEngine* voiceEngine)
{
#ifndef WEBRTC_VOICE_ENGINE_RTP_RTCP_API
    return NULL;
#else
    if (NULL == voiceEngine)
    {
        return NULL;
    }
    VoiceEngineImpl* s = reinterpret_cast<VoiceEngineImpl*> (voiceEngine);
    VoERTP_RTCPImpl* d = s;
    (*d)++;
    return (d);
#endif
}

#ifdef WEBRTC_VOICE_ENGINE_RTP_RTCP_API

VoERTP_RTCPImpl::VoERTP_RTCPImpl()
{
    WEBRTC_TRACE(kTraceMemory, kTraceVoice, VoEId(_instanceId,-1),
                 "VoERTP_RTCPImpl::VoERTP_RTCPImpl() - ctor");
}

VoERTP_RTCPImpl::~VoERTP_RTCPImpl()
{
    WEBRTC_TRACE(kTraceMemory, kTraceVoice, VoEId(_instanceId,-1),
                 "VoERTP_RTCPImpl::~VoERTP_RTCPImpl() - dtor");
}

int VoERTP_RTCPImpl::Release()
{
    WEBRTC_TRACE(kTraceApiCall, kTraceVoice, VoEId(_instanceId,-1),
                 "VoERTP_RTCP::Release()");
    (*this)--;
    int refCount = GetCount();
    if (refCount < 0)
    {
        Reset();  // reset reference counter to zero => OK to delete VE
        _engineStatistics.SetLastError(
            VE_INTERFACE_NOT_FOUND, kTraceWarning);
        return (-1);
    }
    WEBRTC_TRACE(kTraceStateInfo, kTraceVoice, VoEId(_instanceId,-1),
                 "VoERTP_RTCP reference counter = %d", refCount);
    return (refCount);
}

int VoERTP_RTCPImpl::RegisterRTPObserver(int channel, VoERTPObserver& observer)
{
    WEBRTC_TRACE(kTraceApiCall, kTraceVoice, VoEId(_instanceId,-1),
                 "RegisterRTPObserver(channel=%d observer=0x%x)",
                 channel, &observer);
    if (!_engineStatistics.Initialized())
    {
        _engineStatistics.SetLastError(VE_NOT_INITED, kTraceError);
        return -1;
    }
    voe::ScopedChannel sc(_channelManager, channel);
    voe::Channel* channelPtr = sc.ChannelPtr();
    if (channelPtr == NULL)
    {
        _engineStatistics.SetLastError(
            VE_CHANNEL_NOT_VALID, kTraceError,
            "RegisterRTPObserver() failed to locate channel");
        return -1;
    }
    return channelPtr->RegisterRTPObserver(observer);
}

int VoERTP_RTCPImpl::DeRegisterRTPObserver(int channel)
{
    WEBRTC_TRACE(kTraceApiCall, kTraceVoice, VoEId(_instanceId,-1),
                 "DeRegisterRTPObserver(channel=%d)", channel);
    if (!_engineStatistics.Initialized())
    {
        _engineStatistics.SetLastError(VE_NOT_INITED, kTraceError);
        return -1;
    }
    voe::ScopedChannel sc(_channelManager, channel);
    voe::Channel* channelPtr = sc.ChannelPtr();
    if (channelPtr == NULL)
    {
        _engineStatistics.SetLastError(
            VE_CHANNEL_NOT_VALID, kTraceError,
            "DeRegisterRTPObserver() failed to locate channel");
        return -1;
    }
    return channelPtr->DeRegisterRTPObserver();
}

int VoERTP_RTCPImpl::RegisterRTCPObserver(int channel, VoERTCPObserver& observer)
{
    WEBRTC_TRACE(kTraceApiCall, kTraceVoice, VoEId(_instanceId,-1),
                 "RegisterRTCPObserver(channel=%d observer=0x%x)",
                 channel, &observer);
    if (!_engineStatistics.Initialized())
    {
        _engineStatistics.SetLastError(VE_NOT_INITED, kTraceError);
        return -1;
    }
    voe::ScopedChannel sc(_channelManager, channel);
    voe::Channel* channelPtr = sc.ChannelPtr();
    if (channelPtr == NULL)
    {
        _engineStatistics.SetLastError(
            VE_CHANNEL_NOT_VALID, kTraceError,
            "RegisterRTPObserver() failed to locate channel");
        return -1;
    }
    return channelPtr->RegisterRTCPObserver(observer);
}

int VoERTP_RTCPImpl::DeRegisterRTCPObserver(int channel)
{
    WEBRTC_TRACE(kTraceApiCall, kTraceVoice, VoEId(_instanceId,-1),
                 "DeRegisterRTCPObserver(channel=%d)", channel);
    if (!_engineStatistics.Initialized())
    {
        _engineStatistics.SetLastError(VE_NOT_INITED, kTraceError);
        return -1;
    }
    voe::ScopedChannel sc(_channelManager, channel);
    voe::Channel* channelPtr = sc.ChannelPtr();
    if (channelPtr == NULL)
    {
        _engineStatistics.SetLastError(
            VE_CHANNEL_NOT_VALID, kTraceError,
            "DeRegisterRTCPObserver() failed to locate channel");
        return -1;
    }
    return channelPtr->DeRegisterRTCPObserver();
}

int VoERTP_RTCPImpl::SetLocalSSRC(int channel, unsigned int ssrc)
{
    WEBRTC_TRACE(kTraceApiCall, kTraceVoice, VoEId(_instanceId,-1),
                 "SetLocalSSRC(channel=%d, %lu)", channel, ssrc);
    if (!_engineStatistics.Initialized())
    {
        _engineStatistics.SetLastError(VE_NOT_INITED, kTraceError);
        return -1;
    }
    voe::ScopedChannel sc(_channelManager, channel);
    voe::Channel* channelPtr = sc.ChannelPtr();
    if (channelPtr == NULL)
    {
        _engineStatistics.SetLastError(
            VE_CHANNEL_NOT_VALID, kTraceError,
            "SetLocalSSRC() failed to locate channel");
        return -1;
    }
    return channelPtr->SetLocalSSRC(ssrc);
}

int VoERTP_RTCPImpl::GetLocalSSRC(int channel, unsigned int& ssrc)
{
    WEBRTC_TRACE(kTraceApiCall, kTraceVoice, VoEId(_instanceId,-1),
                 "GetLocalSSRC(channel=%d, ssrc=?)", channel);
    if (!_engineStatistics.Initialized())
    {
        _engineStatistics.SetLastError(VE_NOT_INITED, kTraceError);
        return -1;
    }
    voe::ScopedChannel sc(_channelManager, channel);
    voe::Channel* channelPtr = sc.ChannelPtr();
    if (channelPtr == NULL)
    {
        _engineStatistics.SetLastError(
            VE_CHANNEL_NOT_VALID, kTraceError,
            "GetLocalSSRC() failed to locate channel");
        return -1;
    }
    return channelPtr->GetLocalSSRC(ssrc);
}

int VoERTP_RTCPImpl::GetRemoteSSRC(int channel, unsigned int& ssrc)
{
    WEBRTC_TRACE(kTraceApiCall, kTraceVoice, VoEId(_instanceId,-1),
                 "GetRemoteSSRC(channel=%d, ssrc=?)", channel);
    if (!_engineStatistics.Initialized())
    {
        _engineStatistics.SetLastError(VE_NOT_INITED, kTraceError);
        return -1;
    }
    voe::ScopedChannel sc(_channelManager, channel);
    voe::Channel* channelPtr = sc.ChannelPtr();
    if (channelPtr == NULL)
    {
        _engineStatistics.SetLastError(
            VE_CHANNEL_NOT_VALID, kTraceError,
            "GetRemoteSSRC() failed to locate channel");
        return -1;
    }
    return channelPtr->GetRemoteSSRC(ssrc);
}

int VoERTP_RTCPImpl::GetRemoteCSRCs(int channel, unsigned int arrCSRC[15])
{
    WEBRTC_TRACE(kTraceApiCall, kTraceVoice, VoEId(_instanceId,-1),
                 "GetRemoteCSRCs(channel=%d, arrCSRC=?)", channel);
    if (!_engineStatistics.Initialized())
    {
        _engineStatistics.SetLastError(VE_NOT_INITED, kTraceError);
        return -1;
    }
    voe::ScopedChannel sc(_channelManager, channel);
    voe::Channel* channelPtr = sc.ChannelPtr();
    if (channelPtr == NULL)
    {
        _engineStatistics.SetLastError(
            VE_CHANNEL_NOT_VALID, kTraceError,
            "GetRemoteCSRCs() failed to locate channel");
        return -1;
    }
    return channelPtr->GetRemoteCSRCs(arrCSRC);
}


int VoERTP_RTCPImpl::SetRTPAudioLevelIndicationStatus(int channel,
                                                      bool enable,
                                                      unsigned char ID)
{
    WEBRTC_TRACE(kTraceApiCall, kTraceVoice, VoEId(_instanceId,-1),
                 "SetRTPAudioLevelIndicationStatus(channel=%d, enable=%d,"
                 " ID=%u)", channel, enable, ID);
    if (!_engineStatistics.Initialized())
    {
        _engineStatistics.SetLastError(VE_NOT_INITED, kTraceError);
        return -1;
    }
    if (ID < kVoiceEngineMinRtpExtensionId ||
        ID > kVoiceEngineMaxRtpExtensionId)
    {
        // [RFC5285] The 4-bit ID is the local identifier of this element in
        // the range 1-14 inclusive.
        _engineStatistics.SetLastError(
            VE_INVALID_ARGUMENT, kTraceError,
            "SetRTPAudioLevelIndicationStatus() invalid ID parameter");
        return -1;
    }

    // Set AudioProcessingModule level-metric mode based on user input.
    // Note that this setting may conflict with the
    // AudioProcessing::SetMetricsStatus API.
    if (_audioProcessingModulePtr->level_estimator()->Enable(enable) != 0)
    {
        _engineStatistics.SetLastError(
            VE_APM_ERROR, kTraceError,
            "SetRTPAudioLevelIndicationStatus() failed to set level-metric"
            "mode");
        return -1;
    }

    // Ensure that the transmit mixer reads the audio-level metric for each
    // 10ms packet and copies the same value to all active channels.
    // The metric is derived within the AudioProcessingModule.
    _transmitMixerPtr->SetRTPAudioLevelIndicationStatus(enable);

    // Set state and ID for the specified channel.
    voe::ScopedChannel sc(_channelManager, channel);
    voe::Channel* channelPtr = sc.ChannelPtr();
    if (channelPtr == NULL)
    {
        _engineStatistics.SetLastError(
            VE_CHANNEL_NOT_VALID, kTraceError,
            "SetRTPAudioLevelIndicationStatus() failed to locate channel");
        return -1;
    }
    return channelPtr->SetRTPAudioLevelIndicationStatus(enable, ID);
}

int VoERTP_RTCPImpl::GetRTPAudioLevelIndicationStatus(int channel,
                                                      bool& enabled,
                                                      unsigned char& ID)
{
    WEBRTC_TRACE(kTraceApiCall, kTraceVoice, VoEId(_instanceId,-1),
                 "GetRTPAudioLevelIndicationStatus(channel=%d, enable=?, ID=?)",
                 channel);
    if (!_engineStatistics.Initialized())
    {
        _engineStatistics.SetLastError(VE_NOT_INITED, kTraceError);
        return -1;
    }
    voe::ScopedChannel sc(_channelManager, channel);
    voe::Channel* channelPtr = sc.ChannelPtr();
    if (channelPtr == NULL)
    {
        _engineStatistics.SetLastError(
            VE_CHANNEL_NOT_VALID, kTraceError,
            "GetRTPAudioLevelIndicationStatus() failed to locate channel");
        return -1;
    }
    return channelPtr->GetRTPAudioLevelIndicationStatus(enabled, ID);
}

int VoERTP_RTCPImpl::SetRTCPStatus(int channel, bool enable)
{
    WEBRTC_TRACE(kTraceApiCall, kTraceVoice, VoEId(_instanceId,-1),
                 "SetRTCPStatus(channel=%d, enable=%d)", channel, enable);
    if (!_engineStatistics.Initialized())
    {
        _engineStatistics.SetLastError(VE_NOT_INITED, kTraceError);
        return -1;
    }
    voe::ScopedChannel sc(_channelManager, channel);
    voe::Channel* channelPtr = sc.ChannelPtr();
    if (channelPtr == NULL)
    {
        _engineStatistics.SetLastError(
            VE_CHANNEL_NOT_VALID, kTraceError,
            "SetRTCPStatus() failed to locate channel");
        return -1;
    }
    return channelPtr->SetRTCPStatus(enable);
}

int VoERTP_RTCPImpl::GetRTCPStatus(int channel, bool& enabled)
{
    WEBRTC_TRACE(kTraceApiCall, kTraceVoice, VoEId(_instanceId,-1),
                 "GetRTCPStatus(channel=%d)", channel);
    if (!_engineStatistics.Initialized())
    {
        _engineStatistics.SetLastError(VE_NOT_INITED, kTraceError);
        return -1;
    }
    voe::ScopedChannel sc(_channelManager, channel);
    voe::Channel* channelPtr = sc.ChannelPtr();
    if (channelPtr == NULL)
    {
        _engineStatistics.SetLastError(
            VE_CHANNEL_NOT_VALID, kTraceError,
            "GetRTCPStatus() failed to locate channel");
        return -1;
    }
    return channelPtr->GetRTCPStatus(enabled);
}

int VoERTP_RTCPImpl::SetRTCP_CNAME(int channel, const char cName[256])
{
    WEBRTC_TRACE(kTraceApiCall, kTraceVoice, VoEId(_instanceId,-1),
                 "SetRTCP_CNAME(channel=%d, cName=%s)", channel, cName);
    if (!_engineStatistics.Initialized())
    {
        _engineStatistics.SetLastError(VE_NOT_INITED, kTraceError);
        return -1;
    }
    voe::ScopedChannel sc(_channelManager, channel);
    voe::Channel* channelPtr = sc.ChannelPtr();
    if (channelPtr == NULL)
    {
        _engineStatistics.SetLastError(
            VE_CHANNEL_NOT_VALID, kTraceError,
            "SetRTCP_CNAME() failed to locate channel");
        return -1;
    }
    return channelPtr->SetRTCP_CNAME(cName);
}

int VoERTP_RTCPImpl::GetRTCP_CNAME(int channel, char cName[256])
{
    WEBRTC_TRACE(kTraceApiCall, kTraceVoice, VoEId(_instanceId,-1),
                 "GetRTCP_CNAME(channel=%d, cName=?)", channel);
    if (!_engineStatistics.Initialized())
    {
        _engineStatistics.SetLastError(VE_NOT_INITED, kTraceError);
        return -1;
    }
    voe::ScopedChannel sc(_channelManager, channel);
    voe::Channel* channelPtr = sc.ChannelPtr();
    if (channelPtr == NULL)
    {
        _engineStatistics.SetLastError(
            VE_CHANNEL_NOT_VALID, kTraceError,
            "GetRTCP_CNAME() failed to locate channel");
        return -1;
    }
    return channelPtr->GetRTCP_CNAME(cName);
}

int VoERTP_RTCPImpl::GetRemoteRTCP_CNAME(int channel, char cName[256])
{
    WEBRTC_TRACE(kTraceApiCall, kTraceVoice, VoEId(_instanceId,-1),
                 "GetRemoteRTCP_CNAME(channel=%d, cName=?)", channel);
    if (!_engineStatistics.Initialized())
    {
        _engineStatistics.SetLastError(VE_NOT_INITED, kTraceError);
        return -1;
    }
    voe::ScopedChannel sc(_channelManager, channel);
    voe::Channel* channelPtr = sc.ChannelPtr();
    if (channelPtr == NULL)
    {
        _engineStatistics.SetLastError(
            VE_CHANNEL_NOT_VALID, kTraceError,
            "GetRemoteRTCP_CNAME() failed to locate channel");
        return -1;
    }
    return channelPtr->GetRemoteRTCP_CNAME(cName);
}

int VoERTP_RTCPImpl::GetRemoteRTCPData(
    int channel,
    unsigned int& NTPHigh, // from sender info in SR
    unsigned int& NTPLow, // from sender info in SR
    unsigned int& timestamp, // from sender info in SR
    unsigned int& playoutTimestamp, // derived locally
    unsigned int* jitter, // from report block 1 in SR/RR
    unsigned short* fractionLost) // from report block 1 in SR/RR
{
    WEBRTC_TRACE(kTraceApiCall, kTraceVoice, VoEId(_instanceId,-1),
                 "GetRemoteRTCPData(channel=%d,...)", channel);
    if (!_engineStatistics.Initialized())
    {
        _engineStatistics.SetLastError(VE_NOT_INITED, kTraceError);
        return -1;
    }
    voe::ScopedChannel sc(_channelManager, channel);
    voe::Channel* channelPtr = sc.ChannelPtr();
    if (channelPtr == NULL)
    {
        _engineStatistics.SetLastError(
            VE_CHANNEL_NOT_VALID, kTraceError,
            "GetRemoteRTCP_CNAME() failed to locate channel");
        return -1;
    }
    return channelPtr->GetRemoteRTCPData(NTPHigh,
                                         NTPLow,
                                         timestamp,
                                         playoutTimestamp,
                                         jitter,
                                         fractionLost);
}

int VoERTP_RTCPImpl::SendApplicationDefinedRTCPPacket(
    int channel,
    const unsigned char subType,
    unsigned int name,
    const char* data,
    unsigned short dataLengthInBytes)
{
    WEBRTC_TRACE(kTraceApiCall, kTraceVoice, VoEId(_instanceId,-1), 
                 "SendApplicationDefinedRTCPPacket(channel=%d, subType=%u,"
                 "name=%u, data=?, dataLengthInBytes=%u)",
                 channel, subType, name, dataLengthInBytes);
    if (!_engineStatistics.Initialized())
    {
        _engineStatistics.SetLastError(VE_NOT_INITED, kTraceError);
        return -1;
    }
    voe::ScopedChannel sc(_channelManager, channel);
    voe::Channel* channelPtr = sc.ChannelPtr();
    if (channelPtr == NULL)
    {
        _engineStatistics.SetLastError(
            VE_CHANNEL_NOT_VALID, kTraceError,
            "SendApplicationDefinedRTCPPacket() failed to locate channel");
        return -1;
    }
    return channelPtr->SendApplicationDefinedRTCPPacket(subType,
                                                        name,
                                                        data,
                                                        dataLengthInBytes);
}

int VoERTP_RTCPImpl::GetRTPStatistics(int channel,
                                      unsigned int& averageJitterMs,
                                      unsigned int& maxJitterMs,
                                      unsigned int& discardedPackets)
{
    WEBRTC_TRACE(kTraceApiCall, kTraceVoice, VoEId(_instanceId,-1),
                 "GetRTPStatistics(channel=%d,....)", channel);
    if (!_engineStatistics.Initialized())
    {
        _engineStatistics.SetLastError(VE_NOT_INITED, kTraceError);
        return -1;
    }
    voe::ScopedChannel sc(_channelManager, channel);
    voe::Channel* channelPtr = sc.ChannelPtr();
    if (channelPtr == NULL)
    {
        _engineStatistics.SetLastError(
            VE_CHANNEL_NOT_VALID, kTraceError,
            "GetRTPStatistics() failed to locate channel");
        return -1;
    }
    return channelPtr->GetRTPStatistics(averageJitterMs,
                                        maxJitterMs,
                                        discardedPackets);
}

int VoERTP_RTCPImpl::GetRTCPStatistics(int channel, CallStatistics& stats)
{
    WEBRTC_TRACE(kTraceApiCall, kTraceVoice, VoEId(_instanceId,-1),
                 "GetRTCPStatistics(channel=%d)", channel);
    if (!_engineStatistics.Initialized())
    {
        _engineStatistics.SetLastError(VE_NOT_INITED, kTraceError);
        return -1;
    }
    voe::ScopedChannel sc(_channelManager, channel);
    voe::Channel* channelPtr = sc.ChannelPtr();
    if (channelPtr == NULL)
    {
        _engineStatistics.SetLastError(
            VE_CHANNEL_NOT_VALID, kTraceError,
            "GetRTPStatistics() failed to locate channel");
        return -1;
    }
    return channelPtr->GetRTPStatistics(stats);
}

int VoERTP_RTCPImpl::SetFECStatus(int channel, bool enable, int redPayloadtype)
{
    WEBRTC_TRACE(kTraceApiCall, kTraceVoice, VoEId(_instanceId,-1),
                 "SetFECStatus(channel=%d, enable=%d, redPayloadtype=%d)",
                 channel, enable, redPayloadtype);
#ifdef WEBRTC_CODEC_RED
    if (!_engineStatistics.Initialized())
    {
        _engineStatistics.SetLastError(VE_NOT_INITED, kTraceError);
        return -1;
    }
    voe::ScopedChannel sc(_channelManager, channel);
    voe::Channel* channelPtr = sc.ChannelPtr();
    if (channelPtr == NULL)
    {
        _engineStatistics.SetLastError(
            VE_CHANNEL_NOT_VALID, kTraceError,
            "SetFECStatus() failed to locate channel");
        return -1;
    }
    return channelPtr->SetFECStatus(enable, redPayloadtype);
#else
    _engineStatistics.SetLastError(VE_FUNC_NOT_SUPPORTED, kTraceError,
                                   "SetFECStatus() RED is not supported");
    return -1;
#endif
}

int VoERTP_RTCPImpl::GetFECStatus(int channel,
                                  bool& enabled,
                                  int& redPayloadtype)
{
    WEBRTC_TRACE(kTraceApiCall, kTraceVoice, VoEId(_instanceId,-1),
                 "GetFECStatus(channel=%d, enabled=?, redPayloadtype=?)",
                 channel);
#ifdef WEBRTC_CODEC_RED
    if (!_engineStatistics.Initialized())
    {
        _engineStatistics.SetLastError(VE_NOT_INITED, kTraceError);
        return -1;
    }
    voe::ScopedChannel sc(_channelManager, channel);
    voe::Channel* channelPtr = sc.ChannelPtr();
    if (channelPtr == NULL)
    {
        _engineStatistics.SetLastError(
            VE_CHANNEL_NOT_VALID, kTraceError,
            "GetFECStatus() failed to locate channel");
        return -1;
    }
    return channelPtr->GetFECStatus(enabled, redPayloadtype);
#else
    _engineStatistics.SetLastError(VE_FUNC_NOT_SUPPORTED, kTraceError,
                                   "GetFECStatus() RED is not supported");
    return -1;
#endif
}

int VoERTP_RTCPImpl::SetRTPKeepaliveStatus(int channel,
                                           bool enable,
                                           unsigned char unknownPayloadType,
                                           int deltaTransmitTimeSeconds)
{
    WEBRTC_TRACE(kTraceApiCall, kTraceVoice, VoEId(_instanceId,-1), 
                 "SetRTPKeepaliveStatus(channel=%d, enable=%d,"
                 " unknownPayloadType=%u, deltaTransmitTimeSeconds=%d)",
                 channel, enable, unknownPayloadType, deltaTransmitTimeSeconds);
    if (!_engineStatistics.Initialized())
    {
        _engineStatistics.SetLastError(VE_NOT_INITED, kTraceError);
        return -1;
    }
    voe::ScopedChannel sc(_channelManager, channel);
    voe::Channel* channelPtr = sc.ChannelPtr();
    if (channelPtr == NULL)
    {
        _engineStatistics.SetLastError(
            VE_CHANNEL_NOT_VALID, kTraceError,
            "SetRTPKeepaliveStatus() failed to locate channel");
        return -1;
    }
    return channelPtr->SetRTPKeepaliveStatus(enable,
                                             unknownPayloadType,
                                             deltaTransmitTimeSeconds);
}

int VoERTP_RTCPImpl::GetRTPKeepaliveStatus(int channel,
                                           bool& enabled,
                                           unsigned char& unknownPayloadType,
                                           int& deltaTransmitTimeSeconds)
{
    WEBRTC_TRACE(kTraceApiCall, kTraceVoice, VoEId(_instanceId,-1),
                 "GetRTPKeepaliveStatus(channel=%d)", channel);
    if (!_engineStatistics.Initialized())
    {
        _engineStatistics.SetLastError(VE_NOT_INITED, kTraceError);
        return -1;
    }
    voe::ScopedChannel sc(_channelManager, channel);
    voe::Channel* channelPtr = sc.ChannelPtr();
    if (channelPtr == NULL)
    {
        _engineStatistics.SetLastError(
            VE_CHANNEL_NOT_VALID, kTraceError,
            "GetRTPKeepaliveStatus() failed to locate channel");
        return -1;
    }
    return channelPtr->GetRTPKeepaliveStatus(enabled,
                                             unknownPayloadType,
                                             deltaTransmitTimeSeconds);
}

int VoERTP_RTCPImpl::StartRTPDump(int channel,
                                  const char fileNameUTF8[1024],
                                  RTPDirections direction)
{
    WEBRTC_TRACE(kTraceApiCall, kTraceVoice, VoEId(_instanceId,-1),
                 "StartRTPDump(channel=%d, fileNameUTF8=%s, direction=%d)",
                 channel, fileNameUTF8, direction);
    assert(1024 == FileWrapper::kMaxFileNameSize);
    if (!_engineStatistics.Initialized())
    {
        _engineStatistics.SetLastError(VE_NOT_INITED, kTraceError);
        return -1;
    }
    voe::ScopedChannel sc(_channelManager, channel);
    voe::Channel* channelPtr = sc.ChannelPtr();
    if (channelPtr == NULL)
    {
        _engineStatistics.SetLastError(
            VE_CHANNEL_NOT_VALID, kTraceError,
            "StartRTPDump() failed to locate channel");
        return -1;
    }
    return channelPtr->StartRTPDump(fileNameUTF8, direction);
}

int VoERTP_RTCPImpl::StopRTPDump(int channel, RTPDirections direction)
{
    WEBRTC_TRACE(kTraceApiCall, kTraceVoice, VoEId(_instanceId,-1),
                 "StopRTPDump(channel=%d, direction=%d)", channel, direction);
    if (!_engineStatistics.Initialized())
    {
        _engineStatistics.SetLastError(VE_NOT_INITED, kTraceError);
        return -1;
    }
    voe::ScopedChannel sc(_channelManager, channel);
    voe::Channel* channelPtr = sc.ChannelPtr();
    if (channelPtr == NULL)
    {
        _engineStatistics.SetLastError(
            VE_CHANNEL_NOT_VALID, kTraceError,
            "StopRTPDump() failed to locate channel");
        return -1;
    }
    return channelPtr->StopRTPDump(direction);
}

int VoERTP_RTCPImpl::RTPDumpIsActive(int channel, RTPDirections direction)
{
    WEBRTC_TRACE(kTraceApiCall, kTraceVoice, VoEId(_instanceId,-1),
                 "RTPDumpIsActive(channel=%d, direction=%d)",
                 channel, direction);
    if (!_engineStatistics.Initialized())
    {
        _engineStatistics.SetLastError(VE_NOT_INITED, kTraceError);
        return -1;
    }
    voe::ScopedChannel sc(_channelManager, channel);
    voe::Channel* channelPtr = sc.ChannelPtr();
    if (channelPtr == NULL)
    {
        _engineStatistics.SetLastError(
            VE_CHANNEL_NOT_VALID, kTraceError,
            "StopRTPDump() failed to locate channel");
        return -1;
    }
    return channelPtr->RTPDumpIsActive(direction);
}

int VoERTP_RTCPImpl::InsertExtraRTPPacket(int channel,
                                          unsigned char payloadType,
                                          bool markerBit,
                                          const char* payloadData,
                                          unsigned short payloadSize)
{
    WEBRTC_TRACE(kTraceApiCall, kTraceVoice, VoEId(_instanceId,-1),
                 "InsertExtraRTPPacket(channel=%d, payloadType=%u,"
                 " markerBit=%u, payloadSize=%u)",
                 channel, payloadType, markerBit, payloadSize);
    if (!_engineStatistics.Initialized())
    {
        _engineStatistics.SetLastError(VE_NOT_INITED, kTraceError);
        return -1;
    }
    voe::ScopedChannel sc(_channelManager, channel);
    voe::Channel* channelPtr = sc.ChannelPtr();
    if (channelPtr == NULL)
    {
        _engineStatistics.SetLastError(
            VE_CHANNEL_NOT_VALID, kTraceError,
            "StopRTPDump() failed to locate channel");
        return -1;
    }
    return channelPtr->InsertExtraRTPPacket(payloadType,
                                            markerBit,
                                            payloadData,
                                            payloadSize);
}

#endif  // #ifdef WEBRTC_VOICE_ENGINE_RTP_RTCP_API

}  // namespace webrtc
