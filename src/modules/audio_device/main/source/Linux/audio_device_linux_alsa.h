/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_AUDIO_DEVICE_AUDIO_DEVICE_LINUX_ALSA_H
#define WEBRTC_AUDIO_DEVICE_AUDIO_DEVICE_LINUX_ALSA_H

#include "audio_device_generic.h"
#include "critical_section_wrapper.h"
#include "audio_mixer_manager_linux_alsa.h"

#include <sys/soundcard.h>
#include <sys/ioctl.h>

#include <alsa/asoundlib.h>

namespace webrtc
{
class EventWrapper;
class ThreadWrapper;

// Number of continuous buffer check errors before going 0->1
const WebRtc_UWord16 THR_OLD_BUFFER_CHECK_METHOD = 30;
// Number of buffer check errors before going 1->2
const WebRtc_UWord16 THR_IGNORE_BUFFER_CHECK = 30;
// 2.7 seconds (decimal 131071)
const WebRtc_UWord32 ALSA_SNDCARD_BUFF_SIZE_REC = 0x1ffff;
// ~170 ms (decimal 8191) - enough since we only write to buffer if it contains
// less than 50 ms
const WebRtc_UWord32 ALSA_SNDCARD_BUFF_SIZE_PLAY = 0x1fff;

const WebRtc_UWord32 REC_TIMER_PERIOD_MS = 2;
const WebRtc_UWord32 PLAY_TIMER_PERIOD_MS = 5;
const WebRtc_UWord16 PLAYBACK_THRESHOLD = 50;

const WebRtc_UWord32 REC_SAMPLES_PER_MS = 48;
const WebRtc_UWord32 PLAY_SAMPLES_PER_MS = 48;

class AudioDeviceLinuxALSA : public AudioDeviceGeneric
{
public:
    AudioDeviceLinuxALSA(const WebRtc_Word32 id);
    ~AudioDeviceLinuxALSA();

    // Retrieve the currently utilized audio layer
    virtual WebRtc_Word32 ActiveAudioLayer(
        AudioDeviceModule::AudioLayer& audioLayer) const;

    // Main initializaton and termination
    virtual WebRtc_Word32 Init();
    virtual WebRtc_Word32 Terminate();
    virtual bool Initialized() const;

    // Device enumeration
    virtual WebRtc_Word16 PlayoutDevices();
    virtual WebRtc_Word16 RecordingDevices();
    virtual WebRtc_Word32 PlayoutDeviceName(
        WebRtc_UWord16 index,
        WebRtc_Word8 name[kAdmMaxDeviceNameSize],
        WebRtc_Word8 guid[kAdmMaxGuidSize]);
    virtual WebRtc_Word32 RecordingDeviceName(
        WebRtc_UWord16 index,
        WebRtc_Word8 name[kAdmMaxDeviceNameSize],
        WebRtc_Word8 guid[kAdmMaxGuidSize]);

    // Device selection
    virtual WebRtc_Word32 SetPlayoutDevice(WebRtc_UWord16 index);
    virtual WebRtc_Word32 SetPlayoutDevice(
        AudioDeviceModule::WindowsDeviceType device);
    virtual WebRtc_Word32 SetRecordingDevice(WebRtc_UWord16 index);
    virtual WebRtc_Word32 SetRecordingDevice(
        AudioDeviceModule::WindowsDeviceType device);

    // Audio transport initialization
    virtual WebRtc_Word32 PlayoutIsAvailable(bool& available);
    virtual WebRtc_Word32 InitPlayout();
    virtual bool PlayoutIsInitialized() const;
    virtual WebRtc_Word32 RecordingIsAvailable(bool& available);
    virtual WebRtc_Word32 InitRecording();
    virtual bool RecordingIsInitialized() const;

    // Audio transport control
    virtual WebRtc_Word32 StartPlayout();
    virtual WebRtc_Word32 StopPlayout();
    virtual bool Playing() const;
    virtual WebRtc_Word32 StartRecording();
    virtual WebRtc_Word32 StopRecording();
    virtual bool Recording() const;

    // Microphone Automatic Gain Control (AGC)
    virtual WebRtc_Word32 SetAGC(bool enable);
    virtual bool AGC() const;

    // Volume control based on the Windows Wave API (Windows only)
    virtual WebRtc_Word32 SetWaveOutVolume(WebRtc_UWord16 volumeLeft,
                                           WebRtc_UWord16 volumeRight);
    virtual WebRtc_Word32 WaveOutVolume(WebRtc_UWord16& volumeLeft,
                                        WebRtc_UWord16& volumeRight) const;

