/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "voe_external_media_impl.h"

#include "channel.h"
#include "critical_section_wrapper.h"
#include "output_mixer.h"
#include "trace.h"
#include "transmit_mixer.h"
#include "voice_engine_impl.h"
#include "voe_errors.h"

namespace webrtc {

VoEExternalMedia* VoEExternalMedia::GetInterface(VoiceEngine* voiceEngine)
{
#ifndef WEBRTC_VOICE_ENGINE_EXTERNAL_MEDIA_API
    return NULL;
#else
    if (NULL == voiceEngine)
    {
        return NULL;
    }
    VoiceEngineImpl* s = reinterpret_cast<VoiceEngineImpl*> (voiceEngine);
    VoEExternalMediaImpl* d = s;
    (*d)++;
    return (d);
#endif
}

#ifdef WEBRTC_VOICE_ENGINE_EXTERNAL_MEDIA_API

VoEExternalMediaImpl::VoEExternalMediaImpl()
    : playout_delay_ms_(0)
{
    WEBRTC_TRACE(kTraceMemory, kTraceVoice, VoEId(_instanceId,-1),
                 "VoEExternalMediaImpl() - ctor");
}

VoEExternalMediaImpl::~VoEExternalMediaImpl()
{
    WEBRTC_TRACE(kTraceMemory, kTraceVoice, VoEId(_instanceId,-1),
                 "~VoEExternalMediaImpl() - dtor");
}

int VoEExternalMediaImpl::Release()
{
    WEBRTC_TRACE(kTraceApiCall, kTraceVoice, VoEId(_instanceId,-1),
                 "VoEExternalMedia::Release()");
    (*this)--;
    int refCount = GetCount();
    if (refCount < 0)
    {
        Reset();
        _engineStatistics.SetLastError(VE_INTERFACE_NOT_FOUND,
                                       kTraceWarning);
        return (-1);
    }
    WEBRTC_TRACE(kTraceStateInfo, kTraceVoice, VoEId(_instanceId,-1),
                 "VoEExternalMedia reference counter = %d", refCount);
    return (refCount);
}

int VoEExternalMediaImpl::RegisterExternalMediaProcessing(
    int channel,
    ProcessingTypes type,
    VoEMediaProcess& processObject)
{
    WEBRTC_TRACE(kTraceApiCall, kTraceVoice, VoEId(_instanceId,-1),
                 "RegisterExternalMediaProcessing(channel=%d, type=%d, "
                 "processObject=0x%x)", channel, type, &processObject);
    ANDROID_NOT_SUPPORTED();
    IPHONE_NOT_SUPPORTED();
    if (!_engineStatistics.Initialized())
    {
        _engineStatistics.SetLastError(VE_NOT_INITED, kTraceError);
        return -1;
    }
    switch (type)
    {
        case kPlaybackPerChannel:
        case kRecordingPerChannel:
        {
            voe::ScopedChannel sc(_channelManager, channel);
            voe::Channel* channelPtr = sc.ChannelPtr();
            if (channelPtr == NULL)
            {
                _engineStatistics.SetLastError(
                    VE_CHANNEL_NOT_VALID, kTraceError,
                    "RegisterExternalMediaProcessing() "
                    "failed to locate channel");
                return -1;
            }
            return channelPtr->RegisterExternalMediaProcessing(type,
                                                               processObject);
        }
        case kPlaybackAllChannelsMixed:
        {
            return _outputMixerPtr->RegisterExternalMediaProcessing(
                processObject);
        }
        case kRecordingAllChannelsMixed:
        {
            return _transmitMixerPtr->RegisterExternalMediaProcessing(
                processObject);
        }
        default:
        {
            _engineStatistics.SetLastError(
                VE_INVALID_ARGUMENT, kTraceError,
                "RegisterExternalMediaProcessing() invalid process type");
            return -1;
        }
    }
    return 0;
}

int VoEExternalMediaImpl::DeRegisterExternalMediaProcessing(
    int channel,
    ProcessingTypes type)
{
    WEBRTC_TRACE(kTraceApiCall, kTraceVoice, VoEId(_instanceId,-1),
                 "DeRegisterExternalMediaProcessing(channel=%d)", channel);
    ANDROID_NOT_SUPPORTED();
    IPHONE_NOT_SUPPORTED();
    if (!_engineStatistics.Initialized())
    {
        _engineStatistics.SetLastError(VE_NOT_INITED, kTraceError);
        return -1;
    }
    switch (type)
    {
        case kPlaybackPerChannel:
        case kRecordingPerChannel:
        {
            voe::ScopedChannel sc(_channelManager, channel);
            voe::Channel* channelPtr = sc.ChannelPtr();
            if (channelPtr == NULL)
            {
                _engineStatistics.SetLastError(
                    VE_CHANNEL_NOT_VALID, kTraceError,
                    "RegisterExternalMediaProcessing() "
                    "failed to locate channel");
                return -1;
            }
            return channelPtr->DeRegisterExternalMediaProcessing(type);
        }
        case kPlaybackAllChannelsMixed:
        {
            return _outputMixerPtr->DeRegisterExternalMediaProcessing();
        }
        case kRecordingAllChannelsMixed:
        {
            return _transmitMixerPtr->DeRegisterExternalMediaProcessing();
        }
        default:
        {
            _engineStatistics.SetLastError(
                VE_INVALID_ARGUMENT, kTraceError,
                "RegisterExternalMediaProcessing() invalid process type");
            return -1;
        }
    }
}

int VoEExternalMediaImpl::SetExternalRecordingStatus(bool enable)
{
    WEBRTC_TRACE(kTraceApiCall, kTraceVoice, VoEId(_instanceId,-1),
                 "SetExternalRecordingStatus(enable=%d)", enable);
    ANDROID_NOT_SUPPORTED();
    IPHONE_NOT_SUPPORTED();
#ifdef WEBRTC_VOE_EXTERNAL_REC_AND_PLAYOUT
    if (_audioDevicePtr->Recording())
    {
        _engineStatistics.SetLastError(
            VE_ALREADY_SENDING,
            kTraceError,
            "SetExternalRecordingStatus() cannot set state while sending");
        return -1;
    }
    _externalRecording = enable;
    return 0;
#else
    _engineStatistics.SetLastError(
        VE_FUNC_NOT_SUPPORTED,
        kTraceError,
        "SetExternalRecordingStatus() external recording is not supported");
    return -1;
#endif
}

int VoEExternalMediaImpl::ExternalRecordingInsertData(
        const WebRtc_Word16 speechData10ms[],
        int lengthSamples,
        int samplingFreqHz,
        int current_delay_ms)
{
    WEBRTC_TRACE(kTraceStream, kTraceVoice, VoEId(_instanceId,-1),
                 "ExternalRecordingInsertData(speechData10ms=0x%x,"
                 " lengthSamples=%u, samplingFreqHz=%d, current_delay_ms=%d)",
                 &speechData10ms[0], lengthSamples, samplingFreqHz,
              current_delay_ms);
    ANDROID_NOT_SUPPORTED();
    IPHONE_NOT_SUPPORTED();

#ifdef WEBRTC_VOE_EXTERNAL_REC_AND_PLAYOUT
    if (!_engineStatistics.Initialized())
    {
        _engineStatistics.SetLastError(VE_NOT_INITED, kTraceError);
        return -1;
    }
    if (!_externalRecording)
    {
       _engineStatistics.SetLastError(
           VE_INVALID_OPERATION,
           kTraceError,
           "ExternalRecordingInsertData() external recording is not enabled");
        return -1;
    }
    if (NumOfSendingChannels() == 0)
    {
        _engineStatistics.SetLastError(
            VE_ALREADY_SENDING,
            kTraceError,
            "SetExternalRecordingStatus() no channel is sending");
        return -1;
    }
    if ((16000 != samplingFreqHz) && (32000 != samplingFreqHz) &&
        (48000 != samplingFreqHz) && (44000 != samplingFreqHz))
    {
         _engineStatistics.SetLastError(
             VE_INVALID_ARGUMENT,
             kTraceError,
             "SetExternalRecordingStatus() invalid sample rate");
        return -1;
    }
    if ((0 == lengthSamples) ||
        ((lengthSamples % (samplingFreqHz / 100)) != 0))
    {
         _engineStatistics.SetLastError(
             VE_INVALID_ARGUMENT,
             kTraceError,
             "SetExternalRecordingStatus() invalid buffer size");
        return -1;
    }
    if (current_delay_ms < 0)
    {
        _engineStatistics.SetLastError(
            VE_INVALID_ARGUMENT,
            kTraceError,
            "SetExternalRecordingStatus() invalid delay)");
        return -1;
    }

    WebRtc_UWord16 blockSize = samplingFreqHz / 100;
    WebRtc_UWord32 nBlocks = lengthSamples / blockSize;
    WebRtc_Word16 totalDelayMS = 0;
    WebRtc_UWord16 playoutDelayMS = 0;

    for (WebRtc_UWord32 i = 0; i < nBlocks; i++)
    {
        if (!_externalPlayout)
        {
            // Use real playout delay if external playout is not enabled.
            _audioDevicePtr->PlayoutDelay(&playoutDelayMS);
            totalDelayMS = current_delay_ms + playoutDelayMS;
        }
        else
        {
            // Use stored delay value given the last call
            // to ExternalPlayoutGetData.
            totalDelayMS = current_delay_ms + playout_delay_ms_;
            // Compensate for block sizes larger than 10ms
            totalDelayMS -= (WebRtc_Word16)(i*10);
            if (totalDelayMS < 0)
                totalDelayMS = 0;
        }
        _transmitMixerPtr->PrepareDemux(
            (const WebRtc_Word8*)(&speechData10ms[i*blockSize]),
            blockSize,
            1,
            samplingFreqHz,
            totalDelayMS,
            0,
            0);

        _transmitMixerPtr->DemuxAndMix();
        _transmitMixerPtr->EncodeAndSend();
    }
    return 0;
#else
       _engineStatistics.SetLastError(
        VE_FUNC_NOT_SUPPORTED,
        kTraceError,
        "ExternalRecordingInsertData() external recording is not supported");
    return -1;
#endif
}

int VoEExternalMediaImpl::SetExternalPlayoutStatus(bool enable)
{
    WEBRTC_TRACE(kTraceApiCall, kTraceVoice, VoEId(_instanceId,-1),
                 "SetExternalPlayoutStatus(enable=%d)", enable);
    ANDROID_NOT_SUPPORTED();
    IPHONE_NOT_SUPPORTED();
#ifdef WEBRTC_VOE_EXTERNAL_REC_AND_PLAYOUT
    if (_audioDevicePtr->Playing())
    {
        _engineStatistics.SetLastError(
            VE_ALREADY_SENDING,
            kTraceError,
            "SetExternalPlayoutStatus() cannot set state while playing");
        return -1;
    }
    _externalPlayout = enable;
    return 0;
#else
    _engineStatistics.SetLastError(
        VE_FUNC_NOT_SUPPORTED,
        kTraceError,
        "SetExternalPlayoutStatus() external playout is not supported");
    return -1;
#endif
}

int VoEExternalMediaImpl::ExternalPlayoutGetData(
    WebRtc_Word16 speechData10ms[],
    int samplingFreqHz,
    int current_delay_ms,
    int& lengthSamples)
{
    WEBRTC_TRACE(kTraceStream, kTraceVoice, VoEId(_instanceId,-1),
                 "ExternalPlayoutGetData(speechData10ms=0x%x, samplingFreqHz=%d"
                 ",  current_delay_ms=%d)", &speechData10ms[0], samplingFreqHz,
                 current_delay_ms);
    ANDROID_NOT_SUPPORTED();
    IPHONE_NOT_SUPPORTED();
#ifdef WEBRTC_VOE_EXTERNAL_REC_AND_PLAYOUT
    if (!_engineStatistics.Initialized())
    {
        _engineStatistics.SetLastError(VE_NOT_INITED, kTraceError);
        return -1;
    }
    if (!_externalPlayout)
    {
       _engineStatistics.SetLastError(
           VE_INVALID_OPERATION,
           kTraceError,
           "ExternalPlayoutGetData() external playout is not enabled");
        return -1;
    }
    if ((16000 != samplingFreqHz) && (32000 != samplingFreqHz) &&
        (48000 != samplingFreqHz) && (44000 != samplingFreqHz))
    {
        _engineStatistics.SetLastError(
            VE_INVALID_ARGUMENT,
            kTraceError,
            "ExternalPlayoutGetData() invalid sample rate");
        return -1;
    }
    if (current_delay_ms < 0)
    {
        _engineStatistics.SetLastError(
            VE_INVALID_ARGUMENT,
            kTraceError,
            "ExternalPlayoutGetData() invalid delay)");
        return -1;
    }

    AudioFrame audioFrame;

    // Retrieve mixed output at the specified rate
    _outputMixerPtr->MixActiveChannels();
    _outputMixerPtr->DoOperationsOnCombinedSignal();
    _outputMixerPtr->GetMixedAudio(samplingFreqHz, 1, audioFrame);

    // Deliver audio (PCM) samples to the external sink
    memcpy(speechData10ms,
           audioFrame._payloadData,
           sizeof(WebRtc_Word16)*(audioFrame._payloadDataLengthInSamples));
    lengthSamples = audioFrame._payloadDataLengthInSamples;

    // Store current playout delay (to be used by ExternalRecordingInsertData).
    playout_delay_ms_ = current_delay_ms;

    return 0;
#else
    _engineStatistics.SetLastError(
       VE_FUNC_NOT_SUPPORTED,
       kTraceError,
       "ExternalPlayoutGetData() external playout is not supported");
    return -1;
#endif
}

#endif  // WEBRTC_VOICE_ENGINE_EXTERNAL_MEDIA_API

}  // namespace webrtc
