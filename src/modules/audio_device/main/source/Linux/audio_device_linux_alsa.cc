/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <cassert>

#include "audio_device_utility.h"
#include "audio_device_linux_alsa.h"
#include "audio_device_config.h"

#include "event_wrapper.h"
#include "trace.h"
#include "thread_wrapper.h"


webrtc_adm_linux_alsa::AlsaSymbolTable AlsaSymbolTable;

// Accesses ALSA functions through our late-binding symbol table instead of
// directly. This way we don't have to link to libasound, which means our binary
// will work on systems that don't have it.
#define LATE(sym) \
  LATESYM_GET(webrtc_adm_linux_alsa::AlsaSymbolTable, &AlsaSymbolTable, sym)

// Redefine these here to be able to do late-binding
#undef snd_ctl_card_info_alloca
#define snd_ctl_card_info_alloca(ptr) \
        do { *ptr = (snd_ctl_card_info_t *) \
            __builtin_alloca (LATE(snd_ctl_card_info_sizeof)()); \
            memset(*ptr, 0, LATE(snd_ctl_card_info_sizeof)()); } while (0)

#undef snd_pcm_info_alloca
#define snd_pcm_info_alloca(pInfo) \
       do { *pInfo = (snd_pcm_info_t *) \
       __builtin_alloca (LATE(snd_pcm_info_sizeof)()); \
       memset(*pInfo, 0, LATE(snd_pcm_info_sizeof)()); } while (0)

// snd_lib_error_handler_t
void WebrtcAlsaErrorHandler(const char *file,
                          int line,
                          const char *function,
                          int err,
                          const char *fmt,...){};

namespace webrtc
{
AudioDeviceLinuxALSA::AudioDeviceLinuxALSA(const WebRtc_Word32 id) :
    _ptrAudioBuffer(NULL),
    _critSect(*CriticalSectionWrapper::CreateCriticalSection()),
    _timeEventRec(*EventWrapper::Create()),
    _timeEventPlay(*EventWrapper::Create()),
    _recStartEvent(*EventWrapper::Create()),
    _playStartEvent(*EventWrapper::Create()),
    _ptrThreadRec(NULL),
    _ptrThreadPlay(NULL),
    _recThreadID(0),
    _playThreadID(0),
    _id(id),
    _mixerManager(id),
    _inputDeviceIndex(0),
    _outputDeviceIndex(0),
    _inputDeviceIsSpecified(false),
    _outputDeviceIsSpecified(false),
    _handleRecord(NULL),
    _handlePlayout(NULL),
    _recSndcardBuffsize(ALSA_SNDCARD_BUFF_SIZE_REC),
    _playSndcardBuffsize(ALSA_SNDCARD_BUFF_SIZE_PLAY),
    _initialized(false),
    _recIsInitialized(false),
    _playIsInitialized(false),
    _recording(false),
    _playing(false),
    _startRec(false),
    _stopRec(false),
    _startPlay(false),
    _stopPlay(false),
    _AGC(false),
    _buffersizeFromZeroAvail(true),
    _buffersizeFromZeroDelay(true),
    _sndCardPlayDelay(0),
    _sndCardRecDelay(0),
    _numReadyRecSamples(0),
    _previousSndCardPlayDelay(0),
    _delayMonitorStatePlay(0),
    _largeDelayCountPlay(0),
    _bufferCheckMethodPlay(0),
    _bufferCheckMethodRec(0),
    _bufferCheckErrorsPlay(0),
    _bufferCheckErrorsRec(0),
    _lastBufferCheckValuePlay(0),
    _writeErrors(0),
    _playWarning(0),
    _playError(0),
    _recWarning(0),
    _recError(0),
    _samplingFreqRec(REC_SAMPLES_PER_MS),
    _samplingFreqPlay(PLAY_SAMPLES_PER_MS),
    _recChannels(1),
    _playChannels(1),
    _playbackBufferSize(0),
    _recordBufferSize(0),
    _recBuffer(NULL),
    _playBufDelay(80),
    _playBufDelayFixed(80),
    _playBufType(AudioDeviceModule::kAdaptiveBufferSize)
{
    WEBRTC_TRACE(kTraceMemory, kTraceAudioDevice, id,
                 "%s created", __FUNCTION__);
}

// ----------------------------------------------------------------------------
//  AudioDeviceLinuxALSA - dtor
// ----------------------------------------------------------------------------

AudioDeviceLinuxALSA::~AudioDeviceLinuxALSA()
{
    WEBRTC_TRACE(kTraceMemory, kTraceAudioDevice, _id,
                 "%s destroyed", __FUNCTION__);
    
    Terminate();

    if (_recBuffer)
    {
        delete _recBuffer;
    }
    delete &_recStartEvent;
    delete &_playStartEvent;
    delete &_timeEventRec;
    delete &_timeEventPlay;
    delete &_critSect;
}

void AudioDeviceLinuxALSA::AttachAudioBuffer(AudioDeviceBuffer* audioBuffer)
{
    WEBRTC_TRACE(kTraceModuleCall, kTraceAudioDevice, _id,
                 "%s", __FUNCTION__);

    CriticalSectionScoped lock(_critSect);

    _ptrAudioBuffer = audioBuffer;

    // Inform the AudioBuffer about default settings for this implementation.
    // Set all values to zero here since the actual settings will be done by
    // InitPlayout and InitRecording later.
    _ptrAudioBuffer->SetRecordingSampleRate(0);
    _ptrAudioBuffer->SetPlayoutSampleRate(0);
    _ptrAudioBuffer->SetRecordingChannels(0);
    _ptrAudioBuffer->SetPlayoutChannels(0);
}

WebRtc_Word32 AudioDeviceLinuxALSA::ActiveAudioLayer(
    AudioDeviceModule::AudioLayer& audioLayer) const
{
    WEBRTC_TRACE(kTraceModuleCall, kTraceAudioDevice, _id,
                 "%s", __FUNCTION__);
    audioLayer = AudioDeviceModule::kLinuxAlsaAudio;
    return 0;
}

WebRtc_Word32 AudioDeviceLinuxALSA::Init()
{
    WEBRTC_TRACE(kTraceModuleCall, kTraceAudioDevice, _id,
                 "%s", __FUNCTION__);

    CriticalSectionScoped lock(_critSect);

    // Load libasound
    if (!AlsaSymbolTable.Load())
    {
        // Alsa is not installed on
        // this system
        WEBRTC_TRACE(kTraceError, kTraceAudioDevice, _id,
                   "  failed to load symbol table");
        return -1;
    }

    if (_initialized)
    {
        return 0;
    }

    _playWarning = 0;
    _playError = 0;
    _recWarning = 0;
    _recError = 0;

    // RECORDING
    const char* threadName = "webrtc_audio_module_rec_thread";
    _ptrThreadRec = ThreadWrapper::CreateThread(RecThreadFunc,
                                                this,
                                                kRealtimePriority,
                                                threadName);
    if (_ptrThreadRec == NULL)
    {
        WEBRTC_TRACE(kTraceCritical, kTraceAudioDevice, _id,
                     "  failed to create the rec audio thread");
        return -1;
    }

    unsigned int threadID(0);
    if (!_ptrThreadRec->Start(threadID))
    {
        WEBRTC_TRACE(kTraceCritical, kTraceAudioDevice, _id,
                     "  failed to start the rec audio thread");
        delete _ptrThreadRec;
        _ptrThreadRec = NULL;
        return -1;
    }
    _recThreadID = threadID;
    
    const bool periodic(true);
    if (!_timeEventRec.StartTimer(periodic, REC_TIMER_PERIOD_MS))
    {
        WEBRTC_TRACE(kTraceCritical, kTraceAudioDevice, _id,
                     "  failed to start the rec timer event");
        if (_ptrThreadRec->Stop())
        {
            delete _ptrThreadRec;
            _ptrThreadRec = NULL;
        }
        else
        {
            WEBRTC_TRACE(kTraceWarning, kTraceAudioDevice, _id,
                         "  unable to stop the activated rec thread");
        }
        return -1;
    }

    WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, _id,
                 "  periodic rec timer (dT=%d) is now active",
                 REC_TIMER_PERIOD_MS);

    // PLAYOUT
    threadName = "webrtc_audio_module_play_thread";
    _ptrThreadPlay = ThreadWrapper::CreateThread(PlayThreadFunc,
                                                 this,
                                                 kRealtimePriority,
                                                 threadName);
    if (_ptrThreadPlay == NULL)
    {
        WEBRTC_TRACE(kTraceCritical, kTraceAudioDevice, _id,
                     "  failed to create the play audio thread");
        return -1;
    }

    threadID = 0;
    if (!_ptrThreadPlay->Start(threadID))
    {
        WEBRTC_TRACE(kTraceCritical, kTraceAudioDevice, _id,
                     "  failed to start the play audio thread");
        delete _ptrThreadPlay;
        _ptrThreadPlay = NULL;
        return -1;
    }
    _playThreadID = threadID;
    
    if (!_timeEventPlay.StartTimer(periodic, PLAY_TIMER_PERIOD_MS))
    {
        WEBRTC_TRACE(kTraceCritical, kTraceAudioDevice, _id,
                     "  failed to start the play timer event");
        if (_ptrThreadPlay->Stop())
        {
            delete _ptrThreadPlay;
            _ptrThreadPlay = NULL;
        }
        else
        {
            WEBRTC_TRACE(kTraceWarning, kTraceAudioDevice, _id,
                         "  unable to stop the activated play thread");
        }
        return -1;
    }

    WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, _id,
                 "  periodic play timer (dT=%d) is now active", PLAY_TIMER_PERIOD_MS);

    _initialized = true;

    return 0;
}

WebRtc_Word32 AudioDeviceLinuxALSA::Terminate()
{
    WEBRTC_TRACE(kTraceModuleCall, kTraceAudioDevice, _id,
                 "%s", __FUNCTION__);

    if (!_initialized)
    {
        return 0;
    }

    CriticalSectionScoped lock(_critSect);

    _mixerManager.Close();

    // RECORDING
    if (_ptrThreadRec)
    {
        ThreadWrapper* tmpThread = _ptrThreadRec;
        _ptrThreadRec = NULL;
        _critSect.Leave();

        tmpThread->SetNotAlive();
        _timeEventRec.Set();

        if (tmpThread->Stop())
        {
            delete tmpThread;
        }
        else
        {
            WEBRTC_TRACE(kTraceWarning, kTraceAudioDevice, _id,
                         "  failed to close down the rec audio thread");
        }

        _critSect.Enter();
    }

    _timeEventRec.StopTimer();

    // PLAYOUT
    if (_ptrThreadPlay)
    {
        ThreadWrapper* tmpThread = _ptrThreadPlay;
        _ptrThreadPlay = NULL;
        _critSect.Leave();

        tmpThread->SetNotAlive();
        _timeEventPlay.Set();

        if (tmpThread->Stop())
        {
            delete tmpThread;
        }
        else
        {
            WEBRTC_TRACE(kTraceWarning, kTraceAudioDevice, _id,
                         "  failed to close down the play audio thread");
        }

        _critSect.Enter();
    }

    _timeEventPlay.StopTimer();

    _initialized = false;
    _outputDeviceIsSpecified = false;
    _inputDeviceIsSpecified = false;

    return 0;
}

bool AudioDeviceLinuxALSA::Initialized() const
{
    WEBRTC_TRACE(kTraceModuleCall, kTraceAudioDevice, _id,
                 "%s", __FUNCTION__);
    return (_initialized);
}

WebRtc_Word32 AudioDeviceLinuxALSA::SpeakerIsAvailable(bool& available)
{
    WEBRTC_TRACE(kTraceModuleCall, kTraceAudioDevice, _id,
                 "%s", __FUNCTION__);

    bool wasInitialized = _mixerManager.SpeakerIsInitialized();

    // Make an attempt to open up the
    // output mixer corresponding to the currently selected output device.
    //
    if (!wasInitialized && InitSpeaker() == -1)
    {
        available = false;
        return 0;
    }

    // Given that InitSpeaker was successful, we know that a valid speaker
    // exists
    available = true;

    // Close the initialized output mixer
    //
    if (!wasInitialized)
    {
        _mixerManager.CloseSpeaker();
    }

    return 0;
}

WebRtc_Word32 AudioDeviceLinuxALSA::InitSpeaker()
{
    WEBRTC_TRACE(kTraceModuleCall, kTraceAudioDevice, _id,
                 "%s", __FUNCTION__);

    CriticalSectionScoped lock(_critSect);

    if (_playing)
    {
        return -1;
    }

    char devName[kAdmMaxDeviceNameSize] = {0};
    GetDevicesInfo(2, true, _outputDeviceIndex, devName, kAdmMaxDeviceNameSize);
    return _mixerManager.OpenSpeaker(devName);
}

WebRtc_Word32 AudioDeviceLinuxALSA::MicrophoneIsAvailable(bool& available)
{
    WEBRTC_TRACE(kTraceModuleCall, kTraceAudioDevice, _id,
                 "%s", __FUNCTION__);

    bool wasInitialized = _mixerManager.MicrophoneIsInitialized();

    // Make an attempt to open up the
    // input mixer corresponding to the currently selected output device.
    //
    if (!wasInitialized && InitMicrophone() == -1)
    {
        available = false;
        return 0;
    }

    // Given that InitMicrophone was successful, we know that a valid
    // microphone exists
    available = true;

    // Close the initialized input mixer
    //
    if (!wasInitialized)
    {
        _mixerManager.CloseMicrophone();
    }

    return 0;
}

WebRtc_Word32 AudioDeviceLinuxALSA::InitMicrophone()
{
    WEBRTC_TRACE(kTraceModuleCall, kTraceAudioDevice, _id,
                 "%s", __FUNCTION__);

    CriticalSectionScoped lock(_critSect);

    if (_recording)
    {
        return -1;
    }

    char devName[kAdmMaxDeviceNameSize] = {0};
    GetDevicesInfo(2, false, _inputDeviceIndex, devName, kAdmMaxDeviceNameSize);
    return _mixerManager.OpenMicrophone(devName);
}

bool AudioDeviceLinuxALSA::SpeakerIsInitialized() const
{
    WEBRTC_TRACE(kTraceModuleCall, kTraceAudioDevice, _id,
                 "%s", __FUNCTION__);
    return (_mixerManager.SpeakerIsInitialized());
}

