/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <stdio.h>
#include <string.h>

#include "audio_device_test_defines.h"

#include "../source/audio_device_config.h"
#include "../source/audio_device_utility.h"

// Helper functions
#if defined(ANDROID)
char filenameStr[2][256] =
{   0}; // Allow two buffers for those API calls taking two filenames
int currentStr = 0;

char* GetFilename(char* filename)
{
    currentStr = !currentStr;
    sprintf(filenameStr[currentStr], "/sdcard/admtest/%s", filename);
    return filenameStr[currentStr];
}
const char* GetFilename(const char* filename)
{
    currentStr = !currentStr;
    sprintf(filenameStr[currentStr], "/sdcard/admtest/%s", filename);
    return filenameStr[currentStr];
}
int GetResource(char* resource, char* dest, int destLen)
{
    currentStr = !currentStr;
    sprintf(filenameStr[currentStr], "/sdcard/admtest/%s", resource);
    strncpy(dest, filenameStr[currentStr], destLen-1);
    return 0;
}
char* GetResource(char* resource)
{
    currentStr = !currentStr;
    sprintf(filenameStr[currentStr], "/sdcard/admtest/%s", resource);
    return filenameStr[currentStr];
}
const char* GetResource(const char* resource)
{
    currentStr = !currentStr;
    sprintf(filenameStr[currentStr], "/sdcard/admtest/%s", resource);
    return filenameStr[currentStr];
}
#elif !defined(MAC_IPHONE)
char* GetFilename(char* filename)
{
    return filename;
}
const char* GetFilename(const char* filename)
{
    return filename;
}
char* GetResource(char* resource)
{
    return resource;
}
const char* GetResource(const char* resource)
{
    return resource;
}
#endif

using namespace webrtc;

// ----------------------------------------------------------------------------
//  AudioEventObserverAPI
// ----------------------------------------------------------------------------

class AudioEventObserverAPI: public AudioDeviceObserver
{
public:
    AudioEventObserverAPI(AudioDeviceModule* audioDevice) :
        _audioDevice(audioDevice)
    {
    }
    ;
    ~AudioEventObserverAPI()
    {
    }
    ;
    virtual void OnErrorIsReported(const ErrorCode error)
    {
        TEST_LOG("\n[*** ERROR ***] => OnErrorIsReported(%d)\n\n", error);
        _error = error;
        // TEST(_audioDevice->StopRecording() == 0);
        // TEST(_audioDevice->StopPlayout() == 0);
    }
    ;
    virtual void OnWarningIsReported(const WarningCode warning)
    {
        TEST_LOG("\n[*** WARNING ***] => OnWarningIsReported(%d)\n\n", warning);
        _warning = warning;
        TEST(_audioDevice->StopRecording() == 0);
        TEST(_audioDevice->StopPlayout() == 0);
    }
    ;
public:
    ErrorCode _error;
    WarningCode _warning;
private:
    AudioDeviceModule* _audioDevice;
};

class AudioTransportAPI: public AudioTransport
{
public:
    AudioTransportAPI(AudioDeviceModule* audioDevice) :
        _audioDevice(audioDevice), _recCount(0), _playCount(0)
    {
    }
    ;

    ~AudioTransportAPI()
    {
    }
    ;

    virtual WebRtc_Word32 RecordedDataIsAvailable(
        const WebRtc_Word8* audioSamples,
        const WebRtc_UWord32 nSamples,
        const WebRtc_UWord8 nBytesPerSample,
        const WebRtc_UWord8 nChannels,
        const WebRtc_UWord32 sampleRate,
        const WebRtc_UWord32 totalDelay,
        const WebRtc_Word32 clockSkew,
        const WebRtc_UWord32 currentMicLevel,
        WebRtc_UWord32& newMicLevel)
    {
        _recCount++;
        if (_recCount % 100 == 0)
        {
            if (nChannels == 1)
            {
                // mono
                TEST_LOG("-");
            } else if ((nChannels == 2) && (nBytesPerSample == 2))
            {
                // stereo but only using one channel
                TEST_LOG("-|");
            } else
            {
                // stereo
                TEST_LOG("--");
            }
        }

        return 0;
    }

    virtual WebRtc_Word32 NeedMorePlayData(const WebRtc_UWord32 nSamples,
                                           const WebRtc_UWord8 nBytesPerSample,
                                           const WebRtc_UWord8 nChannels,
                                           const WebRtc_UWord32 sampleRate,
                                           WebRtc_Word8* audioSamples,
                                           WebRtc_UWord32& nSamplesOut)
    {
        _playCount++;
        if (_playCount % 100 == 0)
        {
            if (nChannels == 1)
            {
                TEST_LOG("+");
            } else
            {
                TEST_LOG("++");
            }
        }

        nSamplesOut = 480;

        return 0;
    }
    ;
private:
    AudioDeviceModule* _audioDevice;
    WebRtc_UWord32 _recCount;
    WebRtc_UWord32 _playCount;
};

int api_test();


#if !defined(MAC_IPHONE) && !defined(ANDROID)
int api_test();

int main(int /*argc*/, char* /*argv*/[])
{
    api_test();
}
#endif

