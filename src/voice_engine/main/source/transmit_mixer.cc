/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "transmit_mixer.h"

#include "audio_frame_operations.h"
#include "channel.h"
#include "channel_manager.h"
#include "critical_section_wrapper.h"
#include "event_wrapper.h"
#include "statistics.h"
#include "trace.h"
#include "utility.h"
#include "voe_base_impl.h"
#include "voe_external_media.h"

#define WEBRTC_ABS(a)	   (((a) < 0) ? -(a) : (a))

namespace webrtc {

namespace voe {

void 
TransmitMixer::OnPeriodicProcess()
{
    WEBRTC_TRACE(kTraceStream, kTraceVoice, VoEId(_instanceId, -1),
                 "TransmitMixer::OnPeriodicProcess()");

#if defined(WEBRTC_VOICE_ENGINE_TYPING_DETECTION)
    if (_typingNoiseWarning > 0)
    {
        CriticalSectionScoped cs(_callbackCritSect);
        if (_voiceEngineObserverPtr)
        {
            WEBRTC_TRACE(kTraceInfo, kTraceVoice, VoEId(_instanceId, -1),
                         "TransmitMixer::OnPeriodicProcess() => "
                         "CallbackOnError(VE_TYPING_NOISE_WARNING)");
            _voiceEngineObserverPtr->CallbackOnError(-1,
                                                     VE_TYPING_NOISE_WARNING);
        }
        _typingNoiseWarning = 0;
    }
#endif

    if (_saturationWarning > 0)
    {
        CriticalSectionScoped cs(_callbackCritSect);
        if (_voiceEngineObserverPtr)
        {
            WEBRTC_TRACE(kTraceInfo, kTraceVoice, VoEId(_instanceId, -1),
                         "TransmitMixer::OnPeriodicProcess() =>"
                         " CallbackOnError(VE_SATURATION_WARNING)");
            _voiceEngineObserverPtr->CallbackOnError(-1, VE_SATURATION_WARNING);
        }
        _saturationWarning = 0;
    }

    if (_noiseWarning > 0)
    {
        CriticalSectionScoped cs(_callbackCritSect);
        if (_voiceEngineObserverPtr)
        {
            WEBRTC_TRACE(kTraceInfo, kTraceVoice, VoEId(_instanceId, -1),
                         "TransmitMixer::OnPeriodicProcess() =>"
                         "CallbackOnError(VE_NOISE_WARNING)");
            _voiceEngineObserverPtr->CallbackOnError(-1, VE_NOISE_WARNING);
        }
        _noiseWarning = 0;
    }
}


void TransmitMixer::PlayNotification(const WebRtc_Word32 id,
                                     const WebRtc_UWord32 durationMs)
{
    WEBRTC_TRACE(kTraceStream, kTraceVoice, VoEId(_instanceId, -1),
                 "TransmitMixer::PlayNotification(id=%d, durationMs=%d)",
                 id, durationMs);

    // Not implement yet
}
	
void TransmitMixer::RecordNotification(const WebRtc_Word32 id,
                                       const WebRtc_UWord32 durationMs)
{
    WEBRTC_TRACE(kTraceStream, kTraceVoice, VoEId(_instanceId,-1),
                 "TransmitMixer::RecordNotification(id=%d, durationMs=%d)",
                 id, durationMs);

    // Not implement yet
}

void TransmitMixer::PlayFileEnded(const WebRtc_Word32 id)
{
    WEBRTC_TRACE(kTraceStream, kTraceVoice, VoEId(_instanceId, -1),
                 "TransmitMixer::PlayFileEnded(id=%d)", id);

    assert(id == _filePlayerId);

    CriticalSectionScoped cs(_critSect);

    _filePlaying = false;
    WEBRTC_TRACE(kTraceStateInfo, kTraceVoice, VoEId(_instanceId, -1),
                 "TransmitMixer::PlayFileEnded() =>"
                 "file player module is shutdown");
}

void 
TransmitMixer::RecordFileEnded(const WebRtc_Word32 id)
{
    WEBRTC_TRACE(kTraceStream, kTraceVoice, VoEId(_instanceId, -1),
                 "TransmitMixer::RecordFileEnded(id=%d)", id);

    if (id == _fileRecorderId)
    {
        CriticalSectionScoped cs(_critSect);
        _fileRecording = false;
        WEBRTC_TRACE(kTraceStateInfo, kTraceVoice, VoEId(_instanceId, -1),
                     "TransmitMixer::RecordFileEnded() => fileRecorder module"
                     "is shutdown");
    } else if (id == _fileCallRecorderId)
    {
        CriticalSectionScoped cs(_critSect);
        _fileCallRecording = false;
        WEBRTC_TRACE(kTraceStateInfo, kTraceVoice, VoEId(_instanceId, -1),
                     "TransmitMixer::RecordFileEnded() => fileCallRecorder"
                     "module is shutdown");
    }
}

WebRtc_Word32
TransmitMixer::Create(TransmitMixer*& mixer, const WebRtc_UWord32 instanceId)
{
    WEBRTC_TRACE(kTraceMemory, kTraceVoice, VoEId(instanceId, -1),
                 "TransmitMixer::Create(instanceId=%d)", instanceId);
    mixer = new TransmitMixer(instanceId);
    if (mixer == NULL)
    {
        WEBRTC_TRACE(kTraceMemory, kTraceVoice, VoEId(instanceId, -1),
                     "TransmitMixer::Create() unable to allocate memory"
                     "for mixer");
        return -1;
    }
    return 0;
}

void
TransmitMixer::Destroy(TransmitMixer*& mixer)
{
    if (mixer)
    {
        delete mixer;
        mixer = NULL;
    }
}

TransmitMixer::TransmitMixer(const WebRtc_UWord32 instanceId) :
    _engineStatisticsPtr(NULL),
    _channelManagerPtr(NULL),
    _audioProcessingModulePtr(NULL),
    _voiceEngineObserverPtr(NULL),
    _processThreadPtr(NULL),
    _filePlayerPtr(NULL),
    _fileRecorderPtr(NULL),
    _fileCallRecorderPtr(NULL),
    // Avoid conflict with other channels by adding 1024 - 1026,
    // won't use as much as 1024 channels.
    _filePlayerId(instanceId + 1024),
    _fileRecorderId(instanceId + 1025),
    _fileCallRecorderId(instanceId + 1026),
    _filePlaying(false),
    _fileRecording(false),
    _fileCallRecording(false),
    _audioLevel(),
    _critSect(*CriticalSectionWrapper::CreateCriticalSection()),
    _callbackCritSect(*CriticalSectionWrapper::CreateCriticalSection()),
#ifdef WEBRTC_VOICE_ENGINE_TYPING_DETECTION
    _timeActive(0),
    _penaltyCounter(0),
    _typingNoiseWarning(0),
#endif
    _saturationWarning(0),
    _noiseWarning(0),
    _instanceId(instanceId),
    _mixFileWithMicrophone(false),
    _captureLevel(0),
    _externalMedia(false),
    _externalMediaCallbackPtr(NULL),
    _mute(false),
    _remainingMuteMicTimeMs(0),
    _mixingFrequency(0)
{
    WEBRTC_TRACE(kTraceMemory, kTraceVoice, VoEId(_instanceId, -1),
                 "TransmitMixer::TransmitMixer() - ctor");
}
	
TransmitMixer::~TransmitMixer()
{
    WEBRTC_TRACE(kTraceMemory, kTraceVoice, VoEId(_instanceId, -1),
                 "TransmitMixer::~TransmitMixer() - dtor");
    _monitorModule.DeRegisterObserver();
    if (_processThreadPtr)
    {
        _processThreadPtr->DeRegisterModule(&_monitorModule);
    }
    if (_externalMedia)
    {
        DeRegisterExternalMediaProcessing();
    }
    {
        CriticalSectionScoped cs(_critSect);
        if (_fileRecorderPtr)
        {
            _fileRecorderPtr->RegisterModuleFileCallback(NULL);
            _fileRecorderPtr->StopRecording();
            FileRecorder::DestroyFileRecorder(_fileRecorderPtr);
            _fileRecorderPtr = NULL;
        }
        if (_fileCallRecorderPtr)
        {
            _fileCallRecorderPtr->RegisterModuleFileCallback(NULL);
            _fileCallRecorderPtr->StopRecording();
            FileRecorder::DestroyFileRecorder(_fileCallRecorderPtr);
            _fileCallRecorderPtr = NULL;
        }
        if (_filePlayerPtr)
        {
            _filePlayerPtr->RegisterModuleFileCallback(NULL);
            _filePlayerPtr->StopPlayingFile();
            FilePlayer::DestroyFilePlayer(_filePlayerPtr);
            _filePlayerPtr = NULL;
        }
    }
    delete &_critSect;
    delete &_callbackCritSect;
}

WebRtc_Word32
TransmitMixer::SetEngineInformation(ProcessThread& processThread,
                                    Statistics& engineStatistics,
                                    ChannelManager& channelManager)
{
    WEBRTC_TRACE(kTraceInfo, kTraceVoice, VoEId(_instanceId, -1),
                 "TransmitMixer::SetEngineInformation()");

    _processThreadPtr = &processThread;
    _engineStatisticsPtr = &engineStatistics;
    _channelManagerPtr = &channelManager;

    if (_processThreadPtr->RegisterModule(&_monitorModule) == -1)
    {
        WEBRTC_TRACE(kTraceWarning, kTraceVoice, VoEId(_instanceId, -1),
                     "TransmitMixer::SetEngineInformation() failed to"
                     "register the monitor module");
    } else
    {
        _monitorModule.RegisterObserver(*this);
    }

    return 0;
}
	
WebRtc_Word32 
TransmitMixer::RegisterVoiceEngineObserver(VoiceEngineObserver& observer)
{
    WEBRTC_TRACE(kTraceInfo, kTraceVoice, VoEId(_instanceId, -1),
                 "TransmitMixer::RegisterVoiceEngineObserver()");
    CriticalSectionScoped cs(_callbackCritSect);

    if (_voiceEngineObserverPtr)
    {
        _engineStatisticsPtr->SetLastError(
            VE_INVALID_OPERATION, kTraceError,
            "RegisterVoiceEngineObserver() observer already enabled");
        return -1;
    }
    _voiceEngineObserverPtr = &observer;
    return 0;
}

WebRtc_Word32 
TransmitMixer::SetAudioProcessingModule(AudioProcessing* audioProcessingModule)
{
    WEBRTC_TRACE(kTraceInfo, kTraceVoice, VoEId(_instanceId, -1),
                 "TransmitMixer::SetAudioProcessingModule("
                 "audioProcessingModule=0x%x)",
                 audioProcessingModule);
    _audioProcessingModulePtr = audioProcessingModule;
    return 0;
}

WebRtc_Word32 
TransmitMixer::PrepareDemux(const WebRtc_Word8* audioSamples,
                            const WebRtc_UWord32 nSamples,
                            const WebRtc_UWord8 nChannels,
                            const WebRtc_UWord32 samplesPerSec,
                            const WebRtc_UWord16 totalDelayMS,
                            const WebRtc_Word32 clockDrift,
                            const WebRtc_UWord16 currentMicLevel)
{
    WEBRTC_TRACE(kTraceStream, kTraceVoice, VoEId(_instanceId, -1),
                 "TransmitMixer::PrepareDemux(nSamples=%u, nChannels=%u,"
                 "samplesPerSec=%u, totalDelayMS=%u, clockDrift=%u,"
                 "currentMicLevel=%u)", nSamples, nChannels, samplesPerSec,
                 totalDelayMS, clockDrift, currentMicLevel);


    const int mixingFrequency = _mixingFrequency;

    ScopedChannel sc(*_channelManagerPtr);
    void* iterator(NULL);
    Channel* channelPtr = sc.GetFirstChannel(iterator);
    _mixingFrequency = 8000;
    while (channelPtr != NULL)
    {
        if (channelPtr->Sending())
        {
            CodecInst tmpCdc;
            channelPtr->GetSendCodec(tmpCdc);
            if (tmpCdc.plfreq > _mixingFrequency)
                _mixingFrequency = tmpCdc.plfreq;
        }
        channelPtr = sc.GetNextChannel(iterator);
    }


    // --- Resample input audio and create/store the initial audio frame

    if (GenerateAudioFrame((const WebRtc_Word16*) audioSamples,
                           nSamples,
                           nChannels,
                           samplesPerSec,
                           _mixingFrequency) == -1)
    {
        return -1;
    }

    // --- Near-end Voice Quality Enhancement (APM) processing

    APMProcessStream(totalDelayMS, clockDrift, currentMicLevel);

    // --- Annoying typing detection (utilizes the APM/VAD decision)

#ifdef WEBRTC_VOICE_ENGINE_TYPING_DETECTION
    TypingDetection();
#endif

    // --- Mute during DTMF tone if direct feedback is enabled

    if (_remainingMuteMicTimeMs > 0)
    {
        AudioFrameOperations::Mute(_audioFrame);
        _remainingMuteMicTimeMs -= 10;
        if (_remainingMuteMicTimeMs < 0)
        {
            _remainingMuteMicTimeMs = 0;
        }
    }

    // --- Mute signal

    if (_mute)
    {
        AudioFrameOperations::Mute(_audioFrame);
    }

    // --- Measure audio level of speech after APM processing

    _audioLevel.ComputeLevel(_audioFrame);

    // --- Mix with file (does not affect the mixing frequency)

    if (_filePlaying)
    {
        MixOrReplaceAudioWithFile(_mixingFrequency);
    }

    // --- Record to file

    if (_fileRecording)
    {
        RecordAudioToFile(_mixingFrequency);
    }

    // --- External media processing

    if (_externalMedia)
    {
        CriticalSectionScoped cs(_callbackCritSect);
        const bool isStereo = (_audioFrame._audioChannel == 2);
        if (_externalMediaCallbackPtr)
        {
            _externalMediaCallbackPtr->Process(
                -1,
                kRecordingAllChannelsMixed,
                (WebRtc_Word16*) _audioFrame._payloadData,
                _audioFrame._payloadDataLengthInSamples,
                _audioFrame._frequencyInHz,
                isStereo);
        }
    }

    if (_mixingFrequency != mixingFrequency)
    {
        WEBRTC_TRACE(kTraceStream, kTraceVoice, VoEId(_instanceId, -1),
                     "TransmitMixer::TransmitMixer::PrepareDemux() => "
                     "mixing frequency = %d",
                     _mixingFrequency);
    }

    return 0;
}


	
WebRtc_Word32 
TransmitMixer::DemuxAndMix()
{
    WEBRTC_TRACE(kTraceStream, kTraceVoice, VoEId(_instanceId, -1),
                 "TransmitMixer::DemuxAndMix()");

    ScopedChannel sc(*_channelManagerPtr);
    void* iterator(NULL);
    Channel* channelPtr = sc.GetFirstChannel(iterator);
    while (channelPtr != NULL)
    {
        if (channelPtr->InputIsOnHold())
        {
            channelPtr->UpdateLocalTimeStamp();
        } else if (channelPtr->Sending())
        {
            // load temporary audioframe with current (mixed) microphone signal
            AudioFrame tmpAudioFrame = _audioFrame;

            channelPtr->Demultiplex(tmpAudioFrame);
            channelPtr->PrepareEncodeAndSend(_mixingFrequency);
        }
        channelPtr = sc.GetNextChannel(iterator);
    }
				
	return 0;
}
	
WebRtc_Word32 
TransmitMixer::EncodeAndSend()
{
    WEBRTC_TRACE(kTraceStream, kTraceVoice, VoEId(_instanceId, -1),
                 "TransmitMixer::EncodeAndSend()");

    ScopedChannel sc(*_channelManagerPtr);
    void* iterator(NULL);
    Channel* channelPtr = sc.GetFirstChannel(iterator);
    while (channelPtr != NULL)
    {
        if (channelPtr->Sending() && !channelPtr->InputIsOnHold())
        {
            channelPtr->EncodeAndSend();
        }
        channelPtr = sc.GetNextChannel(iterator);
    }
    return 0;
}

WebRtc_UWord32 TransmitMixer::CaptureLevel() const
{
    return _captureLevel;
}

void
TransmitMixer::UpdateMuteMicrophoneTime(const WebRtc_UWord32 lengthMs)
{
    WEBRTC_TRACE(kTraceInfo, kTraceVoice, VoEId(_instanceId, -1),
               "TransmitMixer::UpdateMuteMicrophoneTime(lengthMs=%d)",
               lengthMs);
    _remainingMuteMicTimeMs = lengthMs;
}

WebRtc_Word32 
TransmitMixer::StopSend()
{
    WEBRTC_TRACE(kTraceInfo, kTraceVoice, VoEId(_instanceId, -1),
               "TransmitMixer::StopSend()");
    _audioLevel.Clear();
    return 0;
}

int TransmitMixer::StartPlayingFileAsMicrophone(const char* fileName,
                                                const bool loop,
                                                const FileFormats format,
                                                const int startPosition,
                                                const float volumeScaling,
                                                const int stopPosition,
                                                const CodecInst* codecInst)
{
    WEBRTC_TRACE(kTraceInfo, kTraceVoice, VoEId(_instanceId, -1),
                 "TransmitMixer::StartPlayingFileAsMicrophone("
                 "fileNameUTF8[]=%s,loop=%d, format=%d, volumeScaling=%5.3f,"
                 " startPosition=%d, stopPosition=%d)", fileName, loop,
                 format, volumeScaling, startPosition, stopPosition);

    if (_filePlaying)
    {
        _engineStatisticsPtr->SetLastError(
            VE_ALREADY_PLAYING, kTraceWarning,
            "StartPlayingFileAsMicrophone() is already playing");
        return 0;
    }

    CriticalSectionScoped cs(_critSect);

    // Destroy the old instance
    if (_filePlayerPtr)
    {
        _filePlayerPtr->RegisterModuleFileCallback(NULL);
        FilePlayer::DestroyFilePlayer(_filePlayerPtr);
        _filePlayerPtr = NULL;
    }

    // Dynamically create the instance
    _filePlayerPtr
        = FilePlayer::CreateFilePlayer(_filePlayerId,
                                       (const FileFormats) format);

    if (_filePlayerPtr == NULL)
    {
        _engineStatisticsPtr->SetLastError(
            VE_INVALID_ARGUMENT, kTraceError,
            "StartPlayingFileAsMicrophone() filePlayer format isnot correct");
        return -1;
    }

    const WebRtc_UWord32 notificationTime(0);

    if (_filePlayerPtr->StartPlayingFile(
        fileName,
        loop,
        startPosition,
        volumeScaling,
        notificationTime,
        stopPosition,
        (const CodecInst*) codecInst) != 0)
    {
        _engineStatisticsPtr->SetLastError(
            VE_BAD_FILE, kTraceError,
            "StartPlayingFile() failed to start file playout");
        _filePlayerPtr->StopPlayingFile();
        FilePlayer::DestroyFilePlayer(_filePlayerPtr);
        _filePlayerPtr = NULL;
        return -1;
    }

    _filePlayerPtr->RegisterModuleFileCallback(this);
    _filePlaying = true;

    return 0;
}

int TransmitMixer::StartPlayingFileAsMicrophone(InStream* stream,
                                                const FileFormats format,
                                                const int startPosition,
                                                const float volumeScaling,
                                                const int stopPosition,
                                                const CodecInst* codecInst)
{
    WEBRTC_TRACE(kTraceInfo, kTraceVoice, VoEId(_instanceId,-1),
                 "TransmitMixer::StartPlayingFileAsMicrophone(format=%d,"
                 " volumeScaling=%5.3f, startPosition=%d, stopPosition=%d)",
                 format, volumeScaling, startPosition, stopPosition);
    
    if (stream == NULL)
    {
        _engineStatisticsPtr->SetLastError(
            VE_BAD_FILE, kTraceError,
            "StartPlayingFileAsMicrophone() NULL as input stream");
        return -1;
    }

    if (_filePlaying)
    {
        _engineStatisticsPtr->SetLastError(
            VE_ALREADY_PLAYING, kTraceWarning,
            "StartPlayingFileAsMicrophone() is already playing");
        return 0;
    }

    CriticalSectionScoped cs(_critSect);

    // Destroy the old instance
    if (_filePlayerPtr)
    {
        _filePlayerPtr->RegisterModuleFileCallback(NULL);
        FilePlayer::DestroyFilePlayer(_filePlayerPtr);
        _filePlayerPtr = NULL;
    }

    // Dynamically create the instance
    _filePlayerPtr
        = FilePlayer::CreateFilePlayer(_filePlayerId,
                                       (const FileFormats) format);

    if (_filePlayerPtr == NULL)
    {
        _engineStatisticsPtr->SetLastError(
            VE_INVALID_ARGUMENT, kTraceWarning,
            "StartPlayingFileAsMicrophone() filePlayer format isnot correct");
        return -1;
    }

    const WebRtc_UWord32 notificationTime(0);

    if (_filePlayerPtr->StartPlayingFile(
        (InStream&) *stream,
        startPosition,
        volumeScaling,
        notificationTime,
        stopPosition,
        (const CodecInst*) codecInst) != 0)
    {
        _engineStatisticsPtr->SetLastError(
            VE_BAD_FILE, kTraceError,
            "StartPlayingFile() failed to start file playout");
        _filePlayerPtr->StopPlayingFile();
        FilePlayer::DestroyFilePlayer(_filePlayerPtr);
        _filePlayerPtr = NULL;
        return -1;
    }
    _filePlayerPtr->RegisterModuleFileCallback(this);
    _filePlaying = true;

    return 0;
}

int TransmitMixer::StopPlayingFileAsMicrophone()
{
    WEBRTC_TRACE(kTraceInfo, kTraceVoice, VoEId(_instanceId,-1),
                 "TransmitMixer::StopPlayingFileAsMicrophone()");

    if (!_filePlaying)
    {
        _engineStatisticsPtr->SetLastError(
            VE_INVALID_OPERATION, kTraceWarning,
            "StopPlayingFileAsMicrophone() isnot playing");
        return 0;
    }

    CriticalSectionScoped cs(_critSect);

    if (_filePlayerPtr->StopPlayingFile() != 0)
    {
        _engineStatisticsPtr->SetLastError(
            VE_CANNOT_STOP_PLAYOUT, kTraceError,
            "StopPlayingFile() couldnot stop playing file");
        return -1;
    }

    _filePlayerPtr->RegisterModuleFileCallback(NULL);
    FilePlayer::DestroyFilePlayer(_filePlayerPtr);
    _filePlayerPtr = NULL;
    _filePlaying = false;

    return 0;
}

int TransmitMixer::IsPlayingFileAsMicrophone() const
{
    WEBRTC_TRACE(kTraceInfo, kTraceVoice, VoEId(_instanceId, -1),
                 "TransmitMixer::IsPlayingFileAsMicrophone()");
    return _filePlaying;
}

int TransmitMixer::ScaleFileAsMicrophonePlayout(const float scale)
{
    WEBRTC_TRACE(kTraceInfo, kTraceVoice, VoEId(_instanceId, -1),
                 "TransmitMixer::ScaleFileAsMicrophonePlayout(scale=%5.3f)",
                 scale);

    CriticalSectionScoped cs(_critSect);

    if (!_filePlaying)
    {
        _engineStatisticsPtr->SetLastError(
            VE_INVALID_OPERATION, kTraceError,
            "ScaleFileAsMicrophonePlayout() isnot playing file");
        return -1;
    }

    if ((_filePlayerPtr == NULL) ||
        (_filePlayerPtr->SetAudioScaling(scale) != 0))
    {
        _engineStatisticsPtr->SetLastError(
            VE_BAD_ARGUMENT, kTraceError,
            "SetAudioScaling() failed to scale playout");
        return -1;
    }

    return 0;
}

int TransmitMixer::StartRecordingMicrophone(const WebRtc_Word8* fileName,
                                            const CodecInst* codecInst)
{
    WEBRTC_TRACE(kTraceInfo, kTraceVoice, VoEId(_instanceId, -1),
                 "TransmitMixer::StartRecordingMicrophone(fileName=%s)",
                 fileName);

    if (_fileRecording)
    {
        WEBRTC_TRACE(kTraceWarning, kTraceVoice, VoEId(_instanceId, -1),
                     "StartRecordingMicrophone() is already recording");
        return 0;
    }

    FileFormats format;
    const WebRtc_UWord32 notificationTime(0); // Not supported in VoE
    CodecInst dummyCodec = { 100, "L16", 16000, 320, 1, 320000 };

    if (codecInst != NULL && codecInst->channels != 1)
    {
        _engineStatisticsPtr->SetLastError(
            VE_BAD_ARGUMENT, kTraceError,
            "StartRecordingMicrophone() invalid compression");
        return (-1);
    }
    if (codecInst == NULL)
    {
        format = kFileFormatPcm16kHzFile;
        codecInst = &dummyCodec;
    } else if ((STR_CASE_CMP(codecInst->plname,"L16") == 0) ||
        (STR_CASE_CMP(codecInst->plname,"PCMU") == 0) ||
        (STR_CASE_CMP(codecInst->plname,"PCMA") == 0))
    {
        format = kFileFormatWavFile;
    } else
    {
        format = kFileFormatCompressedFile;
    }

    CriticalSectionScoped cs(_critSect);

    // Destroy the old instance
    if (_fileRecorderPtr)
    {
        _fileRecorderPtr->RegisterModuleFileCallback(NULL);
        FileRecorder::DestroyFileRecorder(_fileRecorderPtr);
        _fileRecorderPtr = NULL;
    }

    _fileRecorderPtr =
        FileRecorder::CreateFileRecorder(_fileRecorderId,
                                         (const FileFormats) format);
    if (_fileRecorderPtr == NULL)
    {
        _engineStatisticsPtr->SetLastError(
            VE_INVALID_ARGUMENT, kTraceError,
            "StartRecordingMicrophone() fileRecorder format isnot correct");
        return -1;
    }

    if (_fileRecorderPtr->StartRecordingAudioFile(
        fileName,
        (const CodecInst&) *codecInst,
        notificationTime) != 0)
    {
        _engineStatisticsPtr->SetLastError(
            VE_BAD_FILE, kTraceError,
            "StartRecordingAudioFile() failed to start file recording");
        _fileRecorderPtr->StopRecording();
        FileRecorder::DestroyFileRecorder(_fileRecorderPtr);
        _fileRecorderPtr = NULL;
        return -1;
    }
    _fileRecorderPtr->RegisterModuleFileCallback(this);
    _fileRecording = true;

    return 0;
}

int TransmitMixer::StartRecordingMicrophone(OutStream* stream,
                                            const CodecInst* codecInst)
{
    WEBRTC_TRACE(kTraceInfo, kTraceVoice, VoEId(_instanceId, -1),
               "TransmitMixer::StartRecordingMicrophone()");

    if (_fileRecording)
    {
        WEBRTC_TRACE(kTraceWarning, kTraceVoice, VoEId(_instanceId, -1),
                   "StartRecordingMicrophone() is already recording");
        return 0;
    }

    FileFormats format;
    const WebRtc_UWord32 notificationTime(0); // Not supported in VoE
    CodecInst dummyCodec = { 100, "L16", 16000, 320, 1, 320000 };

    if (codecInst != NULL && codecInst->channels != 1)
    {
        _engineStatisticsPtr->SetLastError(
            VE_BAD_ARGUMENT, kTraceError,
            "StartRecordingMicrophone() invalid compression");
        return (-1);
    }
    if (codecInst == NULL)
    {
        format = kFileFormatPcm16kHzFile;
        codecInst = &dummyCodec;
    } else if ((STR_CASE_CMP(codecInst->plname,"L16") == 0) ||
        (STR_CASE_CMP(codecInst->plname,"PCMU") == 0) ||
        (STR_CASE_CMP(codecInst->plname,"PCMA") == 0))
    {
        format = kFileFormatWavFile;
    } else
    {
        format = kFileFormatCompressedFile;
    }

    CriticalSectionScoped cs(_critSect);

    // Destroy the old instance
    if (_fileRecorderPtr)
    {
        _fileRecorderPtr->RegisterModuleFileCallback(NULL);
        FileRecorder::DestroyFileRecorder(_fileRecorderPtr);
        _fileRecorderPtr = NULL;
    }

    _fileRecorderPtr =
        FileRecorder::CreateFileRecorder(_fileRecorderId,
                                         (const FileFormats) format);
    if (_fileRecorderPtr == NULL)
    {
        _engineStatisticsPtr->SetLastError(
            VE_INVALID_ARGUMENT, kTraceError,
            "StartRecordingMicrophone() fileRecorder format isnot correct");
        return -1;
    }

    if (_fileRecorderPtr->StartRecordingAudioFile(*stream,
                                                  *codecInst,
                                                  notificationTime) != 0)
    {
    _engineStatisticsPtr->SetLastError(VE_BAD_FILE, kTraceError,
      "StartRecordingAudioFile() failed to start file recording");
    _fileRecorderPtr->StopRecording();
    FileRecorder::DestroyFileRecorder(_fileRecorderPtr);
    _fileRecorderPtr = NULL;
    return -1;
    }

    _fileRecorderPtr->RegisterModuleFileCallback(this);
    _fileRecording = true;

    return 0;
}


int TransmitMixer::StopRecordingMicrophone()
{
    WEBRTC_TRACE(kTraceInfo, kTraceVoice, VoEId(_instanceId, -1),
                 "TransmitMixer::StopRecordingMicrophone()");

    if (!_fileRecording)
    {
        WEBRTC_TRACE(kTraceError, kTraceVoice, VoEId(_instanceId, -1),
                   "StopRecordingMicrophone() isnot recording");
        return -1;
    }

    CriticalSectionScoped cs(_critSect);

    if (_fileRecorderPtr->StopRecording() != 0)
    {
        _engineStatisticsPtr->SetLastError(
            VE_STOP_RECORDING_FAILED, kTraceError,
            "StopRecording(), could not stop recording");
        return -1;
    }
    _fileRecorderPtr->RegisterModuleFileCallback(NULL);
    FileRecorder::DestroyFileRecorder(_fileRecorderPtr);
    _fileRecorderPtr = NULL;
    _fileRecording = false;

    return 0;
}

int TransmitMixer::StartRecordingCall(const WebRtc_Word8* fileName,
                                      const CodecInst* codecInst)
{
    WEBRTC_TRACE(kTraceInfo, kTraceVoice, VoEId(_instanceId, -1),
                 "TransmitMixer::StartRecordingCall(fileName=%s)", fileName);

    if (_fileCallRecording)
    {
        WEBRTC_TRACE(kTraceWarning, kTraceVoice, VoEId(_instanceId, -1),
                     "StartRecordingCall() is already recording");
        return 0;
    }

    FileFormats format;
    const WebRtc_UWord32 notificationTime(0); // Not supported in VoE
    CodecInst dummyCodec = { 100, "L16", 16000, 320, 1, 320000 };

    if (codecInst != NULL && codecInst->channels != 1)
    {
        _engineStatisticsPtr->SetLastError(
            VE_BAD_ARGUMENT, kTraceError,
            "StartRecordingCall() invalid compression");
        return (-1);
    }
    if (codecInst == NULL)
    {
        format = kFileFormatPcm16kHzFile;
        codecInst = &dummyCodec;
    } else if ((STR_CASE_CMP(codecInst->plname,"L16") == 0) ||
        (STR_CASE_CMP(codecInst->plname,"PCMU") == 0) ||
        (STR_CASE_CMP(codecInst->plname,"PCMA") == 0))
    {
        format = kFileFormatWavFile;
    } else
    {
        format = kFileFormatCompressedFile;
    }

    CriticalSectionScoped cs(_critSect);

    // Destroy the old instance
    if (_fileCallRecorderPtr)
    {
        _fileCallRecorderPtr->RegisterModuleFileCallback(NULL);
        FileRecorder::DestroyFileRecorder(_fileCallRecorderPtr);
        _fileCallRecorderPtr = NULL;
    }

    _fileCallRecorderPtr
        = FileRecorder::CreateFileRecorder(_fileCallRecorderId,
                                           (const FileFormats) format);
    if (_fileCallRecorderPtr == NULL)
    {
        _engineStatisticsPtr->SetLastError(
            VE_INVALID_ARGUMENT, kTraceError,
            "StartRecordingCall() fileRecorder format isnot correct");
        return -1;
    }

    if (_fileCallRecorderPtr->StartRecordingAudioFile(
        fileName,
        (const CodecInst&) *codecInst,
        notificationTime) != 0)
    {
        _engineStatisticsPtr->SetLastError(
            VE_BAD_FILE, kTraceError,
            "StartRecordingAudioFile() failed to start file recording");
        _fileCallRecorderPtr->StopRecording();
        FileRecorder::DestroyFileRecorder(_fileCallRecorderPtr);
        _fileCallRecorderPtr = NULL;
        return -1;
    }
    _fileCallRecorderPtr->RegisterModuleFileCallback(this);
    _fileCallRecording = true;

    return 0;
}

int TransmitMixer::StartRecordingCall(OutStream* stream,
                                      const  CodecInst* codecInst)
{
    WEBRTC_TRACE(kTraceInfo, kTraceVoice, VoEId(_instanceId, -1),
                 "TransmitMixer::StartRecordingCall()");

    if (_fileCallRecording)
    {
        WEBRTC_TRACE(kTraceWarning, kTraceVoice, VoEId(_instanceId, -1),
                     "StartRecordingCall() is already recording");
        return 0;
    }

    FileFormats format;
    const WebRtc_UWord32 notificationTime(0); // Not supported in VoE
    CodecInst dummyCodec = { 100, "L16", 16000, 320, 1, 320000 };

    if (codecInst != NULL && codecInst->channels != 1)
    {
        _engineStatisticsPtr->SetLastError(
            VE_BAD_ARGUMENT, kTraceError,
            "StartRecordingCall() invalid compression");
        return (-1);
    }
    if (codecInst == NULL)
    {
        format = kFileFormatPcm16kHzFile;
        codecInst = &dummyCodec;
    } else if ((STR_CASE_CMP(codecInst->plname,"L16") == 0) ||
        (STR_CASE_CMP(codecInst->plname,"PCMU") == 0) ||
        (STR_CASE_CMP(codecInst->plname,"PCMA") == 0))
    {
        format = kFileFormatWavFile;
    } else
    {
        format = kFileFormatCompressedFile;
    }

    CriticalSectionScoped cs(_critSect);

    // Destroy the old instance
    if (_fileCallRecorderPtr)
    {
        _fileCallRecorderPtr->RegisterModuleFileCallback(NULL);
        FileRecorder::DestroyFileRecorder(_fileCallRecorderPtr);
        _fileCallRecorderPtr = NULL;
    }

    _fileCallRecorderPtr =
        FileRecorder::CreateFileRecorder(_fileCallRecorderId,
                                         (const FileFormats) format);
    if (_fileCallRecorderPtr == NULL)
    {
        _engineStatisticsPtr->SetLastError(
            VE_INVALID_ARGUMENT, kTraceError,
            "StartRecordingCall() fileRecorder format isnot correct");
        return -1;
    }

    if (_fileCallRecorderPtr->StartRecordingAudioFile(*stream,
                                                      *codecInst,
                                                      notificationTime) != 0)
    {
    _engineStatisticsPtr->SetLastError(VE_BAD_FILE, kTraceError,
     "StartRecordingAudioFile() failed to start file recording");
    _fileCallRecorderPtr->StopRecording();
    FileRecorder::DestroyFileRecorder(_fileCallRecorderPtr);
    _fileCallRecorderPtr = NULL;
    return -1;
    }
     
    _fileCallRecorderPtr->RegisterModuleFileCallback(this);
    _fileCallRecording = true;

    return 0;
}

int TransmitMixer::StopRecordingCall()
{
    WEBRTC_TRACE(kTraceInfo, kTraceVoice, VoEId(_instanceId, -1),
                 "TransmitMixer::StopRecordingCall()");

    if (!_fileCallRecording)
    {
        WEBRTC_TRACE(kTraceError, kTraceVoice, VoEId(_instanceId, -1),
                     "StopRecordingCall() file isnot recording");
        return -1;
    }

    CriticalSectionScoped cs(_critSect);

    if (_fileCallRecorderPtr->StopRecording() != 0)
    {
        _engineStatisticsPtr->SetLastError(
            VE_STOP_RECORDING_FAILED, kTraceError,
            "StopRecording(), could not stop recording");
        return -1;
    }

    _fileCallRecorderPtr->RegisterModuleFileCallback(NULL);
    FileRecorder::DestroyFileRecorder(_fileCallRecorderPtr);
    _fileCallRecorderPtr = NULL;
    _fileCallRecording = false;

    return 0;
}

void 
TransmitMixer::SetMixWithMicStatus(bool mix)
{
    _mixFileWithMicrophone = mix;
}

int TransmitMixer::RegisterExternalMediaProcessing(
    VoEMediaProcess& proccess_object)
{
    WEBRTC_TRACE(kTraceInfo, kTraceVoice, VoEId(_instanceId, -1),
                 "TransmitMixer::RegisterExternalMediaProcessing()");

    CriticalSectionScoped cs(_callbackCritSect);
    _externalMediaCallbackPtr = &proccess_object;
    _externalMedia = true;

    return 0;
}

int TransmitMixer::DeRegisterExternalMediaProcessing()
{
    WEBRTC_TRACE(kTraceInfo, kTraceVoice, VoEId(_instanceId, -1),
                 "TransmitMixer::DeRegisterExternalMediaProcessing()");

    CriticalSectionScoped cs(_callbackCritSect);
    _externalMedia = false;
    _externalMediaCallbackPtr = NULL;

    return 0;
}

int
TransmitMixer::SetMute(bool enable)
{
    WEBRTC_TRACE(kTraceInfo, kTraceVoice, VoEId(_instanceId, -1),
                 "TransmitMixer::SetMute(enable=%d)", enable);
    _mute = enable;
    return 0;
}

bool
TransmitMixer::Mute() const
{
    return _mute;
}

WebRtc_Word8 TransmitMixer::AudioLevel() const
{
    // Speech + file level [0,9]
    return _audioLevel.Level();
}

WebRtc_Word16 TransmitMixer::AudioLevelFullRange() const
{
    // Speech + file level [0,32767]
    return _audioLevel.LevelFullRange();
}

bool TransmitMixer::IsRecordingCall()
{
    return _fileCallRecording;
}

bool TransmitMixer::IsRecordingMic()
{

    return _fileRecording;
}

WebRtc_Word32 
TransmitMixer::GenerateAudioFrame(const WebRtc_Word16 audioSamples[],
                                  const WebRtc_UWord32 nSamples,
                                  const WebRtc_UWord8 nChannels,
                                  const WebRtc_UWord32 samplesPerSec,
                                  const int mixingFrequency)
{
    WEBRTC_TRACE(kTraceStream, kTraceVoice, VoEId(_instanceId, -1),
                 "TransmitMixer::GenerateAudioFrame(nSamples=%u,"
                 "samplesPerSec=%u, mixingFrequency=%u)",
                 nSamples, samplesPerSec, mixingFrequency);

    if (_audioResampler.ResetIfNeeded(samplesPerSec,
                                        mixingFrequency,
                                        kResamplerSynchronous) != 0)
    {
        WEBRTC_TRACE(kTraceError, kTraceVoice, VoEId(_instanceId, -1),
                     "TransmitMixer::GenerateAudioFrame() unable to resample");
        return -1;
    }
    if (_audioResampler.Push(
        (WebRtc_Word16*) audioSamples,
        nSamples,
        _audioFrame._payloadData,
        AudioFrame::kMaxAudioFrameSizeSamples,
        (int&) _audioFrame._payloadDataLengthInSamples) == -1)
    {
        WEBRTC_TRACE(kTraceError, kTraceVoice, VoEId(_instanceId, -1),
                     "TransmitMixer::GenerateAudioFrame() resampling failed");
        return -1;
    }

    _audioFrame._id = _instanceId;
    _audioFrame._timeStamp = -1;
    _audioFrame._frequencyInHz = mixingFrequency;
    _audioFrame._speechType = AudioFrame::kNormalSpeech;
    _audioFrame._vadActivity = AudioFrame::kVadUnknown;
    _audioFrame._audioChannel = nChannels;

    return 0;
}

WebRtc_Word32 TransmitMixer::RecordAudioToFile(
    const WebRtc_UWord32 mixingFrequency)
{
    assert(_audioFrame._audioChannel == 1);

    CriticalSectionScoped cs(_critSect);
    if (_fileRecorderPtr == NULL)
    {
        WEBRTC_TRACE(kTraceWarning, kTraceVoice, VoEId(_instanceId, -1),
                     "TransmitMixer::RecordAudioToFile() filerecorder doesnot"
                     "exist");
        return -1;
    }

    if (_fileRecorderPtr->RecordAudioToFile(_audioFrame) != 0)
    {
        WEBRTC_TRACE(kTraceWarning, kTraceVoice, VoEId(_instanceId, -1),
                     "TransmitMixer::RecordAudioToFile() file recording"
                     "failed");
        return -1;
    }

    return 0;
}

WebRtc_Word32 TransmitMixer::MixOrReplaceAudioWithFile(
    const int mixingFrequency)
{
    WebRtc_Word16 fileBuffer[320];

    WebRtc_UWord32 fileSamples(0);

    {
        CriticalSectionScoped cs(_critSect);
        if (_filePlayerPtr == NULL)
        {
            WEBRTC_TRACE(kTraceWarning, kTraceVoice,
                         VoEId(_instanceId, -1),
                         "TransmitMixer::MixOrReplaceAudioWithFile()"
                         "fileplayer doesnot exist");
            return -1;
        }

        if (_filePlayerPtr->Get10msAudioFromFile(fileBuffer,
                                                 fileSamples,
                                                 mixingFrequency) == -1)
        {
            WEBRTC_TRACE(kTraceWarning, kTraceVoice, VoEId(_instanceId, -1),
                         "TransmitMixer::MixOrReplaceAudioWithFile() file"
                         " mixing failed");
            return -1;
        }
    }

    if (_mixFileWithMicrophone)
    {
        Utility::MixWithSat(_audioFrame._payloadData,
                             fileBuffer,
                             (WebRtc_UWord16) fileSamples);
        assert(_audioFrame._payloadDataLengthInSamples == fileSamples);
    } else
    {
        // replace ACM audio with file
        _audioFrame.UpdateFrame(-1,
                                -1,
                                fileBuffer,
                                (WebRtc_UWord16) fileSamples, mixingFrequency,
                                AudioFrame::kNormalSpeech,
                                AudioFrame::kVadUnknown,
                                1);

    }
    return 0;
}

WebRtc_Word32 TransmitMixer::APMProcessStream(
    const WebRtc_UWord16 totalDelayMS,
    const WebRtc_Word32 clockDrift,
    const WebRtc_UWord16 currentMicLevel)
{
    WebRtc_UWord16 captureLevel(currentMicLevel);

    // If the frequency has changed we need to change APM settings
    // Sending side is "master"
    if (_audioProcessingModulePtr->sample_rate_hz()
        != _audioFrame._frequencyInHz)
    {
        if (_audioProcessingModulePtr->set_sample_rate_hz(
            _audioFrame._frequencyInHz))
        {
            WEBRTC_TRACE(kTraceWarning, kTraceVoice, VoEId(_instanceId, -1),
                         "AudioProcessingModule::set_sample_rate_hz("
                         "_frequencyInHz=%u) => error",
                         _audioFrame._frequencyInHz);
        }
    }

    if (_audioProcessingModulePtr->set_stream_delay_ms(totalDelayMS) == -1)
    {
        WEBRTC_TRACE(kTraceWarning, kTraceVoice, VoEId(_instanceId, -1),
                     "AudioProcessingModule::set_stream_delay_ms("
                     "totalDelayMS=%u) => error",
                     totalDelayMS);
    }
    if (_audioProcessingModulePtr->gain_control()->set_stream_analog_level(
        captureLevel) == -1)
    {
        WEBRTC_TRACE(kTraceWarning, kTraceVoice, VoEId(_instanceId, -1),
                   "AudioProcessingModule::set_stream_analog_level "
                   "(captureLevel=%u,) => error",
                   captureLevel);
    }
    if (_audioProcessingModulePtr->echo_cancellation()->
        is_drift_compensation_enabled())
    {
        if (_audioProcessingModulePtr->echo_cancellation()->
            set_stream_drift_samples(clockDrift) == -1)
        {
            WEBRTC_TRACE(kTraceWarning, kTraceVoice, VoEId(_instanceId, -1),
                       "AudioProcessingModule::set_stream_drift_samples("
                       "clockDrift=%u,) => error",
                       clockDrift);
        }
    }
    if (_audioProcessingModulePtr->ProcessStream(&_audioFrame) == -1)
    {
        WEBRTC_TRACE(kTraceWarning, kTraceVoice, VoEId(_instanceId, -1),
                   "AudioProcessingModule::ProcessStream() => error");
    }
    captureLevel
        = _audioProcessingModulePtr->gain_control()->stream_analog_level();

    // Store new capture level (only updated when analog AGC is enabled)
    _captureLevel = captureLevel;

    // Log notifications
    if (_audioProcessingModulePtr->gain_control()->stream_is_saturated())
    {
        if (_saturationWarning == 1)
        {
            WEBRTC_TRACE(kTraceWarning, kTraceVoice, VoEId(_instanceId, -1),
                       "TransmitMixer::APMProcessStream() pending "
                       "saturation warning exists");
        }
        _saturationWarning = 1; // triggers callback from moduleprocess thread
        WEBRTC_TRACE(kTraceWarning, kTraceVoice, VoEId(_instanceId, -1),
                   "TransmitMixer::APMProcessStream() VE_SATURATION_WARNING "
                   "message has been posted for callback");
    }

    return 0;
}

#ifdef WEBRTC_VOICE_ENGINE_TYPING_DETECTION
int TransmitMixer::TypingDetection()
{
    // We let the VAD determine if we're using this feature or not.
    if (_audioFrame._vadActivity == AudioFrame::kVadUnknown)
    {
        return (0);
    }

    int keyPressed = EventWrapper::KeyPressed();

    if (keyPressed < 0)
    {
        return (-1);
    }

    if (_audioFrame._vadActivity == AudioFrame::kVadActive)
        _timeActive++;
    else
        _timeActive = 0;

    if (keyPressed && (_audioFrame._vadActivity == AudioFrame::kVadActive)
        && (_timeActive < 10))
    {
        _penaltyCounter += 100;
        if (_penaltyCounter > 300)
        {
            if (_typingNoiseWarning == 1)
            {
                WEBRTC_TRACE(kTraceWarning, kTraceVoice,
                           VoEId(_instanceId, -1),
                           "TransmitMixer::TypingDetection() pending "
                               "noise-saturation warning exists");
            }
            // triggers callback from the module process thread
            _typingNoiseWarning = 1;
            WEBRTC_TRACE(kTraceWarning, kTraceVoice, VoEId(_instanceId, -1),
                       "TransmitMixer::TypingDetection() "
                       "VE_TYPING_NOISE_WARNING message has been posted for"
                       "callback");
        }
    }

    if (_penaltyCounter > 0)
        _penaltyCounter--;

    return (0);
}
#endif

int TransmitMixer::GetMixingFrequency()
{
    assert(_mixingFrequency!=0);
    return (_mixingFrequency);
}

}  //  namespace voe

}  //  namespace webrtc