bool AudioDeviceLinuxALSA::MicrophoneIsInitialized() const
{
    WEBRTC_TRACE(kTraceModuleCall, kTraceAudioDevice, _id,
                 "%s", __FUNCTION__);
    return (_mixerManager.MicrophoneIsInitialized());
}

WebRtc_Word32 AudioDeviceLinuxALSA::SpeakerVolumeIsAvailable(bool& available)
{
    WEBRTC_TRACE(kTraceModuleCall, kTraceAudioDevice, _id,
                 "%s", __FUNCTION__);

    bool wasInitialized = _mixerManager.SpeakerIsInitialized();

    // Make an attempt to open up the
    // output mixer corresponding to the currently selected output device.
    if (!wasInitialized && InitSpeaker() == -1)
    {
        // If we end up here it means that the selected speaker has no volume
        // control.
        available = false;
        return 0;
    }

    // Given that InitSpeaker was successful, we know that a volume control
    // exists
    available = true;

    // Close the initialized output mixer
    if (!wasInitialized)
    {
        _mixerManager.CloseSpeaker();
    }

    return 0;
}

WebRtc_Word32 AudioDeviceLinuxALSA::SetSpeakerVolume(WebRtc_UWord32 volume)
{
    WEBRTC_TRACE(kTraceModuleCall, kTraceAudioDevice, _id,
                 "SetSpeakerVolume(volume=%u)", volume);

    return (_mixerManager.SetSpeakerVolume(volume));
}

WebRtc_Word32 AudioDeviceLinuxALSA::SpeakerVolume(WebRtc_UWord32& volume) const
{
    WEBRTC_TRACE(kTraceModuleCall, kTraceAudioDevice, _id,
                 "%s", __FUNCTION__);

    WebRtc_UWord32 level(0);

    if (_mixerManager.SpeakerVolume(level) == -1)
    {
        return -1;
    }

    volume = level;
    
    return 0;
}


WebRtc_Word32 AudioDeviceLinuxALSA::SetWaveOutVolume(WebRtc_UWord16 volumeLeft,
                                                     WebRtc_UWord16 volumeRight)
{
    WEBRTC_TRACE(kTraceModuleCall, kTraceAudioDevice, _id,
                 "SetWaveOutVolume(volumeLeft=%u, volumeRight=%u)",
        volumeLeft, volumeRight);

    WEBRTC_TRACE(kTraceWarning, kTraceAudioDevice, _id,
                 "  API call not supported on this platform");
    return -1;
}

WebRtc_Word32 AudioDeviceLinuxALSA::WaveOutVolume(
    WebRtc_UWord16& /*volumeLeft*/,
    WebRtc_UWord16& /*volumeRight*/) const
{
    WEBRTC_TRACE(kTraceModuleCall, kTraceAudioDevice, _id,
                 "%s", __FUNCTION__);

    WEBRTC_TRACE(kTraceWarning, kTraceAudioDevice, _id,
                 "  API call not supported on this platform");
    return -1;
}

WebRtc_Word32 AudioDeviceLinuxALSA::MaxSpeakerVolume(
    WebRtc_UWord32& maxVolume) const
{
    WEBRTC_TRACE(kTraceModuleCall, kTraceAudioDevice, _id,
                 "%s", __FUNCTION__);

    WebRtc_UWord32 maxVol(0);

    if (_mixerManager.MaxSpeakerVolume(maxVol) == -1)
    {
        return -1;
    }

    maxVolume = maxVol;
    
    return 0;
}

WebRtc_Word32 AudioDeviceLinuxALSA::MinSpeakerVolume(
    WebRtc_UWord32& minVolume) const
{
   WEBRTC_TRACE(kTraceModuleCall, kTraceAudioDevice, _id,
                "%s", __FUNCTION__);

    WebRtc_UWord32 minVol(0);

    if (_mixerManager.MinSpeakerVolume(minVol) == -1)
    {
        return -1;
    }

    minVolume = minVol;
    
    return 0;
}

WebRtc_Word32 AudioDeviceLinuxALSA::SpeakerVolumeStepSize(
    WebRtc_UWord16& stepSize) const
{
    WEBRTC_TRACE(kTraceModuleCall, kTraceAudioDevice, _id,
                 "%s", __FUNCTION__);

    WebRtc_UWord16 delta(0); 
     
    if (_mixerManager.SpeakerVolumeStepSize(delta) == -1)
    {
        return -1;
    }

    stepSize = delta;

    return 0;
}

WebRtc_Word32 AudioDeviceLinuxALSA::SpeakerMuteIsAvailable(bool& available)
{
    WEBRTC_TRACE(kTraceModuleCall, kTraceAudioDevice, _id,
                 "%s", __FUNCTION__);

    bool isAvailable(false);
    bool wasInitialized = _mixerManager.SpeakerIsInitialized();

    // Make an attempt to open up the
    // output mixer corresponding to the currently selected output device.
    //
    if (!wasInitialized && InitSpeaker() == -1)
    {
        // If we end up here it means that the selected speaker has no volume
        // control, hence it is safe to state that there is no mute control
        // already at this stage.
        available = false;
        return 0;
    }

    // Check if the selected speaker has a mute control
    _mixerManager.SpeakerMuteIsAvailable(isAvailable);

    available = isAvailable;

    // Close the initialized output mixer
    if (!wasInitialized)
    {
        _mixerManager.CloseSpeaker();
    }

    return 0;
}

WebRtc_Word32 AudioDeviceLinuxALSA::SetSpeakerMute(bool enable)
{
    WEBRTC_TRACE(kTraceModuleCall, kTraceAudioDevice, _id,
                 "SetSpeakerMute(enable=%u)", enable);
    return (_mixerManager.SetSpeakerMute(enable));
}

WebRtc_Word32 AudioDeviceLinuxALSA::SpeakerMute(bool& enabled) const
{
    WEBRTC_TRACE(kTraceModuleCall, kTraceAudioDevice, _id,
                 "%s", __FUNCTION__);

    bool muted(0); 
        
    if (_mixerManager.SpeakerMute(muted) == -1)
    {
        return -1;
    }

    enabled = muted;
    
    return 0;
}

WebRtc_Word32 AudioDeviceLinuxALSA::MicrophoneMuteIsAvailable(bool& available)
{
    WEBRTC_TRACE(kTraceModuleCall, kTraceAudioDevice, _id,
                 "%s", __FUNCTION__);

    bool isAvailable(false);
    bool wasInitialized = _mixerManager.MicrophoneIsInitialized();

    // Make an attempt to open up the
    // input mixer corresponding to the currently selected input device.
    //
    if (!wasInitialized && InitMicrophone() == -1)
    {
        // If we end up here it means that the selected microphone has no volume
        // control, hence it is safe to state that there is no mute control
        // already at this stage.
        available = false;
        return 0;
    }

    // Check if the selected microphone has a mute control
    //
    _mixerManager.MicrophoneMuteIsAvailable(isAvailable);
    available = isAvailable;

    // Close the initialized input mixer
    //
    if (!wasInitialized)
    {
        _mixerManager.CloseMicrophone();
    }

    return 0;
}

WebRtc_Word32 AudioDeviceLinuxALSA::SetMicrophoneMute(bool enable)
{
    WEBRTC_TRACE(kTraceModuleCall, kTraceAudioDevice, _id,
                 "SetMicrophoneMute(enable=%u)", enable);
    return (_mixerManager.SetMicrophoneMute(enable));
}

// ----------------------------------------------------------------------------
//  MicrophoneMute
// ----------------------------------------------------------------------------

WebRtc_Word32 AudioDeviceLinuxALSA::MicrophoneMute(bool& enabled) const
{
    WEBRTC_TRACE(kTraceModuleCall, kTraceAudioDevice, _id,
                 "%s", __FUNCTION__);

    bool muted(0); 
        
    if (_mixerManager.MicrophoneMute(muted) == -1)
    {
        return -1;
    }

    enabled = muted;
    return 0;
}

WebRtc_Word32 AudioDeviceLinuxALSA::MicrophoneBoostIsAvailable(bool& available)
{
    WEBRTC_TRACE(kTraceModuleCall, kTraceAudioDevice, _id,
                 "%s", __FUNCTION__);
    
    bool isAvailable(false);
    bool wasInitialized = _mixerManager.MicrophoneIsInitialized();

    // Enumerate all avaliable microphone and make an attempt to open up the
    // input mixer corresponding to the currently selected input device.
    //
    if (!wasInitialized && InitMicrophone() == -1)
    {
        // If we end up here it means that the selected microphone has no volume
        // control, hence it is safe to state that there is no boost control
        // already at this stage.
        available = false;
        return 0;
    }

    // Check if the selected microphone has a boost control
    _mixerManager.MicrophoneBoostIsAvailable(isAvailable);
    available = isAvailable;

    // Close the initialized input mixer
    if (!wasInitialized)
    {
        _mixerManager.CloseMicrophone();
    }

    return 0;
}

WebRtc_Word32 AudioDeviceLinuxALSA::SetMicrophoneBoost(bool enable)
{
    WEBRTC_TRACE(kTraceModuleCall, kTraceAudioDevice, _id,
                 "SetMicrophoneBoost(enable=%u)", enable);

    return (_mixerManager.SetMicrophoneBoost(enable));
}

WebRtc_Word32 AudioDeviceLinuxALSA::MicrophoneBoost(bool& enabled) const
{
    WEBRTC_TRACE(kTraceModuleCall, kTraceAudioDevice, _id,
                 "%s", __FUNCTION__);

    bool onOff(0); 
        
    if (_mixerManager.MicrophoneBoost(onOff) == -1)
    {
        return -1;
    }

    enabled = onOff;
    
    return 0;
}

WebRtc_Word32 AudioDeviceLinuxALSA::StereoRecordingIsAvailable(bool& available)
{
    WEBRTC_TRACE(kTraceModuleCall, kTraceAudioDevice, _id,
                 "%s", __FUNCTION__);

    CriticalSectionScoped lock(_critSect);

    // If we already have initialized in stereo it's obviously available
    if (_recIsInitialized && (2 == _recChannels))
    {
        available = true;
        return 0;
    }

    // Save rec states and the number of rec channels
    bool recIsInitialized = _recIsInitialized;
    bool recording = _recording;
    int recChannels = _recChannels;

    available = false;
    
    // Stop/uninitialize recording if initialized (and possibly started)
    if (_recIsInitialized)
    {
        StopRecording();
    }

    // Try init in stereo;
    _recChannels = 2;
    if (InitRecording() == 0)
    {
        available = true;
    }

    // Stop/uninitialize recording
    StopRecording();

    // Recover previous states
    _recChannels = recChannels;
    if (recIsInitialized)
    {
        InitRecording();
    }
    if (recording)
    {
        StartRecording();
    }

    return 0;
}

WebRtc_Word32 AudioDeviceLinuxALSA::SetStereoRecording(bool enable)
{
    WEBRTC_TRACE(kTraceModuleCall, kTraceAudioDevice, _id,
                 "SetStereoRecording(enable=%u)", enable);

    if (enable)
        _recChannels = 2;
    else
        _recChannels = 1;

    return 0;
}

WebRtc_Word32 AudioDeviceLinuxALSA::StereoRecording(bool& enabled) const
{
    WEBRTC_TRACE(kTraceModuleCall, kTraceAudioDevice, _id,
                 "%s", __FUNCTION__);

    if (_recChannels == 2)
        enabled = true;
    else
        enabled = false;

    return 0;
}

WebRtc_Word32 AudioDeviceLinuxALSA::StereoPlayoutIsAvailable(bool& available)
{
    WEBRTC_TRACE(kTraceModuleCall, kTraceAudioDevice, _id,
                 "%s", __FUNCTION__);

    CriticalSectionScoped lock(_critSect);

    // If we already have initialized in stereo it's obviously available
    if (_playIsInitialized && (2 == _playChannels))
    {
        available = true;
        return 0;
    }

    // Save rec states and the number of rec channels
    bool playIsInitialized = _playIsInitialized;
    bool playing = _playing;
    int playChannels = _playChannels;

    available = false;
    
    // Stop/uninitialize recording if initialized (and possibly started)
    if (_playIsInitialized)
    {
        StopPlayout();
    }

    // Try init in stereo;
    _playChannels = 2;
    if (InitPlayout() == 0)
    {
        available = true;
    }

    // Stop/uninitialize recording
    StopPlayout();

    // Recover previous states
    _playChannels = playChannels;
    if (playIsInitialized)
    {
        InitPlayout();
    }
    if (playing)
    {
        StartPlayout();
    }

    return 0;
}

WebRtc_Word32 AudioDeviceLinuxALSA::SetStereoPlayout(bool enable)
{
    WEBRTC_TRACE(kTraceModuleCall, kTraceAudioDevice, _id,
                 "SetStereoPlayout(enable=%u)", enable);

    if (enable)
        _playChannels = 2;
    else
        _playChannels = 1;

    return 0;
}

WebRtc_Word32 AudioDeviceLinuxALSA::StereoPlayout(bool& enabled) const
{
    WEBRTC_TRACE(kTraceModuleCall, kTraceAudioDevice, _id,
                 "%s", __FUNCTION__);

    if (_playChannels == 2)
        enabled = true;
    else
        enabled = false;

    return 0;
}

WebRtc_Word32 AudioDeviceLinuxALSA::SetAGC(bool enable)
{
    WEBRTC_TRACE(kTraceModuleCall, kTraceAudioDevice, _id,
                 "SetAGC(enable=%d)", enable);

    _AGC = enable;

    return 0;
}

bool AudioDeviceLinuxALSA::AGC() const
{
    WEBRTC_TRACE(kTraceModuleCall, kTraceAudioDevice, _id,
                 "%s", __FUNCTION__);

    return _AGC;
}