int api_test()
{
    int i(0);

    TEST_LOG("========================================\n");
    TEST_LOG("API Test of the WebRtcAudioDevice Module\n");
    TEST_LOG("========================================\n\n");

    ProcessThread* processThread = ProcessThread::CreateProcessThread();
    processThread->Start();

    // =======================================================
    // AudioDeviceModule::Create
    //
    // Windows:
    //      if (WEBRTC_WINDOWS_CORE_AUDIO_BUILD)
    //          user can select between default (Core) or Wave
    //      else
    //          user can select between default (Wave) or Wave
    // =======================================================

    const WebRtc_Word32 myId = 444;
    AudioDeviceModule* audioDevice(NULL);

#if defined(_WIN32)
    TEST((audioDevice = AudioDeviceModule::Create(
        myId, AudioDeviceModule::kLinuxAlsaAudio)) == NULL);
#if defined(WEBRTC_WINDOWS_CORE_AUDIO_BUILD)
    TEST_LOG("WEBRTC_WINDOWS_CORE_AUDIO_BUILD is defined!\n\n");
    // create default implementation (=Core Audio) instance
    TEST((audioDevice = AudioDeviceModule::Create(
        myId, AudioDeviceModule::kPlatformDefaultAudio)) != NULL);
    AudioDeviceModule::Destroy(audioDevice);
    // create non-default (=Wave Audio) instance
    TEST((audioDevice = AudioDeviceModule::Create(
        myId, AudioDeviceModule::kWindowsWaveAudio)) != NULL);
    AudioDeviceModule::Destroy(audioDevice);
    // explicitly specify usage of Core Audio (same as default)
    TEST((audioDevice = AudioDeviceModule::Create(
        myId, AudioDeviceModule::kWindowsCoreAudio)) != NULL);
#else
    // TEST_LOG("WEBRTC_WINDOWS_CORE_AUDIO_BUILD is *not* defined!\n");
    TEST((audioDevice = AudioDeviceModule::Create(
        myId, AudioDeviceModule::kWindowsCoreAudio)) == NULL);
    // create default implementation (=Wave Audio) instance
    TEST((audioDevice = AudioDeviceModule::Create(
        myId, AudioDeviceModule::kPlatformDefaultAudio)) != NULL);
    AudioDeviceModule::Destroy(audioDevice);
    // explicitly specify usage of Wave Audio (same as default)
    TEST((audioDevice = AudioDeviceModule::Create(
        myId, AudioDeviceModule::kWindowsWaveAudio)) != NULL);
#endif
#endif

#if defined(ANDROID)
    // Fails tests
    TEST((audioDevice = AudioDeviceModule::Create(
        myId, AudioDeviceModule::kWindowsWaveAudio)) == NULL);
    TEST((audioDevice = AudioDeviceModule::Create(
        myId, AudioDeviceModule::kWindowsCoreAudio)) == NULL);
    TEST((audioDevice = AudioDeviceModule::Create(
        myId, AudioDeviceModule::kLinuxAlsaAudio)) == NULL);
    TEST((audioDevice = AudioDeviceModule::Create(
        myId, AudioDeviceModule::kLinuxPulseAudio)) == NULL);
    // Create default implementation instance
    TEST((audioDevice = AudioDeviceModule::Create(
        myId, AudioDeviceModule::kPlatformDefaultAudio)) != NULL);
#elif defined(WEBRTC_LINUX)
    TEST((audioDevice = AudioDeviceModule::Create(
        myId, AudioDeviceModule::kWindowsWaveAudio)) == NULL);
    TEST((audioDevice = AudioDeviceModule::Create(
        myId, AudioDeviceModule::kWindowsCoreAudio)) == NULL);
    // create default implementation (=ALSA Audio) instance
    TEST((audioDevice = AudioDeviceModule::Create(
        myId, AudioDeviceModule::kPlatformDefaultAudio)) != NULL);
    AudioDeviceModule::Destroy(audioDevice);
    // explicitly specify usage of Pulse Audio (same as default)
    TEST((audioDevice = AudioDeviceModule::Create(
        myId, AudioDeviceModule::kLinuxPulseAudio)) != NULL);
#endif

#if defined(WEBRTC_MAC)
    // Fails tests
    TEST((audioDevice = AudioDeviceModule::Create(
        myId, AudioDeviceModule::kWindowsWaveAudio)) == NULL);
    TEST((audioDevice = AudioDeviceModule::Create(
        myId, AudioDeviceModule::kWindowsCoreAudio)) == NULL);
    TEST((audioDevice = AudioDeviceModule::Create(
        myId, AudioDeviceModule::kLinuxAlsaAudio)) == NULL);
    TEST((audioDevice = AudioDeviceModule::Create(
        myId, AudioDeviceModule::kLinuxPulseAudio)) == NULL);
    // Create default implementation instance
    TEST((audioDevice = AudioDeviceModule::Create(
        myId, AudioDeviceModule::kPlatformDefaultAudio)) != NULL);
#endif

    if (audioDevice == NULL)
    {
#ifdef _WIN32
        goto Exit;
#else
        TEST_LOG("Failed creating audio device object! \n");
        return 0;
#endif
    }

    processThread->RegisterModule(audioDevice);

    // ===============
    // Module::Version
    // ===============

    WebRtc_Word8 version[256];
    WebRtc_UWord32 remainingBufferInBytes = 256;
    WebRtc_UWord32 tooFewBytes = 10;
    WebRtc_UWord32 position = 0;

    TEST(audioDevice->Version(version, tooFewBytes, position) == -1);
    TEST(audioDevice->Version(NULL, remainingBufferInBytes, position) == -1);

    TEST(audioDevice->Version(version, remainingBufferInBytes, position) == 0);
    TEST(position == 18); // assumes "AudioDevice x.y.z" + NULL
    TEST(remainingBufferInBytes == (256-position));

    TEST_LOG("Version: %s\n\n", version);

    TEST_LOG("Testing...\n\n");

    // =====================
    // RegisterEventObserver
    // =====================

    AudioEventObserverAPI* eventObserver =
        new AudioEventObserverAPI(audioDevice);

    TEST(audioDevice->RegisterEventObserver(NULL) == 0);
    TEST(audioDevice->RegisterEventObserver(eventObserver) == 0);
    TEST(audioDevice->RegisterEventObserver(NULL) == 0);

    // =====================
    // RegisterAudioCallback
    // =====================

    AudioTransportAPI* audioTransport = new AudioTransportAPI(audioDevice);

    TEST(audioDevice->RegisterAudioCallback(NULL) == 0);
    TEST(audioDevice->RegisterAudioCallback(audioTransport) == 0);
    TEST(audioDevice->RegisterAudioCallback(NULL) == 0);

    // ====
    // Init
    // ====

    TEST(audioDevice->Init() == 0);
    TEST(audioDevice->Initialized() == true);
    TEST(audioDevice->Init() == 0);
    TEST(audioDevice->Initialized() == true);
    TEST(audioDevice->Terminate() == 0);
    TEST(audioDevice->Initialized() == false);
    TEST(audioDevice->Init() == 0);
    TEST(audioDevice->Initialized() == true);
    TEST(audioDevice->Terminate() == 0);
    TEST(audioDevice->Initialized() == false);

    // =========
    // Terminate
    // =========

    TEST(audioDevice->Init() == 0);
    TEST(audioDevice->Initialized() == true);
    TEST(audioDevice->Terminate() == 0);
    TEST(audioDevice->Initialized() == false);
    TEST(audioDevice->Terminate() == 0);
    TEST(audioDevice->Initialized() == false);
    TEST(audioDevice->Init() == 0);
    TEST(audioDevice->Initialized() == true);
    TEST(audioDevice->Terminate() == 0);
    TEST(audioDevice->Initialized() == false);

    // ------------------------------------------------------------------------
    // Ensure that we keep audio device initialized for all API tests:
    //
    TEST(audioDevice->Init() == 0);
    // ------------------------------------------------------------------------

    // goto SHORTCUT;

    WebRtc_Word16 nDevices(0);

    // ==============
    // PlayoutDevices
    // ==============

    TEST((nDevices = audioDevice->PlayoutDevices()) > 0);
    TEST((nDevices = audioDevice->PlayoutDevices()) > 0);

    // ================
    // RecordingDevices
    // ================

    TEST((nDevices = audioDevice->RecordingDevices()) > 0);
    TEST((nDevices = audioDevice->RecordingDevices()) > 0);

    // =================
    // PlayoutDeviceName
    // =================

    WebRtc_Word8 name[kAdmMaxDeviceNameSize];
    WebRtc_Word8 guid[kAdmMaxGuidSize];

    nDevices = audioDevice->PlayoutDevices();

    // fail tests
    TEST(audioDevice->PlayoutDeviceName(-2, name, guid) == -1);
    TEST(audioDevice->PlayoutDeviceName(nDevices, name, guid) == -1);
    TEST(audioDevice->PlayoutDeviceName(0, NULL, guid) == -1);

    // bulk tests
    TEST(audioDevice->PlayoutDeviceName(0, name, NULL) == 0);
#ifdef _WIN32
    TEST(audioDevice->PlayoutDeviceName(-1, name, NULL) == 0); // shall be mapped to 0
#else
    TEST(audioDevice->PlayoutDeviceName(-1, name, NULL) == -1);
#endif
    for (i = 0; i < nDevices; i++)
    {
        TEST(audioDevice->PlayoutDeviceName(i, name, guid) == 0);
        TEST(audioDevice->PlayoutDeviceName(i, name, NULL) == 0);
    }

    // ===================
    // RecordingDeviceName
    // ===================

    nDevices = audioDevice->RecordingDevices();

    // fail tests
    TEST(audioDevice->RecordingDeviceName(-2, name, guid) == -1);
    TEST(audioDevice->RecordingDeviceName(nDevices, name, guid) == -1);
    TEST(audioDevice->RecordingDeviceName(0, NULL, guid) == -1);

    // bulk tests
    TEST(audioDevice->RecordingDeviceName(0, name, NULL) == 0);
#ifdef _WIN32
    TEST(audioDevice->RecordingDeviceName(-1, name, NULL) == 0); // shall me mapped to 0
#else
    TEST(audioDevice->RecordingDeviceName(-1, name, NULL) == -1);
#endif
    for (i = 0; i < nDevices; i++)
    {
        TEST(audioDevice->RecordingDeviceName(i, name, guid) == 0);
        TEST(audioDevice->RecordingDeviceName(i, name, NULL) == 0);
    }

    // ================
    // SetPlayoutDevice
    // ================

    nDevices = audioDevice->PlayoutDevices();

    // fail tests
    TEST(audioDevice->SetPlayoutDevice(-1) == -1);
    TEST(audioDevice->SetPlayoutDevice(nDevices) == -1);

    // bulk tests
#ifdef _WIN32
    TEST(audioDevice->SetPlayoutDevice(AudioDeviceModule::kDefaultCommunicationDevice) == 0);
    TEST(audioDevice->SetPlayoutDevice(AudioDeviceModule::kDefaultDevice) == 0);
#else
    TEST(audioDevice->SetPlayoutDevice(AudioDeviceModule::kDefaultCommunicationDevice) == -1);
    TEST(audioDevice->SetPlayoutDevice(AudioDeviceModule::kDefaultDevice) == -1);
#endif
    for (i = 0; i < nDevices; i++)
    {
        TEST(audioDevice->SetPlayoutDevice(i) == 0);
    }

    // ==================
    // SetRecordingDevice
    // ==================

    nDevices = audioDevice->RecordingDevices();

    // fail tests
    TEST(audioDevice->SetRecordingDevice(-1) == -1);
    TEST(audioDevice->SetRecordingDevice(nDevices) == -1);

    // bulk tests
#ifdef _WIN32
    TEST(audioDevice->SetRecordingDevice(
        AudioDeviceModule::kDefaultCommunicationDevice) == 0);
    TEST(audioDevice->SetRecordingDevice(AudioDeviceModule::kDefaultDevice) == 0);
#else
    TEST(audioDevice->SetRecordingDevice(
        AudioDeviceModule::kDefaultCommunicationDevice) == -1);
    TEST(audioDevice->SetRecordingDevice(
        AudioDeviceModule::kDefaultDevice) == -1);
#endif
    for (i = 0; i < nDevices; i++)
    {
        TEST(audioDevice->SetRecordingDevice(i) == 0);
    }

    // ==================
    // PlayoutIsAvailable
    // ==================

    bool available(false);

    TEST(audioDevice->Terminate() == 0);
    TEST(audioDevice->Init() == 0);

#ifdef _WIN32
    TEST(audioDevice->SetPlayoutDevice(
        AudioDeviceModule::kDefaultCommunicationDevice) == 0);
    TEST(audioDevice->PlayoutIsAvailable(&available) == 0);
    TEST(audioDevice->PlayoutIsInitialized() == false); // availability check should not initialize

    TEST(audioDevice->SetPlayoutDevice(AudioDeviceModule::kDefaultDevice) == 0);
    TEST(audioDevice->PlayoutIsAvailable(&available) == 0);
    TEST(audioDevice->PlayoutIsInitialized() == false);
#endif

    nDevices = audioDevice->PlayoutDevices();
    for (i = 0; i < nDevices; i++)
    {
        TEST(audioDevice->SetPlayoutDevice(i) == 0);
        TEST(audioDevice->PlayoutIsAvailable(&available) == 0);
        TEST(audioDevice->PlayoutIsInitialized() == false);
    }

    // ====================
    // RecordingIsAvailable
    // ====================

    TEST(audioDevice->Terminate() == 0);
    TEST(audioDevice->Init() == 0);

#ifdef _WIN32
    TEST(audioDevice->SetRecordingDevice(AudioDeviceModule::kDefaultCommunicationDevice) == 0);
    TEST(audioDevice->RecordingIsAvailable(&available) == 0);
    TEST(audioDevice->RecordingIsInitialized() == false);

    TEST(audioDevice->SetRecordingDevice(AudioDeviceModule::kDefaultDevice) == 0);
    TEST(audioDevice->RecordingIsAvailable(&available) == 0);
    TEST(audioDevice->RecordingIsInitialized() == false);
#endif

    nDevices = audioDevice->RecordingDevices();
    for (i = 0; i < nDevices; i++)
    {
        TEST(audioDevice->SetRecordingDevice(i) == 0);
        TEST(audioDevice->RecordingIsAvailable(&available) == 0);
        TEST(audioDevice->RecordingIsInitialized() == false);
    }

    // ===========
    // InitPlayout
    // ===========

    TEST(audioDevice->Terminate() == 0);
    TEST(audioDevice->Init() == 0);

    // check initial state
    TEST(audioDevice->PlayoutIsInitialized() == false);

    // ensure that device must be set before we can initialize
    TEST(audioDevice->InitPlayout() == -1);
    TEST(audioDevice->SetPlayoutDevice(MACRO_DEFAULT_DEVICE) == 0);
    TEST(audioDevice->InitPlayout() == 0);
    TEST(audioDevice->PlayoutIsInitialized() == true);

    // bulk tests
    TEST(audioDevice->PlayoutIsAvailable(&available) == 0);
    if (available)
    {
        TEST(audioDevice->InitPlayout() == 0);
        TEST(audioDevice->PlayoutIsInitialized() == true);
        TEST(audioDevice->InitPlayout() == 0);
        TEST(audioDevice->SetPlayoutDevice(MACRO_DEFAULT_COMMUNICATION_DEVICE) == -1);
        TEST(audioDevice->StopPlayout() == 0);
        TEST(audioDevice->PlayoutIsInitialized() == false);
    }

    TEST(audioDevice->SetPlayoutDevice(MACRO_DEFAULT_COMMUNICATION_DEVICE) == 0);
    TEST(audioDevice->PlayoutIsAvailable(&available) == 0);
    if (available)
    {
        TEST(audioDevice->InitPlayout() == 0);
        // Sleep is needed for e.g. iPhone since we after stopping then starting may
        // have a hangover time of a couple of ms before initialized.
        AudioDeviceUtility::Sleep(50);
        TEST(audioDevice->PlayoutIsInitialized() == true);
    }

    nDevices = audioDevice->PlayoutDevices();
    for (i = 0; i < nDevices; i++)
    {
        TEST(audioDevice->PlayoutIsAvailable(&available) == 0);
        if (available)
        {
            TEST(audioDevice->StopPlayout() == 0);
            TEST(audioDevice->PlayoutIsInitialized() == false);
            TEST(audioDevice->SetPlayoutDevice(i) == 0);
            TEST(audioDevice->PlayoutIsAvailable(&available) == 0);
            if (available)
            {
                TEST(audioDevice->InitPlayout() == 0);
                TEST(audioDevice->PlayoutIsInitialized() == true);
            }
        }
    }

    TEST(audioDevice->StopPlayout() == 0);

    // =============
    // InitRecording
    // =============

    TEST(audioDevice->Terminate() == 0);
    TEST(audioDevice->Init() == 0);

    // check initial state
    TEST(audioDevice->RecordingIsInitialized() == false);

    // ensure that device must be set before we can initialize
    TEST(audioDevice->InitRecording() == -1);
    TEST(audioDevice->SetRecordingDevice(MACRO_DEFAULT_DEVICE) == 0);
    TEST(audioDevice->InitRecording() == 0);
    TEST(audioDevice->RecordingIsInitialized() == true);

    // bulk tests
    TEST(audioDevice->RecordingIsAvailable(&available) == 0);
    if (available)
    {
        TEST(audioDevice->InitRecording() == 0);
        TEST(audioDevice->RecordingIsInitialized() == true);
        TEST(audioDevice->InitRecording() == 0);
        TEST(audioDevice->SetRecordingDevice(MACRO_DEFAULT_COMMUNICATION_DEVICE) == -1);
        TEST(audioDevice->StopRecording() == 0);
        TEST(audioDevice->RecordingIsInitialized() == false);
    }

    TEST(audioDevice->SetRecordingDevice(MACRO_DEFAULT_COMMUNICATION_DEVICE) == 0);
    TEST(audioDevice->RecordingIsAvailable(&available) == 0);
    if (available)
    {
        TEST(audioDevice->InitRecording() == 0);
        AudioDeviceUtility::Sleep(50);
        TEST(audioDevice->RecordingIsInitialized() == true);
    }

    nDevices = audioDevice->RecordingDevices();
    for (i = 0; i < nDevices; i++)
    {
        TEST(audioDevice->RecordingIsAvailable(&available) == 0);
        if (available)
        {
            TEST(audioDevice->StopRecording() == 0);
            TEST(audioDevice->RecordingIsInitialized() == false);
            TEST(audioDevice->SetRecordingDevice(i) == 0);
            TEST(audioDevice->RecordingIsAvailable(&available) == 0);
            if (available)
            {
                TEST(audioDevice->InitRecording() == 0);
                TEST(audioDevice->RecordingIsInitialized() == true);
            }
        }
    }

    TEST(audioDevice->StopRecording() == 0);

    // ============
    // StartPlayout
    // StopPlayout
    // ============

    TEST(audioDevice->Terminate() == 0);
    TEST(audioDevice->Init() == 0);

    TEST(audioDevice->RegisterAudioCallback(NULL) == 0);

    // check initial states
    TEST(audioDevice->Initialized() == true);
    TEST(audioDevice->PlayoutIsInitialized() == false);
    TEST(audioDevice->Playing() == false);

    TEST(audioDevice->StartPlayout() == -1);
    TEST(audioDevice->StopPlayout() == 0);

#ifdef _WIN32
    // kDefaultCommunicationDevice
    TEST(audioDevice->SetPlayoutDevice(
        AudioDeviceModule::kDefaultCommunicationDevice) == 0);
    TEST(audioDevice->PlayoutIsAvailable(&available) == 0);
    if (available)
    {
        TEST(audioDevice->PlayoutIsInitialized() == false);
        TEST(audioDevice->InitPlayout() == 0);
        TEST(audioDevice->StartPlayout() == 0);
        TEST(audioDevice->Playing() == true);
        TEST(audioDevice->RegisterAudioCallback(audioTransport) == 0);
        TEST(audioDevice->StopPlayout() == 0);
        TEST(audioDevice->Playing() == false);
        TEST(audioDevice->RegisterAudioCallback(NULL) == 0);
    }
#endif

    // repeat test but for kDefaultDevice
    TEST(audioDevice->SetPlayoutDevice(MACRO_DEFAULT_DEVICE) == 0);
    TEST(audioDevice->PlayoutIsAvailable(&available) == 0);
    if (available)
    {
        TEST(audioDevice->PlayoutIsInitialized() == false);
        TEST(audioDevice->InitPlayout() == 0);
        TEST(audioDevice->StartPlayout() == 0);
        TEST(audioDevice->Playing() == true);
        TEST(audioDevice->RegisterAudioCallback(audioTransport) == 0);
        TEST(audioDevice->StopPlayout() == 0);
        TEST(audioDevice->Playing() == false);
    }

    // repeat test for all devices
    nDevices = audioDevice->PlayoutDevices();
    for (i = 0; i < nDevices; i++)
    {
        TEST(audioDevice->SetPlayoutDevice(i) == 0);
        TEST(audioDevice->PlayoutIsAvailable(&available) == 0);
        if (available)
        {
            TEST(audioDevice->PlayoutIsInitialized() == false);
            TEST(audioDevice->InitPlayout() == 0);
            TEST(audioDevice->StartPlayout() == 0);
            TEST(audioDevice->Playing() == true);
            TEST(audioDevice->RegisterAudioCallback(audioTransport) == 0);
            TEST(audioDevice->StopPlayout() == 0);
            TEST(audioDevice->Playing() == false);
        }
    }

    // ==============
    // StartRecording
    // StopRecording
    // ==============

    TEST(audioDevice->Terminate() == 0);
    TEST(audioDevice->Init() == 0);

    TEST(audioDevice->RegisterAudioCallback(NULL) == 0);

    // check initial states
    TEST(audioDevice->Initialized() == true);
    TEST(audioDevice->RecordingIsInitialized() == false);
    TEST(audioDevice->Recording() == false);

    TEST(audioDevice->StartRecording() == -1);
    TEST(audioDevice->StopRecording() == 0);

#ifdef _WIN32
    // kDefaultCommunicationDevice
    TEST(audioDevice->SetRecordingDevice(
        AudioDeviceModule::kDefaultCommunicationDevice) == 0);
    TEST(audioDevice->RecordingIsAvailable(&available) == 0);
    if (available)
    {
        TEST(audioDevice->RecordingIsInitialized() == false);
        TEST(audioDevice->InitRecording() == 0);
        TEST(audioDevice->StartRecording() == 0);
        TEST(audioDevice->Recording() == true);
        TEST(audioDevice->RegisterAudioCallback(audioTransport) == 0);
        TEST(audioDevice->StopRecording() == 0);
        TEST(audioDevice->Recording() == false);
        TEST(audioDevice->RegisterAudioCallback(NULL) == 0);
    }
#endif

    // repeat test but for kDefaultDevice
    TEST(audioDevice->SetRecordingDevice(MACRO_DEFAULT_DEVICE) == 0);
    TEST(audioDevice->RecordingIsAvailable(&available) == 0);
    if (available)
    {
        TEST(audioDevice->RecordingIsInitialized() == false);
        TEST(audioDevice->InitRecording() == 0);
        TEST(audioDevice->StartRecording() == 0);
        TEST(audioDevice->Recording() == true);
        TEST(audioDevice->RegisterAudioCallback(audioTransport) == 0);
        TEST(audioDevice->StopRecording() == 0);
        TEST(audioDevice->Recording() == false);
    }

    // repeat test for all devices
    nDevices = audioDevice->RecordingDevices();
    for (i = 0; i < nDevices; i++)
    {
        TEST(audioDevice->SetRecordingDevice(i) == 0);
        TEST(audioDevice->RecordingIsAvailable(&available) == 0);
        if (available)
        {
            TEST(audioDevice->RecordingIsInitialized() == false);
            TEST(audioDevice->InitRecording() == 0);
            TEST(audioDevice->StartRecording() == 0);
            TEST(audioDevice->Recording() == true);
            TEST(audioDevice->RegisterAudioCallback(audioTransport) == 0);
            TEST(audioDevice->StopRecording() == 0);
            TEST(audioDevice->Recording() == false);
        }
    }

    WebRtc_UWord32 vol(0);

#if defined(_WIN32) && !defined(WEBRTC_WINDOWS_CORE_AUDIO_BUILD)

    // ================
    // SetWaveOutVolume
    // GetWaveOutVolume
    // ================

    // NOTE 1: Windows Wave only!
    // NOTE 2: It seems like the waveOutSetVolume API returns
    // MMSYSERR_NOTSUPPORTED on some Vista machines!

    const WebRtc_UWord16 maxVol(0xFFFF);
    WebRtc_UWord16 volL, volR;

    TEST(audioDevice->Terminate() == 0);
    TEST(audioDevice->Init() == 0);

    // check initial states
    TEST(audioDevice->Initialized() == true);
    TEST(audioDevice->PlayoutIsInitialized() == false);
    TEST(audioDevice->Playing() == false);

    // make dummy test to see if this API is supported
    WebRtc_Word32 works = audioDevice->SetWaveOutVolume(vol, vol);
    WARNING(works == 0);

    if (works == 0)
    {
        // set volume without open playout device
        for (vol = 0; vol <= maxVol; vol += (maxVol/5))
        {
            TEST(audioDevice->SetWaveOutVolume(vol, vol) == 0);
            TEST(audioDevice->WaveOutVolume(volL, volR) == 0);
            TEST((volL==vol) && (volR==vol));
        }

        // repeat test but this time with an open (default) output device
        TEST(audioDevice->SetPlayoutDevice(AudioDeviceModule::kDefaultDevice) == 0);
        TEST(audioDevice->InitPlayout() == 0);
        TEST(audioDevice->PlayoutIsInitialized() == true);
        for (vol = 0; vol <= maxVol; vol += (maxVol/5))
        {
            TEST(audioDevice->SetWaveOutVolume(vol, vol) == 0);
            TEST(audioDevice->WaveOutVolume(volL, volR) == 0);
            TEST((volL==vol) && (volR==vol));
        }

        // as above but while playout is active
        TEST(audioDevice->StartPlayout() == 0);
        TEST(audioDevice->Playing() == true);
        for (vol = 0; vol <= maxVol; vol += (maxVol/5))
        {
            TEST(audioDevice->SetWaveOutVolume(vol, vol) == 0);
            TEST(audioDevice->WaveOutVolume(volL, volR) == 0);
            TEST((volL==vol) && (volR==vol));
        }
    }

    TEST(audioDevice->StopPlayout() == 0);
    TEST(audioDevice->Playing() == false);

#endif  // defined(_WIN32) && !defined(WEBRTC_WINDOWS_CORE_AUDIO_BUILD)
    // ==================
    // SpeakerIsAvailable
    // ==================

    TEST(audioDevice->Terminate() == 0);
    TEST(audioDevice->Init() == 0);

    // check initial states
    TEST(audioDevice->Initialized() == true);
    TEST(audioDevice->PlayoutIsInitialized() == false);
    TEST(audioDevice->Playing() == false);
    TEST(audioDevice->SpeakerIsInitialized() == false);

#ifdef _WIN32
    // check the kDefaultCommunicationDevice
    TEST(audioDevice->SetPlayoutDevice(
        AudioDeviceModule::kDefaultCommunicationDevice) == 0);
    TEST(audioDevice->SpeakerIsAvailable(&available) == 0);
    // check for availability should not lead to initialization
    TEST(audioDevice->SpeakerIsInitialized() == false);
#endif

    // check the kDefaultDevice
    TEST(audioDevice->SetPlayoutDevice(MACRO_DEFAULT_DEVICE) == 0);
    TEST(audioDevice->SpeakerIsAvailable(&available) == 0);
    TEST(audioDevice->SpeakerIsInitialized() == false);

    // check all availiable devices
    nDevices = audioDevice->PlayoutDevices();
    for (i = 0; i < nDevices; i++)
    {
        TEST(audioDevice->SetPlayoutDevice(i) == 0);
        TEST(audioDevice->SpeakerIsAvailable(&available) == 0);
        TEST(audioDevice->SpeakerIsInitialized() == false);
    }

    // ===========
    // InitSpeaker
    // ===========

    // NOTE: we call Terminate followed by Init to ensure that any existing output mixer
    // handle is set to NULL. The mixer handle is closed and reopend again for each call to
    // SetPlayoutDevice.

    TEST(audioDevice->Terminate() == 0);
    TEST(audioDevice->Init() == 0);

    // check initial states
    TEST(audioDevice->Initialized() == true);
    TEST(audioDevice->PlayoutIsInitialized() == false);
    TEST(audioDevice->Playing() == false);
    TEST(audioDevice->SpeakerIsInitialized() == false);

    // kDefaultCommunicationDevice
    TEST(audioDevice->SetPlayoutDevice(MACRO_DEFAULT_COMMUNICATION_DEVICE) == 0);
    TEST(audioDevice->SpeakerIsAvailable(&available) == 0);
    if (available)
    {
        TEST(audioDevice->InitSpeaker() == 0);
    }

    // fail tests
    TEST(audioDevice->PlayoutIsAvailable(&available) == 0);
    if (available)
    {
        TEST(audioDevice->InitPlayout() == 0);
        TEST(audioDevice->StartPlayout() == 0);
        TEST(audioDevice->InitSpeaker() == -1);
        TEST(audioDevice->StopPlayout() == 0);
    }

    // kDefaultDevice
    TEST(audioDevice->SetPlayoutDevice(MACRO_DEFAULT_DEVICE) == 0);
    TEST(audioDevice->SpeakerIsAvailable(&available) == 0);
    if (available)
    {
        TEST(audioDevice->InitSpeaker() == 0);
    }

    // repeat test for all devices
    nDevices = audioDevice->PlayoutDevices();
    for (i = 0; i < nDevices; i++)
    {
        TEST(audioDevice->SetPlayoutDevice(i) == 0);
        TEST(audioDevice->SpeakerIsAvailable(&available) == 0);
        if (available)
        {
            TEST(audioDevice->InitSpeaker() == 0);
        }
    }

    // =====================
    // MicrophoneIsAvailable
    // =====================

    TEST(audioDevice->Terminate() == 0);
    TEST(audioDevice->Init() == 0);

    // check initial states
    TEST(audioDevice->Initialized() == true);
    TEST(audioDevice->RecordingIsInitialized() == false);
    TEST(audioDevice->Recording() == false);
    TEST(audioDevice->MicrophoneIsInitialized() == false);

#ifdef _WIN32
    // check the kDefaultCommunicationDevice
    TEST(audioDevice->SetRecordingDevice(
        AudioDeviceModule::kDefaultCommunicationDevice) == 0);
    TEST(audioDevice->MicrophoneIsAvailable(&available) == 0);
    // check for availability should not lead to initialization
    TEST(audioDevice->MicrophoneIsInitialized() == false);
#endif

    // check the kDefaultDevice
    TEST(audioDevice->SetRecordingDevice(MACRO_DEFAULT_DEVICE) == 0);
    TEST(audioDevice->MicrophoneIsAvailable(&available) == 0);
    TEST(audioDevice->MicrophoneIsInitialized() == false);

    // check all availiable devices
    nDevices = audioDevice->RecordingDevices();
    for (i = 0; i < nDevices; i++)
    {
        TEST(audioDevice->SetRecordingDevice(i) == 0);
        TEST(audioDevice->MicrophoneIsAvailable(&available) == 0);
        TEST(audioDevice->MicrophoneIsInitialized() == false);
    }

    // ==============
    // InitMicrophone
    // ==============

    // NOTE: we call Terminate followed by Init to ensure that any existing input mixer
    // handle is set to NULL. The mixer handle is closed and reopend again for each call to
    // SetRecordingDevice.

    TEST(audioDevice->Terminate() == 0);
    TEST(audioDevice->Init() == 0);

    // check initial states
    TEST(audioDevice->Initialized() == true);
    TEST(audioDevice->RecordingIsInitialized() == false);
    TEST(audioDevice->Recording() == false);
    TEST(audioDevice->MicrophoneIsInitialized() == false);

    // kDefaultCommunicationDevice
    TEST(audioDevice->SetRecordingDevice(MACRO_DEFAULT_COMMUNICATION_DEVICE) == 0);
    TEST(audioDevice->MicrophoneIsAvailable(&available) == 0);
    if (available)
    {
        TEST(audioDevice->InitMicrophone() == 0);
    }

    // fail tests
    TEST(audioDevice->RecordingIsAvailable(&available) == 0);
    if (available)
    {
        TEST(audioDevice->InitRecording() == 0);
        TEST(audioDevice->StartRecording() == 0);
        TEST(audioDevice->InitMicrophone() == -1);
        TEST(audioDevice->StopRecording() == 0);
    }

    // kDefaultDevice
    TEST(audioDevice->SetRecordingDevice(MACRO_DEFAULT_DEVICE) == 0);
    TEST(audioDevice->MicrophoneIsAvailable(&available) == 0);
    if (available)
    {
        TEST(audioDevice->InitMicrophone() == 0);
    }

    // repeat test for all devices
    nDevices = audioDevice->RecordingDevices();
    for (i = 0; i < nDevices; i++)
    {
        TEST(audioDevice->SetRecordingDevice(i) == 0);
        TEST(audioDevice->MicrophoneIsAvailable(&available) == 0);
        if (available)
        {
            TEST(audioDevice->InitMicrophone() == 0);
        }
    }

    // ========================
    // SpeakerVolumeIsAvailable
    // ========================

    TEST(audioDevice->Terminate() == 0);
    TEST(audioDevice->Init() == 0);

    // check initial states
    TEST(audioDevice->Initialized() == true);
    TEST(audioDevice->PlayoutIsInitialized() == false);
    TEST(audioDevice->Playing() == false);
    TEST(audioDevice->SpeakerIsInitialized() == false);

#ifdef _WIN32
    // check the kDefaultCommunicationDevice
    TEST(audioDevice->SetPlayoutDevice(
        AudioDeviceModule::kDefaultCommunicationDevice) == 0);
    TEST(audioDevice->SpeakerVolumeIsAvailable(&available) == 0);
    // check for availability should not lead to initialization
    TEST(audioDevice->SpeakerIsInitialized() == false);
#endif

    // check the kDefaultDevice
    TEST(audioDevice->SetPlayoutDevice(MACRO_DEFAULT_DEVICE) == 0);
    TEST(audioDevice->SpeakerVolumeIsAvailable(&available) == 0);
    TEST(audioDevice->SpeakerIsInitialized() == false);

    // check all availiable devices
    nDevices = audioDevice->PlayoutDevices();
    for (i = 0; i < nDevices; i++)
    {
        TEST(audioDevice->SetPlayoutDevice(i) == 0);
        TEST(audioDevice->SpeakerVolumeIsAvailable(&available) == 0);
        TEST(audioDevice->SpeakerIsInitialized() == false);
    }

    // ================
    // SetSpeakerVolume
    // SpeakerVolume
    // MaxSpeakerVolume
    // MinSpeakerVolume
    // ================

    WebRtc_UWord32 volume(0);
    WebRtc_UWord32 maxVolume(0);
    WebRtc_UWord32 minVolume(0);
    WebRtc_UWord16 stepSize(0);

    TEST(audioDevice->Terminate() == 0);
    TEST(audioDevice->Init() == 0);

    // check initial states
    TEST(audioDevice->Initialized() == true);
    TEST(audioDevice->PlayoutIsInitialized() == false);
    TEST(audioDevice->Playing() == false);
    TEST(audioDevice->SpeakerIsInitialized() == false);

    // fail tests
    TEST(audioDevice->SetSpeakerVolume(0) == -1); // speaker must be initialized first
    TEST(audioDevice->SpeakerVolume(&volume) == -1);
    TEST(audioDevice->MaxSpeakerVolume(&maxVolume) == -1);
    TEST(audioDevice->MinSpeakerVolume(&minVolume) == -1);
    TEST(audioDevice->SpeakerVolumeStepSize(&stepSize) == -1);

#if defined(_WIN32) && !defined(WEBRTC_WINDOWS_CORE_AUDIO_BUILD)
    // test for warning (can e.g. happen on Vista with Wave API)
    TEST(audioDevice->SetPlayoutDevice(AudioDeviceModule::kDefaultDevice) == 0);
    TEST(audioDevice->SpeakerVolumeIsAvailable(&available) == 0);
    if (available)
    {
        TEST(audioDevice->InitSpeaker() == 0);
        TEST(audioDevice->SetSpeakerVolume(19001) == 0);
        TEST(audioDevice->SpeakerVolume(&volume) == 0);
        WARNING(volume == 19001);
    }
#endif

#ifdef _WIN32
    // use kDefaultCommunicationDevice and modify/retrieve the volume
    TEST(audioDevice->SetPlayoutDevice(
        AudioDeviceModule::kDefaultCommunicationDevice) == 0);
    TEST(audioDevice->SpeakerVolumeIsAvailable(&available) == 0);
    if (available)
    {
        TEST(audioDevice->InitSpeaker() == 0);
        TEST(audioDevice->MaxSpeakerVolume(&maxVolume) == 0);
        TEST(audioDevice->MinSpeakerVolume(&minVolume) == 0);
        TEST(audioDevice->SpeakerVolumeStepSize(&stepSize) == 0);
        for (vol = minVolume; vol < (int)maxVolume; vol += 20*stepSize)
        {
            TEST(audioDevice->SetSpeakerVolume(vol) == 0);
            TEST(audioDevice->SpeakerVolume(&volume) == 0);
            TEST((volume == vol) || (volume == vol-1));
        }
    }
#endif

    // use kDefaultDevice and modify/retrieve the volume
    TEST(audioDevice->SetPlayoutDevice(MACRO_DEFAULT_DEVICE) == 0);
    TEST(audioDevice->SpeakerVolumeIsAvailable(&available) == 0);
    if (available)
    {
        TEST(audioDevice->InitSpeaker() == 0);
        TEST(audioDevice->MaxSpeakerVolume(&maxVolume) == 0);
        TEST(audioDevice->MinSpeakerVolume(&minVolume) == 0);
        TEST(audioDevice->SpeakerVolumeStepSize(&stepSize) == 0);
        WebRtc_UWord32 step = (maxVolume - minVolume) / 10;
        step = (step < stepSize ? stepSize : step);
        for (vol = minVolume; vol <= maxVolume; vol += step)
        {
            TEST(audioDevice->SetSpeakerVolume(vol) == 0);
            TEST(audioDevice->SpeakerVolume(&volume) == 0);
            TEST((volume == vol) || (volume == vol-1));
        }
    }

    // use all (indexed) devices and modify/retrieve the volume
    nDevices = audioDevice->PlayoutDevices();
    for (i = 0; i < nDevices; i++)
    {
        TEST(audioDevice->SetPlayoutDevice(i) == 0);
        TEST(audioDevice->SpeakerVolumeIsAvailable(&available) == 0);
        if (available)
        {
            TEST(audioDevice->InitSpeaker() == 0);
            TEST(audioDevice->MaxSpeakerVolume(&maxVolume) == 0);
            TEST(audioDevice->MinSpeakerVolume(&minVolume) == 0);
            TEST(audioDevice->SpeakerVolumeStepSize(&stepSize) == 0);
            WebRtc_UWord32 step = (maxVolume - minVolume) / 10;
            step = (step < stepSize ? stepSize : step);
            for (vol = minVolume; vol <= maxVolume; vol += step)
            {
                TEST(audioDevice->SetSpeakerVolume(vol) == 0);
                TEST(audioDevice->SpeakerVolume(&volume) == 0);
                TEST((volume == vol) || (volume == vol-1));
            }
        }
    }

    // restore reasonable level
    TEST(audioDevice->SetPlayoutDevice(MACRO_DEFAULT_DEVICE) == 0);
    TEST(audioDevice->SpeakerVolumeIsAvailable(&available) == 0);
    if (available)
    {
        TEST(audioDevice->InitSpeaker() == 0);
        TEST(audioDevice->MaxSpeakerVolume(&maxVolume) == 0);
        TEST(audioDevice->SetSpeakerVolume(maxVolume < 10 ?
            maxVolume/3 : maxVolume/10) == 0);
    }

    // ======
    // SetAGC
    // AGC
    // ======

    // NOTE: The AGC API only enables/disables the AGC. To ensure that it will
    // have an effect, use it in combination with MicrophoneVolumeIsAvailable.

    TEST(audioDevice->Terminate() == 0);
    TEST(audioDevice->Init() == 0);

    // check initial states
    TEST(audioDevice->Initialized() == true);
    TEST(audioDevice->RecordingIsInitialized() == false);
    TEST(audioDevice->Recording() == false);
    TEST(audioDevice->MicrophoneIsInitialized() == false);
    TEST(audioDevice->AGC() == false);

    // set/get tests
    TEST(audioDevice->SetAGC(true) == 0);
    TEST(audioDevice->AGC() == true);
    TEST(audioDevice->SetAGC(false) == 0);
    TEST(audioDevice->AGC() == false);

    // ===========================
    // MicrophoneVolumeIsAvailable
    // ===========================

    TEST(audioDevice->Terminate() == 0);
    TEST(audioDevice->Init() == 0);

    // check initial states
    TEST(audioDevice->Initialized() == true);
    TEST(audioDevice->RecordingIsInitialized() == false);
    TEST(audioDevice->Recording() == false);
    TEST(audioDevice->MicrophoneIsInitialized() == false);

#ifdef _WIN32
    // check the kDefaultCommunicationDevice
    TEST(audioDevice->SetRecordingDevice(
        AudioDeviceModule::kDefaultCommunicationDevice) == 0);
    TEST(audioDevice->MicrophoneVolumeIsAvailable(&available) == 0);
    // check for availability should not lead to initialization
    TEST(audioDevice->MicrophoneIsInitialized() == false);
#endif

    // check the kDefaultDevice
    TEST(audioDevice->SetRecordingDevice(MACRO_DEFAULT_DEVICE) == 0);
    TEST(audioDevice->MicrophoneVolumeIsAvailable(&available) == 0);
    TEST(audioDevice->MicrophoneIsInitialized() == false);

    // check all availiable devices
    nDevices = audioDevice->RecordingDevices();
    for (i = 0; i < nDevices; i++)
    {
        TEST(audioDevice->SetRecordingDevice(i) == 0);
        TEST(audioDevice->MicrophoneVolumeIsAvailable(&available) == 0);
        TEST(audioDevice->MicrophoneIsInitialized() == false);
    }

    // ===================
    // SetMicrophoneVolume
    // MicrophoneVolume
    // MaxMicrophoneVolume
    // MinMicrophoneVolume
    // ===================

    TEST(audioDevice->Terminate() == 0);
    TEST(audioDevice->Init() == 0);

    // check initial states
    TEST(audioDevice->Initialized() == true);
    TEST(audioDevice->RecordingIsInitialized() == false);
    TEST(audioDevice->Recording() == false);
    TEST(audioDevice->MicrophoneIsInitialized() == false);

    // fail tests
    TEST(audioDevice->SetMicrophoneVolume(0) == -1); // must be initialized first
    TEST(audioDevice->MicrophoneVolume(&volume) == -1);
    TEST(audioDevice->MaxMicrophoneVolume(&maxVolume) == -1);
    TEST(audioDevice->MinMicrophoneVolume(&minVolume) == -1);
    TEST(audioDevice->MicrophoneVolumeStepSize(&stepSize) == -1);

#if defined(_WIN32) && !defined(WEBRTC_WINDOWS_CORE_AUDIO_BUILD)
    // test for warning (can e.g. happen on Vista with Wave API)
    TEST(audioDevice->SetRecordingDevice(AudioDeviceModule::kDefaultDevice) == 0);
    TEST(audioDevice->MicrophoneVolumeIsAvailable(&available) == 0);
    if (available)
    {
        TEST(audioDevice->InitMicrophone() == 0);
        TEST(audioDevice->SetMicrophoneVolume(19001) == 0);
        TEST(audioDevice->MicrophoneVolume(&volume) == 0);
        WARNING(volume == 19001);
    }
#endif

#ifdef _WIN32
    // initialize kDefaultCommunicationDevice and modify/retrieve the volume
    TEST(audioDevice->SetRecordingDevice(
        AudioDeviceModule::kDefaultCommunicationDevice) == 0);
    TEST(audioDevice->MicrophoneVolumeIsAvailable(&available) == 0);
    if (available)
    {
        TEST(audioDevice->InitMicrophone() == 0);
        TEST(audioDevice->MaxMicrophoneVolume(&maxVolume) == 0);
        TEST(audioDevice->MinMicrophoneVolume(&minVolume) == 0);
        TEST(audioDevice->MicrophoneVolumeStepSize(&stepSize) == 0);
        for (vol = minVolume; vol < (int)maxVolume; vol += 10*stepSize)
        {
            TEST(audioDevice->SetMicrophoneVolume(vol) == 0);
            TEST(audioDevice->MicrophoneVolume(&volume) == 0);
            TEST((volume == vol) || (volume == vol-1));
        }
    }
#endif

    // reinitialize kDefaultDevice and modify/retrieve the volume
    TEST(audioDevice->SetRecordingDevice(MACRO_DEFAULT_DEVICE) == 0);
    TEST(audioDevice->MicrophoneVolumeIsAvailable(&available) == 0);
    if (available)
    {
        TEST(audioDevice->InitMicrophone() == 0);
        TEST(audioDevice->MaxMicrophoneVolume(&maxVolume) == 0);
        TEST(audioDevice->MinMicrophoneVolume(&minVolume) == 0);
        TEST(audioDevice->MicrophoneVolumeStepSize(&stepSize) == 0);
        for (vol = minVolume; vol < maxVolume; vol += 10 * stepSize)
        {
            TEST(audioDevice->SetMicrophoneVolume(vol) == 0);
            TEST(audioDevice->MicrophoneVolume(&volume) == 0);
            TEST((volume == vol) || (volume == vol-1));
        }
    }

    // use all (indexed) devices and modify/retrieve the volume
    nDevices = audioDevice->RecordingDevices();
    for (i = 0; i < nDevices; i++)
    {
        TEST(audioDevice->SetRecordingDevice(i) == 0);
        TEST(audioDevice->MicrophoneVolumeIsAvailable(&available) == 0);
        if (available)
        {
            TEST(audioDevice->InitMicrophone() == 0);
            TEST(audioDevice->MaxMicrophoneVolume(&maxVolume) == 0);
            TEST(audioDevice->MinMicrophoneVolume(&minVolume) == 0);
            TEST(audioDevice->MicrophoneVolumeStepSize(&stepSize) == 0);
            for (vol = minVolume; vol < maxVolume; vol += 20 * stepSize)
            {
                TEST(audioDevice->SetMicrophoneVolume(vol) == 0);
                TEST(audioDevice->MicrophoneVolume(&volume) == 0);
                TEST((volume == vol) || (volume == vol-1));
            }
        }
    }

    // restore reasonable level
    TEST(audioDevice->SetRecordingDevice(MACRO_DEFAULT_DEVICE) == 0);
    TEST(audioDevice->MicrophoneVolumeIsAvailable(&available) == 0);
    if (available)
    {
        TEST(audioDevice->InitMicrophone() == 0);
        TEST(audioDevice->MaxMicrophoneVolume(&maxVolume) == 0);
        TEST(audioDevice->SetMicrophoneVolume(maxVolume/10) == 0);
    }

    // ======================
    // SpeakerMuteIsAvailable
    // ======================

    TEST(audioDevice->Terminate() == 0);
    TEST(audioDevice->Init() == 0);

    // check initial states
    TEST(audioDevice->Initialized() == true);
    TEST(audioDevice->PlayoutIsInitialized() == false);
    TEST(audioDevice->Playing() == false);
    TEST(audioDevice->SpeakerIsInitialized() == false);

#ifdef _WIN32
    // check the kDefaultCommunicationDevice
    TEST(audioDevice->SetPlayoutDevice(
        AudioDeviceModule::kDefaultCommunicationDevice) == 0);
    TEST(audioDevice->SpeakerMuteIsAvailable(&available) == 0);
    // check for availability should not lead to initialization
    TEST(audioDevice->SpeakerIsInitialized() == false);
#endif

    // check the kDefaultDevice
    TEST(audioDevice->SetPlayoutDevice(MACRO_DEFAULT_DEVICE) == 0);
    TEST(audioDevice->SpeakerMuteIsAvailable(&available) == 0);
    TEST(audioDevice->SpeakerIsInitialized() == false);

    // check all availiable devices
    nDevices = audioDevice->PlayoutDevices();
    for (i = 0; i < nDevices; i++)
    {
        TEST(audioDevice->SetPlayoutDevice(i) == 0);
        TEST(audioDevice->SpeakerMuteIsAvailable(&available) == 0);
        TEST(audioDevice->SpeakerIsInitialized() == false);
    }

    // =========================
    // MicrophoneMuteIsAvailable
    // =========================

    TEST(audioDevice->Terminate() == 0);
    TEST(audioDevice->Init() == 0);

    // check initial states
    TEST(audioDevice->Initialized() == true);
    TEST(audioDevice->RecordingIsInitialized() == false);
    TEST(audioDevice->Recording() == false);
    TEST(audioDevice->MicrophoneIsInitialized() == false);

#ifdef _WIN32
    // check the kDefaultCommunicationDevice
    TEST(audioDevice->SetRecordingDevice(
        AudioDeviceModule::kDefaultCommunicationDevice) == 0);
    TEST(audioDevice->MicrophoneMuteIsAvailable(&available) == 0);
    // check for availability should not lead to initialization
    #endif
    TEST(audioDevice->MicrophoneIsInitialized() == false);

    // check the kDefaultDevice
    TEST(audioDevice->SetRecordingDevice(MACRO_DEFAULT_DEVICE) == 0);
    TEST(audioDevice->MicrophoneMuteIsAvailable(&available) == 0);
    TEST(audioDevice->MicrophoneIsInitialized() == false);

    // check all availiable devices
    nDevices = audioDevice->RecordingDevices();
    for (i = 0; i < nDevices; i++)
    {
        TEST(audioDevice->SetRecordingDevice(i) == 0);
        TEST(audioDevice->MicrophoneMuteIsAvailable(&available) == 0);
        TEST(audioDevice->MicrophoneIsInitialized() == false);
    }

    // ==========================
    // MicrophoneBoostIsAvailable
    // ==========================

    TEST(audioDevice->Terminate() == 0);
    TEST(audioDevice->Init() == 0);

    // check initial states
    TEST(audioDevice->Initialized() == true);
    TEST(audioDevice->RecordingIsInitialized() == false);
    TEST(audioDevice->Recording() == false);
    TEST(audioDevice->MicrophoneIsInitialized() == false);

#ifdef _WIN32
    // check the kDefaultCommunicationDevice
    TEST(audioDevice->SetRecordingDevice(
        AudioDeviceModule::kDefaultCommunicationDevice) == 0);
    TEST(audioDevice->MicrophoneBoostIsAvailable(&available) == 0);
    // check for availability should not lead to initialization
    TEST(audioDevice->MicrophoneIsInitialized() == false);
#endif

    // check the kDefaultDevice
    TEST(audioDevice->SetRecordingDevice(MACRO_DEFAULT_DEVICE) == 0);
    TEST(audioDevice->MicrophoneBoostIsAvailable(&available) == 0);
    TEST(audioDevice->MicrophoneIsInitialized() == false);

    // check all availiable devices
    nDevices = audioDevice->RecordingDevices();
    for (i = 0; i < nDevices; i++)
    {
        TEST(audioDevice->SetRecordingDevice(i) == 0);
        TEST(audioDevice->MicrophoneBoostIsAvailable(&available) == 0);
        TEST(audioDevice->MicrophoneIsInitialized() == false);
    }

    // ==============
    // SetSpeakerMute
    // SpeakerMute
    // ==============

    bool enabled(false);

    TEST(audioDevice->Terminate() == 0);
    TEST(audioDevice->Init() == 0);

    // check initial states
    TEST(audioDevice->Initialized() == true);
    TEST(audioDevice->PlayoutIsInitialized() == false);
    TEST(audioDevice->Playing() == false);
    TEST(audioDevice->SpeakerIsInitialized() == false);

    // fail tests
    TEST(audioDevice->SetSpeakerMute(true) == -1); // requires initialization
    TEST(audioDevice->SpeakerMute(&enabled) == -1);

#ifdef _WIN32
    // initialize kDefaultCommunicationDevice and modify/retrieve the mute state
    TEST(audioDevice->SetPlayoutDevice(AudioDeviceModule::kDefaultCommunicationDevice) == 0);
    TEST(audioDevice->SpeakerMuteIsAvailable(&available) == 0);
    if (available)
    {
        TEST(audioDevice->InitSpeaker() == 0);
        TEST(audioDevice->SetSpeakerMute(true) == 0);
        TEST(audioDevice->SpeakerMute(&enabled) == 0);
        TEST(enabled == true);
        TEST(audioDevice->SetSpeakerMute(false) == 0);
        TEST(audioDevice->SpeakerMute(&enabled) == 0);
        TEST(enabled == false);
    }
#endif

    // reinitialize kDefaultDevice and modify/retrieve the mute state
    TEST(audioDevice->SetPlayoutDevice(MACRO_DEFAULT_DEVICE) == 0);
    TEST(audioDevice->SpeakerMuteIsAvailable(&available) == 0);
    if (available)
    {
        TEST(audioDevice->InitSpeaker() == 0);
        TEST(audioDevice->SetSpeakerMute(true) == 0);
        TEST(audioDevice->SpeakerMute(&enabled) == 0);
        TEST(enabled == true);
        TEST(audioDevice->SetSpeakerMute(false) == 0);
        TEST(audioDevice->SpeakerMute(&enabled) == 0);
        TEST(enabled == false);
    }

    // reinitialize the default device (0) and modify/retrieve the mute state
    TEST(audioDevice->SetPlayoutDevice(0) == 0);
    TEST(audioDevice->SpeakerMuteIsAvailable(&available) == 0);
    if (available)
    {
        TEST(audioDevice->InitSpeaker() == 0);
        TEST(audioDevice->SetSpeakerMute(true) == 0);
        TEST(audioDevice->SpeakerMute(&enabled) == 0);
        TEST(enabled == true);
        TEST(audioDevice->SetSpeakerMute(false) == 0);
        TEST(audioDevice->SpeakerMute(&enabled) == 0);
        TEST(enabled == false);
    }

    // ==================
    // SetMicrophoneMute
    // MicrophoneMute
    // ==================

    TEST(audioDevice->Terminate() == 0);
    TEST(audioDevice->Init() == 0);

    // check initial states
    TEST(audioDevice->Initialized() == true);
    TEST(audioDevice->RecordingIsInitialized() == false);
    TEST(audioDevice->Recording() == false);
    TEST(audioDevice->MicrophoneIsInitialized() == false);

    // fail tests
    TEST(audioDevice->SetMicrophoneMute(true) == -1); // requires initialization
    TEST(audioDevice->MicrophoneMute(&enabled) == -1);

#ifdef _WIN32
    // initialize kDefaultCommunicationDevice and modify/retrieve the mute
    TEST(audioDevice->SetRecordingDevice(
        AudioDeviceModule::kDefaultCommunicationDevice) == 0);
    TEST(audioDevice->MicrophoneMuteIsAvailable(&available) == 0);
    if (available)
    {
        TEST(audioDevice->InitMicrophone() == 0);
        TEST(audioDevice->SetMicrophoneMute(true) == 0);
        TEST(audioDevice->MicrophoneMute(&enabled) == 0);
        TEST(enabled == true);
        TEST(audioDevice->SetMicrophoneMute(false) == 0);
        TEST(audioDevice->MicrophoneMute(&enabled) == 0);
        TEST(enabled == false);
    }
#endif

    // reinitialize kDefaultDevice and modify/retrieve the mute
    TEST(audioDevice->SetRecordingDevice(MACRO_DEFAULT_DEVICE) == 0);
    TEST(audioDevice->MicrophoneMuteIsAvailable(&available) == 0);
    if (available)
    {
        TEST(audioDevice->InitMicrophone() == 0);
        TEST(audioDevice->SetMicrophoneMute(true) == 0);
        TEST(audioDevice->MicrophoneMute(&enabled) == 0);
        TEST(enabled == true);
        TEST(audioDevice->SetMicrophoneMute(false) == 0);
        TEST(audioDevice->MicrophoneMute(&enabled) == 0);
        TEST(enabled == false);
    }

    // reinitialize the default device (0) and modify/retrieve the Mute
    TEST(audioDevice->SetRecordingDevice(0) == 0);
    TEST(audioDevice->MicrophoneMuteIsAvailable(&available) == 0);
    if (available)
    {
        TEST(audioDevice->InitMicrophone() == 0);
        TEST(audioDevice->SetMicrophoneMute(true) == 0);
        TEST(audioDevice->MicrophoneMute(&enabled) == 0);
        TEST(enabled == true);
        TEST(audioDevice->SetMicrophoneMute(false) == 0);
        TEST(audioDevice->MicrophoneMute(&enabled) == 0);
        TEST(enabled == false);
    }

    // ==================
    // SetMicrophoneBoost
    // MicrophoneBoost
    // ==================

    TEST(audioDevice->Terminate() == 0);
    TEST(audioDevice->Init() == 0);

    // check initial states
    TEST(audioDevice->Initialized() == true);
    TEST(audioDevice->RecordingIsInitialized() == false);
    TEST(audioDevice->Recording() == false);
    TEST(audioDevice->MicrophoneIsInitialized() == false);

    // fail tests
    TEST(audioDevice->SetMicrophoneBoost(true) == -1); // requires initialization
    TEST(audioDevice->MicrophoneBoost(&enabled) == -1);

#ifdef _WIN32
    // initialize kDefaultCommunicationDevice and modify/retrieve the boost
    TEST(audioDevice->SetRecordingDevice(
        AudioDeviceModule::kDefaultCommunicationDevice) == 0);
    TEST(audioDevice->MicrophoneBoostIsAvailable(&available) == 0);
    if (available)
    {
        TEST(audioDevice->InitMicrophone() == 0);
        TEST(audioDevice->SetMicrophoneBoost(true) == 0);
        TEST(audioDevice->MicrophoneBoost(&enabled) == 0);
        TEST(enabled == true);
        TEST(audioDevice->SetMicrophoneBoost(false) == 0);
        TEST(audioDevice->MicrophoneBoost(&enabled) == 0);
        TEST(enabled == false);
    }
#endif

    // reinitialize kDefaultDevice and modify/retrieve the boost
    TEST(audioDevice->SetRecordingDevice(MACRO_DEFAULT_DEVICE) == 0);
    TEST(audioDevice->MicrophoneBoostIsAvailable(&available) == 0);
    if (available)
    {
        TEST(audioDevice->InitMicrophone() == 0);
        TEST(audioDevice->SetMicrophoneBoost(true) == 0);
        TEST(audioDevice->MicrophoneBoost(&enabled) == 0);
        TEST(enabled == true);
        TEST(audioDevice->SetMicrophoneBoost(false) == 0);
        TEST(audioDevice->MicrophoneBoost(&enabled) == 0);
        TEST(enabled == false);
    }

    // reinitialize the default device (0) and modify/retrieve the boost
    TEST(audioDevice->SetRecordingDevice(0) == 0);
    TEST(audioDevice->MicrophoneBoostIsAvailable(&available) == 0);
    if (available)
    {
        TEST(audioDevice->InitMicrophone() == 0);
        TEST(audioDevice->SetMicrophoneBoost(true) == 0);
        TEST(audioDevice->MicrophoneBoost(&enabled) == 0);
        TEST(enabled == true);
        TEST(audioDevice->SetMicrophoneBoost(false) == 0);
        TEST(audioDevice->MicrophoneBoost(&enabled) == 0);
        TEST(enabled == false);
    }

    // ================
    // SetStereoPlayout
    // StereoPlayout
    // ================

    TEST(audioDevice->Terminate() == 0);
    TEST(audioDevice->Init() == 0);

    // check initial states
    TEST(audioDevice->Initialized() == true);
    TEST(audioDevice->PlayoutIsInitialized() == false);
    TEST(audioDevice->Playing() == false);

    // fail tests
    TEST(audioDevice->InitPlayout() == -1);
    TEST(audioDevice->SetPlayoutDevice(MACRO_DEFAULT_COMMUNICATION_DEVICE) == 0);
    TEST(audioDevice->InitPlayout() == 0);
    TEST(audioDevice->PlayoutIsInitialized() == true);
    // must be performed before initialization
    TEST(audioDevice->SetStereoPlayout(true) == -1);

    // ensure that we can set the stereo mode for playout
    TEST(audioDevice->StopPlayout() == 0);
    TEST(audioDevice->PlayoutIsInitialized() == false);

    // initialize kDefaultCommunicationDevice and modify/retrieve stereo support
    TEST(audioDevice->SetPlayoutDevice(MACRO_DEFAULT_COMMUNICATION_DEVICE) == 0);
    TEST(audioDevice->StereoPlayoutIsAvailable(&available) == 0);
    if (available)
    {
        TEST(audioDevice->SetStereoPlayout(true) == 0);
        TEST(audioDevice->StereoPlayout(&enabled) == 0);
        TEST(enabled == true);
        TEST(audioDevice->SetStereoPlayout(false) == 0);
        TEST(audioDevice->StereoPlayout(&enabled) == 0);
        TEST(enabled == false);
        TEST(audioDevice->SetStereoPlayout(true) == 0);
        TEST(audioDevice->StereoPlayout(&enabled) == 0);
        TEST(enabled == true);
    }

    // initialize kDefaultDevice and modify/retrieve stereo support
    TEST(audioDevice->SetPlayoutDevice(MACRO_DEFAULT_DEVICE) == 0);
    TEST(audioDevice->StereoPlayoutIsAvailable(&available) == 0);
    if (available)
    {
        TEST(audioDevice->SetStereoPlayout(true) == 0);
        TEST(audioDevice->StereoPlayout(&enabled) == 0);
        TEST(enabled == true);
        TEST(audioDevice->SetStereoPlayout(false) == 0);
        TEST(audioDevice->StereoPlayout(&enabled) == 0);
        TEST(enabled == false);
        TEST(audioDevice->SetStereoPlayout(true) == 0);
        TEST(audioDevice->StereoPlayout(&enabled) == 0);
        TEST(enabled == true);
    }

    // initialize default device (0) and modify/retrieve stereo support
    TEST(audioDevice->SetPlayoutDevice(0) == 0);
    TEST(audioDevice->StereoPlayoutIsAvailable(&available) == 0);
    if (available)
    {
        TEST(audioDevice->SetStereoPlayout(true) == 0);
        TEST(audioDevice->StereoPlayout(&enabled) == 0);
        TEST(enabled == true);
        TEST(audioDevice->SetStereoPlayout(false) == 0);
        TEST(audioDevice->StereoPlayout(&enabled) == 0);
        TEST(enabled == false);
        TEST(audioDevice->SetStereoPlayout(true) == 0);
        TEST(audioDevice->StereoPlayout(&enabled) == 0);
        TEST(enabled == true);
    }

    // ==================
    // SetStereoRecording
    // StereoRecording
    // ==================

    TEST(audioDevice->Terminate() == 0);
    TEST(audioDevice->Init() == 0);

    // check initial states
    TEST(audioDevice->Initialized() == true);
    TEST(audioDevice->RecordingIsInitialized() == false);
    TEST(audioDevice->Playing() == false);

    // fail tests
    TEST(audioDevice->InitRecording() == -1);
    TEST(audioDevice->SetRecordingDevice(MACRO_DEFAULT_COMMUNICATION_DEVICE) == 0);
    TEST(audioDevice->InitRecording() == 0);
    TEST(audioDevice->RecordingIsInitialized() == true);
    // must be performed before initialization
    TEST(audioDevice->SetStereoRecording(true) == -1);

    // ensures that we can set the stereo mode for recording
    TEST(audioDevice->StopRecording() == 0);
    TEST(audioDevice->RecordingIsInitialized() == false);

    // initialize kDefaultCommunicationDevice and modify/retrieve stereo support
    TEST(audioDevice->SetRecordingDevice(MACRO_DEFAULT_COMMUNICATION_DEVICE) == 0);
    TEST(audioDevice->StereoRecordingIsAvailable(&available) == 0);
    if (available)
    {
        TEST(audioDevice->SetStereoRecording(true) == 0);
        TEST(audioDevice->StereoRecording(&enabled) == 0);
        TEST(enabled == true);
        TEST(audioDevice->SetStereoRecording(false) == 0);
        TEST(audioDevice->StereoRecording(&enabled) == 0);
        TEST(enabled == false);
    }

    // initialize kDefaultDevice and modify/retrieve stereo support
    TEST(audioDevice->SetRecordingDevice(MACRO_DEFAULT_DEVICE) == 0);
    TEST(audioDevice->StereoRecordingIsAvailable(&available) == 0);
    if (available)
    {
        TEST(audioDevice->SetStereoRecording(true) == 0);
        TEST(audioDevice->StereoRecording(&enabled) == 0);
        TEST(enabled == true);
        TEST(audioDevice->SetStereoRecording(false) == 0);
        TEST(audioDevice->StereoRecording(&enabled) == 0);
        TEST(enabled == false);
    }

    // initialize default device (0) and modify/retrieve stereo support
    TEST(audioDevice->SetRecordingDevice(0) == 0);
    TEST(audioDevice->StereoRecordingIsAvailable(&available) == 0);
    if (available)
    {
        TEST(audioDevice->SetStereoRecording(true) == 0);
        TEST(audioDevice->StereoRecording(&enabled) == 0);
        TEST(enabled == true);
        TEST(audioDevice->SetStereoRecording(false) == 0);
        TEST(audioDevice->StereoRecording(&enabled) == 0);
        TEST(enabled == false);
    }

    // ===================
    // SetRecordingChannel
    // RecordingChannel
    // ==================

    // the user in Win Core Audio

    AudioDeviceModule::ChannelType channelType(AudioDeviceModule::kChannelBoth);

    TEST(audioDevice->Terminate() == 0);
    TEST(audioDevice->Init() == 0);

    // check initial states
    TEST(audioDevice->Initialized() == true);
    TEST(audioDevice->RecordingIsInitialized() == false);
    TEST(audioDevice->Playing() == false);

    // fail tests
    TEST(audioDevice->SetStereoRecording(false) == 0);
    TEST(audioDevice->SetRecordingChannel(AudioDeviceModule::kChannelBoth) == -1);

    // initialize kDefaultCommunicationDevice and modify/retrieve stereo support
    TEST(audioDevice->SetRecordingDevice(MACRO_DEFAULT_COMMUNICATION_DEVICE) == 0);
    TEST(audioDevice->StereoRecordingIsAvailable(&available) == 0);
    if (available)
    {
        TEST(audioDevice->SetStereoRecording(true) == 0);
        TEST(audioDevice->SetRecordingChannel(AudioDeviceModule::kChannelBoth) == 0);
        TEST(audioDevice->RecordingChannel(&channelType) == 0);
        TEST(channelType == AudioDeviceModule::kChannelBoth);
        TEST(audioDevice->SetRecordingChannel(AudioDeviceModule::kChannelLeft) == 0);
        TEST(audioDevice->RecordingChannel(&channelType) == 0);
        TEST(channelType == AudioDeviceModule::kChannelLeft);
        TEST(audioDevice->SetRecordingChannel(AudioDeviceModule::kChannelRight) == 0);
        TEST(audioDevice->RecordingChannel(&channelType) == 0);
        TEST(channelType == AudioDeviceModule::kChannelRight);
    }

    // ================
    // SetPlayoutBuffer
    // PlayoutBuffer
    // ================

    AudioDeviceModule::BufferType bufferType;
    WebRtc_UWord16 sizeMS(0);

    TEST(audioDevice->Terminate() == 0);
    TEST(audioDevice->Init() == 0);

    // check initial states
    TEST(audioDevice->Initialized() == true);
    TEST(audioDevice->PlayoutIsInitialized() == false);
    TEST(audioDevice->Playing() == false);
    TEST(audioDevice->PlayoutBuffer(&bufferType, &sizeMS) == 0);
#if defined(_WIN32) || defined(ANDROID) || defined(MAC_IPHONE)
    TEST(bufferType == AudioDeviceModule::kAdaptiveBufferSize);
#else
    TEST(bufferType == AudioDeviceModule::kFixedBufferSize);
#endif

    // fail tests
    TEST(audioDevice->InitPlayout() == -1); // must set device first
    TEST(audioDevice->SetPlayoutDevice(MACRO_DEFAULT_COMMUNICATION_DEVICE) == 0);
    TEST(audioDevice->InitPlayout() == 0);
    TEST(audioDevice->PlayoutIsInitialized() == true);
    TEST(audioDevice->SetPlayoutBuffer(AudioDeviceModule::kAdaptiveBufferSize,
                                       100) == -1);
    TEST(audioDevice->StopPlayout() == 0);
    TEST(audioDevice->SetPlayoutBuffer(AudioDeviceModule::kFixedBufferSize,
                                       kAdmMinPlayoutBufferSizeMs-1) == -1);
    TEST(audioDevice->SetPlayoutBuffer(AudioDeviceModule::kFixedBufferSize,
                                       kAdmMaxPlayoutBufferSizeMs+1) == -1);

    // bulk tests (all should be successful)
    TEST(audioDevice->PlayoutIsInitialized() == false);
#ifdef _WIN32
    TEST(audioDevice->SetPlayoutBuffer(AudioDeviceModule::kAdaptiveBufferSize,
                                       0) == 0);
    TEST(audioDevice->PlayoutBuffer(&bufferType, &sizeMS) == 0);
    TEST(bufferType == AudioDeviceModule::kAdaptiveBufferSize);
    TEST(audioDevice->SetPlayoutBuffer(AudioDeviceModule::kAdaptiveBufferSize,
                                       10000) == 0);
    TEST(audioDevice->PlayoutBuffer(&bufferType, &sizeMS) == 0);
    TEST(bufferType == AudioDeviceModule::kAdaptiveBufferSize);
#endif
#if defined(ANDROID) || defined(MAC_IPHONE)
    TEST(audioDevice->SetPlayoutBuffer(AudioDeviceModule::kFixedBufferSize,
                                       kAdmMinPlayoutBufferSizeMs) == -1);
#else
    TEST(audioDevice->SetPlayoutBuffer(AudioDeviceModule::kFixedBufferSize,
                                       kAdmMinPlayoutBufferSizeMs) == 0);
    TEST(audioDevice->PlayoutBuffer(&bufferType, &sizeMS) == 0);
    TEST(bufferType == AudioDeviceModule::kFixedBufferSize);
    TEST(sizeMS == kAdmMinPlayoutBufferSizeMs);
    TEST(audioDevice->SetPlayoutBuffer(AudioDeviceModule::kFixedBufferSize,
                                       kAdmMaxPlayoutBufferSizeMs) == 0);
    TEST(audioDevice->PlayoutBuffer(&bufferType, &sizeMS) == 0);
    TEST(bufferType == AudioDeviceModule::kFixedBufferSize);
    TEST(sizeMS == kAdmMaxPlayoutBufferSizeMs);
    TEST(audioDevice->SetPlayoutBuffer(AudioDeviceModule::kFixedBufferSize,
                                       100) == 0);
    TEST(audioDevice->PlayoutBuffer(&bufferType, &sizeMS) == 0);
    TEST(bufferType == AudioDeviceModule::kFixedBufferSize);
    TEST(sizeMS == 100);
#endif

#ifdef _WIN32
    // restore default
    TEST(audioDevice->SetPlayoutBuffer(AudioDeviceModule::kAdaptiveBufferSize,
                                       0) == 0);
    TEST(audioDevice->PlayoutBuffer(&bufferType, &sizeMS) == 0);
#endif

    // ============
    // PlayoutDelay
    // ============

    // NOTE: this API is better tested in a functional test

    TEST(audioDevice->Terminate() == 0);
    TEST(audioDevice->Init() == 0);

    // check initial states
    TEST(audioDevice->Initialized() == true);
    TEST(audioDevice->PlayoutIsInitialized() == false);
    TEST(audioDevice->Playing() == false);

    // bulk tests
    TEST(audioDevice->PlayoutDelay(&sizeMS) == 0);
    TEST(audioDevice->PlayoutDelay(&sizeMS) == 0);

    // ==============
    // RecordingDelay
    // ==============

    // NOTE: this API is better tested in a functional test

    TEST(audioDevice->Terminate() == 0);
    TEST(audioDevice->Init() == 0);

    // check initial states
    TEST(audioDevice->Initialized() == true);
    TEST(audioDevice->RecordingIsInitialized() == false);
    TEST(audioDevice->Recording() == false);

    // bulk tests
    TEST(audioDevice->RecordingDelay(&sizeMS) == 0);
    TEST(audioDevice->RecordingDelay(&sizeMS) == 0);

    // =======
    // CPULoad
    // =======

    // NOTE: this API is better tested in a functional test

    WebRtc_UWord16 load(0);

    TEST(audioDevice->Terminate() == 0);
    TEST(audioDevice->Init() == 0);

    // check initial states
    TEST(audioDevice->Initialized() == true);

    // bulk tests
#ifdef _WIN32
    TEST(audioDevice->CPULoad(&load) == 0);
    TEST(load == 0);
#else
    TEST(audioDevice->CPULoad(&load) == -1);
#endif

    // ===========================
    // StartRawOutputFileRecording
    // StopRawOutputFileRecording
    // ===========================

    // NOTE: this API is better tested in a functional test

    TEST(audioDevice->Terminate() == 0);
    TEST(audioDevice->Init() == 0);

    // check initial states
    TEST(audioDevice->Initialized() == true);
    TEST(audioDevice->PlayoutIsInitialized() == false);
    TEST(audioDevice->Playing() == false);

    // fail tests
    TEST(audioDevice->StartRawOutputFileRecording(NULL) == -1);

    // bulk tests
    TEST(audioDevice->StartRawOutputFileRecording(
        GetFilename("raw_output_not_playing.pcm")) == 0);
    TEST(audioDevice->StopRawOutputFileRecording() == 0);
    TEST(audioDevice->SetPlayoutDevice(MACRO_DEFAULT_COMMUNICATION_DEVICE) == 0);
    TEST(audioDevice->InitPlayout() == 0);
    TEST(audioDevice->StartPlayout() == 0);
    TEST(audioDevice->StartRawOutputFileRecording(
        GetFilename("raw_output_playing.pcm")) == 0);
    AudioDeviceUtility::Sleep(100);
    TEST(audioDevice->StopRawOutputFileRecording() == 0);
    TEST(audioDevice->StopPlayout() == 0);
    TEST(audioDevice->StartRawOutputFileRecording(
        GetFilename("raw_output_not_playing.pcm")) == 0);
    TEST(audioDevice->StopRawOutputFileRecording() == 0);

    // results after this test:
    //
    // - size of raw_output_not_playing.pcm shall be 0
    // - size of raw_output_playing.pcm shall be > 0

    // ==========================
    // StartRawInputFileRecording
    // StopRawInputFileRecording
    // ==========================

    // NOTE: this API is better tested in a functional test

    TEST(audioDevice->Terminate() == 0);
    TEST(audioDevice->Init() == 0);

    // check initial states
    TEST(audioDevice->Initialized() == true);
    TEST(audioDevice->RecordingIsInitialized() == false);
    TEST(audioDevice->Playing() == false);

    // fail tests
    TEST(audioDevice->StartRawInputFileRecording(NULL) == -1);

    // bulk tests
    TEST(audioDevice->StartRawInputFileRecording(
        GetFilename("raw_input_not_recording.pcm")) == 0);
    TEST(audioDevice->StopRawInputFileRecording() == 0);
    TEST(audioDevice->SetRecordingDevice(MACRO_DEFAULT_DEVICE) == 0);
    TEST(audioDevice->InitRecording() == 0);
    TEST(audioDevice->StartRecording() == 0);
    TEST(audioDevice->StartRawInputFileRecording(
        GetFilename("raw_input_recording.pcm")) == 0);
    AudioDeviceUtility::Sleep(100);
    TEST(audioDevice->StopRawInputFileRecording() == 0);
    TEST(audioDevice->StopRecording() == 0);
    TEST(audioDevice->StartRawInputFileRecording(
        GetFilename("raw_input_not_recording.pcm")) == 0);
    TEST(audioDevice->StopRawInputFileRecording() == 0);

    // results after this test:
    //
    // - size of raw_input_not_recording.pcm shall be 0
    // - size of raw_input_not_recording.pcm shall be > 0

    // ===================
    // RecordingSampleRate
    // ===================

    WebRtc_UWord32 sampleRate(0);

    TEST(audioDevice->Terminate() == 0);
    TEST(audioDevice->Init() == 0);

    // check initial states
    TEST(audioDevice->Initialized() == true);

    // bulk tests
    TEST(audioDevice->RecordingSampleRate(&sampleRate) == 0);
#if defined(_WIN32) && !defined(WEBRTC_WINDOWS_CORE_AUDIO_BUILD)
    TEST(sampleRate == 48000);
#elif defined(ANDROID)
    TEST_LOG("Recording sample rate is %u\n\n", sampleRate);
    TEST((sampleRate == 44000) || (sampleRate == 16000));
#elif defined(MAC_IPHONE)
    TEST_LOG("Recording sample rate is %u\n\n", sampleRate);
    TEST((sampleRate == 44000) || (sampleRate == 16000) || (sampleRate == 8000));
#endif

    // @TODO(xians) - add tests for all platforms here...

    // =================
    // PlayoutSampleRate
    // =================

    TEST(audioDevice->Terminate() == 0);
    TEST(audioDevice->Init() == 0);

    // check initial states
    TEST(audioDevice->Initialized() == true);

    // bulk tests
    TEST(audioDevice->PlayoutSampleRate(&sampleRate) == 0);
#if defined(_WIN32) && !defined(WEBRTC_WINDOWS_CORE_AUDIO_BUILD)
    TEST(sampleRate == 48000);
#elif defined(ANDROID)
    TEST_LOG("Playout sample rate is %u\n\n", sampleRate);
    TEST((sampleRate == 44000) || (sampleRate == 16000));
#elif defined(MAC_IPHONE)
    TEST_LOG("Playout sample rate is %u\n\n", sampleRate);
    TEST((sampleRate == 44000) || (sampleRate == 16000) || (sampleRate == 8000));
#endif

    // ==========================
    // ResetAudioDevice
    // ==========================

    TEST(audioDevice->Terminate() == 0);
    TEST(audioDevice->Init() == 0);

    // check initial states
    TEST(audioDevice->Initialized() == true);
    TEST(audioDevice->PlayoutIsInitialized() == false);
    TEST(audioDevice->Playing() == false);
    TEST(audioDevice->RecordingIsInitialized() == false);
    TEST(audioDevice->Recording() == false);

    TEST(audioDevice->SetPlayoutDevice(MACRO_DEFAULT_DEVICE) == 0);
    TEST(audioDevice->SetRecordingDevice(MACRO_DEFAULT_DEVICE) == 0);

#if defined(MAC_IPHONE) 
    // Not playing or recording, should just return 0
    TEST(audioDevice->ResetAudioDevice() == 0);

    TEST(audioDevice->InitRecording() == 0);
    TEST(audioDevice->StartRecording() == 0);
    TEST(audioDevice->InitPlayout() == 0);
    TEST(audioDevice->StartPlayout() == 0);
    for (int l=0; l<20; ++l)
    {
        TEST_LOG("Resetting sound device several time with pause %d ms\n", l);
        TEST(audioDevice->ResetAudioDevice() == 0);
        AudioDeviceUtility::Sleep(l);
    }
#else
    // Fail tests
    TEST(audioDevice->ResetAudioDevice() == -1);
    TEST(audioDevice->InitRecording() == 0);
    TEST(audioDevice->StartRecording() == 0);
    TEST(audioDevice->InitPlayout() == 0);
    TEST(audioDevice->StartPlayout() == 0);
    TEST(audioDevice->ResetAudioDevice() == -1);
#endif
    TEST(audioDevice->StopRecording() == 0);
    TEST(audioDevice->StopPlayout() == 0);

    // ==========================
    // SetPlayoutSpeaker
    // ==========================

    TEST(audioDevice->Terminate() == 0);
    TEST(audioDevice->Init() == 0);

    // check initial states
    TEST(audioDevice->Initialized() == true);
    TEST(audioDevice->PlayoutIsInitialized() == false);
    TEST(audioDevice->Playing() == false);

    TEST(audioDevice->SetPlayoutDevice(MACRO_DEFAULT_DEVICE) == 0);

    bool loudspeakerOn(false);
#if defined(MAC_IPHONE)
    // Not playing or recording, should just return a success
    TEST(audioDevice->SetLoudspeakerStatus(true) == 0);
    TEST(audioDevice->GetLoudspeakerStatus(loudspeakerOn) == 0);
    TEST(loudspeakerOn == true);
    TEST(audioDevice->SetLoudspeakerStatus(false) == 0);
    TEST(audioDevice->GetLoudspeakerStatus(loudspeakerOn) == 0);
    TEST(loudspeakerOn == false);

    TEST(audioDevice->InitPlayout() == 0);
    TEST(audioDevice->StartPlayout() == 0);
    TEST(audioDevice->SetLoudspeakerStatus(true) == 0);
    TEST(audioDevice->GetLoudspeakerStatus(loudspeakerOn) == 0);
    TEST(loudspeakerOn == true);
    TEST(audioDevice->SetLoudspeakerStatus(false) == 0);
    TEST(audioDevice->GetLoudspeakerStatus(loudspeakerOn) == 0);
    TEST(loudspeakerOn == false);

#else
    // Fail tests
    TEST(audioDevice->SetLoudspeakerStatus(true) == -1);
    TEST(audioDevice->SetLoudspeakerStatus(false) == -1);
    TEST(audioDevice->SetLoudspeakerStatus(true) == -1);
    TEST(audioDevice->SetLoudspeakerStatus(false) == -1);

    TEST(audioDevice->InitPlayout() == 0);
    TEST(audioDevice->StartPlayout() == 0);
    TEST(audioDevice->GetLoudspeakerStatus(&loudspeakerOn) == -1);
#endif
    TEST(audioDevice->StopPlayout() == 0);

#ifdef _WIN32
    Exit:
#endif

    // ------------------------------------------------------------------------
    // Terminate the module when all tests are done:
    //
    TEST(audioDevice->Terminate() == 0);
    // ------------------------------------------------------------------------

    // ===================================================
    // AudioDeviceModule::Destroy
    // ===================================================


    // release the ProcessThread object
    if (processThread)
    {
        processThread->DeRegisterModule(audioDevice);
        processThread->Stop();
        ProcessThread::DestroyProcessThread(processThread);
    }

    // delete the event observer
    if (eventObserver)
    {
        delete eventObserver;
        eventObserver = NULL;
    }

    // delete the audio transport
    if (audioTransport)
    {
        delete audioTransport;
        audioTransport = NULL;
    }

    // release the AudioDeviceModule object
    if (audioDevice)
        AudioDeviceModule::Destroy(audioDevice);

    TEST_LOG("\n");
    PRINT_TEST_RESULTS;

    return 0;
}