    // Audio mixer initialization
    virtual WebRtc_Word32 SpeakerIsAvailable(bool& available);
    virtual WebRtc_Word32 InitSpeaker();
    virtual bool SpeakerIsInitialized() const;
    virtual WebRtc_Word32 MicrophoneIsAvailable(bool& available);
    virtual WebRtc_Word32 InitMicrophone();
    virtual bool MicrophoneIsInitialized() const;

    // Speaker volume controls
    virtual WebRtc_Word32 SpeakerVolumeIsAvailable(bool& available);
    virtual WebRtc_Word32 SetSpeakerVolume(WebRtc_UWord32 volume);
    virtual WebRtc_Word32 SpeakerVolume(WebRtc_UWord32& volume) const;
    virtual WebRtc_Word32 MaxSpeakerVolume(WebRtc_UWord32& maxVolume) const;
    virtual WebRtc_Word32 MinSpeakerVolume(WebRtc_UWord32& minVolume) const;
    virtual WebRtc_Word32 SpeakerVolumeStepSize(WebRtc_UWord16& stepSize) const;

    // Microphone volume controls
    virtual WebRtc_Word32 MicrophoneVolumeIsAvailable(bool& available);
    virtual WebRtc_Word32 SetMicrophoneVolume(WebRtc_UWord32 volume);
    virtual WebRtc_Word32 MicrophoneVolume(WebRtc_UWord32& volume) const;
    virtual WebRtc_Word32 MaxMicrophoneVolume(WebRtc_UWord32& maxVolume) const;
    virtual WebRtc_Word32 MinMicrophoneVolume(WebRtc_UWord32& minVolume) const;
    virtual WebRtc_Word32 MicrophoneVolumeStepSize(
        WebRtc_UWord16& stepSize) const;

    // Speaker mute control
    virtual WebRtc_Word32 SpeakerMuteIsAvailable(bool& available);
    virtual WebRtc_Word32 SetSpeakerMute(bool enable);
    virtual WebRtc_Word32 SpeakerMute(bool& enabled) const;
    
    // Microphone mute control
    virtual WebRtc_Word32 MicrophoneMuteIsAvailable(bool& available);
    virtual WebRtc_Word32 SetMicrophoneMute(bool enable);
    virtual WebRtc_Word32 MicrophoneMute(bool& enabled) const;

    // Microphone boost control
    virtual WebRtc_Word32 MicrophoneBoostIsAvailable(bool& available);
    virtual WebRtc_Word32 SetMicrophoneBoost(bool enable);
    virtual WebRtc_Word32 MicrophoneBoost(bool& enabled) const;

    // Stereo support
    virtual WebRtc_Word32 StereoPlayoutIsAvailable(bool& available);
    virtual WebRtc_Word32 SetStereoPlayout(bool enable);
    virtual WebRtc_Word32 StereoPlayout(bool& enabled) const;
    virtual WebRtc_Word32 StereoRecordingIsAvailable(bool& available);
    virtual WebRtc_Word32 SetStereoRecording(bool enable);
    virtual WebRtc_Word32 StereoRecording(bool& enabled) const;
   
    // Delay information and control
    virtual WebRtc_Word32 SetPlayoutBuffer(
        const AudioDeviceModule::BufferType type,
        WebRtc_UWord16 sizeMS);
    virtual WebRtc_Word32 PlayoutBuffer(
        AudioDeviceModule::BufferType& type,
        WebRtc_UWord16& sizeMS) const;
    virtual WebRtc_Word32 PlayoutDelay(WebRtc_UWord16& delayMS) const;
    virtual WebRtc_Word32 RecordingDelay(WebRtc_UWord16& delayMS) const;

    // CPU load
    virtual WebRtc_Word32 CPULoad(WebRtc_UWord16& load) const;

public:
    virtual bool PlayoutWarning() const;
    virtual bool PlayoutError() const;
    virtual bool RecordingWarning() const;
    virtual bool RecordingError() const;
    virtual void ClearPlayoutWarning();
    virtual void ClearPlayoutError();
    virtual void ClearRecordingWarning();
    virtual void ClearRecordingError();

public:
    virtual void AttachAudioBuffer(AudioDeviceBuffer* audioBuffer);

private:
    WebRtc_Word32 GetDevicesInfo(const WebRtc_Word32 function,
                                 const bool playback,
                                 const WebRtc_Word32 enumDeviceNo = 0,
                                 char* enumDeviceName = NULL,
                                 const WebRtc_Word32 ednLen = 0) const;
    WebRtc_Word32 ErrorRecovery(WebRtc_Word32 error, snd_pcm_t* deviceHandle);
    void FillPlayoutBuffer();

private:
    void Lock() { _critSect.Enter(); };
    void UnLock() { _critSect.Leave(); };
private:
    inline WebRtc_Word32 InputSanityCheckAfterUnlockedPeriod() const;
    inline WebRtc_Word32 OutputSanityCheckAfterUnlockedPeriod() const;