WebRtc_Word32 AudioDeviceLinuxALSA::MicrophoneVolumeIsAvailable(bool& available)
{
    WEBRTC_TRACE(kTraceModuleCall, kTraceAudioDevice, _id,
                 "%s", __FUNCTION__);

    bool wasInitialized = _mixerManager.MicrophoneIsInitialized();

    // Make an attempt to open up the
    // input mixer corresponding to the currently selected output device.
    if (!wasInitialized && InitMicrophone() == -1)
    {
        // If we end up here it means that the selected microphone has no volume
        // control.
        available = false;
        return 0;
    }

    // Given that InitMicrophone was successful, we know that a volume control
    // exists
    available = true;

    // Close the initialized input mixer
    if (!wasInitialized)
    {
        _mixerManager.CloseMicrophone();
    }

    return 0;
}

WebRtc_Word32 AudioDeviceLinuxALSA::SetMicrophoneVolume(WebRtc_UWord32 volume)
{
    WEBRTC_TRACE(kTraceModuleCall, kTraceAudioDevice, _id,
                 "SetMicrophoneVolume(volume=%u)", volume);

    return (_mixerManager.SetMicrophoneVolume(volume));
 
    return 0;
}

WebRtc_Word32 AudioDeviceLinuxALSA::MicrophoneVolume(WebRtc_UWord32& volume) const
{
    WEBRTC_TRACE(kTraceModuleCall, kTraceAudioDevice, _id,
                 "%s", __FUNCTION__);

    WebRtc_UWord32 level(0);

    if (_mixerManager.MicrophoneVolume(level) == -1)
    {
        WEBRTC_TRACE(kTraceWarning, kTraceAudioDevice, _id,
                     "  failed to retrive current microphone level");
        return -1;
    }

    volume = level;
    
    return 0;
}

WebRtc_Word32 AudioDeviceLinuxALSA::MaxMicrophoneVolume(
    WebRtc_UWord32& maxVolume) const
{
    WEBRTC_TRACE(kTraceModuleCall, kTraceAudioDevice, _id,
                 "%s", __FUNCTION__);

    WebRtc_UWord32 maxVol(0);

    if (_mixerManager.MaxMicrophoneVolume(maxVol) == -1)
    {
        return -1;
    }

    maxVolume = maxVol;

    return 0;
}

WebRtc_Word32 AudioDeviceLinuxALSA::MinMicrophoneVolume(
    WebRtc_UWord32& minVolume) const
{
   WEBRTC_TRACE(kTraceModuleCall, kTraceAudioDevice, _id,
                "%s", __FUNCTION__);

    WebRtc_UWord32 minVol(0);

    if (_mixerManager.MinMicrophoneVolume(minVol) == -1)
    {
        return -1;
    }

    minVolume = minVol;

    return 0;
}

WebRtc_Word32 AudioDeviceLinuxALSA::MicrophoneVolumeStepSize(
    WebRtc_UWord16& stepSize) const
{
    WEBRTC_TRACE(kTraceModuleCall, kTraceAudioDevice, _id,
                 "%s", __FUNCTION__);

    WebRtc_UWord16 delta(0); 
        
    if (_mixerManager.MicrophoneVolumeStepSize(delta) == -1)
    {
        return -1;
    }

    stepSize = delta;

    return 0;
}

WebRtc_Word16 AudioDeviceLinuxALSA::PlayoutDevices()
{
    WEBRTC_TRACE(kTraceModuleCall, kTraceAudioDevice, _id,
                 "%s", __FUNCTION__);

    return (WebRtc_Word16)GetDevicesInfo(0, true);
}

WebRtc_Word32 AudioDeviceLinuxALSA::SetPlayoutDevice(WebRtc_UWord16 index)
{
    WEBRTC_TRACE(kTraceModuleCall, kTraceAudioDevice, _id,
                 "SetPlayoutDevice(index=%u)", index);

    if (_playIsInitialized)
    {
        return -1;
    }

    WebRtc_UWord32 nDevices = GetDevicesInfo(0, true);
    WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, _id,
                 "  number of availiable audio output devices is %u", nDevices);

    if (index > (nDevices-1))
    {
        WEBRTC_TRACE(kTraceError, kTraceAudioDevice, _id,
                     "  device index is out of range [0,%u]", (nDevices-1));
        return -1;
    }

    _outputDeviceIndex = index;
    _outputDeviceIsSpecified = true;

    return 0;
}

WebRtc_Word32 AudioDeviceLinuxALSA::SetPlayoutDevice(
    AudioDeviceModule::WindowsDeviceType /*device*/)
{
    WEBRTC_TRACE(kTraceError, kTraceAudioDevice, _id,
                 "WindowsDeviceType not supported");
    return -1;
}

WebRtc_Word32 AudioDeviceLinuxALSA::PlayoutDeviceName(
    WebRtc_UWord16 index,
    WebRtc_Word8 name[kAdmMaxDeviceNameSize],
    WebRtc_Word8 guid[kAdmMaxGuidSize])
{
    WEBRTC_TRACE(kTraceModuleCall, kTraceAudioDevice, _id,
               "PlayoutDeviceName(index=%u)", index);

    const WebRtc_UWord16 nDevices(PlayoutDevices());

    if ((index > (nDevices-1)) || (name == NULL))
    {
        return -1;
    }

    memset(name, 0, kAdmMaxDeviceNameSize);

    if (guid != NULL)
    {
        memset(guid, 0, kAdmMaxGuidSize);
    }

    return GetDevicesInfo(1, true, index, name, kAdmMaxDeviceNameSize);
}

WebRtc_Word32 AudioDeviceLinuxALSA::RecordingDeviceName(
    WebRtc_UWord16 index,
    WebRtc_Word8 name[kAdmMaxDeviceNameSize],
    WebRtc_Word8 guid[kAdmMaxGuidSize])
{
    WEBRTC_TRACE(kTraceModuleCall, kTraceAudioDevice, _id,
               "RecordingDeviceName(index=%u)", index);

    const WebRtc_UWord16 nDevices(RecordingDevices());

    if ((index > (nDevices-1)) || (name == NULL))
    {
        return -1;
    }

    memset(name, 0, kAdmMaxDeviceNameSize);

    if (guid != NULL)
    {
        memset(guid, 0, kAdmMaxGuidSize);
    }
    
    return GetDevicesInfo(1, false, index, name, kAdmMaxDeviceNameSize);
}

WebRtc_Word16 AudioDeviceLinuxALSA::RecordingDevices()
{
    WEBRTC_TRACE(kTraceModuleCall, kTraceAudioDevice, _id,
                 "%s", __FUNCTION__);

    return (WebRtc_Word16)GetDevicesInfo(0, false);
}

WebRtc_Word32 AudioDeviceLinuxALSA::SetRecordingDevice(WebRtc_UWord16 index)
{
    WEBRTC_TRACE(kTraceModuleCall, kTraceAudioDevice, _id,
                 "SetRecordingDevice(index=%u)", index);

    if (_recIsInitialized)
    {
        return -1;
    }

    WebRtc_UWord32 nDevices = GetDevicesInfo(0, false);
    WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, _id,
                 "  number of availiable audio input devices is %u", nDevices);

    if (index > (nDevices-1))
    {
        WEBRTC_TRACE(kTraceError, kTraceAudioDevice, _id,
                     "  device index is out of range [0,%u]", (nDevices-1));
        return -1;
    }

    _inputDeviceIndex = index;
    _inputDeviceIsSpecified = true;

    return 0;
}

// ----------------------------------------------------------------------------
//  SetRecordingDevice II (II)
// ----------------------------------------------------------------------------

WebRtc_Word32 AudioDeviceLinuxALSA::SetRecordingDevice(
    AudioDeviceModule::WindowsDeviceType /*device*/)
{
    WEBRTC_TRACE(kTraceError, kTraceAudioDevice, _id,
                 "WindowsDeviceType not supported");
    return -1;
}

WebRtc_Word32 AudioDeviceLinuxALSA::PlayoutIsAvailable(bool& available)
{
    WEBRTC_TRACE(kTraceModuleCall, kTraceAudioDevice, _id,
                 "%s", __FUNCTION__);
    
    available = false;

    // Try to initialize the playout side with mono
    // Assumes that user set num channels after calling this function
    _playChannels = 1;
    WebRtc_Word32 res = InitPlayout();

    // Cancel effect of initialization
    StopPlayout();

    if (res != -1)
    {
        available = true;
    }
    else
    {
        // It may be possible to play out in stereo
        res = StereoPlayoutIsAvailable(available);
        if (available)
        {
            // Then set channels to 2 so InitPlayout doesn't fail
            _playChannels = 2;
        }
    }
    
    return res;
}

WebRtc_Word32 AudioDeviceLinuxALSA::RecordingIsAvailable(bool& available)
{
    WEBRTC_TRACE(kTraceModuleCall, kTraceAudioDevice, _id,
                 "%s", __FUNCTION__);
    
    available = false;

    // Try to initialize the recording side with mono
    // Assumes that user set num channels after calling this function
    _recChannels = 1;
    WebRtc_Word32 res = InitRecording();

    // Cancel effect of initialization
    StopRecording();

    if (res != -1)
    {
        available = true;
    }
    else
    {
        // It may be possible to record in stereo
        res = StereoRecordingIsAvailable(available);
        if (available)
        {
            // Then set channels to 2 so InitPlayout doesn't fail
            _recChannels = 2;
        }
    }
    
    return res;
}

WebRtc_Word32 AudioDeviceLinuxALSA::InitPlayout()
{
    WEBRTC_TRACE(kTraceModuleCall, kTraceAudioDevice, _id,
                 "%s", __FUNCTION__);

    int errVal = 0;

    snd_pcm_uframes_t   numFrames = 0;
    snd_pcm_hw_params_t *paramsPlayout;

    CriticalSectionScoped lock(_critSect);
    if (_playing)
    {
        return -1;
    }

    if (!_outputDeviceIsSpecified)
    {
        return -1;
    }

    if (_playIsInitialized)
    {
        return 0;
    }
    // Initialize the speaker (devices might have been added or removed)
    if (InitSpeaker() == -1)
    {
        WEBRTC_TRACE(kTraceWarning, kTraceAudioDevice, _id,
                     "  InitSpeaker() failed");
    }

    // Start by closing any existing wave-output devices
    //
    if (_handlePlayout != NULL)
    {
        LATE(snd_pcm_close)(_handlePlayout);
        _handlePlayout=NULL;
        _playIsInitialized = false;
        if (errVal < 0)
        {
            WEBRTC_TRACE(kTraceError, kTraceAudioDevice, _id,
                         "  Error closing current playout sound device, error:"
                         " %s", LATE(snd_strerror)(errVal));
        }
    }

    // Open PCM device for playout
    char deviceName[kAdmMaxDeviceNameSize] = {0};
    GetDevicesInfo(2, true, _outputDeviceIndex, deviceName,
                   kAdmMaxDeviceNameSize);

    WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, _id,
                 "  InitPlayout open (%s)", deviceName);

    errVal = LATE(snd_pcm_open)
                 (&_handlePlayout,
                  deviceName,
                  SND_PCM_STREAM_PLAYBACK,
                  SND_PCM_NONBLOCK);

    if (errVal == -EBUSY) // Device busy - try some more!
    {
        for (int i=0; i < 5; i++)
        {
            sleep(1);
            errVal = LATE(snd_pcm_open)
                         (&_handlePlayout,
                          deviceName,
                          SND_PCM_STREAM_PLAYBACK,
                          SND_PCM_NONBLOCK);
            if (errVal == 0)
            {
                break;
            }
        }
    }
    if (errVal < 0)
    {
        WEBRTC_TRACE(kTraceError, kTraceAudioDevice, _id,
                     "     unable to open playback device: %s (%d)",
                     LATE(snd_strerror)(errVal),
                     errVal);
        _handlePlayout=NULL;
        return -1;
    }

    // Allocate hardware paramterers 
    errVal = LATE(snd_pcm_hw_params_malloc)(&paramsPlayout);
    if (errVal < 0)
    {
        WEBRTC_TRACE(kTraceError, kTraceAudioDevice, _id,
                     "     hardware params malloc, error: %s",
                     LATE(snd_strerror)(errVal));
        if (_handlePlayout)
        {
            LATE(snd_pcm_close)(_handlePlayout);
            _handlePlayout=NULL;
            if (errVal < 0)
            {
                WEBRTC_TRACE(kTraceError, kTraceAudioDevice, _id,
                             "     Error closing playout sound device, error: %s",
                             LATE(snd_strerror)(errVal));
            }
        }
        return -1;
    }

    errVal = LATE(snd_pcm_hw_params_any)(_handlePlayout, paramsPlayout);
    if (errVal < 0)
    {
        WEBRTC_TRACE(kTraceError, kTraceAudioDevice, _id,
                     "     hardware params_any, error: %s",
                     LATE(snd_strerror)(errVal));
        if (_handlePlayout)
        {
            LATE(snd_pcm_close)(_handlePlayout);
            _handlePlayout=NULL;
            if (errVal < 0)
            {
                WEBRTC_TRACE(kTraceError, kTraceAudioDevice, _id,
                             "     Error closing playout sound device, error: %s",
                             LATE(snd_strerror)(errVal));
            }
        }
        return -1;
    }

    // Set stereo sample order
    errVal = LATE(snd_pcm_hw_params_set_access)
                 (_handlePlayout,
                  paramsPlayout,
                  SND_PCM_ACCESS_RW_INTERLEAVED);
    if (errVal < 0)
    {
        WEBRTC_TRACE(kTraceError, kTraceAudioDevice, _id,
                     "     hardware params set access, error: %s",
                     LATE(snd_strerror)(errVal));
        if (_handlePlayout)
        {
            LATE(snd_pcm_close)(_handlePlayout);
            _handlePlayout=NULL;
            if (errVal < 0)
            {
                WEBRTC_TRACE(kTraceError, kTraceAudioDevice, _id,
                             "     Error closing playout sound device, error: %s",
                             LATE(snd_strerror)(errVal));
            }
        }
        return -1;
    }

    // Set sample format
#if defined(WEBRTC_BIG_ENDIAN)
    errVal = LATE(snd_pcm_hw_params_set_format)
                 (_handlePlayout,
                  paramsPlayout,
                  SND_PCM_FORMAT_S16_BE);
#else
    errVal = LATE(snd_pcm_hw_params_set_format)
                 (_handlePlayout,
                  paramsPlayout,
                  SND_PCM_FORMAT_S16_LE);
#endif
    if (errVal < 0)
    {
        WEBRTC_TRACE(kTraceError, kTraceAudioDevice, _id,
                     "     hardware params set format, error: %s",
                     LATE(snd_strerror)(errVal));
        if (_handlePlayout)
        {
            LATE(snd_pcm_close)(_handlePlayout);
            _handlePlayout=NULL;
            if (errVal < 0)
            {
                WEBRTC_TRACE(kTraceError, kTraceAudioDevice, _id,
                             "     Error closing playout sound device, error: %s",
                             LATE(snd_strerror)(errVal));
            }
        }
        return -1;
    }

    // Set stereo/mono
    errVal = LATE(snd_pcm_hw_params_set_channels)
                 (_handlePlayout,
                  paramsPlayout,
                  _playChannels);
    if (errVal < 0)
    {
        WEBRTC_TRACE(kTraceError, kTraceAudioDevice, _id,
                     "     hardware params set channels(%d), error: %s",
                     _playChannels,
                     LATE(snd_strerror)(errVal));

        if (_handlePlayout)
        {
            LATE(snd_pcm_close)(_handlePlayout);
            _handlePlayout = NULL;
            if (errVal < 0)
            {
                WEBRTC_TRACE(kTraceError, kTraceAudioDevice, _id,
                             "     Error closing playout sound device, error: %s",
                             LATE(snd_strerror)(errVal));
            }
        }
        return -1;
    }

    // Set sampling rate to use
    _samplingFreqPlay = PLAY_SAMPLES_PER_MS;
    WebRtc_UWord32 samplingRate = _samplingFreqPlay*1000;

    // Set sample rate
    unsigned int exactRate = samplingRate;
    errVal = LATE(snd_pcm_hw_params_set_rate_near)
                 (_handlePlayout,
                  paramsPlayout,
                  &exactRate,
                  0);
    if (errVal < 0)
    {
        WEBRTC_TRACE(kTraceError, kTraceAudioDevice, _id,
                     "     hardware params set rate near(%d), error: %s",
                     samplingRate,
                     LATE(snd_strerror)(errVal));
        if (_handlePlayout)
        {
            LATE(snd_pcm_close)(_handlePlayout);
            _handlePlayout=NULL;
            if (errVal < 0)
            {
                WEBRTC_TRACE(kTraceError, kTraceAudioDevice, _id,
                             "     Error closing playout sound device, error: %s",
                             LATE(snd_strerror)(errVal));
            }
        }
        return -1;
    }
    if (exactRate != samplingRate)
    {
        WEBRTC_TRACE(kTraceWarning, kTraceAudioDevice, _id,
                     "     Soundcard does not support sample rate %d Hz, %d Hz"
                     " used instead.",
                     samplingRate,
                     exactRate);

        // We use this rate instead
        _samplingFreqPlay = (WebRtc_UWord32)(exactRate / 1000);
    }

    // Set buffer size, in frames
    numFrames = ALSA_SNDCARD_BUFF_SIZE_PLAY;
    WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, _id,
                 "     set playout, numFrames: %d, bufer size: %d",
                 numFrames,
                 _playSndcardBuffsize);
    errVal = LATE(snd_pcm_hw_params_set_buffer_size_near)
                 (_handlePlayout,
                  paramsPlayout,
                  &_playSndcardBuffsize);
    if (errVal < 0)
    {
        WEBRTC_TRACE(kTraceError, kTraceAudioDevice, _id,
                     "     hardware params set buffer size near(%d), error: %s",
                     (int) numFrames,
                     LATE(snd_strerror)(errVal));
        if (_handlePlayout)
        {
            LATE(snd_pcm_close)(_handlePlayout);
            _handlePlayout = NULL;
            if (errVal < 0)
            {
                WEBRTC_TRACE(kTraceError, kTraceAudioDevice, _id,
                             "     Error closing playout sound device, error: %s",
                             LATE(snd_strerror)(errVal));
            }
        }
        return -1;
    }
    if (numFrames != _playSndcardBuffsize)
    {
        WEBRTC_TRACE(kTraceWarning, kTraceAudioDevice, _id,
                     "     Allocated record buffersize: %d frames",
                     (int)_playSndcardBuffsize);
    }

    // Write settings to the devices
    errVal = LATE(snd_pcm_hw_params)(_handlePlayout, paramsPlayout);
    if (errVal < 0)
    {
        WEBRTC_TRACE(kTraceError, kTraceAudioDevice, _id,
                     "     hardware params(_handlePlayout, paramsPlayout),"
                     " error: %s",
                     LATE(snd_strerror)(errVal));
        if (_handlePlayout)
        {
            LATE(snd_pcm_close)(_handlePlayout);
            _handlePlayout = NULL;
            if (errVal < 0)
            {
                WEBRTC_TRACE(kTraceError, kTraceAudioDevice, _id,
                             "     Error closing playout sound device, error: %s",
                             LATE(snd_strerror)(errVal));
            }
        }
        return -1;
    }

    // Free parameter struct memory
    LATE(snd_pcm_hw_params_free)(paramsPlayout);
    paramsPlayout = NULL;

    if (_ptrAudioBuffer)
    {
        // Update audio buffer with the selected parameters
        _ptrAudioBuffer->SetPlayoutSampleRate(_samplingFreqPlay*1000);
        _ptrAudioBuffer->SetPlayoutChannels((WebRtc_UWord8)_playChannels);
    }

    // Set play buffer size
    _playbackBufferSize = _samplingFreqPlay * 10 * _playChannels * 2;

    // Init varaibles used for play
    _previousSndCardPlayDelay = 0;
    _largeDelayCountPlay = 0;
    _delayMonitorStatePlay = 0;
    _bufferCheckMethodPlay = 0;
    _bufferCheckErrorsPlay = 0;
    _lastBufferCheckValuePlay = 0;
    _playWarning = 0;
    _playError = 0;

    if (_handlePlayout != NULL)
    {
        _playIsInitialized = true;
        return 0;
    }
    else 
    {
        return -1;
    }

    return 0;
}

WebRtc_Word32 AudioDeviceLinuxALSA::InitRecording()
{
    WEBRTC_TRACE(kTraceModuleCall, kTraceAudioDevice, _id,
                 "%s", __FUNCTION__);

    int errVal = 0;
    snd_pcm_uframes_t   numFrames = 0;
    snd_pcm_hw_params_t *paramsRecord;

    CriticalSectionScoped lock(_critSect);

    if (_recording)
    {
        return -1;
    }

    if (!_inputDeviceIsSpecified)
    {
        return -1;
    }

    if (_recIsInitialized)
    {
        return 0;
    }

    // Initialize the microphone (devices might have been added or removed)
    if (InitMicrophone() == -1)
    {
        WEBRTC_TRACE(kTraceWarning, kTraceAudioDevice, _id,
                   "  InitMicrophone() failed");
    }

    // Start by closing any existing pcm-input devices
    //
    if (_handleRecord != NULL)
    {
        int errVal = LATE(snd_pcm_close)(_handleRecord);
        _handleRecord = NULL;
        _recIsInitialized = false;
        if (errVal < 0)
        {
            WEBRTC_TRACE(kTraceError, kTraceAudioDevice, _id,
                         "     Error closing current recording sound device,"
                         " error: %s",
                         LATE(snd_strerror)(errVal));
        }
    }

    // Open PCM device for recording
    // The corresponding settings for playout are made after the record settings
    char deviceName[kAdmMaxDeviceNameSize] = {0};
    GetDevicesInfo(2, false, _inputDeviceIndex, deviceName,
                   kAdmMaxDeviceNameSize);

    WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, _id,
                 "InitRecording open (%s)", deviceName);
    errVal = LATE(snd_pcm_open)
                 (&_handleRecord,
                  deviceName,
                  SND_PCM_STREAM_CAPTURE,
                  SND_PCM_NONBLOCK);

    // Available modes: 0 = blocking, SND_PCM_NONBLOCK, SND_PCM_ASYNC
    if (errVal == -EBUSY) // Device busy - try some more!
    {
        for (int i=0; i < 5; i++)
        {
            sleep(1);
            errVal = LATE(snd_pcm_open)
                         (&_handleRecord,
                          deviceName,
                          SND_PCM_STREAM_CAPTURE,
                          SND_PCM_NONBLOCK);
            if (errVal == 0)
            {
                break;
            }
        }
    }
    if (errVal < 0)
    {
        WEBRTC_TRACE(kTraceError, kTraceAudioDevice, _id,
                     "    unable to open record device: %s",
                     LATE(snd_strerror)(errVal));
        _handleRecord = NULL;
        return -1;
    }

    // Allocate hardware paramterers
    errVal = LATE(snd_pcm_hw_params_malloc)(&paramsRecord);
    if (errVal < 0)
    {
        WEBRTC_TRACE(kTraceError, kTraceAudioDevice, _id,
                     "    hardware params malloc, error: %s",
                     LATE(snd_strerror)(errVal));
        if (_handleRecord)
        {
            errVal = LATE(snd_pcm_close)(_handleRecord);
            _handleRecord = NULL;
            if (errVal < 0)
            {
                WEBRTC_TRACE(kTraceError, kTraceAudioDevice, _id,
                             "     Error closing recording sound device, error: %s",
                             LATE(snd_strerror)(errVal));
            }
        }
        return -1;
    }

    errVal = LATE(snd_pcm_hw_params_any)(_handleRecord, paramsRecord);
    if (errVal < 0)
    {
        WEBRTC_TRACE(kTraceError, kTraceAudioDevice, _id,
                     "    hardware params any, error: %s",
                     LATE(snd_strerror)(errVal));
        if (_handleRecord)
        {
            errVal = LATE(snd_pcm_close)(_handleRecord);
            _handleRecord = NULL;
            if (errVal < 0)
            {
                WEBRTC_TRACE(kTraceError, kTraceAudioDevice, _id,
                             "     Error closing recording sound device, error:"
                             " %s", LATE(snd_strerror)(errVal));
            }
        }           
        return -1;
    }

    // Set stereo sample order
    errVal = LATE(snd_pcm_hw_params_set_access)
                 (_handleRecord,
                  paramsRecord,
                  SND_PCM_ACCESS_RW_INTERLEAVED);
    if (errVal < 0)
    {
        WEBRTC_TRACE(kTraceError, kTraceAudioDevice, _id,
                     "    harware params set access, error: %s",
                     LATE(snd_strerror)(errVal));
        if (_handleRecord)
        {
            errVal = LATE(snd_pcm_close)(_handleRecord);
            _handleRecord = NULL;
            if (errVal < 0)
            {
                WEBRTC_TRACE(kTraceError, kTraceAudioDevice, _id,
                             "     Error closing recording sound device, error:"
                             " %s", LATE(snd_strerror)(errVal));
            }
        }
        return -1;
    }

    // Set sample format
#if defined(WEBRTC_BIG_ENDIAN)
    errVal = LATE(snd_pcm_hw_params_set_format)
                 (_handleRecord,
                  paramsRecord,
                  SND_PCM_FORMAT_S16_BE);
#else
    errVal = LATE(snd_pcm_hw_params_set_format)
                 (_handleRecord,
                  paramsRecord,
                  SND_PCM_FORMAT_S16_LE);
#endif
    if (errVal < 0)
    {
        WEBRTC_TRACE(kTraceError, kTraceAudioDevice, _id,
                     "    harware params set format, error: %s",
                     LATE(snd_strerror)(errVal));
        if (_handleRecord)
        {
            errVal = LATE(snd_pcm_close)(_handleRecord);
            _handleRecord = NULL;
            if (errVal < 0)
            {
                WEBRTC_TRACE(kTraceError, kTraceAudioDevice, _id,
                             "     Error closing recording sound device,"
                             " error: %s",
                             LATE(snd_strerror)(errVal));
            }
        }
        return -1;
    }

    // Set stereo/mono
    errVal = LATE(snd_pcm_hw_params_set_channels)(
        _handleRecord, paramsRecord, _recChannels);
    if (errVal < 0)
    {
        WEBRTC_TRACE(kTraceError, kTraceAudioDevice, _id,
                     "    harware params set channels (%d), error: %s",
                     _recChannels,
                     LATE(snd_strerror)(errVal));
        if (_handleRecord)
        {
            errVal = LATE(snd_pcm_close)(_handleRecord);
            _handleRecord = NULL;
            if (errVal < 0)
            {
                WEBRTC_TRACE(kTraceError, kTraceAudioDevice, _id,
                             "     Error closing recording sound device, "
                             "error: %s",
                             LATE(snd_strerror)(errVal));
            }
        }
        return -1;
    }

    // Set sampling rate to use
    _samplingFreqRec = REC_SAMPLES_PER_MS;
    WebRtc_UWord32 samplingRate = _samplingFreqRec*1000;

    // Set sample rate
    unsigned int exactRate = samplingRate;
    errVal = LATE(snd_pcm_hw_params_set_rate_near)
                 (_handleRecord,
                  paramsRecord,
                  &exactRate,
                  0);
    if (errVal < 0)
    {
        WEBRTC_TRACE(kTraceError, kTraceAudioDevice, _id,
                     "    hardware params set rate near(%d), error: %s",
                     samplingRate,
                     LATE(snd_strerror)(errVal));
        if (_handleRecord)
        {
            errVal = LATE(snd_pcm_close)(_handleRecord);
            _handleRecord = NULL;
            if (errVal < 0)
            {
                WEBRTC_TRACE(kTraceError, kTraceAudioDevice, _id,
                             "     Error closing recording sound device,"
                             " error: %s",
                             LATE(snd_strerror)(errVal));
            }
        }
        return -1;
    }
    if (exactRate != samplingRate)
    {
        WEBRTC_TRACE(kTraceWarning, kTraceAudioDevice, _id,
                     "  Sound device does not support sample rate %d Hz, %d Hz"
                     " used instead.",
                     samplingRate,
                     exactRate);

        // We use this rate instead
        _samplingFreqRec = (WebRtc_UWord32)(exactRate / 1000);
    }

    // Set buffer size, in frames.
    numFrames = ALSA_SNDCARD_BUFF_SIZE_REC;
    WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, _id,
                 "     set record, numFrames: %d, buffer size: %d",
                 numFrames,
                 _recSndcardBuffsize);

    errVal = LATE(snd_pcm_hw_params_set_buffer_size_near)
                 (_handleRecord,
                  paramsRecord,
                  &_recSndcardBuffsize);
    if (errVal < 0)
    {
        WEBRTC_TRACE(kTraceError, kTraceAudioDevice, _id,
                     "    hardware params set buffer size near(%d), error: %s",
                     (int) numFrames,
                     LATE(snd_strerror)(errVal));

        if (_handleRecord)
        {
            errVal = LATE(snd_pcm_close)(_handleRecord);
            _handleRecord = NULL;
            if (errVal < 0)
            {
                WEBRTC_TRACE(kTraceError, kTraceAudioDevice, _id,
                             "     Error closing recording sound device, "
                             "error: %s",
                             LATE(snd_strerror)(errVal));
            }
        }
        return -1;
    }
    if (numFrames != _recSndcardBuffsize)
    {
        WEBRTC_TRACE(kTraceWarning, kTraceAudioDevice, _id,
                     "     Allocated record buffersize: %d frames",
                     (int)_recSndcardBuffsize);
    }

    // Write settings to the devices
    errVal = LATE(snd_pcm_hw_params)(_handleRecord, paramsRecord);
    if (errVal < 0)
    {
        WEBRTC_TRACE(kTraceError, kTraceAudioDevice, _id,
                     "    hardware params, error: %s",
                     LATE(snd_strerror)(errVal));
        if (_handleRecord)
        {
            errVal = LATE(snd_pcm_close)(_handleRecord);
            _handleRecord = NULL;
            if (errVal < 0)
            {
                WEBRTC_TRACE(kTraceError, kTraceAudioDevice, _id,
                             "     Error closing recording sound device, error: %s",
                             LATE(snd_strerror)(errVal));
            }
        }
        return -1;
    }

    // Free prameter struct memory
    LATE(snd_pcm_hw_params_free)(paramsRecord);
    paramsRecord = NULL;

    if (_ptrAudioBuffer)
    {
        // Update audio buffer with the selected parameters
        _ptrAudioBuffer->SetRecordingSampleRate(_samplingFreqRec*1000);
        _ptrAudioBuffer->SetRecordingChannels((WebRtc_UWord8)_recChannels);
    }

    // Set rec buffer size and create buffer
    _recordBufferSize = _samplingFreqRec * 10 * _recChannels * 2;
    _recBuffer = new WebRtc_Word16[_recordBufferSize / 2];

    // Init rec varaibles
    _bufferCheckMethodRec = 0;
    _bufferCheckErrorsRec = 0;

    if (_handleRecord != NULL)
    {
        // Mark recording side as initialized
        _recIsInitialized = true;
        return 0;
    }
    else
    {
        return -1;
    }

    return 0;
}