    WebRtc_Word32 PrepareStartRecording();
    WebRtc_Word32 GetPlayoutBufferDelay();
    WebRtc_Word32 GetRecordingBufferDelay(bool preRead);

private:
    static bool RecThreadFunc(void*);
    static bool PlayThreadFunc(void*);
    bool RecThreadProcess();
    bool PlayThreadProcess();

private:
    AudioDeviceBuffer* _ptrAudioBuffer;
    
    CriticalSectionWrapper& _critSect;
    EventWrapper& _timeEventRec;
    EventWrapper& _timeEventPlay;
    EventWrapper& _recStartEvent;
    EventWrapper& _playStartEvent;

    ThreadWrapper* _ptrThreadPlay;
    ThreadWrapper* _ptrThreadRec;
    WebRtc_UWord32 _recThreadID;
    WebRtc_UWord32 _playThreadID;

    WebRtc_Word32 _id;

    AudioMixerManagerLinuxALSA _mixerManager;

    bool _usingInputDeviceIndex;
    bool _usingOutputDeviceIndex;
    AudioDeviceModule::WindowsDeviceType _inputDevice;
    AudioDeviceModule::WindowsDeviceType _outputDevice;
    WebRtc_UWord16 _inputDeviceIndex;
    WebRtc_UWord16 _outputDeviceIndex;
    bool _inputDeviceIsSpecified;
    bool _outputDeviceIsSpecified;

    snd_pcm_t* _handleRecord;
    snd_pcm_t* _handlePlayout;

    snd_pcm_uframes_t _recSndcardBuffsize;
    snd_pcm_uframes_t _playSndcardBuffsize;

    WebRtc_UWord32 _samplingFreqRec;
    WebRtc_UWord32 _samplingFreqPlay;
    WebRtc_UWord8 _recChannels;
    WebRtc_UWord8 _playChannels;

    WebRtc_UWord32 _playbackBufferSize;
    WebRtc_UWord32 _recordBufferSize;
    WebRtc_Word16* _recBuffer;
    AudioDeviceModule::BufferType _playBufType;

private:
    bool _initialized;
    bool _recording;
    bool _playing;
    bool _recIsInitialized;
    bool _playIsInitialized;
    bool _startRec;
    bool _stopRec;
    bool _startPlay;
    bool _stopPlay;
    bool _AGC;
    bool _buffersizeFromZeroAvail;
    bool _buffersizeFromZeroDelay;

    WebRtc_UWord32 _sndCardPlayDelay;        // Just to store last value
    WebRtc_UWord32 _previousSndCardPlayDelay; // Stores previous _sndCardPlayDelay value
    WebRtc_UWord8 _delayMonitorStatePlay; // 0 normal, 1 monitor delay change (after error)
    WebRtc_Word16 _largeDelayCountPlay;  // Used when monitoring delay change
    WebRtc_UWord32 _sndCardRecDelay;
    WebRtc_UWord32 _numReadyRecSamples;

    WebRtc_UWord8 _bufferCheckMethodPlay;
    WebRtc_UWord8 _bufferCheckMethodRec;
    WebRtc_UWord32 _bufferCheckErrorsPlay;
    WebRtc_UWord32 _bufferCheckErrorsRec;
    WebRtc_Word32 _lastBufferCheckValuePlay;
    WebRtc_Word32 _writeErrors;

    WebRtc_UWord16 _playWarning;
    WebRtc_UWord16 _playError;
    WebRtc_UWord16 _recWarning;
    WebRtc_UWord16 _recError;

    WebRtc_UWord16 _playBufDelay;                 // playback delay
    WebRtc_UWord16 _playBufDelayFixed;            // fixed playback delay
};

}

#endif  // MODULES_AUDIO_DEVICE_MAIN_SOURCE_LINUX_AUDIO_DEVICE_LINUX_ALSA_H_