WebRtc_Word32 AudioDeviceLinuxALSA::StartRecording()
{
    WEBRTC_TRACE(kTraceModuleCall, kTraceAudioDevice, _id,
                 "%s", __FUNCTION__);

    if (!_recIsInitialized)
    {
        return -1;
    }
    
    if (_recording)
    {
        return 0;
    }

    // prepare and start the recording
    int errVal=0;
    errVal = LATE(snd_pcm_prepare)(_handleRecord);
    if (errVal<0)
    {
        WEBRTC_TRACE(kTraceError, kTraceAudioDevice, _id,
                     "     cannot prepare audio record interface for use (%s)\n",
                     LATE(snd_strerror)(errVal));
        return -1;
    }
        
    errVal = LATE(snd_pcm_start)(_handleRecord);
    if (errVal < 0)
    {
        WEBRTC_TRACE(kTraceError, kTraceAudioDevice, _id,
                     "     Error starting record interface: %s",
                     LATE(snd_strerror)(errVal));
        return -1;
    }

/*
        // DEBUG: Write info about PCM
        snd_output_t *output = NULL;
        errVal = LATE(snd_output_stdio_attach)(&output, stdout, 0);
        if (errVal < 0) {
                printf("Output failed: %s\n", snd_strerror(errVal));
                return 0;
        }
        LATE(snd_pcm_dump)(_handleRecord, output);
*/

    // set state to ensure that the recording starts from the audio thread
    _startRec = true;

    // the audio thread will signal when recording has stopped
    if (kEventTimeout == _recStartEvent.Wait(10000))
    {
        _startRec = false;
        StopRecording();
        WEBRTC_TRACE(kTraceError, kTraceAudioDevice, _id,
                     "  failed to activate recording");
        return -1;
    }

    if (_recording)
    {
        // the recording state is set by the audio thread after recording has
        // started
        WEBRTC_TRACE(kTraceModuleCall, kTraceAudioDevice, _id,
                     "  recording is now active");
    }
    else
    {
        WEBRTC_TRACE(kTraceError, kTraceAudioDevice, _id,
                     "  failed to activate recording");
        return -1;
    }

    return 0;
}

WebRtc_Word32 AudioDeviceLinuxALSA::StopRecording()
{
    WEBRTC_TRACE(kTraceModuleCall, kTraceAudioDevice, _id,
                 "%s", __FUNCTION__);

    CriticalSectionScoped lock(_critSect);

    if (!_recIsInitialized)
    {
        return 0;
    }

    if (_handleRecord == NULL)
    {
        return -1;
    }

    // make sure we don't start recording (it's asynchronous), assuming that
    // we are under lock
    _startRec = false;

    // stop and close pcm recording device
    int errVal = LATE(snd_pcm_drop)(_handleRecord);
    if (errVal < 0)
    {
        WEBRTC_TRACE(kTraceError, kTraceAudioDevice, _id,
                     "     Error stop recording: %s",
                     LATE(snd_strerror)(errVal));
    }

    errVal = LATE(snd_pcm_close)(_handleRecord);
    if (errVal < 0)
    {
        WEBRTC_TRACE(kTraceError, kTraceAudioDevice, _id,
                     "     Error closing record sound device, error: %s",
                     LATE(snd_strerror)(errVal));
    }
    
    // check if we have muted and unmute if so
    bool muteEnabled = false;
    MicrophoneMute(muteEnabled);
    if (muteEnabled)
    {
        SetMicrophoneMute(false);
    }

    _recIsInitialized = false;
    _recording = false;

    // set the pcm input handle to NULL
    _handleRecord = NULL;
    WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, _id,
                 "  _handleRecord is now set to NULL");

    // delete the rec buffer
    if (_recBuffer)
    {
        delete _recBuffer;
        _recBuffer = NULL;
    }

    return 0;
}

bool AudioDeviceLinuxALSA::RecordingIsInitialized() const
{
    WEBRTC_TRACE(kTraceModuleCall, kTraceAudioDevice, _id,
                 "%s", __FUNCTION__);
    return (_recIsInitialized);
}

bool AudioDeviceLinuxALSA::Recording() const
{
    WEBRTC_TRACE(kTraceModuleCall, kTraceAudioDevice, _id,
                 "%s", __FUNCTION__);
    return (_recording);
}

bool AudioDeviceLinuxALSA::PlayoutIsInitialized() const
{
    WEBRTC_TRACE(kTraceModuleCall, kTraceAudioDevice, _id,
                 "%s", __FUNCTION__);
    return (_playIsInitialized);
}

WebRtc_Word32 AudioDeviceLinuxALSA::StartPlayout()
{
    WEBRTC_TRACE(kTraceModuleCall, kTraceAudioDevice, _id,
                 "%s", __FUNCTION__);
    if (!_playIsInitialized)
    {
        return -1;
    }
    
    if (_playing)
    {
        return 0;
    }
    // prepare playout
    int errVal=0;
    errVal = LATE(snd_pcm_prepare)(_handlePlayout);
    if (errVal<0)
    {
        WEBRTC_TRACE(kTraceError, kTraceAudioDevice, _id,
                     "     cannot prepare audio playout interface for use: %s",
                     LATE(snd_strerror)(errVal));
        return -1;
    }

    // Don't call snd_pcm_start here, it will start implicitly at first write.
/*
        // DEBUG: Write info about PCM
        snd_output_t *output = NULL;
        errVal = LATE(snd_output_stdio_attach)(&output, stdout, 0);
        if (errVal < 0) {
                printf("Output failed: %s\n", snd_strerror(errVal));
                return 0;
        }
        LATE(snd_pcm_dump)(_handlePlayout, output);
*/

    // set state to ensure that playout starts from the audio thread
    _startPlay = true;

    // the audio thread will signal when recording has started
    if (kEventTimeout == _playStartEvent.Wait(10000))
    {
        _startPlay = false;
        StopPlayout();
        WEBRTC_TRACE(kTraceError, kTraceAudioDevice, _id,
                     "  failed to activate playout");
        return -1;
    }

    if (_playing)
    {
        // the playing state is set by the audio thread after playout has started
        WEBRTC_TRACE(kTraceModuleCall, kTraceAudioDevice, _id,
                     "  playing is now active");
    }
    else
    {
        WEBRTC_TRACE(kTraceError, kTraceAudioDevice, _id,
                     "  failed to activate playing");
        return -1;
    }

    return 0;
}

WebRtc_Word32 AudioDeviceLinuxALSA::StopPlayout()
{
    WEBRTC_TRACE(kTraceModuleCall, kTraceAudioDevice, _id,
                 "%s", __FUNCTION__);

    CriticalSectionScoped lock(_critSect);

    if (!_playIsInitialized)
    {
        return 0;
    }

    if (_handlePlayout == NULL)
    {
        return -1;
    }

    _playIsInitialized = false;
    _playing = false;

    // stop and close pcm playout device
    int errVal = LATE(snd_pcm_drop)(_handlePlayout);
    if (errVal < 0)
    {
        WEBRTC_TRACE(kTraceError, kTraceAudioDevice, _id,
                     "     Error stop playing: %s",
                     LATE(snd_strerror)(errVal));
    }

    errVal = LATE(snd_pcm_close)(_handlePlayout);
    if (errVal < 0)
    {
        WEBRTC_TRACE(kTraceError, kTraceAudioDevice, _id,
                     "     Error closing playout sound device, error: %s",
                     LATE(snd_strerror)(errVal));
    }

    // set the pcm input handle to NULL
    _handlePlayout = NULL;
    WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, _id,
                 "  _handlePlayout is now set to NULL");

    return 0;
}

WebRtc_Word32 AudioDeviceLinuxALSA::PlayoutDelay(WebRtc_UWord16& delayMS) const
{
    delayMS = (WebRtc_UWord16)_sndCardPlayDelay;
    return 0;
}

WebRtc_Word32 AudioDeviceLinuxALSA::RecordingDelay(WebRtc_UWord16& delayMS) const
{
    delayMS = (WebRtc_UWord16)_sndCardRecDelay;
    return 0;
}

bool AudioDeviceLinuxALSA::Playing() const
{
    WEBRTC_TRACE(kTraceModuleCall, kTraceAudioDevice, _id,
                 "%s", __FUNCTION__);
    return (_playing);
}
// ----------------------------------------------------------------------------
//  SetPlayoutBuffer
// ----------------------------------------------------------------------------

WebRtc_Word32 AudioDeviceLinuxALSA::SetPlayoutBuffer(
    const AudioDeviceModule::BufferType type,
    WebRtc_UWord16 sizeMS)
{
    WEBRTC_TRACE(kTraceModuleCall, kTraceAudioDevice, _id,
                 "SetPlayoutBuffer(type=%u, sizeMS=%u)", type, sizeMS);
    _playBufType = type;
    if (type == AudioDeviceModule::kFixedBufferSize)
    {
        _playBufDelayFixed = sizeMS;
    }
    return 0;
}

WebRtc_Word32 AudioDeviceLinuxALSA::PlayoutBuffer(
    AudioDeviceModule::BufferType& type,
    WebRtc_UWord16& sizeMS) const
{
    WEBRTC_TRACE(kTraceModuleCall, kTraceAudioDevice, _id,
                 "%s", __FUNCTION__);
    type = _playBufType;
    if (type == AudioDeviceModule::kFixedBufferSize)
    {
        sizeMS = _playBufDelayFixed; 
    }
    else
    {
        sizeMS = _playBufDelay; 
    }

    return 0;
}

WebRtc_Word32 AudioDeviceLinuxALSA::CPULoad(WebRtc_UWord16& load) const
{
    WEBRTC_TRACE(kTraceModuleCall, kTraceAudioDevice, _id,
                 "%s", __FUNCTION__);

    WEBRTC_TRACE(kTraceWarning, kTraceAudioDevice, _id,
               "  API call not supported on this platform");
    return -1;
}

bool AudioDeviceLinuxALSA::PlayoutWarning() const
{
    return (_playWarning > 0);
}

bool AudioDeviceLinuxALSA::PlayoutError() const
{
    return (_playError > 0);
}

bool AudioDeviceLinuxALSA::RecordingWarning() const
{
    return (_recWarning > 0);
}

bool AudioDeviceLinuxALSA::RecordingError() const
{
    return (_recError > 0);
}

void AudioDeviceLinuxALSA::ClearPlayoutWarning()
{
    _playWarning = 0;
}

void AudioDeviceLinuxALSA::ClearPlayoutError()
{
    _playError = 0;
}

void AudioDeviceLinuxALSA::ClearRecordingWarning()
{
    _recWarning = 0;
}

void AudioDeviceLinuxALSA::ClearRecordingError()
{
    _recError = 0;
}

// ============================================================================
//                                 Private Methods
// ============================================================================

WebRtc_Word32 AudioDeviceLinuxALSA::GetDevicesInfo(
    const WebRtc_Word32 function,
    const bool playback,
    const WebRtc_Word32 enumDeviceNo,
    char* enumDeviceName,
    const WebRtc_Word32 ednLen) const
{
    WEBRTC_TRACE(kTraceModuleCall, kTraceAudioDevice, _id,
                 "%s", __FUNCTION__);
    
    // Device enumeration based on libjingle implementation
    // by Tristan Schmelcher at Google Inc.

    const char *type = playback ? "Output" : "Input";
    // dmix and dsnoop are only for playback and capture, respectively, but ALSA
    // stupidly includes them in both lists.
    const char *ignorePrefix = playback ? "dsnoop:" : "dmix:" ;
    // (ALSA lists many more "devices" of questionable interest, but we show them
    // just in case the weird devices may actually be desirable for some
    // users/systems.)

    int err;
    int enumCount(0);
    bool keepSearching(true);

    void **hints;
    err = LATE(snd_device_name_hint)(-1,     // All cards
                                     "pcm",  // Only PCM devices
                                     &hints);
    if (err != 0)
    {
        WEBRTC_TRACE(kTraceError, kTraceAudioDevice, _id,
                     "GetDevicesInfo - device name hint error: %s",
                     LATE(snd_strerror)(err));
        return -1;
    }

    for (void **list = hints; *list != NULL; ++list)
    {
        char *actualType = LATE(snd_device_name_get_hint)(*list, "IOID");
        if (actualType)
        {   // NULL means it's both.
            bool wrongType = (strcmp(actualType, type) != 0);
            free(actualType);
            if (wrongType)
            {
                // Wrong type of device (i.e., input vs. output).
                continue;
            }
        }

        char *name = LATE(snd_device_name_get_hint)(*list, "NAME");
        if (!name)
        {
            WEBRTC_TRACE(kTraceError, kTraceAudioDevice, _id,
                         "Device has no name");
            // Skip it.
            continue;
        }

        // Now check if we actually want to show this device.
        if (strcmp(name, "default") != 0 &&
            strcmp(name, "null") != 0 &&
            strcmp(name, "pulse") != 0 &&
            strncmp(name, ignorePrefix, strlen(ignorePrefix)) != 0)
        {
            // Yes, we do.
            char *desc = LATE(snd_device_name_get_hint)(*list, "DESC");
            if (!desc)
            {
                // Virtual devices don't necessarily have descriptions.
                // Use their names instead
                desc = name;
            }

            if (0 == function)
            {
                WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, _id,
                             "    Enum device %d - %s", enumCount, name);

            }
            if ((1 == function) && (enumDeviceNo == enumCount))
            {

                // We have found the enum device, copy the name to buffer
                strncpy(enumDeviceName, desc, ednLen);
                enumDeviceName[ednLen-1] = '\0';
                keepSearching = false;
            }
            if ((2 == function) && (enumDeviceNo == enumCount))
            {
                // We have found the enum device, copy the name to buffer
                strncpy(enumDeviceName, name, ednLen);
                enumDeviceName[ednLen-1] = '\0';
                keepSearching = false;
            }
            if (keepSearching)
            {
                ++enumCount;
            }

            if (desc != name)
            {
                free(desc);
            }
        }

        free(name);

        if (!keepSearching)
        {
            break;
        }
    }

    err = LATE(snd_device_name_free_hint)(hints);
    if (err != 0)
    {
        WEBRTC_TRACE(kTraceError, kTraceAudioDevice, _id,
                     "GetDevicesInfo - device name free hint error: %s",
                     LATE(snd_strerror)(err));
        // Continue and return true anyways, since we did get the whole list.
    }

    if (0 == function)
    {
        return enumCount; // Normal return point for function 0
    }

    if (keepSearching)
    {
        // If we get here for function 1 and 2, we didn't find the specified
        // enum device
        WEBRTC_TRACE(kTraceError, kTraceAudioDevice, _id,
                     "GetDevicesInfo - Could not find device name or numbers");
        return -1;
    }

    return 0;
}

void AudioDeviceLinuxALSA::FillPlayoutBuffer()
{
    WEBRTC_TRACE(kTraceDebug, kTraceAudioDevice, _id,
                 "Filling playout buffer");

    WebRtc_Word32 sizeBytes = _playbackBufferSize;
    WebRtc_Word32 blockFrames = sizeBytes / (2 * _playChannels);
    WebRtc_Word16 sendoutOnCard[sizeBytes / 2];
    WebRtc_Word32 samplingFreq = _samplingFreqPlay * 1000;

    if (samplingFreq == 44000)
    {
        // Convert to sndcard samplerate
        samplingFreq = 44100;
    }

    memset(sendoutOnCard, 0, sizeBytes);

    int maxWrites = 3;
    int avail = blockFrames+1;
    if (0 == _bufferCheckMethodPlay)
    {
        // Normal case
        maxWrites = (_playSndcardBuffsize / samplingFreq) / 10 + 3;
        avail = LATE(snd_pcm_avail_update)(_handlePlayout);
    }

    while ((avail >= blockFrames) && (maxWrites > 0))
    {
        int written = LATE(snd_pcm_writei)
                          (_handlePlayout,
                           sendoutOnCard,
                           blockFrames);

        if (written != blockFrames)
        {
            if (written < 0)
            {
                WEBRTC_TRACE(kTraceWarning, kTraceAudioDevice, _id,
                             "    Error writing to sound device (1), error: %s",
                             LATE(snd_strerror)(written));
            }
            else
            {
                int remainingFrames = (blockFrames-written);
                WEBRTC_TRACE(kTraceStream, kTraceAudioDevice, _id,
                             "Written %d playout frames to soundcard, trying to "
                             "write the remaining %d frames",
                             written, remainingFrames);

                written = LATE(snd_pcm_writei)
                              (_handlePlayout,
                               &sendoutOnCard[written*2],
                               remainingFrames);

                if( written == remainingFrames )
                {
                    WEBRTC_TRACE(kTraceStream, kTraceAudioDevice,
                                 _id,  "     %d frames were written",
                               written);
                    written = blockFrames;
                }
                else
                {
                    WEBRTC_TRACE(kTraceWarning,
                                 kTraceAudioDevice, _id,
                                 "     Error writing to sound device (2),"
                                 " error: %s",
                                 LATE(snd_strerror)(written));

                    // Try to recover
                    ErrorRecovery(written, _handlePlayout);
                }
            }
        }

        --maxWrites;
        if (0 == _bufferCheckMethodPlay)
        {
            avail = LATE(snd_pcm_avail_update)(_handlePlayout);
            WEBRTC_TRACE(kTraceDebug, kTraceAudioDevice, _id,
                         "  snd_pcm_avail_update returned %d", avail);
        }
    }

    // Write one extra so that we push the buffer full
    LATE(snd_pcm_writei)(_handlePlayout, sendoutOnCard, blockFrames);
    avail = LATE(snd_pcm_avail_update)(_handlePlayout);
    WEBRTC_TRACE(kTraceDebug, kTraceAudioDevice, _id,
                 "  snd_pcm_avail_update returned %d", avail);
}

WebRtc_Word32 AudioDeviceLinuxALSA::InputSanityCheckAfterUnlockedPeriod() const
{
    if (_handleRecord == NULL)
    {
        WEBRTC_TRACE(kTraceError, kTraceAudioDevice, _id,
                     "  input state has been modified during unlocked period");
        return -1;
    }
    return 0;
}

WebRtc_Word32 AudioDeviceLinuxALSA::OutputSanityCheckAfterUnlockedPeriod() const
{
    if (_handlePlayout == NULL)
    {
        WEBRTC_TRACE(kTraceError, kTraceAudioDevice, _id,
                     "  output state has been modified during unlocked period");
        return -1;
    }
    return 0;
}

WebRtc_Word32 AudioDeviceLinuxALSA::PrepareStartRecording()
{
    WebRtc_Word32 res(0);
    snd_pcm_sframes_t delayInFrames(0);

    // Check if mic is muted
    bool muteEnabled = false;
    MicrophoneMute(muteEnabled);
    if (muteEnabled)
    {
        SetMicrophoneMute(false);
    }

    // Check delay and available frames before reset
    delayInFrames = -1;
    res = LATE(snd_pcm_delay)(_handleRecord, &delayInFrames);
    res = LATE(snd_pcm_avail_update)(_handleRecord);
    WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, _id,
                 "Before reset: delayInFrames = %d, available frames = %d",
                 delayInFrames, res);

    // Reset pcm
    res = LATE(snd_pcm_reset)(_handleRecord);
    if (res < 0 )
    {
        WEBRTC_TRACE(kTraceWarning, kTraceAudioDevice, _id,
                     "Error resetting pcm: %s (%d)",
                     LATE(snd_strerror)(res), res);
    }

    // Check delay and available frames after reset
    delayInFrames = -1;
    res = LATE(snd_pcm_delay)(_handleRecord, &delayInFrames);
    res = LATE(snd_pcm_avail_update)(_handleRecord);
    WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, _id,
                 "After reset: delayInFrames = %d, available frames = %d "
                 "(rec buf size = %u)",
                 delayInFrames, res, _recSndcardBuffsize);

    if (res < 0)
    {
        res = 0;
    }

    if (delayInFrames < 0)
    {
        delayInFrames = 0;
    }

     // True if the driver gives the actual number of frames in the buffer (normal case).
    // Cast is safe after check above.
    _buffersizeFromZeroAvail = (unsigned int)res < (_recSndcardBuffsize/2);
    _buffersizeFromZeroDelay = (unsigned int)delayInFrames < (_recSndcardBuffsize/2);

    return 0;
}

WebRtc_Word32 AudioDeviceLinuxALSA::GetPlayoutBufferDelay()
{
    WebRtc_Word32  msPlay(0);
    WebRtc_Word32  res(0);
    WebRtc_UWord32 samplesPerMs = _samplingFreqPlay;

    snd_pcm_sframes_t delayInFrames(0);

    // Check how much is in playout buffer and check delay
    if (0 == _bufferCheckMethodPlay)
    {
        // Using snd_pcm_avail_update for checking buffer is the method that
        // shall be used according to documentation. If we however detect that
        // returned available buffer is larger than the buffer size, we switch
        // to using snd_pcm_delay. See -391.

        // Get delay - distance between current application frame position and
        // sound frame position.
        // This is only used for giving delay measurement to VE.
        bool calcDelayFromAvail = false;
        res = LATE(snd_pcm_delay)(_handlePlayout, &delayInFrames);
        if (res < 0)
        {
            _writeErrors++;
            if ( _writeErrors > 50 )
            {
                if (_playError == 1)
                {
                    WEBRTC_TRACE(kTraceWarning,
                                 kTraceAudioDevice, _id,
                                 "  pending playout error exists");
                }
                _playError = 1;  // triggers callback from module process thread
                WEBRTC_TRACE(kTraceError, kTraceAudioDevice, _id,
                             "  kPlayoutError message posted: _writeErrors=%u",
                             _writeErrors);
            }   
                
            WEBRTC_TRACE(kTraceWarning, kTraceAudioDevice, _id,
                         "LinuxALSASndCardStream::playThreadProcess(), "
                         "snd_pcm_delay error (1): %s (%d)",
                         LATE(snd_strerror)(res), res);
            calcDelayFromAvail = true;
            ErrorRecovery(res, _handlePlayout);
            _delayMonitorStatePlay = 1; // Go to delay monitor state
            WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, _id,
                         "    Going to delay monitor state");
        }
        else
        {
            _writeErrors=0;
            _sndCardPlayDelay = delayInFrames / samplesPerMs;
        }
        
        // Check if we should write more data to the soundcard. Updates
        // the r/w pointer.
        int avail = LATE(snd_pcm_avail_update)(_handlePlayout);
       if (avail < 0)
        {
            WEBRTC_TRACE(kTraceError, kTraceAudioDevice, _id,
                         "LinuxALSASndCardStream::playThreadProcess(),"
                         " snd_pcm_avail_update error: %s (%d)",
                         LATE(snd_strerror)(avail), avail);
            res = ErrorRecovery(avail, _handlePlayout);
            if (avail == -EPIPE)
            {
                res = LATE(snd_pcm_prepare)(_handlePlayout);
                if (res < 0)
                {
                    WEBRTC_TRACE(kTraceError, kTraceAudioDevice,
                                 _id, "ErrorRecovery failed: %s",
                                 LATE(snd_strerror)(res));
                }
                FillPlayoutBuffer();
                msPlay = 0;
            }
            else
            {
                msPlay = 25;
            }
            WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, _id,
                         "   Guessed ms in playout buffer = %d", msPlay);
            _delayMonitorStatePlay = 1; // Go to delay monitor state
            WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, _id,
                         "    Going to delay monitor state");
        }
        else
        {
            // Calculate filled part of playout buffer size in ms
            // Safe since _playSndcardBuffsize is a small number
            int pb = (int)_playSndcardBuffsize;
            assert(pb >= 0);
            // If avail_update returns a value larger than playout buffer and it
            // doesn't keep decreasing we switch method of checking the buffer.
            if ((avail > pb) && (avail >= _lastBufferCheckValuePlay))
            {
                msPlay = 0; // Continue to write to buffer
                ++_bufferCheckErrorsPlay;
                WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, _id,
                             "    _bufferCheckErrorsPlay = %d",
                             _bufferCheckErrorsPlay);
                if (_bufferCheckErrorsPlay > 50)
                {
                    WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice,
                                 _id,
                                 "    Switching to delay buffer check method "
                                 "for playout");
                    _bufferCheckMethodPlay = 1; // Switch to using snd_pcm_delay
                    _bufferCheckErrorsPlay = 0;
                }
            }
            else
            {
                msPlay = pb > avail ? (pb - avail) / samplesPerMs : 0;
                _bufferCheckErrorsPlay = 0;
            }
            _lastBufferCheckValuePlay = avail;
        }
        
        if (calcDelayFromAvail)
        {
            _sndCardPlayDelay = msPlay;
        }
        // Here we monitor the delay value if we had an error
        if (0 == _delayMonitorStatePlay)
        {
            // Normal state, just store delay value
            _previousSndCardPlayDelay = _sndCardPlayDelay;
        }
        else if (1 == _delayMonitorStatePlay)
        {
            // We had an error, check if we get stuck in a long delay in playout.
            // If so, restart device completely. Workaround for PulseAudio.
            if ((_sndCardPlayDelay > 200) &&
                ((_sndCardPlayDelay > _previousSndCardPlayDelay * 2) ||
                (_sndCardPlayDelay > _previousSndCardPlayDelay + 200)))
            {
                if (_largeDelayCountPlay < 0) _largeDelayCountPlay = 0;
                ++_largeDelayCountPlay;
                WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, _id,
                             "    _largeDelayCountPlay = %d",
                             _largeDelayCountPlay);
                if (_largeDelayCountPlay > 50)
                {
                    WEBRTC_TRACE(kTraceWarning,
                                 kTraceAudioDevice, _id,
                                 "    Detected stuck in long delay after error "
                                 "- restarting playout device");
                    WEBRTC_TRACE(kTraceDebug, kTraceAudioDevice,
                                 _id,
                                 "    _previousSndCardPlayDelay = %d,"
                                 " _sndCardPlayDelay = %d",
                                 _previousSndCardPlayDelay, _sndCardPlayDelay);
                    StopPlayout();
                    InitPlayout();
                    res = LATE(snd_pcm_prepare)(_handlePlayout);
                    if (res < 0)
                    {
                        WEBRTC_TRACE(kTraceError,
                                     kTraceAudioDevice, _id,
                                     "     Cannot prepare audio playout "
                                     "interface for use: %s (%d)",
                                     LATE(snd_strerror)(res), res);
                    }
                    FillPlayoutBuffer();
                    _startPlay = true;
                    _delayMonitorStatePlay = 0;
                    _largeDelayCountPlay = 0;
                    // Make sure we only restart the device once. We could have had
                    // an error due to e.g. changed sink route in PulseAudio which would correctly
                    // lead to a larger delay. In this case we shouldn't get stuck restarting.
                    _previousSndCardPlayDelay = _sndCardPlayDelay;
                    return -1;
                }
            }
            else
            {
                // No error, keep count of OK tests
                if (_largeDelayCountPlay > 0) _largeDelayCountPlay = 0;
                --_largeDelayCountPlay;
                if (_largeDelayCountPlay < -50)
                {
                    // After a couple of OK monitor tests, go back to normal state
                    _delayMonitorStatePlay = 0;
                    _largeDelayCountPlay = 0;
                    WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice,
                                 _id,  "    Leaving delay monitor state");
                    WEBRTC_TRACE(kTraceDebug, kTraceAudioDevice,
                                 _id,
                                 "    _previousSndCardPlayDelay = %d,"
                                 " _sndCardPlayDelay = %d",
                                 _previousSndCardPlayDelay, _sndCardPlayDelay);
                }
            }
        }
        else
        {
            // Should never happen
            assert(false);
        }
   }
    else if (1 == _bufferCheckMethodPlay)
    {
        // Check if we should write more data to the soundcard
        // alternative method to get the delay (snd_pcm_avail_update() seem to
        // give unreliable vaules in some cases!, i.e. with dmix) <- TL
        // distance between current application frame position and sound frame
        // position
        res = LATE(snd_pcm_delay)(_handlePlayout, &delayInFrames);
        if (res < 0 || res > (int)_playSndcardBuffsize)
        {
            int recoveryRes = ErrorRecovery(res, _handlePlayout);
            if (res == -EPIPE)
            {
                WEBRTC_TRACE(kTraceError, kTraceAudioDevice, _id,
                             "LinuxALSASndCardStream::playThreadProcess(), "
                             "outbuffer underrun");
                if (recoveryRes < 0)
                {
                WEBRTC_TRACE(kTraceError, kTraceAudioDevice, _id,
                             "ErrorRecovery failed: %s",
                             LATE(snd_strerror)(res));
                }
                msPlay = 0;
            }
            else
            {
                _writeErrors++;
                if (_writeErrors > 50)
                {
                    if (_playError == 1)
                    {
                        WEBRTC_TRACE(kTraceWarning,
                                     kTraceAudioDevice, _id,
                                     "  pending playout error exists");
                    }
                    _playError = 1;  // triggers callback from module process thread
                    WEBRTC_TRACE(kTraceError, kTraceAudioDevice,
                                 _id,
                                 "  kPlayoutError message posted:"
                                 " _writeErrors=%u", _writeErrors);
                }   
            
                WEBRTC_TRACE(kTraceWarning, kTraceAudioDevice,
                             _id,
                             "LinuxALSASndCardStream::playThreadProcess(),"
                             " snd_pcm_delay error (2): %s (%d)",
                           LATE(snd_strerror)(res), res);
                WEBRTC_TRACE(kTraceError, kTraceAudioDevice, _id,
                             "Playout buffer size=%d", _playSndcardBuffsize);
                msPlay = 25;
                WEBRTC_TRACE(kTraceStream, kTraceAudioDevice, _id,
                             "   Guessed ms in playout buffer = %d", msPlay);
            }
        }
        else
        {   
            _writeErrors = 0;
            msPlay = delayInFrames / samplesPerMs; // playout buffer delay in ms
            _sndCardPlayDelay = msPlay;
        }
    }
    else
    {
        // Unknown _bufferCheckMethodPlay value, should never happen
        assert(false);    
    }

    /*
        delayInFrames = -1;
        snd_pcm_delay(_handlePlayout, &delayInFrames);
       // DEBUG END
*/
    
    WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, _id,
                 "msplay = %d", msPlay);
    return msPlay;
}

WebRtc_Word32 AudioDeviceLinuxALSA::GetRecordingBufferDelay(bool preRead)
{ 
    WebRtc_Word32  msRec(0);
    WebRtc_Word32  res(0);
    WebRtc_UWord32 samplesPerMs = _samplingFreqRec;

    snd_pcm_sframes_t delayInFrames(0);

    if ((0 == _bufferCheckMethodRec) || (1 == _bufferCheckMethodRec))
    {
        // Get delay, only used for input to VE
        bool calcDelayFromAvail = false;
        res = LATE(snd_pcm_delay)(_handleRecord, &delayInFrames);
        if (res < 0)
        {
            WEBRTC_TRACE(kTraceWarning, kTraceAudioDevice, _id,
                         "LinuxALSASndCardStream::recThreadfun(),"
                         " snd_pcm_delay (3) error: %s (%d)",
                         LATE(snd_strerror)(res), res);
            ErrorRecovery(res, _handleRecord);
            calcDelayFromAvail = true; // Must get estimate below instead
        }
        else if (0 == _bufferCheckMethodRec)
        {
            if (_buffersizeFromZeroDelay)
            {
                // Normal case
                _sndCardRecDelay = delayInFrames / samplesPerMs;
            }
            else
            {
                // Safe since _recSndcardBuffsize is a small number
                int rb = (int)_recSndcardBuffsize;
                assert(rb >= 0);
                _sndCardRecDelay = (rb >= delayInFrames ?
                    rb - delayInFrames : rb) / samplesPerMs;
            }
        }
        // if method == 1 we calculate delay below to keep algorithm same as
        // when we didn't have method 0.

        // Check if we have data in rec buffer. Updates the r/w pointer.
        int avail = -1;
        if (0 == _bufferCheckMethodRec)
        {
            avail = res = LATE(snd_pcm_avail_update)(_handleRecord);
        }
        if (res >= 0)
        {
            // We must check that state == RUNNING, otherwise we might have a
            // false buffer value.
            // Normal case
            if (LATE(snd_pcm_state)(_handleRecord) == SND_PCM_STATE_RUNNING)
            {
                if (0 == _bufferCheckMethodRec)
                {   // Safe since _recSndcardBuffsize is a small number
                    int rb = (int)_recSndcardBuffsize;
                    if (_buffersizeFromZeroAvail)
                    {
                        // Normal case
                        msRec = avail / samplesPerMs;
                    }
                    else
                    {
                        assert(rb >= 0);
                        msRec = (rb >= avail ? rb - avail : rb) / samplesPerMs;
                    }
                    
                    if (calcDelayFromAvail)
                    {
                        _sndCardRecDelay = msRec;
                    }

                    if ((msRec == 0) || (avail > rb))
                    {
                        ++_bufferCheckErrorsRec;
                        WEBRTC_TRACE(kTraceInfo,
                                     kTraceAudioDevice, _id,
                                     "    _bufferCheckErrorsRec: %d (avail=%d)",
                                     _bufferCheckErrorsRec, avail);
                        if (_bufferCheckErrorsRec >= THR_OLD_BUFFER_CHECK_METHOD)
                        {
                            WEBRTC_TRACE(kTraceInfo,
                                         kTraceAudioDevice, _id,
                                         "   Switching to delay buffer check"
                                         " method for recording");
                            _bufferCheckMethodRec = 1;
                            _bufferCheckErrorsRec = 0;
                        }
                    }
                    else
                    {
                        _bufferCheckErrorsRec = 0;
                    }
                }
                else // 1 == _bufferCheckMethodRec
                {
                    if (_buffersizeFromZeroDelay)
                    {
                        msRec = delayInFrames / samplesPerMs;
                    }
                    else
                    {
                        msRec =
                            (_recSndcardBuffsize - delayInFrames) / samplesPerMs;
                    }
                    _sndCardRecDelay = msRec; 

                    if (msRec == 0)
                    {
                        ++_bufferCheckErrorsRec;
                        WEBRTC_TRACE(kTraceInfo,
                                     kTraceAudioDevice, _id,
                                     "    _bufferCheckErrorsRec: %d",
                                     _bufferCheckErrorsRec);
                        if (_bufferCheckErrorsRec >= THR_IGNORE_BUFFER_CHECK)
                        {
                            // The delay has been zero too many times, ignore
                            // the delay value!
                            WEBRTC_TRACE(kTraceInfo,
                                         kTraceAudioDevice, _id,
                                         "   Switching to Ignore Delay Mode");
                            _bufferCheckMethodRec = 2;
                            _bufferCheckErrorsRec = 0;
                        }
                    }
                }
            }
            else if (LATE(snd_pcm_state)(_handleRecord) == SND_PCM_STATE_XRUN)
            {
                // We've probably had a buffer overrun
                WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, _id,
                             "Record buffer overrun, trying to recover");
                // Handle pipe error (overrun)
                res = ErrorRecovery(-EPIPE, _handleRecord);
                if (res < 0)
                {
                    // We were not able to recover from the error.
                    // CRITICAL?
                    WEBRTC_TRACE(kTraceError, kTraceAudioDevice,
                                 _id,
                                 "Can't recover from buffer overrun, "
                                 "error: %s (%d)",
                                 LATE(snd_strerror)(res), res);
                    return -1;
                }
                msRec = _recSndcardBuffsize / samplesPerMs;
            }
        }
        else
        {
            // Something went wrong asking for the delay / buffer. Try to
            // recover and make a guess.
            WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, _id,
                         "LinuxALSASndCardStream::recThreadfun(), "
                         "snd_pcm_avail_update: %s (%d)",
                         LATE(snd_strerror)(res), res);
            res = ErrorRecovery(avail, _handleRecord);
            if (preRead)
            {
                if (res == 1)
                {
                    // Recovered from buffer overrun, continue and read data.
                    msRec = _recSndcardBuffsize / samplesPerMs;
                } 
                else
                {
                    return -1;
                } 
            }
            else // We have a previous msRec value and have read maximum 10 ms since then.
            {
                if (res < 0)
                {
                    return -1;
                }

                msRec = _sndCardRecDelay - 10;

                if (calcDelayFromAvail)
                {
                    _sndCardRecDelay = msRec;
                }
            }
        }
    }
    else if (2 == _bufferCheckMethodRec)
    {
        // We've stopped asking for the number of samples on soundcard.
        msRec = 0;
    }
    else
    {
        // Should never happen
        WEBRTC_TRACE(kTraceCritical, kTraceAudioDevice, _id,
                   "Unknown buffer check method (%d)", _bufferCheckMethodRec);
        assert(false);
    }

    return msRec;
}

WebRtc_Word32 AudioDeviceLinuxALSA::ErrorRecovery(WebRtc_Word32 error,
                                                  snd_pcm_t* deviceHandle)
{
    int st = LATE(snd_pcm_state)(deviceHandle);
    WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, _id,
               "Trying to recover from error: %s (%d) (state %d)",
               LATE(snd_strerror)(error), error, st);

    // It is recommended to use snd_pcm_recover for all errors. If that function
    // cannot handle the error, the input error code will be returned, otherwise
    // 0 is returned. From snd_pcm_recover API doc: "This functions handles
    // -EINTR (interrupted system call),-EPIPE (overrun or underrun) and
    // -ESTRPIPE (stream is suspended) error codes trying to prepare given
    // stream for next I/O."

    // snd_pcm_recover isn't available in older alsa, e.g. on the FC4 machine
    // in Sthlm lab.

    int res = LATE(snd_pcm_recover)(deviceHandle, error, 1);
    if (0 == res)
    {
        WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, _id,
                   "    Recovery - snd_pcm_recover OK");

        if (error == -EPIPE &&  // Buffer underrun/overrun.
            LATE(snd_pcm_stream)(deviceHandle) == SND_PCM_STREAM_CAPTURE)
        {
            // For capture streams we also have to repeat the explicit start()
            // to get data flowing again.
            int err = LATE(snd_pcm_start)(deviceHandle);
            if (err != 0)
            {
                WEBRTC_TRACE(kTraceError, kTraceAudioDevice, _id,
                             "  Recovery - snd_pcm_start error: %u", err);
                return -1;
            }
        }

        return -EPIPE == error ? 1 : 0;
    }

    return res;
}

// ============================================================================
//                                  Thread Methods
// ============================================================================

bool AudioDeviceLinuxALSA::PlayThreadFunc(void* pThis)
{
    return (static_cast<AudioDeviceLinuxALSA*>(pThis)->PlayThreadProcess());
}

bool AudioDeviceLinuxALSA::RecThreadFunc(void* pThis)
{
    return (static_cast<AudioDeviceLinuxALSA*>(pThis)->RecThreadProcess());
}

bool AudioDeviceLinuxALSA::PlayThreadProcess()
{
    WebRtc_Word32 written(0);
    WebRtc_Word32 msPlay(0);

    // Number of (stereo) samples
    WebRtc_Word32 numPlaySamples = _playbackBufferSize / (2 * _playChannels);
    WebRtc_Word8 playBuffer[_playbackBufferSize];

    switch (_timeEventPlay.Wait(1000))
    {
    case kEventSignaled:
        break;
    case kEventError:
        WEBRTC_TRACE(kTraceWarning, kTraceAudioDevice, _id,
                   "EventWrapper::Wait() failed => restarting timer");
        _timeEventPlay.StopTimer();
        _timeEventPlay.StartTimer(true, PLAY_TIMER_PERIOD_MS);
        return true;
    case kEventTimeout:
        return true;
    }
    
    Lock();

    if (_startPlay)
    {
        WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, _id,
                   "_startPlay true, performing initial actions");

        _startPlay = false;

        // Fill playout buffer with zeroes
        FillPlayoutBuffer();

        _bufferCheckErrorsPlay = 0;
        _playing = true;
        _playStartEvent.Set();
    }
    
    if(_playing)
    {
        // get number of ms of sound that remains in the sound card buffer for
        // playback
        msPlay = GetPlayoutBufferDelay();
        if (msPlay == -1)
        {
            UnLock();
            return true;
        }

        // write more data if below threshold
        if (msPlay < PLAYBACK_THRESHOLD)
        {
            // ask for new PCM data to be played out using the AudioDeviceBuffer
            // ensure that this callback is executed without taking the
            // audio-thread lock
            // 
            UnLock();
            WebRtc_Word32 nSamples =
                (WebRtc_Word32)_ptrAudioBuffer->RequestPlayoutData(numPlaySamples);
            Lock();

            if (OutputSanityCheckAfterUnlockedPeriod() == -1)
            {
                UnLock();
                return true;
            }

            nSamples = _ptrAudioBuffer->GetPlayoutData(playBuffer);
            if (nSamples != numPlaySamples)
            {
                WEBRTC_TRACE(kTraceError, kTraceAudioDevice, _id,
                             "  invalid number of output samples(%d)", nSamples);
            }

            written = LATE(snd_pcm_writei)(_handlePlayout, playBuffer, numPlaySamples);
            if (written != numPlaySamples)
            {
                if (written < 0)
                { 
                    WEBRTC_TRACE(kTraceError, kTraceAudioDevice,
                                 _id,
                                 "Error writing to sound device (7), error: %d/%s",
                                 written,
                                 LATE(snd_strerror)(written));

                    // Try to recover
                    ErrorRecovery(written, _handlePlayout);
                    _delayMonitorStatePlay = 1; // Go to delay monitor state
                    WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice,
                                 _id,
                                 "    Going to delay monitor state");
                }
                else
                {
                    _writeErrors = 0;
                    int remainingFrames = (numPlaySamples - written);
                    written = LATE(snd_pcm_writei)
                                  (_handlePlayout,
                                   &playBuffer[written*2],
                                   remainingFrames);
                    if( written == remainingFrames )
                    {
                        written = numPlaySamples;
                    }
                    else
                    {
                        if (written < 0)
                        {
                            WEBRTC_TRACE(kTraceError,
                                         kTraceAudioDevice, _id,
                                         "Error writing to sound device (8), "
                                         "error: %d/%s, numPlaySamples=%d, "
                                         "remainingFrames=%d",
                                         written, LATE(snd_strerror)(written),
                                         numPlaySamples, remainingFrames);

                            // Try to recover
                            ErrorRecovery(written, _handlePlayout);
                        }
                        else
                        {
                            WEBRTC_TRACE(kTraceWarning,
                                         kTraceAudioDevice, _id,
                                         "Could not write all playout data (1),"
                                         " numPlaySamples=%d, remainingFrames=%d,"
                                         " written=%d",
                                         numPlaySamples, remainingFrames, written);
                        }
                    }
                }
            }
            else
            {
                _writeErrors = 0;
            }

            // Write more data if we are more than 10 ms under the threshold.
            if (msPlay < PLAYBACK_THRESHOLD - 10)
            {
                // ask for new PCM data to be played out using the
                // AudioDeviceBuffer ensure that this callback is executed
                // without taking the audio-thread lock
                // 
                UnLock();
                WebRtc_Word32 nSamples = (WebRtc_Word32)
                    _ptrAudioBuffer->RequestPlayoutData(numPlaySamples);
                Lock();

                if (OutputSanityCheckAfterUnlockedPeriod() == -1)
                {
                    UnLock();
                    return true;
                }

                nSamples = _ptrAudioBuffer->GetPlayoutData(playBuffer);
                if (nSamples != numPlaySamples)
                {
                    WEBRTC_TRACE(kTraceError, kTraceAudioDevice,
                                 _id, "  invalid number of output samples(%d)",
                                 nSamples);
                }

                written = LATE(snd_pcm_writei)(
                    _handlePlayout, playBuffer, numPlaySamples);
                if (written != numPlaySamples)
                {
                    if (written < 0)
                    {
                        WEBRTC_TRACE(kTraceError,
                                     kTraceAudioDevice, _id,
                                     "Error writing to sound device (9), "
                                     "error: %s", LATE(snd_strerror)(written));

                        // Try to recover
                        ErrorRecovery(written, _handlePlayout);
                        _delayMonitorStatePlay = 1; // Go to delay monitor state
                        WEBRTC_TRACE(kTraceInfo,
                                     kTraceAudioDevice, _id,
                                     "    Going to delay monitor state");
                    }
                    else
                    {
                        int remainingFrames = (numPlaySamples - written);
                        written = LATE(snd_pcm_writei)
                                      (_handlePlayout,
                                       &playBuffer[written*2],
                                       remainingFrames);
                        if (written == remainingFrames)
                        {
                            written = numPlaySamples;
                        }
                        else
                        {
                            if (written < 0)
                            {
                                WEBRTC_TRACE(kTraceError,
                                             kTraceAudioDevice, _id,
                                             "Error writing to sound device (10),"
                                             " error: %d/%s, numPlaySamples=%d,"
                                             " remainingFrames=%d",
                                             written, LATE(snd_strerror)(written),
                                             numPlaySamples, remainingFrames);

                                // Try to recover
                                ErrorRecovery(written, _handlePlayout);
                            }
                            else
                            {
                                WEBRTC_TRACE(kTraceWarning,
                                             kTraceAudioDevice, _id,
                                             "Could not write all playout data"
                                             " (2), numPlaySamples=%d, "
                                             "remainingFrames=%d, written=%d",
                                             numPlaySamples, remainingFrames,
                                             written);
                            }
                        }
                    }

                } 
            } // msPlay < PLAYBACK_THRESHOLD - 10

        } // msPlay < PLAYBACK_THRESHOLD

    } // _playing

    UnLock();
    return true;
}

bool AudioDeviceLinuxALSA::RecThreadProcess()
{
    WebRtc_Word32  msRec(0);
    WebRtc_Word32  framesInRecData(0);

    // Number of (stereo) samples to record
    WebRtc_Word32  recBufSizeInSamples = _recordBufferSize / (2 * _recChannels);
    WebRtc_Word16  tmpBuffer[_recordBufferSize / 2];
    WebRtc_UWord32 samplesPerMs = _samplingFreqRec;

    switch (_timeEventRec.Wait(1000))
    {
    case kEventSignaled:
        break;
    case kEventError:
        WEBRTC_TRACE(kTraceWarning, kTraceAudioDevice, _id,
                     "EventWrapper::Wait() failed => restarting timer");
        _timeEventRec.StopTimer();
        _timeEventRec.StartTimer(true, REC_TIMER_PERIOD_MS);
        return true;
    case kEventTimeout:
        return true;
    }

    Lock();

    if (_startRec)
    {
        WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, _id,
                     "_startRec true, performing initial actions");

        if (PrepareStartRecording() == 0)
        {
            _bufferCheckErrorsRec = 0;
            _startRec = false;
            _recording = true;
            _recStartEvent.Set();
        }
    }

    if (_recording)
    {
        // get number of ms of sound that remains in the sound card buffer for
        // playback
        msRec = GetRecordingBufferDelay(true);
        if (msRec == -1)
        {
            UnLock();
            return true;
        }

        // read data if a whole frame has been captured
        // or if we are in ignore delay mode (check method 2)
        if ((msRec > 10) || (2 == _bufferCheckMethodRec))
        {
            // Read 10 ms of data from soundcard
            framesInRecData = LATE(snd_pcm_readi)
                                  (_handleRecord,
                                   tmpBuffer,
                                   recBufSizeInSamples);

            if (framesInRecData < 0)
            {
                WEBRTC_TRACE(kTraceError, kTraceAudioDevice, _id,
                             "pcm read error (1)");
                ErrorRecovery(framesInRecData, _handleRecord);
                UnLock();
                return true;
            }
            else if (framesInRecData + (WebRtc_Word32)_numReadyRecSamples <
                recBufSizeInSamples)
            {
                for (int idx = 0; idx < framesInRecData*_recChannels; idx++)
                {
                    _recBuffer[_numReadyRecSamples*_recChannels + idx] =
                        tmpBuffer[idx];
                }
                _numReadyRecSamples += framesInRecData;

                framesInRecData = LATE(snd_pcm_readi)
                                      (_handleRecord,
                                       tmpBuffer,
                                       recBufSizeInSamples - _numReadyRecSamples);

                if (framesInRecData < 0)
                {
                    WEBRTC_TRACE(kTraceError, kTraceAudioDevice,
                                 _id, "pcm read error (2)");
                    ErrorRecovery(framesInRecData, _handleRecord);
                    UnLock();
                    return true;
                }
                else if (framesInRecData + (WebRtc_Word32)_numReadyRecSamples ==
                    recBufSizeInSamples)
                {
                    // We got all the data we need, go on as normal.
                }
                else 
                {
                    // We still don't have enough data, copy what we have and leave.
                    for (int idx = 0; idx < framesInRecData*_recChannels; idx++)
                    {
                        _recBuffer[_numReadyRecSamples*_recChannels + idx] =
                            tmpBuffer[idx];
                    }
                    _numReadyRecSamples += framesInRecData;
                    WEBRTC_TRACE(kTraceStream,
                                 kTraceAudioDevice, _id,
                               "     %d samples copied. Not enough, return and"
                               " wait for more.",
                               framesInRecData);
                    UnLock();
                    return true;
                }
            }

            // get recording buffer delay after reading
            // to have a value to use for the AEC
            msRec = GetRecordingBufferDelay(false);
            if (msRec == -1)
            {
                UnLock();
                return true;
            }

            // calculate the number of samples to copy
            // to have a full buffer
            int copySamples = 0;
            if ((WebRtc_Word32)_numReadyRecSamples + framesInRecData >=
                recBufSizeInSamples)
            {
                copySamples = recBufSizeInSamples - _numReadyRecSamples;
            }
            else 
            {
                copySamples = framesInRecData;
            }
        
            // fill up buffer
            for (int idx = 0; idx < copySamples*_recChannels; idx++)
            {
                _recBuffer[_numReadyRecSamples*_recChannels + idx] =
                    tmpBuffer[idx];
            }

            _numReadyRecSamples += copySamples;
            framesInRecData -= copySamples;

            // Send data, if we have 10ms data...
            if ((WebRtc_Word32)_numReadyRecSamples == recBufSizeInSamples)
            {
                WebRtc_UWord32 currentMicLevel(0);
                WebRtc_UWord32 newMicLevel(0);
                WebRtc_Word32 msRecDelay = 0 == _bufferCheckMethodRec ?
                    _sndCardRecDelay : msRec;
                WebRtc_Word32 msReady = _numReadyRecSamples / samplesPerMs;
                WebRtc_Word32 msStored = framesInRecData / samplesPerMs;
                WebRtc_Word32 blockSize = recBufSizeInSamples / samplesPerMs;

                // TODO(xians): The blockSize - 25 term brings the delay measurement
                // into line with the Windows interpretation. Investigate if this 
                // works properly with different block sizes.
                // TODO(xians): Should only the rec delay from snd_pcm_delay be taken
                // into account? See ALSA API doc.
                // Probably we want to add the remaining data in the buffer as
                // well or is that already in any of the variables?
                WebRtc_Word32 msTotalRecDelay = msRecDelay + msReady +
                    msStored + blockSize - 25;
                if (msTotalRecDelay < 0)
                {
                    msTotalRecDelay = 0;
                }
                // store the recorded buffer (no action will be taken if the
                // #recorded samples is not a full buffer)
                _ptrAudioBuffer->SetRecordedBuffer(
                    (WebRtc_Word8 *)&_recBuffer[0], _numReadyRecSamples);

                if (AGC())
                {
                    // store current mic level in the audio buffer if AGC is enabled
                    if (MicrophoneVolume(currentMicLevel) == 0)
                    {
                        if (currentMicLevel == 0xffffffff)
                        {
                            currentMicLevel = 100;
                        }
                        // this call does not affect the actual microphone volume
                        _ptrAudioBuffer->SetCurrentMicLevel(currentMicLevel);                
                    }
                }

                // store vqe delay values
                _ptrAudioBuffer->SetVQEData(_sndCardPlayDelay,
                                            msTotalRecDelay,
                                            0);

                // deliver recorded samples at specified sample rate, mic level
                // etc. to the observer using callback
                UnLock();
                _ptrAudioBuffer->DeliverRecordedData();
                Lock();

                if (InputSanityCheckAfterUnlockedPeriod() == -1)
                {
                    UnLock();
                    return true;
                }

                if (AGC())
                {
                    newMicLevel = _ptrAudioBuffer->NewMicLevel();
                    if (newMicLevel != 0)
                    {
                        // The VQE will only deliver non-zero microphone levels
                        //when a change is needed.
                         // Set this new mic level (received from the observer
                        // as return value in the callback).
                        WEBRTC_TRACE(kTraceStream,
                                     kTraceAudioDevice, _id,
                                     "  AGC change of volume: old=%u => new=%u",
                                     currentMicLevel,  newMicLevel);
                        if (SetMicrophoneVolume(newMicLevel) == -1)
                        {
                            WEBRTC_TRACE(kTraceWarning,
                                         kTraceAudioDevice, _id,
                                       "  the required modification of the"
                                       " microphone volume failed");
                        }
                    }
                }

                _numReadyRecSamples = 0;

                // if there are remaining samples in tmpBuffer
                // copy those to _recBuffer
                if (framesInRecData > 0)
                {
                    WEBRTC_TRACE(kTraceStream,
                                 kTraceAudioDevice, _id,
                                 "   Got rest samples, copy %d samples to rec"
                                 " buffer", framesInRecData);
                    for (int idx = 0; idx < framesInRecData; idx++)
                    {
                        _recBuffer[idx] = tmpBuffer[copySamples+idx];
                    }

                    _numReadyRecSamples = framesInRecData;
                }

            } // if (_numReadyRecSamples == recBufSizeInSamples)

        } // (msRec > 10) || (2 == _bufferCheckMethodRec)

    } // _recording

    UnLock();
    return true;
}

}
