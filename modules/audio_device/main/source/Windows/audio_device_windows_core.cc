/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#pragma warning(disable: 4995)  //  name was marked as #pragma deprecated

#if (_MSC_VER >= 1310) && (_MSC_VER < 1400)
// Reports the major and minor versions of the compiler.
// For example, 1310 for Microsoft Visual C++ .NET 2003. 1310 represents version 13 and a 1.0 point release.
// The Visual C++ 2005 compiler version is 1400.
// Type cl /? at the command line to see the major and minor versions of your compiler along with the build number.
#pragma message(">> INFO: Windows Core Audio is not supported in VS 2003")
#endif

#include "audio_device_config.h"

#if defined(WEBRTC_WINDOWS_CORE_AUDIO_BUILD)
#pragma message(">> INFO: WEBRTC_WINDOWS_CORE_AUDIO_BUILD is defined")
#else
#pragma message(">> INFO: WEBRTC_WINDOWS_CORE_AUDIO_BUILD is *not* defined")
#endif

#ifdef WEBRTC_WINDOWS_CORE_AUDIO_BUILD

#include "audio_device_utility.h"
#include "audio_device_windows_core.h"
#include "trace.h"

#include <windows.h>
#include <mmsystem.h>
#include <cassert>

#include <comdef.h>
#include "Functiondiscoverykeys_devpkey.h"
#include <strsafe.h>


// Macro that calls a COM method returning HRESULT value.
#define EXIT_ON_ERROR(hres)    do { if (FAILED(hres)) goto Exit; } while(0)

// Macro that releases a COM object if not NULL.
#define SAFE_RELEASE(p)     do { if ((p)) { (p)->Release(); (p) = NULL; } } while(0)

#define ROUND(x) ((x) >=0 ? (int)((x) + 0.5) : (int)((x) - 0.5))

// REFERENCE_TIME time units per millisecond
#define REFTIMES_PER_MILLISEC  10000

typedef struct tagTHREADNAME_INFO
{
   DWORD dwType;        // must be 0x1000
   LPCSTR szName;       // pointer to name (in user addr space)
   DWORD dwThreadID;    // thread ID (-1=caller thread)
   DWORD dwFlags;       // reserved for future use, must be zero
} THREADNAME_INFO;

enum { COM_THREADING_MODEL = COINIT_MULTITHREADED };

namespace webrtc {

// ============================================================================
//                              Static Methods
// ============================================================================

// ----------------------------------------------------------------------------
//  CoreAudioIsSupported
// ----------------------------------------------------------------------------

bool AudioDeviceWindowsCore::CoreAudioIsSupported()
{
    WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, -1, "%s", __FUNCTION__);

    bool MMDeviceIsAvailable(false);
    bool coreAudioIsSupported(false);
    bool coUninitializeIsRequired(true);

    HRESULT hr(S_OK);
    TCHAR buf[MAXERRORLENGTH];
    LPCTSTR errorText;

    // 1) Initialize the COM library (make Windows load the DLLs).
    //
    // CoInitializeEx must be called at least once, and is usually called only once,
    // for each thread that uses the COM library. Multiple calls to CoInitializeEx
    // by the same thread are allowed as long as they pass the same concurrency flag,
    // but subsequent valid calls return S_FALSE.
    // To close the COM library gracefully on a thread, each successful call to
    // CoInitializeEx, including any call that returns S_FALSE, must be balanced
    // by a corresponding call to CoUninitialize.
    //
    hr = CoInitializeEx(NULL, COM_THREADING_MODEL);
    if (FAILED(hr))
    {
        // Avoid calling CoUninitialize() since CoInitializeEx() failed.
        coUninitializeIsRequired = false;

        if (RPC_E_CHANGED_MODE == hr)
        {
            // Calling thread has already initialized COM to be used in a single-threaded
            // apartment (STA). We are then prevented from using MTA.
            // Details: hr = 0x80010106 <=> "Cannot change thread mode after it is set".
            WEBRTC_TRACE(kTraceWarning, kTraceAudioDevice, -1,
                "AudioDeviceWindowsCore::CoreAudioIsSupported() CoInitializeEx(NULL, COM_THREADING_MODEL) => RPC_E_CHANGED_MODE");
        }
        _com_error error(hr);
        WEBRTC_TRACE(kTraceError, kTraceAudioDevice, -1,
            "AudioDeviceWindowsCore::CoreAudioIsSupported() Failed to initialize the COM library", hr);
        WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, -1,
            "AudioDeviceWindowsCore::CoreAudioIsSupported() CoInitializeEx(COM_THREADING_MODEL) failed (hr=0x%x)", hr);
        StringCchPrintf(buf, MAXERRORLENGTH, TEXT("Error details: "));
        errorText = error.ErrorMessage();
        StringCchCat(buf, MAXERRORLENGTH, errorText);
        WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, -1, "%s", buf);
    }

    // ...it is OK to enter this scope even if CoInitializeEx() failed

    // 2) Check if the MMDevice API is available.
    //
    // The Windows Multimedia Device (MMDevice) API enables audio clients to
    // discover audio endpoint devices, determine their capabilities, and create
    // driver instances for those devices.
    // Header file Mmdeviceapi.h defines the interfaces in the MMDevice API.
    // The MMDevice API consists of several interfaces. The first of these is the
    // IMMDeviceEnumerator interface. To access the interfaces in the MMDevice API,
    // a client obtains a reference to the IMMDeviceEnumerator interface of a
    // device-enumerator object by calling the CoCreateInstance function.
    //
    // Through the IMMDeviceEnumerator interface, the client can obtain references
    // to the other interfaces in the MMDevice API. The MMDevice API implements
    // the following interfaces:
    //
    // IMMDevice            Represents an audio device.
    // IMMDeviceCollection  Represents a collection of audio devices.
    // IMMDeviceEnumerator  Provides methods for enumerating audio devices.
    // IMMEndpoint          Represents an audio endpoint device.
    //
    IMMDeviceEnumerator* pIMMD(NULL);
    const CLSID CLSID_MMDeviceEnumerator = __uuidof(MMDeviceEnumerator);
    const IID IID_IMMDeviceEnumerator = __uuidof(IMMDeviceEnumerator);

    hr = CoCreateInstance(
            CLSID_MMDeviceEnumerator,   // GUID value of MMDeviceEnumerator coclass
            NULL,
            CLSCTX_ALL,
            IID_IMMDeviceEnumerator,    // GUID value of the IMMDeviceEnumerator interface
            (void**)&pIMMD );

    if (FAILED(hr))
    {
        _com_error error(hr);
        WEBRTC_TRACE(kTraceError, kTraceAudioDevice, -1,
            "AudioDeviceWindowsCore::CoreAudioIsSupported() Failed to create the required COM object", hr);
        WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, -1,
            "AudioDeviceWindowsCore::CoreAudioIsSupported() CoCreateInstance(MMDeviceEnumerator) failed (hr=0x%x)", hr);
        StringCchPrintf(buf, MAXERRORLENGTH, TEXT("Error details: "));
        errorText = error.ErrorMessage();
        StringCchCat(buf, MAXERRORLENGTH, errorText);
        WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, -1, "%s", buf);
    }
    else
    {
        MMDeviceIsAvailable = true;
        WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, -1,
            "AudioDeviceWindowsCore::CoreAudioIsSupported() CoCreateInstance(MMDeviceEnumerator) succeeded", hr);
        SAFE_RELEASE(pIMMD);
    }

    // 3) Uninitialize COM but only if required.
    //
    // COM will be reinitialized again when the Core Audio ADM is created.
    //
    if (coUninitializeIsRequired)
    {
        WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, -1, "AudioDeviceWindowsCore::CoreAudioIsSupported() calls CoUninitialize()");
        CoUninitialize();
    }

    // 4) Verify that we can create and initialize our Core Audio class.
    //
    // Also, perform a limited "API test" to ensure that Core Audio is supported for all devices.
    //
    if (MMDeviceIsAvailable)
    {
        coreAudioIsSupported = false;

        AudioDeviceWindowsCore* p = new AudioDeviceWindowsCore(-1);
        if (p == NULL)
        {
            return false;
        }

        int ok(0);
        int temp_ok(0);
        bool available(false);

        ok |= p->Init();

        WebRtc_Word16 numDevsRec = p->RecordingDevices();
        for (WebRtc_UWord16 i = 0; i < numDevsRec; i++)
        {
            ok |= p->SetRecordingDevice(i);
            temp_ok = p->RecordingIsAvailable(available);
            ok |= temp_ok;
            ok |= (available == false);
            if (available)
            {
                ok |= p->InitMicrophone();
            }
            if (ok)
            {
                WEBRTC_TRACE(kTraceWarning, kTraceAudioDevice, -1,
                    "AudioDeviceWindowsCore::CoreAudioIsSupported() Failed to use Core Audio Recording for device id=%i", i);
            }
        }

        WebRtc_Word16 numDevsPlay = p->PlayoutDevices();
        for (WebRtc_UWord16 i = 0; i < numDevsPlay; i++)
        {
            ok |= p->SetPlayoutDevice(i);
            temp_ok = p->PlayoutIsAvailable(available);
            ok |= temp_ok;
            ok |= (available == false);
            if (available)
            {
                ok |= p->InitSpeaker();
            }
            if (ok)
            {
                WEBRTC_TRACE(kTraceWarning, kTraceAudioDevice, -1 ,
                    "AudioDeviceWindowsCore::CoreAudioIsSupported() Failed to use Core Audio Playout for device id=%i", i);
            }
        }

        ok |= p->Terminate();

        if (ok == 0)
        {
            coreAudioIsSupported = true;
        }

        delete p;
    }

    if (coreAudioIsSupported)
    {
        WEBRTC_TRACE(kTraceStateInfo, kTraceAudioDevice, -1, "*** Windows Core Audio is supported ***");
    }
    else
    {
        WEBRTC_TRACE(kTraceStateInfo, kTraceAudioDevice, -1, "*** Windows Core Audio is NOT supported => will revert to the Wave API ***");
    }

    return (coreAudioIsSupported);
}

// ============================================================================
//                            Construction & Destruction
// ============================================================================

// ----------------------------------------------------------------------------
//  AudioDeviceWindowsCore() - ctor
// ----------------------------------------------------------------------------

AudioDeviceWindowsCore::AudioDeviceWindowsCore(const WebRtc_Word32 id) :
    _critSect(*CriticalSectionWrapper::CreateCriticalSection()),
    _volumeMutex(*CriticalSectionWrapper::CreateCriticalSection()),
    _id(id),
    _ptrAudioBuffer(NULL),
    _ptrEnumerator(NULL),
    _ptrRenderCollection(NULL),
    _ptrCaptureCollection(NULL),
    _ptrDeviceOut(NULL),
    _ptrDeviceIn(NULL),
    _ptrClientOut(NULL),
    _ptrClientIn(NULL),
    _ptrRenderClient(NULL),
    _ptrCaptureClient(NULL),
    _ptrCaptureVolume(NULL),
    _ptrRenderSimpleVolume(NULL),
    _ptrRenderEndpointVolume(NULL),
    _playAudioFrameSize(0),
    _playSampleRate(0),
    _playBlockSize(0),
    _playChannels(2),
    _sndCardPlayDelay(0),
    _sndCardRecDelay(0),
    _sampleDriftAt48kHz(0),
    _driftAccumulator(0),
    _writtenSamples(0),
    _readSamples(0),
    _playAcc(0),
    _recAudioFrameSize(0),
    _recSampleRate(0),
    _recBlockSize(0),
    _recChannels(2),
    _avrtLibrary(NULL),
    _winSupportAvrt(false),
    _hRenderSamplesReadyEvent(NULL),
    _hPlayThread(NULL),
    _hCaptureSamplesReadyEvent(NULL),
    _hRecThread(NULL),
    _hShutdownRenderEvent(NULL),
    _hShutdownCaptureEvent(NULL),
    _hRenderStartedEvent(NULL),
    _hCaptureStartedEvent(NULL),
    _hGetCaptureVolumeThread(NULL),
    _hSetCaptureVolumeThread(NULL),
    _hSetCaptureVolumeEvent(NULL),
    _coUninitializeIsRequired(true),
    _initialized(false),
    _recording(false),
    _playing(false),
    _recIsInitialized(false),
    _playIsInitialized(false),
    _speakerIsInitialized(false),
    _microphoneIsInitialized(false),
    _AGC(false),
    _playWarning(0),
    _playError(0),
    _recWarning(0),
    _recError(0),
    _playBufType(AudioDeviceModule::kAdaptiveBufferSize),
    _playBufDelay(80),
    _playBufDelayFixed(80),
    _usingInputDeviceIndex(false),
    _usingOutputDeviceIndex(false),
    _inputDevice(AudioDeviceModule::kDefaultCommunicationDevice),
    _outputDevice(AudioDeviceModule::kDefaultCommunicationDevice),
    _inputDeviceIndex(0),
    _outputDeviceIndex(0),
    _newMicLevel(0)
{
    WEBRTC_TRACE(kTraceMemory, kTraceAudioDevice, id, "%s created", __FUNCTION__);

    // Try to load the Avrt DLL
    if (!_avrtLibrary)
    {
        // Get handle to the Avrt DLL module.
        _avrtLibrary = LoadLibrary(TEXT("Avrt.dll"));
        if (_avrtLibrary)
        {
            // Handle is valid (should only happen if OS larger than vista & win7).
            // Try to get the function addresses.
            WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, _id, "AudioDeviceWindowsCore::AudioDeviceWindowsCore() The Avrt DLL module is now loaded");

            _PAvRevertMmThreadCharacteristics = (PAvRevertMmThreadCharacteristics)GetProcAddress(_avrtLibrary, "AvRevertMmThreadCharacteristics");
            _PAvSetMmThreadCharacteristicsA = (PAvSetMmThreadCharacteristicsA)GetProcAddress(_avrtLibrary, "AvSetMmThreadCharacteristicsA");
            _PAvSetMmThreadPriority = (PAvSetMmThreadPriority)GetProcAddress(_avrtLibrary, "AvSetMmThreadPriority");

            if ( _PAvRevertMmThreadCharacteristics &&
                 _PAvSetMmThreadCharacteristicsA &&
                 _PAvSetMmThreadPriority)
            {
                WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, _id, "AudioDeviceWindowsCore::AudioDeviceWindowsCore() AvRevertMmThreadCharacteristics() is OK");
                WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, _id, "AudioDeviceWindowsCore::AudioDeviceWindowsCore() AvSetMmThreadCharacteristicsA() is OK");
                WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, _id, "AudioDeviceWindowsCore::AudioDeviceWindowsCore() AvSetMmThreadPriority() is OK");
                _winSupportAvrt = true;
            }
        }
    }

    // Create our samples ready events - we want auto reset events that start in the not-signaled state.
    // The state of an auto-reset event object remains signaled until a single waiting thread is released,
    // at which time the system automatically sets the state to nonsignaled. If no threads are waiting,
    // the event object's state remains signaled.
    // (Except for _hShutdownCaptureEvent, which is used to shutdown multiple threads).
    _hRenderSamplesReadyEvent = CreateEvent(NULL, FALSE, FALSE, NULL);
    _hCaptureSamplesReadyEvent = CreateEvent(NULL, FALSE, FALSE, NULL);
    _hShutdownRenderEvent = CreateEvent(NULL, FALSE, FALSE, NULL);
    _hShutdownCaptureEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
    _hRenderStartedEvent = CreateEvent(NULL, FALSE, FALSE, NULL);
    _hCaptureStartedEvent = CreateEvent(NULL, FALSE, FALSE, NULL);
    _hSetCaptureVolumeEvent = CreateEvent(NULL, FALSE, FALSE, NULL);

    _perfCounterFreq.QuadPart = 1;
    _perfCounterFactor = 0.0;
    _avgCPULoad = 0.0;

    // list of number of channels to use on recording side
    _recChannelsPrioList[0] = 2;    // stereo is prio 1
    _recChannelsPrioList[1] = 1;    // mono is prio 2

    // list of number of channels to use on playout side
    _playChannelsPrioList[0] = 2;    // stereo is prio 1
    _playChannelsPrioList[1] = 1;    // mono is prio 2

    // Initialize the COM library
    HRESULT hr = CoInitializeEx(NULL,  COM_THREADING_MODEL);
    if (FAILED(hr))
    {
        _coUninitializeIsRequired = false;
        if (hr == RPC_E_CHANGED_MODE)
        {
            WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, _id,
                "AudioDeviceWindowsCore::AudioDeviceWindowsCore() CoInitializeEx(NULL,  COM_THREADING_MODEL) => RPC_E_CHANGED_MODE");
        }
    }

    // We know that this API will work since it has already been verified in
    // CoreAudioIsSupported, hence no need to check for errors here as well.

    // Retrive the IMMDeviceEnumerator API (should load the MMDevAPI.dll)
    CoCreateInstance(
      __uuidof(MMDeviceEnumerator),
      NULL,
      CLSCTX_ALL,
      __uuidof(IMMDeviceEnumerator),
      (void**)&_ptrEnumerator);

    if (_coUninitializeIsRequired)
    {
        WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, _id,
            "AudioDeviceWindowsCore::AudioDeviceWindowsCore() matching call to CoUninitialize() is required");
    }
    assert(NULL != _ptrEnumerator);
}

// ----------------------------------------------------------------------------
//  AudioDeviceWindowsCore() - dtor
// ----------------------------------------------------------------------------

AudioDeviceWindowsCore::~AudioDeviceWindowsCore()
{
    WEBRTC_TRACE(kTraceMemory, kTraceAudioDevice, _id, "%s destroyed", __FUNCTION__);

    Terminate();

    _ptrAudioBuffer = NULL;

    SAFE_RELEASE(_ptrEnumerator);
    SAFE_RELEASE(_ptrRenderCollection);
    SAFE_RELEASE(_ptrCaptureCollection);
    SAFE_RELEASE(_ptrDeviceOut);
    SAFE_RELEASE(_ptrDeviceIn);
    SAFE_RELEASE(_ptrClientOut);
    SAFE_RELEASE(_ptrClientIn);
    SAFE_RELEASE(_ptrRenderClient);
    SAFE_RELEASE(_ptrCaptureClient);
    SAFE_RELEASE(_ptrCaptureVolume);
    SAFE_RELEASE(_ptrRenderSimpleVolume);
    SAFE_RELEASE(_ptrRenderEndpointVolume);

    if (_coUninitializeIsRequired)
    {
        WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, _id, "AudioDeviceWindowsCore::~AudioDeviceWindowsCore() calling CoUninitialize()...");
        CoUninitialize();
    }

    if (NULL != _hRenderSamplesReadyEvent)
    {
        CloseHandle(_hRenderSamplesReadyEvent);
        _hRenderSamplesReadyEvent = NULL;
    }

    if (NULL != _hCaptureSamplesReadyEvent)
    {
        CloseHandle(_hCaptureSamplesReadyEvent);
        _hCaptureSamplesReadyEvent = NULL;
    }

    if (NULL != _hRenderStartedEvent)
    {
        CloseHandle(_hRenderStartedEvent);
        _hRenderStartedEvent = NULL;
    }

    if (NULL != _hCaptureStartedEvent)
    {
        CloseHandle(_hCaptureStartedEvent);
        _hCaptureStartedEvent = NULL;
    }

    if (NULL != _hShutdownRenderEvent)
    {
        CloseHandle(_hShutdownRenderEvent);
        _hShutdownRenderEvent = NULL;
    }

    if (NULL != _hShutdownCaptureEvent)
    {
        CloseHandle(_hShutdownCaptureEvent);
        _hShutdownCaptureEvent = NULL;
    }

    if (NULL != _hSetCaptureVolumeEvent)
    {
        CloseHandle(_hSetCaptureVolumeEvent);
        _hSetCaptureVolumeEvent = NULL;
    }

    if (_avrtLibrary)
    {
        BOOL freeOK = FreeLibrary(_avrtLibrary);
        if (!freeOK)
        {
            WEBRTC_TRACE(kTraceWarning, kTraceAudioDevice, _id,
                "AudioDeviceWindowsCore::~AudioDeviceWindowsCore() failed to free the loaded Avrt DLL module correctly");
        }
        else
        {
            WEBRTC_TRACE(kTraceWarning, kTraceAudioDevice, _id,
                "AudioDeviceWindowsCore::~AudioDeviceWindowsCore() the Avrt DLL module is now unloaded");
        }
    }

    delete &_critSect;
    delete &_volumeMutex;
}

// ============================================================================
//                                     API
// ============================================================================

// ----------------------------------------------------------------------------
//  AttachAudioBuffer
// ----------------------------------------------------------------------------

void AudioDeviceWindowsCore::AttachAudioBuffer(AudioDeviceBuffer* audioBuffer)
{
    WEBRTC_TRACE(kTraceModuleCall, kTraceAudioDevice, _id, "%s", __FUNCTION__);

    _ptrAudioBuffer = audioBuffer;

    // Inform the AudioBuffer about default settings for this implementation.
    // Set all values to zero here since the actual settings will be done by
    // InitPlayout and InitRecording later.
    _ptrAudioBuffer->SetRecordingSampleRate(0);
    _ptrAudioBuffer->SetPlayoutSampleRate(0);
    _ptrAudioBuffer->SetRecordingChannels(0);
    _ptrAudioBuffer->SetPlayoutChannels(0);
}

// ----------------------------------------------------------------------------
//  ActiveAudioLayer
// ----------------------------------------------------------------------------

WebRtc_Word32 AudioDeviceWindowsCore::ActiveAudioLayer(AudioDeviceModule::AudioLayer& audioLayer) const
{
    WEBRTC_TRACE(kTraceModuleCall, kTraceAudioDevice, _id, "%s", __FUNCTION__);
    audioLayer = AudioDeviceModule::kWindowsCoreAudio;
    return 0;
}

// ----------------------------------------------------------------------------
//  Init
// ----------------------------------------------------------------------------

WebRtc_Word32 AudioDeviceWindowsCore::Init()
{
    WEBRTC_TRACE(kTraceModuleCall, kTraceAudioDevice, _id, "%s", __FUNCTION__);

    CriticalSectionScoped lock(_critSect);

    if (_initialized)
    {
        return 0;
    }

    _playWarning = 0;
    _playError = 0;
    _recWarning = 0;
    _recError = 0;

    // Enumerate all audio rendering and capturing endpoint devices.
    // Note that, some of these will not be able to select by the user.
    // The complete collection is for internal use only.
    //
    _EnumerateEndpointDevicesAll(eRender);
    _EnumerateEndpointDevicesAll(eCapture);

    _initialized = true;

    return 0;
}

// ----------------------------------------------------------------------------
//  Terminate
// ----------------------------------------------------------------------------

WebRtc_Word32 AudioDeviceWindowsCore::Terminate()
{
    WEBRTC_TRACE(kTraceModuleCall, kTraceAudioDevice, _id, "%s", __FUNCTION__);

    CriticalSectionScoped lock(_critSect);

    if (!_initialized)
    {
        return 0;
    }

    _initialized = false;
    _speakerIsInitialized = false;
    _microphoneIsInitialized = false;
    _playing = false;
    _recording = false;

    SAFE_RELEASE(_ptrRenderCollection);
    SAFE_RELEASE(_ptrCaptureCollection);
    SAFE_RELEASE(_ptrDeviceOut);
    SAFE_RELEASE(_ptrDeviceIn);
    SAFE_RELEASE(_ptrClientOut);
    SAFE_RELEASE(_ptrClientIn);
    SAFE_RELEASE(_ptrRenderClient);
    SAFE_RELEASE(_ptrCaptureClient);
    SAFE_RELEASE(_ptrCaptureVolume);
    SAFE_RELEASE(_ptrRenderSimpleVolume);
    SAFE_RELEASE(_ptrRenderEndpointVolume);

    return 0;
}

// ----------------------------------------------------------------------------
//  Initialized
// ----------------------------------------------------------------------------

bool AudioDeviceWindowsCore::Initialized() const
{
    WEBRTC_TRACE(kTraceModuleCall, kTraceAudioDevice, _id, "%s", __FUNCTION__);
    return (_initialized);
}

// ----------------------------------------------------------------------------
//  SpeakerIsAvailable
// ----------------------------------------------------------------------------

WebRtc_Word32 AudioDeviceWindowsCore::SpeakerIsAvailable(bool& available)
{
    WEBRTC_TRACE(kTraceModuleCall, kTraceAudioDevice, _id, "%s", __FUNCTION__);

    CriticalSectionScoped lock(_critSect);

    if (_ptrDeviceOut == NULL)
    {
        return -1;
    }

    available = true;

    return 0;
}

// ----------------------------------------------------------------------------
//  InitSpeaker
// ----------------------------------------------------------------------------

WebRtc_Word32 AudioDeviceWindowsCore::InitSpeaker()
{
    WEBRTC_TRACE(kTraceModuleCall, kTraceAudioDevice, _id, "%s", __FUNCTION__);

    CriticalSectionScoped lock(_critSect);

    if (_playing)
    {
        return -1;
    }

    if (_ptrDeviceOut == NULL)
    {
        return -1;
    }

    if (_usingOutputDeviceIndex)
    {
        WebRtc_Word16 nDevices = PlayoutDevices();
        if (_outputDeviceIndex > (nDevices - 1))
        {
            WEBRTC_TRACE(kTraceError, kTraceAudioDevice, _id, "current device selection is invalid => unable to initialize");
            return -1;
        }
    }

    WebRtc_Word32 ret(0);

    SAFE_RELEASE(_ptrDeviceOut);
    if (_usingOutputDeviceIndex)
    {
        // Refresh the selected rendering endpoint device using current index
        ret = _GetListDevice(eRender, _outputDeviceIndex, &_ptrDeviceOut);
    }
    else
    {
        ERole role;
        (_outputDevice == AudioDeviceModule::kDefaultDevice) ? role = eConsole : role = eCommunications;
        // Refresh the selected rendering endpoint device using role
        ret = _GetDefaultDevice(eRender, role, &_ptrDeviceOut);
    }

    if (ret != 0 || (_ptrDeviceOut == NULL))
    {
        WEBRTC_TRACE(kTraceError, kTraceAudioDevice, _id, "failed to initialize the rendering enpoint device");
        SAFE_RELEASE(_ptrDeviceOut);
        return -1;
    }

    ret = _ptrDeviceOut->Activate(
              __uuidof(IAudioEndpointVolume),
              CLSCTX_ALL,
              NULL,
              reinterpret_cast<void **>(&_ptrRenderEndpointVolume));
    if (ret != 0 || _ptrRenderEndpointVolume == NULL)
    {
        WEBRTC_TRACE(kTraceError, kTraceAudioDevice, _id,
                    "  failed to initialize the render endpoint volume");
        SAFE_RELEASE(_ptrRenderEndpointVolume);
        return -1;
    }

    IAudioSessionManager* pManager = NULL;	
    ret = _ptrDeviceOut->Activate(__uuidof(IAudioSessionManager),
                                  CLSCTX_ALL,
                                  NULL,
                                  (void**)&pManager);
    if (ret != 0 || pManager == NULL)
    {
        WEBRTC_TRACE(kTraceError, kTraceAudioDevice, _id,
                    "  failed to initialize the render manager");
        SAFE_RELEASE(pManager);
        return -1;
    }

    ret = pManager->GetSimpleAudioVolume(NULL, FALSE, &_ptrRenderSimpleVolume);
    if (ret != 0 || _ptrRenderSimpleVolume == NULL)
    {
        WEBRTC_TRACE(kTraceError, kTraceAudioDevice, _id,
                    "  failed to initialize the render simple volume");
        SAFE_RELEASE(pManager);
        SAFE_RELEASE(_ptrRenderSimpleVolume);
        return -1;
    }
    SAFE_RELEASE(pManager);


    _speakerIsInitialized = true;

    return 0;
}

// ----------------------------------------------------------------------------
//  MicrophoneIsAvailable
// ----------------------------------------------------------------------------

WebRtc_Word32 AudioDeviceWindowsCore::MicrophoneIsAvailable(bool& available)
{
    WEBRTC_TRACE(kTraceModuleCall, kTraceAudioDevice, _id, "%s", __FUNCTION__);

    CriticalSectionScoped lock(_critSect);

    if (_ptrDeviceIn == NULL)
    {
        return -1;
    }

    available = true;

    return 0;
}

// ----------------------------------------------------------------------------
//  InitMicrophone
// ----------------------------------------------------------------------------

WebRtc_Word32 AudioDeviceWindowsCore::InitMicrophone()
{
    WEBRTC_TRACE(kTraceModuleCall, kTraceAudioDevice, _id, "%s", __FUNCTION__);

    CriticalSectionScoped lock(_critSect);

    if (_recording)
    {
        return -1;
    }

    if (_ptrDeviceIn == NULL)
    {
        return -1;
    }

    if (_usingInputDeviceIndex)
    {
        WebRtc_Word16 nDevices = RecordingDevices();
        if (_inputDeviceIndex > (nDevices - 1))
        {
            WEBRTC_TRACE(kTraceError, kTraceAudioDevice, _id, "current device selection is invalid => unable to initialize");
            return -1;
        }
    }

    WebRtc_Word32 ret(0);

    SAFE_RELEASE(_ptrDeviceIn);
    if (_usingInputDeviceIndex)
    {
        // Refresh the selected capture endpoint device using current index
        ret = _GetListDevice(eCapture, _inputDeviceIndex, &_ptrDeviceIn);
    }
    else
    {
        ERole role;
        (_inputDevice == AudioDeviceModule::kDefaultDevice) ? role = eConsole : role = eCommunications;
        // Refresh the selected capture endpoint device using role
        ret = _GetDefaultDevice(eCapture, role, &_ptrDeviceIn);
    }

    if (ret != 0 || (_ptrDeviceIn == NULL))
    {
        WEBRTC_TRACE(kTraceError, kTraceAudioDevice, _id, "failed to initialize the capturing enpoint device");
        SAFE_RELEASE(_ptrDeviceIn);
        return -1;
    }

    ret = _ptrDeviceIn->Activate(__uuidof(IAudioEndpointVolume),
                                 CLSCTX_ALL,
                                 NULL,
                                 reinterpret_cast<void **>(&_ptrCaptureVolume));
    if (ret != 0 || _ptrCaptureVolume == NULL)
    {
        WEBRTC_TRACE(kTraceError, kTraceAudioDevice, _id,
                    "  failed to initialize the capture volume");
        SAFE_RELEASE(_ptrCaptureVolume);
        return -1;
    }

    _microphoneIsInitialized = true;

    return 0;
}

// ----------------------------------------------------------------------------
//  SpeakerIsInitialized
// ----------------------------------------------------------------------------

bool AudioDeviceWindowsCore::SpeakerIsInitialized() const
{
    WEBRTC_TRACE(kTraceModuleCall, kTraceAudioDevice, _id, "%s", __FUNCTION__);

    return (_speakerIsInitialized);
}

// ----------------------------------------------------------------------------
//  MicrophoneIsInitialized
// ----------------------------------------------------------------------------

bool AudioDeviceWindowsCore::MicrophoneIsInitialized() const
{
    WEBRTC_TRACE(kTraceModuleCall, kTraceAudioDevice, _id, "%s", __FUNCTION__);

    return (_microphoneIsInitialized);
}

// ----------------------------------------------------------------------------
//  SpeakerVolumeIsAvailable
// ----------------------------------------------------------------------------

WebRtc_Word32 AudioDeviceWindowsCore::SpeakerVolumeIsAvailable(bool& available)
{
    WEBRTC_TRACE(kTraceModuleCall, kTraceAudioDevice, _id, "%s", __FUNCTION__);

    CriticalSectionScoped lock(_critSect);

    if (_ptrDeviceOut == NULL)
    {
        return -1;
    }

    HRESULT hr = S_OK;
    IAudioSessionManager* pManager = NULL;
    ISimpleAudioVolume* pVolume = NULL;

    hr = _ptrDeviceOut->Activate(__uuidof(IAudioSessionManager), CLSCTX_ALL, NULL, (void**)&pManager);
    EXIT_ON_ERROR(hr);

    hr = pManager->GetSimpleAudioVolume(NULL, FALSE, &pVolume);
    EXIT_ON_ERROR(hr);

    float volume(0.0f);
    hr = pVolume->GetMasterVolume(&volume);
    if (FAILED(hr))
    {
        available = false;
    }
    available = true;

    SAFE_RELEASE(pManager);
    SAFE_RELEASE(pVolume);

    return 0;

Exit:
    _TraceCOMError(hr);
    SAFE_RELEASE(pManager);
    SAFE_RELEASE(pVolume);
    return -1;
}

// ----------------------------------------------------------------------------
//  SetSpeakerVolume
// ----------------------------------------------------------------------------

WebRtc_Word32 AudioDeviceWindowsCore::SetSpeakerVolume(WebRtc_UWord32 volume)
{
    WEBRTC_TRACE(kTraceModuleCall, kTraceAudioDevice, _id, "AudioDeviceWindowsCore::SetSpeakerVolume(volume=%u)", volume);

    {
        CriticalSectionScoped lock(_critSect);

        if (!_speakerIsInitialized)
        {
        return -1;
        }

        if (_ptrDeviceOut == NULL)
        {
            return -1;
        }
    }

    if (volume < (WebRtc_UWord32)MIN_CORE_SPEAKER_VOLUME ||
        volume > (WebRtc_UWord32)MAX_CORE_SPEAKER_VOLUME)
    {
        return -1;
    }

    HRESULT hr = S_OK;

    // scale input volume to valid range (0.0 to 1.0)
    const float fLevel = (float)volume/MAX_CORE_SPEAKER_VOLUME;
    _volumeMutex.Enter();
    hr = _ptrRenderSimpleVolume->SetMasterVolume(fLevel,NULL);
    _volumeMutex.Leave();
    EXIT_ON_ERROR(hr);

    return 0;

Exit:
    _TraceCOMError(hr);
    return -1;
}

// ----------------------------------------------------------------------------
//  SpeakerVolume
// ----------------------------------------------------------------------------

WebRtc_Word32 AudioDeviceWindowsCore::SpeakerVolume(WebRtc_UWord32& volume) const
{
    WEBRTC_TRACE(kTraceModuleCall, kTraceAudioDevice, _id, "%s", __FUNCTION__);

    {
        CriticalSectionScoped lock(_critSect);

        if (!_speakerIsInitialized)
        {
            return -1;
        }

        if (_ptrDeviceOut == NULL)
        {
            return -1;
        }
    }

    HRESULT hr = S_OK;
    float fLevel(0.0f);

    _volumeMutex.Enter();
    hr = _ptrRenderSimpleVolume->GetMasterVolume(&fLevel);
    _volumeMutex.Leave();
    EXIT_ON_ERROR(hr);

    // scale input volume range [0.0,1.0] to valid output range
    volume = static_cast<WebRtc_UWord32> (fLevel*MAX_CORE_SPEAKER_VOLUME);

    return 0;

Exit:
    _TraceCOMError(hr);
    return -1;
}

// ----------------------------------------------------------------------------
//  SetWaveOutVolume
// ----------------------------------------------------------------------------

WebRtc_Word32 AudioDeviceWindowsCore::SetWaveOutVolume(WebRtc_UWord16 volumeLeft, WebRtc_UWord16 volumeRight)
{
    WEBRTC_TRACE(kTraceModuleCall, kTraceAudioDevice, _id, "AudioDeviceWindowsCore::SetWaveOutVolume(volumeLeft=%u, volumeRight=%u)",
        volumeLeft, volumeRight);
    return -1;
}

// ----------------------------------------------------------------------------
//  WaveOutVolume
// ----------------------------------------------------------------------------

WebRtc_Word32 AudioDeviceWindowsCore::WaveOutVolume(WebRtc_UWord16& volumeLeft, WebRtc_UWord16& volumeRight) const
{
    WEBRTC_TRACE(kTraceModuleCall, kTraceAudioDevice, _id, "%s", __FUNCTION__);
    return -1;
}

// ----------------------------------------------------------------------------
//  MaxSpeakerVolume
//
//  The internal range for Core Audio is 0.0 to 1.0, where 0.0 indicates
//  silence and 1.0 indicates full volume (no attenuation).
//  We add our (webrtc-internal) own max level to match the Wave API and
//  how it is used today in VoE.
// ----------------------------------------------------------------------------

WebRtc_Word32 AudioDeviceWindowsCore::MaxSpeakerVolume(WebRtc_UWord32& maxVolume) const
{
    WEBRTC_TRACE(kTraceModuleCall, kTraceAudioDevice, _id, "%s", __FUNCTION__);

    if (!_speakerIsInitialized)
    {
        return -1;
    }

    maxVolume = static_cast<WebRtc_UWord32> (MAX_CORE_SPEAKER_VOLUME);

    return 0;
}

// ----------------------------------------------------------------------------
//  MinSpeakerVolume
// ----------------------------------------------------------------------------

WebRtc_Word32 AudioDeviceWindowsCore::MinSpeakerVolume(WebRtc_UWord32& minVolume) const
{
    WEBRTC_TRACE(kTraceModuleCall, kTraceAudioDevice, _id, "%s", __FUNCTION__);

    if (!_speakerIsInitialized)
    {
        return -1;
    }

    minVolume = static_cast<WebRtc_UWord32> (MIN_CORE_SPEAKER_VOLUME);

    return 0;
}

// ----------------------------------------------------------------------------
//  SpeakerVolumeStepSize
// ----------------------------------------------------------------------------

WebRtc_Word32 AudioDeviceWindowsCore::SpeakerVolumeStepSize(WebRtc_UWord16& stepSize) const
{
    WEBRTC_TRACE(kTraceModuleCall, kTraceAudioDevice, _id, "%s", __FUNCTION__);

    if (!_speakerIsInitialized)
    {
        return -1;
    }

    stepSize = CORE_SPEAKER_VOLUME_STEP_SIZE;

    return 0;
}

// ----------------------------------------------------------------------------
//  SpeakerMuteIsAvailable
// ----------------------------------------------------------------------------

WebRtc_Word32 AudioDeviceWindowsCore::SpeakerMuteIsAvailable(bool& available)
{
    WEBRTC_TRACE(kTraceModuleCall, kTraceAudioDevice, _id, "%s", __FUNCTION__);

    CriticalSectionScoped lock(_critSect);

    if (_ptrDeviceOut == NULL)
    {
        return -1;
    }

    HRESULT hr = S_OK;
    IAudioSessionManager* pManager = NULL;
    ISimpleAudioVolume* pVolume = NULL;

    hr = _ptrDeviceOut->Activate(__uuidof(IAudioSessionManager), CLSCTX_ALL,NULL, (void**)&pManager);
    EXIT_ON_ERROR(hr);

    hr = pManager->GetSimpleAudioVolume(NULL, FALSE, &pVolume);
    EXIT_ON_ERROR(hr);

    BOOL mute;
    hr = pVolume->GetMute(&mute);
    if (FAILED(hr))
    {
        available = false;
    }
    available = true;

    SAFE_RELEASE(pManager);
    SAFE_RELEASE(pVolume);

    return 0;

Exit:
    _TraceCOMError(hr);
    SAFE_RELEASE(pManager);
    SAFE_RELEASE(pVolume);
    return -1;
}

// ----------------------------------------------------------------------------
//  SetSpeakerMute
// ----------------------------------------------------------------------------

WebRtc_Word32 AudioDeviceWindowsCore::SetSpeakerMute(bool enable)
{
    WEBRTC_TRACE(kTraceModuleCall, kTraceAudioDevice, _id, "AudioDeviceWindowsCore::SetSpeakerMute(enable=%u)", enable);

    CriticalSectionScoped lock(_critSect);

    if (!_speakerIsInitialized)
    {
        return -1;
    }

    if (_ptrDeviceOut == NULL)
    {
        return -1;
    }

    HRESULT hr = S_OK;
    IAudioEndpointVolume* pVolume = NULL;

    hr = _ptrDeviceOut->Activate(__uuidof(IAudioEndpointVolume), CLSCTX_ALL, NULL,  reinterpret_cast<void**>(&pVolume));
    EXIT_ON_ERROR(hr);

    const BOOL mute(enable);
    hr = pVolume->SetMute(mute, NULL);
    EXIT_ON_ERROR(hr);

    SAFE_RELEASE(pVolume);

    return 0;

Exit:
    _TraceCOMError(hr);
    SAFE_RELEASE(pVolume);
    return -1;
}

// ----------------------------------------------------------------------------
//  SpeakerMute
// ----------------------------------------------------------------------------

WebRtc_Word32 AudioDeviceWindowsCore::SpeakerMute(bool& enabled) const
{
    WEBRTC_TRACE(kTraceModuleCall, kTraceAudioDevice, _id, "%s", __FUNCTION__);

    if (!_speakerIsInitialized)
    {
        return -1;
    }

    if (_ptrDeviceOut == NULL)
    {
        return -1;
    }

    HRESULT hr = S_OK;
    IAudioEndpointVolume* pVolume = NULL;

    hr = _ptrDeviceOut->Activate(__uuidof(IAudioEndpointVolume), CLSCTX_ALL, NULL,  reinterpret_cast<void**>(&pVolume));
    EXIT_ON_ERROR(hr);

    BOOL mute;
    hr = pVolume->GetMute(&mute);
    EXIT_ON_ERROR(hr);

    enabled = (mute == TRUE) ? true : false;

    SAFE_RELEASE(pVolume);

    return 0;

Exit:
    _TraceCOMError(hr);
    SAFE_RELEASE(pVolume);
    return -1;
}

// ----------------------------------------------------------------------------
//  MicrophoneMuteIsAvailable
// ----------------------------------------------------------------------------

WebRtc_Word32 AudioDeviceWindowsCore::MicrophoneMuteIsAvailable(bool& available)
{
    WEBRTC_TRACE(kTraceModuleCall, kTraceAudioDevice, _id, "%s", __FUNCTION__);

    CriticalSectionScoped lock(_critSect);

    if (_ptrDeviceIn == NULL)
    {
        return -1;
    }

    HRESULT hr = S_OK;
    IAudioEndpointVolume* pVolume = NULL;

    hr = _ptrDeviceIn->Activate(__uuidof(IAudioEndpointVolume), CLSCTX_ALL, NULL,  reinterpret_cast<void**>(&pVolume));
    EXIT_ON_ERROR(hr);

    BOOL mute;
    hr = pVolume->GetMute(&mute);
    if (FAILED(hr))
    {
        available = false;
    }
    available = true;

    SAFE_RELEASE(pVolume);
    return 0;

Exit:
    _TraceCOMError(hr);
    SAFE_RELEASE(pVolume);
    return -1;
}

// ----------------------------------------------------------------------------
//  SetMicrophoneMute
// ----------------------------------------------------------------------------

WebRtc_Word32 AudioDeviceWindowsCore::SetMicrophoneMute(bool enable)
{
    WEBRTC_TRACE(kTraceModuleCall, kTraceAudioDevice, _id, "AudioDeviceWindowsCore::SetMicrophoneMute(enable=%u)", enable);

    if (!_microphoneIsInitialized)
    {
        return -1;
    }

    if (_ptrDeviceIn == NULL)
    {
        return -1;
    }

    HRESULT hr = S_OK;
    IAudioEndpointVolume* pVolume = NULL;

    hr = _ptrDeviceIn->Activate(__uuidof(IAudioEndpointVolume), CLSCTX_ALL, NULL,  reinterpret_cast<void**>(&pVolume));
    EXIT_ON_ERROR(hr);

    const BOOL mute(enable);
    hr = pVolume->SetMute(mute, NULL);
    EXIT_ON_ERROR(hr);

    SAFE_RELEASE(pVolume);
    return 0;

Exit:
    _TraceCOMError(hr);
    SAFE_RELEASE(pVolume);
    return -1;
}

// ----------------------------------------------------------------------------
//  MicrophoneMute
// ----------------------------------------------------------------------------

WebRtc_Word32 AudioDeviceWindowsCore::MicrophoneMute(bool& enabled) const
{
    WEBRTC_TRACE(kTraceModuleCall, kTraceAudioDevice, _id, "%s", __FUNCTION__);

    if (!_microphoneIsInitialized)
    {
        return -1;
    }

    HRESULT hr = S_OK;
    IAudioEndpointVolume* pVolume = NULL;

    hr = _ptrDeviceIn->Activate(__uuidof(IAudioEndpointVolume), CLSCTX_ALL, NULL,  reinterpret_cast<void**>(&pVolume));
    EXIT_ON_ERROR(hr);

    BOOL mute;
    hr = pVolume->GetMute(&mute);
    EXIT_ON_ERROR(hr);

    enabled = (mute == TRUE) ? true : false;

    SAFE_RELEASE(pVolume);
    return 0;

Exit:
    _TraceCOMError(hr);
    SAFE_RELEASE(pVolume);
    return -1;
}

// ----------------------------------------------------------------------------
//  MicrophoneBoostIsAvailable
// ----------------------------------------------------------------------------

WebRtc_Word32 AudioDeviceWindowsCore::MicrophoneBoostIsAvailable(bool& available)
{
    WEBRTC_TRACE(kTraceModuleCall, kTraceAudioDevice, _id, "%s", __FUNCTION__);

    available = false;
    return 0;
}

// ----------------------------------------------------------------------------
//  SetMicrophoneBoost
// ----------------------------------------------------------------------------

WebRtc_Word32 AudioDeviceWindowsCore::SetMicrophoneBoost(bool enable)
{
    WEBRTC_TRACE(kTraceModuleCall, kTraceAudioDevice, _id, "AudioDeviceWindowsCore::SetMicrophoneBoost(enable=%u)", enable);

    if (!_microphoneIsInitialized)
    {
        return -1;
    }

    return -1;
}

// ----------------------------------------------------------------------------
//  MicrophoneBoost
// ----------------------------------------------------------------------------

WebRtc_Word32 AudioDeviceWindowsCore::MicrophoneBoost(bool& enabled) const
{
    WEBRTC_TRACE(kTraceModuleCall, kTraceAudioDevice, _id, "%s", __FUNCTION__);

    if (!_microphoneIsInitialized)
    {
        return -1;
    }

    return -1;
}

// ----------------------------------------------------------------------------
//  StereoRecordingIsAvailable
// ----------------------------------------------------------------------------

WebRtc_Word32 AudioDeviceWindowsCore::StereoRecordingIsAvailable(bool& available)
{
    WEBRTC_TRACE(kTraceModuleCall, kTraceAudioDevice, _id, "%s", __FUNCTION__);

    available = true;
    return 0;
}

// ----------------------------------------------------------------------------
//  SetStereoRecording
// ----------------------------------------------------------------------------

WebRtc_Word32 AudioDeviceWindowsCore::SetStereoRecording(bool enable)
{
    WEBRTC_TRACE(kTraceModuleCall, kTraceAudioDevice, _id, "AudioDeviceWindowsCore::SetStereoRecording(enable=%u)", enable);

    CriticalSectionScoped lock(_critSect);

    if (enable)
    {
        _recChannelsPrioList[0] = 2;    // try stereo first
        _recChannelsPrioList[1] = 1;
        _recChannels = 2;
    }
    else
    {
        _recChannelsPrioList[0] = 1;    // try mono first
        _recChannelsPrioList[1] = 2;
        _recChannels = 1;
    }

    return 0;
}

// ----------------------------------------------------------------------------
//  StereoRecording
// ----------------------------------------------------------------------------

WebRtc_Word32 AudioDeviceWindowsCore::StereoRecording(bool& enabled) const
{
    WEBRTC_TRACE(kTraceModuleCall, kTraceAudioDevice, _id, "%s", __FUNCTION__);

    if (_recChannels == 2)
        enabled = true;
    else
        enabled = false;

    return 0;
}

// ----------------------------------------------------------------------------
//  StereoPlayoutIsAvailable
// ----------------------------------------------------------------------------

WebRtc_Word32 AudioDeviceWindowsCore::StereoPlayoutIsAvailable(bool& available)
{
    WEBRTC_TRACE(kTraceModuleCall, kTraceAudioDevice, _id, "%s", __FUNCTION__);

    available = true;
    return 0;
}

// ----------------------------------------------------------------------------
//  SetStereoPlayout
// ----------------------------------------------------------------------------

WebRtc_Word32 AudioDeviceWindowsCore::SetStereoPlayout(bool enable)
{
    WEBRTC_TRACE(kTraceModuleCall, kTraceAudioDevice, _id, "AudioDeviceWindowsCore::SetStereoPlayout(enable=%u)", enable);

    CriticalSectionScoped lock(_critSect);

    if (enable)
    {
        _playChannelsPrioList[0] = 2;    // try stereo first
        _playChannelsPrioList[1] = 1;
        _playChannels = 2;
    }
    else
    {
        _playChannelsPrioList[0] = 1;    // try mono first
        _playChannelsPrioList[1] = 2;
        _playChannels = 1;
    }

    return 0;
}

// ----------------------------------------------------------------------------
//  StereoPlayout
// ----------------------------------------------------------------------------

WebRtc_Word32 AudioDeviceWindowsCore::StereoPlayout(bool& enabled) const
{
    WEBRTC_TRACE(kTraceModuleCall, kTraceAudioDevice, _id, "%s", __FUNCTION__);

    if (_playChannels == 2)
        enabled = true;
    else
        enabled = false;

    return 0;
}

// ----------------------------------------------------------------------------
//  SetAGC
// ----------------------------------------------------------------------------

WebRtc_Word32 AudioDeviceWindowsCore::SetAGC(bool enable)
{
    WEBRTC_TRACE(kTraceModuleCall, kTraceAudioDevice, _id, "AudioDeviceWindowsCore::SetAGC(enable=%d)", enable);
    CriticalSectionScoped lock(_critSect);
    _AGC = enable;
    return 0;
}

// ----------------------------------------------------------------------------
//  AGC
// ----------------------------------------------------------------------------

bool AudioDeviceWindowsCore::AGC() const
{
    CriticalSectionScoped lock(_critSect);
    return _AGC;
}

// ----------------------------------------------------------------------------
//  MicrophoneVolumeIsAvailable
// ----------------------------------------------------------------------------

WebRtc_Word32 AudioDeviceWindowsCore::MicrophoneVolumeIsAvailable(bool& available)
{
    WEBRTC_TRACE(kTraceModuleCall, kTraceAudioDevice, _id, "%s", __FUNCTION__);

    CriticalSectionScoped lock(_critSect);

    if (_ptrDeviceIn == NULL)
    {
        return -1;
    }

    HRESULT hr = S_OK;
    IAudioEndpointVolume* pVolume = NULL;

    hr = _ptrDeviceIn->Activate(__uuidof(IAudioEndpointVolume), CLSCTX_ALL, NULL, reinterpret_cast<void**>(&pVolume));
    EXIT_ON_ERROR(hr);

    float volume(0.0f);
    hr = pVolume->GetMasterVolumeLevelScalar(&volume);
    if (FAILED(hr))
    {
        available = false;
    }
    available = true;

    SAFE_RELEASE(pVolume);
    return 0;

Exit:
    _TraceCOMError(hr);
    SAFE_RELEASE(pVolume);
    return -1;
}

// ----------------------------------------------------------------------------
//  SetMicrophoneVolume
// ----------------------------------------------------------------------------

WebRtc_Word32 AudioDeviceWindowsCore::SetMicrophoneVolume(WebRtc_UWord32 volume)
{
    WEBRTC_TRACE(kTraceStream, kTraceAudioDevice, _id, "AudioDeviceWindowsCore::SetMicrophoneVolume(volume=%u)", volume);

    {
        CriticalSectionScoped lock(_critSect);

        if (!_microphoneIsInitialized)
        {
            return -1;
        }

        if (_ptrDeviceIn == NULL)
        {
            return -1;
        }
    }

    if (volume < static_cast<WebRtc_UWord32>(MIN_CORE_MICROPHONE_VOLUME) ||
        volume > static_cast<WebRtc_UWord32>(MAX_CORE_MICROPHONE_VOLUME))
    {
        return -1;
    }

    HRESULT hr = S_OK;
    // scale input volume to valid range (0.0 to 1.0)
    const float fLevel = static_cast<float>(volume)/MAX_CORE_MICROPHONE_VOLUME;
    _volumeMutex.Enter();
    _ptrCaptureVolume->SetMasterVolumeLevelScalar(fLevel, NULL);
    _volumeMutex.Leave();
    EXIT_ON_ERROR(hr);

    return 0;

Exit:
    _TraceCOMError(hr);
    return -1;
}

// ----------------------------------------------------------------------------
//  MicrophoneVolume
// ----------------------------------------------------------------------------

WebRtc_Word32 AudioDeviceWindowsCore::MicrophoneVolume(WebRtc_UWord32& volume) const
{
    {
        CriticalSectionScoped lock(_critSect);

        if (!_microphoneIsInitialized)
        {
            return -1;
        }

        if (_ptrDeviceIn == NULL)
        {
            return -1;
        }
    }

    HRESULT hr = S_OK;
    float fLevel(0.0f);
    volume = 0;
    _volumeMutex.Enter();
    hr = _ptrCaptureVolume->GetMasterVolumeLevelScalar(&fLevel);
    _volumeMutex.Leave();
    EXIT_ON_ERROR(hr);

    // scale input volume range [0.0,1.0] to valid output range
    volume = static_cast<WebRtc_UWord32> (fLevel*MAX_CORE_MICROPHONE_VOLUME);

    return 0;

Exit:
    _TraceCOMError(hr);
    return -1;
}

// ----------------------------------------------------------------------------
//  MaxMicrophoneVolume
//
//  The internal range for Core Audio is 0.0 to 1.0, where 0.0 indicates
//  silence and 1.0 indicates full volume (no attenuation).
//  We add our (webrtc-internal) own max level to match the Wave API and
//  how it is used today in VoE.
// ----------------------------------------------------------------------------

WebRtc_Word32 AudioDeviceWindowsCore::MaxMicrophoneVolume(WebRtc_UWord32& maxVolume) const
{
    WEBRTC_TRACE(kTraceStream, kTraceAudioDevice, _id, "%s", __FUNCTION__);

    if (!_microphoneIsInitialized)
    {
        return -1;
    }

    maxVolume = static_cast<WebRtc_UWord32> (MAX_CORE_MICROPHONE_VOLUME);

    return 0;
}

// ----------------------------------------------------------------------------
//  MinMicrophoneVolume
// ----------------------------------------------------------------------------

WebRtc_Word32 AudioDeviceWindowsCore::MinMicrophoneVolume(WebRtc_UWord32& minVolume) const
{
    WEBRTC_TRACE(kTraceModuleCall, kTraceAudioDevice, _id, "%s", __FUNCTION__);

    if (!_microphoneIsInitialized)
    {
        return -1;
    }

    minVolume = static_cast<WebRtc_UWord32> (MIN_CORE_MICROPHONE_VOLUME);

    return 0;
}

// ----------------------------------------------------------------------------
//  MicrophoneVolumeStepSize
// ----------------------------------------------------------------------------

WebRtc_Word32 AudioDeviceWindowsCore::MicrophoneVolumeStepSize(WebRtc_UWord16& stepSize) const
{
    WEBRTC_TRACE(kTraceModuleCall, kTraceAudioDevice, _id, "%s", __FUNCTION__);

    if (!_microphoneIsInitialized)
    {
        return -1;
    }

    stepSize = CORE_MICROPHONE_VOLUME_STEP_SIZE;

    return 0;
}

// ----------------------------------------------------------------------------
//  PlayoutDevices
// ----------------------------------------------------------------------------

WebRtc_Word16 AudioDeviceWindowsCore::PlayoutDevices()
{
    WEBRTC_TRACE(kTraceModuleCall, kTraceAudioDevice, _id, "%s", __FUNCTION__);

    CriticalSectionScoped lock(_critSect);

    if (_RefreshDeviceList(eRender) != -1)
    {
        return (_DeviceListCount(eRender));
    }

    return -1;
}

// ----------------------------------------------------------------------------
//  SetPlayoutDevice I (II)
// ----------------------------------------------------------------------------

WebRtc_Word32 AudioDeviceWindowsCore::SetPlayoutDevice(WebRtc_UWord16 index)
{
    WEBRTC_TRACE(kTraceModuleCall, kTraceAudioDevice, _id, "AudioDeviceWindowsCore::SetPlayoutDevice(index=%u)", index);

    if (_playIsInitialized)
    {
        return -1;
    }

    // Get current number of available rendering endpoint devices and refresh the rendering collection.
    UINT nDevices = PlayoutDevices();

    if (index < 0 || index > (nDevices-1))
    {
        WEBRTC_TRACE(kTraceError, kTraceAudioDevice, _id, "device index is out of range [0,%u]", (nDevices-1));
        return -1;
    }

    CriticalSectionScoped lock(_critSect);

    HRESULT hr(S_OK);

    assert(_ptrRenderCollection != NULL);

    //  Select an endpoint rendering device given the specified index
    SAFE_RELEASE(_ptrDeviceOut);
    hr = _ptrRenderCollection->Item(
                                 index,
                                 &_ptrDeviceOut);
    if (FAILED(hr))
    {
        _TraceCOMError(hr);
        SAFE_RELEASE(_ptrDeviceOut);
        return -1;
    }

    WCHAR szDeviceName[MAX_PATH];
    const int bufferLen = sizeof(szDeviceName)/sizeof(szDeviceName)[0];

    // Get the endpoint device's friendly-name
    if (_GetDeviceName(_ptrDeviceOut, szDeviceName, bufferLen) == 0)
    {
        WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, _id, "friendly name: \"%S\"", szDeviceName);
    }

    _usingOutputDeviceIndex = true;
    _outputDeviceIndex = index;

    return 0;
}

// ----------------------------------------------------------------------------
//  SetPlayoutDevice II (II)
// ----------------------------------------------------------------------------

WebRtc_Word32 AudioDeviceWindowsCore::SetPlayoutDevice(AudioDeviceModule::WindowsDeviceType device)
{
    if (_playIsInitialized)
    {
        return -1;
    }

    ERole role(eCommunications);

    if (device == AudioDeviceModule::kDefaultDevice)
    {
        role = eConsole;
        WEBRTC_TRACE(kTraceModuleCall, kTraceAudioDevice, _id, "AudioDeviceWindowsCore::SetPlayoutDevice(kDefaultDevice)");
    }
    else if (device == AudioDeviceModule::kDefaultCommunicationDevice)
    {
        role = eCommunications;
        WEBRTC_TRACE(kTraceModuleCall, kTraceAudioDevice, _id, "AudioDeviceWindowsCore::SetPlayoutDevice(kDefaultCommunicationDevice)");
    }

    CriticalSectionScoped lock(_critSect);

    // Refresh the list of rendering endpoint devices
    _RefreshDeviceList(eRender);

    HRESULT hr(S_OK);

    assert(_ptrEnumerator != NULL);

    //  Select an endpoint rendering device given the specified role
    SAFE_RELEASE(_ptrDeviceOut);
    hr = _ptrEnumerator->GetDefaultAudioEndpoint(
                           eRender,
                           role,
                           &_ptrDeviceOut);
    if (FAILED(hr))
    {
        _TraceCOMError(hr);
        SAFE_RELEASE(_ptrDeviceOut);
        return -1;
    }

    WCHAR szDeviceName[MAX_PATH];
    const int bufferLen = sizeof(szDeviceName)/sizeof(szDeviceName)[0];

    // Get the endpoint device's friendly-name
    if (_GetDeviceName(_ptrDeviceOut, szDeviceName, bufferLen) == 0)
    {
        WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, _id, "friendly name: \"%S\"", szDeviceName);
    }

    _usingOutputDeviceIndex = false;
    _outputDevice = device;

    return 0;
}

// ----------------------------------------------------------------------------
//  PlayoutDeviceName
// ----------------------------------------------------------------------------

WebRtc_Word32 AudioDeviceWindowsCore::PlayoutDeviceName(WebRtc_UWord16 index, WebRtc_Word8 name[kAdmMaxDeviceNameSize], WebRtc_Word8 guid[kAdmMaxGuidSize])
{
    WEBRTC_TRACE(kTraceModuleCall, kTraceAudioDevice, _id, "AudioDeviceWindowsCore::PlayoutDeviceName(index=%u)", index);

    bool defaultCommunicationDevice(false);
    const WebRtc_Word16 nDevices(PlayoutDevices());  // also updates the list of devices

    // Special fix for the case when the user selects '-1' as index (<=> Default Communication Device)
    if (index == (WebRtc_UWord16)(-1))
    {
        defaultCommunicationDevice = true;
        index = 0;
        WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, _id, "Default Communication endpoint device will be used");
    }

    if ((index > (nDevices-1)) || (name == NULL))
    {
        return -1;
    }

    memset(name, 0, kAdmMaxDeviceNameSize);

    if (guid != NULL)
    {
        memset(guid, 0, kAdmMaxGuidSize);
    }

    CriticalSectionScoped lock(_critSect);

    HRESULT hr(S_OK);
    WebRtc_Word32 ret(-1);
    WCHAR szDeviceName[MAX_PATH];
    const int bufferLen = sizeof(szDeviceName)/sizeof(szDeviceName)[0];

    // Get the endpoint device's friendly-name
    if (defaultCommunicationDevice)
    {
        ret = _GetDefaultDeviceName(eRender, eCommunications, szDeviceName, bufferLen);
    }
    else
    {
        ret = _GetListDeviceName(eRender, index, szDeviceName, bufferLen);
    }

    if (ret == 0)
    {
        // Convert the endpoint device's friendly-name to UTF-8
        if (WideCharToMultiByte(CP_UTF8, 0, szDeviceName, -1, name, kAdmMaxDeviceNameSize, NULL, NULL) == 0)
        {
            WEBRTC_TRACE(kTraceError, kTraceAudioDevice, _id, "WideCharToMultiByte(CP_UTF8) failed with error code %d", GetLastError());
        }
    }

    // Get the endpoint ID string (uniquely identifies the device among all audio endpoint devices)
    if (defaultCommunicationDevice)
    {
        ret = _GetDefaultDeviceID(eRender, eCommunications, szDeviceName, bufferLen);
    }
    else
    {
        ret = _GetListDeviceID(eRender, index, szDeviceName, bufferLen);
    }

    if (guid != NULL && ret == 0)
    {
        // Convert the endpoint device's ID string to UTF-8
        if (WideCharToMultiByte(CP_UTF8, 0, szDeviceName, -1, guid, kAdmMaxGuidSize, NULL, NULL) == 0)
        {
            WEBRTC_TRACE(kTraceError, kTraceAudioDevice, _id, "WideCharToMultiByte(CP_UTF8) failed with error code %d", GetLastError());
        }
    }

    return ret;
}

// ----------------------------------------------------------------------------
//  RecordingDeviceName
// ----------------------------------------------------------------------------

WebRtc_Word32 AudioDeviceWindowsCore::RecordingDeviceName(WebRtc_UWord16 index, WebRtc_Word8 name[kAdmMaxDeviceNameSize], WebRtc_Word8 guid[kAdmMaxGuidSize])
{
    WEBRTC_TRACE(kTraceModuleCall, kTraceAudioDevice, _id, "AudioDeviceWindowsCore::RecordingDeviceName(index=%u)", index);

    bool defaultCommunicationDevice(false);
    const WebRtc_Word16 nDevices(RecordingDevices());  // also updates the list of devices

    // Special fix for the case when the user selects '-1' as index (<=> Default Communication Device)
    if (index == (WebRtc_UWord16)(-1))
    {
        defaultCommunicationDevice = true;
        index = 0;
        WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, _id, "Default Communication endpoint device will be used");
    }

    if ((index > (nDevices-1)) || (name == NULL))
    {
        return -1;
    }

    memset(name, 0, kAdmMaxDeviceNameSize);

    if (guid != NULL)
    {
        memset(guid, 0, kAdmMaxGuidSize);
    }

    CriticalSectionScoped lock(_critSect);

    HRESULT hr(S_OK);
    WebRtc_Word32 ret(-1);
    WCHAR szDeviceName[MAX_PATH];
    const int bufferLen = sizeof(szDeviceName)/sizeof(szDeviceName)[0];

    // Get the endpoint device's friendly-name
    if (defaultCommunicationDevice)
    {
        ret = _GetDefaultDeviceName(eCapture, eCommunications, szDeviceName, bufferLen);
    }
    else
    {
        ret = _GetListDeviceName(eCapture, index, szDeviceName, bufferLen);
    }

    if (ret == 0)
    {
        // Convert the endpoint device's friendly-name to UTF-8
        if (WideCharToMultiByte(CP_UTF8, 0, szDeviceName, -1, name, kAdmMaxDeviceNameSize, NULL, NULL) == 0)
        {
            WEBRTC_TRACE(kTraceError, kTraceAudioDevice, _id, "WideCharToMultiByte(CP_UTF8) failed with error code %d", GetLastError());
        }
    }

    // Get the endpoint ID string (uniquely identifies the device among all audio endpoint devices)
    if (defaultCommunicationDevice)
    {
        ret = _GetDefaultDeviceID(eCapture, eCommunications, szDeviceName, bufferLen);
    }
    else
    {
        ret = _GetListDeviceID(eCapture, index, szDeviceName, bufferLen);
    }

    if (guid != NULL && ret == 0)
    {
        // Convert the endpoint device's ID string to UTF-8
        if (WideCharToMultiByte(CP_UTF8, 0, szDeviceName, -1, guid, kAdmMaxGuidSize, NULL, NULL) == 0)
        {
            WEBRTC_TRACE(kTraceError, kTraceAudioDevice, _id, "WideCharToMultiByte(CP_UTF8) failed with error code %d", GetLastError());
        }
    }

    return ret;
}

// ----------------------------------------------------------------------------
//  RecordingDevices
// ----------------------------------------------------------------------------

WebRtc_Word16 AudioDeviceWindowsCore::RecordingDevices()
{
    WEBRTC_TRACE(kTraceModuleCall, kTraceAudioDevice, _id, "%s", __FUNCTION__);

    CriticalSectionScoped lock(_critSect);

    if (_RefreshDeviceList(eCapture) != -1)
    {
        return (_DeviceListCount(eCapture));
    }

    return -1;
}

// ----------------------------------------------------------------------------
//  SetRecordingDevice I (II)
// ----------------------------------------------------------------------------

WebRtc_Word32 AudioDeviceWindowsCore::SetRecordingDevice(WebRtc_UWord16 index)
{
    WEBRTC_TRACE(kTraceModuleCall, kTraceAudioDevice, _id, "AudioDeviceWindowsCore::SetRecordingDevice(index=%u)", index);

    if (_recIsInitialized)
    {
        return -1;
    }

    // Get current number of available capture endpoint devices and refresh the capture collection.
    UINT nDevices = RecordingDevices();

    if (index < 0 || index > (nDevices-1))
    {
        WEBRTC_TRACE(kTraceError, kTraceAudioDevice, _id, "device index is out of range [0,%u]", (nDevices-1));
        return -1;
    }

    CriticalSectionScoped lock(_critSect);

    HRESULT hr(S_OK);

    assert(_ptrCaptureCollection != NULL);

    // Select an endpoint capture device given the specified index
    SAFE_RELEASE(_ptrDeviceIn);
    hr = _ptrCaptureCollection->Item(
                                 index,
                                 &_ptrDeviceIn);
    if (FAILED(hr))
    {
        _TraceCOMError(hr);
        SAFE_RELEASE(_ptrDeviceIn);
        return -1;
    }

    WCHAR szDeviceName[MAX_PATH];
    const int bufferLen = sizeof(szDeviceName)/sizeof(szDeviceName)[0];

    // Get the endpoint device's friendly-name
    if (_GetDeviceName(_ptrDeviceIn, szDeviceName, bufferLen) == 0)
    {
        WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, _id, "friendly name: \"%S\"", szDeviceName);
    }

    _usingInputDeviceIndex = true;
    _inputDeviceIndex = index;

    return 0;
}

// ----------------------------------------------------------------------------
//  SetRecordingDevice II (II)
// ----------------------------------------------------------------------------

WebRtc_Word32 AudioDeviceWindowsCore::SetRecordingDevice(AudioDeviceModule::WindowsDeviceType device)
{
    if (_recIsInitialized)
    {
        return -1;
    }

    ERole role(eCommunications);

    if (device == AudioDeviceModule::kDefaultDevice)
    {
        role = eConsole;
        WEBRTC_TRACE(kTraceModuleCall, kTraceAudioDevice, _id, "AudioDeviceWindowsCore::SetRecordingDevice(kDefaultDevice)");
    }
    else if (device == AudioDeviceModule::kDefaultCommunicationDevice)
    {
        role = eCommunications;
        WEBRTC_TRACE(kTraceModuleCall, kTraceAudioDevice, _id, "AudioDeviceWindowsCore::SetRecordingDevice(kDefaultCommunicationDevice)");
    }

    CriticalSectionScoped lock(_critSect);

    // Refresh the list of capture endpoint devices
    _RefreshDeviceList(eCapture);

    HRESULT hr(S_OK);

    assert(_ptrEnumerator != NULL);

    //  Select an endpoint capture device given the specified role
    SAFE_RELEASE(_ptrDeviceIn);
    hr = _ptrEnumerator->GetDefaultAudioEndpoint(
                           eCapture,
                           role,
                           &_ptrDeviceIn);
    if (FAILED(hr))
    {
        _TraceCOMError(hr);
        SAFE_RELEASE(_ptrDeviceIn);
        return -1;
    }

    WCHAR szDeviceName[MAX_PATH];
    const int bufferLen = sizeof(szDeviceName)/sizeof(szDeviceName)[0];

    // Get the endpoint device's friendly-name
    if (_GetDeviceName(_ptrDeviceIn, szDeviceName, bufferLen) == 0)
    {
        WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, _id, "friendly name: \"%S\"", szDeviceName);
    }

    _usingInputDeviceIndex = false;
    _inputDevice = device;

    return 0;
}

// ----------------------------------------------------------------------------
//  PlayoutIsAvailable
// ----------------------------------------------------------------------------

WebRtc_Word32 AudioDeviceWindowsCore::PlayoutIsAvailable(bool& available)
{
    WEBRTC_TRACE(kTraceModuleCall, kTraceAudioDevice, _id, "%s", __FUNCTION__);

    available = false;

    // Try to initialize the playout side
    WebRtc_Word32 res = InitPlayout();

    // Cancel effect of initialization
    StopPlayout();

    if (res != -1)
    {
        available = true;
    }

    return 0;
}

// ----------------------------------------------------------------------------
//  RecordingIsAvailable
// ----------------------------------------------------------------------------

WebRtc_Word32 AudioDeviceWindowsCore::RecordingIsAvailable(bool& available)
{
    WEBRTC_TRACE(kTraceModuleCall, kTraceAudioDevice, _id, "%s", __FUNCTION__);

    available = false;

    // Try to initialize the recording side
    WebRtc_Word32 res = InitRecording();

    // Cancel effect of initialization
    StopRecording();

    if (res != -1)
    {
        available = true;
    }

    return 0;
}

// ----------------------------------------------------------------------------
//  InitPlayout
// ----------------------------------------------------------------------------

WebRtc_Word32 AudioDeviceWindowsCore::InitPlayout()
{
    WEBRTC_TRACE(kTraceModuleCall, kTraceAudioDevice, _id, "%s", __FUNCTION__);

    CriticalSectionScoped lock(_critSect);

    if (_playing)
    {
        return -1;
    }

    if (_playIsInitialized)
    {
        return 0;
    }

    if (_ptrDeviceOut == NULL)
    {
        return -1;
    }

    // Initialize the speaker (devices might have been added or removed)
    if (InitSpeaker() == -1)
    {
        WEBRTC_TRACE(kTraceWarning, kTraceAudioDevice, _id, "InitSpeaker() failed");
    }

    // Ensure that the updated rendering endpoint device is valid
    if (_ptrDeviceOut == NULL)
    {
        return -1;
    }

    HRESULT hr = S_OK;
    WAVEFORMATEX* pWfxOut = NULL;
    WAVEFORMATEX Wfx;
    WAVEFORMATEX* pWfxClosestMatch = NULL;

    // Create COM object with IAudioClient interface.
    SAFE_RELEASE(_ptrClientOut);
    hr = _ptrDeviceOut->Activate(
                          __uuidof(IAudioClient),
                          CLSCTX_ALL,
                          NULL,
                          (void**)&_ptrClientOut);
    EXIT_ON_ERROR(hr);

    // Retrieve the stream format that the audio engine uses for its internal
    // processing (mixing) of shared-mode streams.
    hr = _ptrClientOut->GetMixFormat(&pWfxOut);
    if (SUCCEEDED(hr))
    {
        WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, _id, "Audio Engine's current rendering mix format:");
        // format type
        WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, _id, "wFormatTag     : 0x%X (%u)", pWfxOut->wFormatTag, pWfxOut->wFormatTag);
        // number of channels (i.e. mono, stereo...)
        WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, _id, "nChannels      : %d", pWfxOut->nChannels);
        // sample rate
        WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, _id, "nSamplesPerSec : %d", pWfxOut->nSamplesPerSec);
        // for buffer estimation
        WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, _id, "nAvgBytesPerSec: %d", pWfxOut->nAvgBytesPerSec);
        // block size of data
        WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, _id, "nBlockAlign    : %d", pWfxOut->nBlockAlign);
        // number of bits per sample of mono data
        WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, _id, "wBitsPerSample : %d", pWfxOut->wBitsPerSample);
        WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, _id, "cbSize         : %d", pWfxOut->cbSize);
    }

    // Set wave format
    Wfx.wFormatTag = WAVE_FORMAT_PCM;
    Wfx.wBitsPerSample = 16;
    Wfx.cbSize = 0;

    const int freqs[6] = {48000, 44100, 16000, 96000, 32000, 8000};
    hr = S_FALSE;

    // Iterate over frequencies and channels, in order of priority
    for (int freq = 0; freq < sizeof(freqs)/sizeof(freqs[0]); freq++)
    {
        for (int chan = 0; chan < sizeof(_playChannelsPrioList)/sizeof(_playChannelsPrioList[0]); chan++)
        {
            Wfx.nChannels = _playChannelsPrioList[chan];
            Wfx.nSamplesPerSec = freqs[freq];
            Wfx.nBlockAlign = Wfx.nChannels * Wfx.wBitsPerSample / 8;
            Wfx.nAvgBytesPerSec = Wfx.nSamplesPerSec * Wfx.nBlockAlign;
            // If the method succeeds and the audio endpoint device supports the specified stream format,
            // it returns S_OK. If the method succeeds and provides a closest match to the specified format,
            // it returns S_FALSE.
            hr = _ptrClientOut->IsFormatSupported(
                                  AUDCLNT_SHAREMODE_SHARED,
                                  &Wfx,
                                  &pWfxClosestMatch);
            if (hr == S_OK)
            {
                break;
            }
            else
            {
                WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, _id, "nChannels=%d, nSamplesPerSec=%d is not supported",
                    Wfx.nChannels, Wfx.nSamplesPerSec);
            }
        }
        if (hr == S_OK)
            break;
    }

    if (hr == S_OK)
    {
        _playAudioFrameSize = Wfx.nBlockAlign;
        _playBlockSize = Wfx.nSamplesPerSec/100;
        _playSampleRate = Wfx.nSamplesPerSec;
        _devicePlaySampleRate = Wfx.nSamplesPerSec; // The device itself continues to run at 44.1 kHz.
        _devicePlayBlockSize = Wfx.nSamplesPerSec/100;
        if (_playBlockSize == 441)
        {
            _playSampleRate = 44000;    // we are actually running at 44000 Hz and *not* 44100 Hz
            _playBlockSize = 440;       // adjust to size we can handle
        }
        _playChannels = Wfx.nChannels;

        WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, _id, "VoE selected this rendering format:");
        WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, _id, "wFormatTag         : 0x%X (%u)", Wfx.wFormatTag, Wfx.wFormatTag);
        WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, _id, "nChannels          : %d", Wfx.nChannels);
        WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, _id, "nSamplesPerSec     : %d", Wfx.nSamplesPerSec);
        WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, _id, "nAvgBytesPerSec    : %d", Wfx.nAvgBytesPerSec);
        WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, _id, "nBlockAlign        : %d", Wfx.nBlockAlign);
        WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, _id, "wBitsPerSample     : %d", Wfx.wBitsPerSample);
        WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, _id, "cbSize             : %d", Wfx.cbSize);
        WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, _id, "Additional settings:");
        WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, _id, "_playAudioFrameSize: %d", _playAudioFrameSize);
        WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, _id, "_playBlockSize     : %d", _playBlockSize);
        WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, _id, "_playChannels      : %d", _playChannels);
    }

    _Get44kHzDrift();

    // Create a rendering stream.
    //
    // ****************************************************************************
    // For a shared-mode stream that uses event-driven buffering, the caller must
    // set both hnsPeriodicity and hnsBufferDuration to 0. The Initialize method
    // determines how large a buffer to allocate based on the scheduling period
    // of the audio engine. Although the client's buffer processing thread is
    // event driven, the basic buffer management process, as described previously,
    // is unaltered.
    // Each time the thread awakens, it should call IAudioClient::GetCurrentPadding
    // to determine how much data to write to a rendering buffer or read from a capture
    // buffer. In contrast to the two buffers that the Initialize method allocates
    // for an exclusive-mode stream that uses event-driven buffering, a shared-mode
    // stream requires a single buffer.
    // ****************************************************************************
    //
    REFERENCE_TIME hnsBufferDuration = 0;  // ask for minimum buffer size (default)
    if (_devicePlaySampleRate == 44100)
    {
        // Ask for a larger buffer size (30ms) when using 44.1kHz as render rate.
        // There seems to be a larger risk of underruns for 44.1 compared
        // with the default rate (48kHz). When using default, we set the requested
        // buffer duration to 0, which sets the buffer to the minimum size
        // required by the engine thread. The actual buffer size can then be
        // read by GetBufferSize() and it is 20ms on most machines.
        hnsBufferDuration = 30*10000;
    }
    hr = _ptrClientOut->Initialize(
                          AUDCLNT_SHAREMODE_SHARED,             // share Audio Engine with other applications
                          AUDCLNT_STREAMFLAGS_EVENTCALLBACK,    // processing of the audio buffer by the client will be event driven
                          hnsBufferDuration,                    // requested buffer capacity as a time value (in 100-nanosecond units)
                          0,                                    // periodicity
                          &Wfx,                                 // selected wave format
                          NULL);                                // session GUID

    if (FAILED(hr))
    {
        WEBRTC_TRACE(kTraceError, kTraceAudioDevice, _id, "IAudioClient::Initialize() failed:");
        if (pWfxClosestMatch != NULL)
        {
            WEBRTC_TRACE(kTraceError, kTraceAudioDevice, _id, "closest mix format: #channels=%d, samples/sec=%d, bits/sample=%d",
                pWfxClosestMatch->nChannels, pWfxClosestMatch->nSamplesPerSec, pWfxClosestMatch->wBitsPerSample);
        }
        else
        {
            WEBRTC_TRACE(kTraceError, kTraceAudioDevice, _id, "no format suggested");
        }
    }
    EXIT_ON_ERROR(hr);

    if (_ptrAudioBuffer)
    {
        // Update the audio buffer with the selected parameters
        _ptrAudioBuffer->SetPlayoutSampleRate(_playSampleRate);
        _ptrAudioBuffer->SetPlayoutChannels((WebRtc_UWord8)_playChannels);
    }
    else
    {
        // We can enter this state during CoreAudioIsSupported() when no AudioDeviceImplementation
        // has been created, hence the AudioDeviceBuffer does not exist.
        // It is OK to end up here since we don't initiate any media in CoreAudioIsSupported().
        WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, _id, "AudioDeviceBuffer must be attached before streaming can start");
    }

    // Get the actual size of the shared (endpoint buffer).
    // Typical value is 960 audio frames <=> 20ms @ 48kHz sample rate.
    UINT bufferFrameCount(0);
    hr = _ptrClientOut->GetBufferSize(
                          &bufferFrameCount);
    if (SUCCEEDED(hr))
    {
        WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, _id, "IAudioClient::GetBufferSize() => %u (<=> %u bytes)",
            bufferFrameCount, bufferFrameCount*_playAudioFrameSize);
    }

    // Set the event handle that the system signals when an audio buffer is ready
    // to be processed by the client.
    hr = _ptrClientOut->SetEventHandle(
                          _hRenderSamplesReadyEvent);
    EXIT_ON_ERROR(hr);

    // Get an IAudioRenderClient interface.
    SAFE_RELEASE(_ptrRenderClient);
    hr = _ptrClientOut->GetService(
                          __uuidof(IAudioRenderClient),
                          (void**)&_ptrRenderClient);
    EXIT_ON_ERROR(hr);

    // Mark playout side as initialized
    _playIsInitialized = true;

    CoTaskMemFree(pWfxOut);
    CoTaskMemFree(pWfxClosestMatch);

    WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, _id, "render side is now initialized");
    return 0;

Exit:
    _TraceCOMError(hr);
    CoTaskMemFree(pWfxOut);
    CoTaskMemFree(pWfxClosestMatch);
    SAFE_RELEASE(_ptrClientOut);
    SAFE_RELEASE(_ptrRenderClient);
    return -1;
}

// ----------------------------------------------------------------------------
//  InitRecording
// ----------------------------------------------------------------------------

WebRtc_Word32 AudioDeviceWindowsCore::InitRecording()
{
    WEBRTC_TRACE(kTraceModuleCall, kTraceAudioDevice, _id, "%s", __FUNCTION__);

    CriticalSectionScoped lock(_critSect);

    if (_recording)
    {
        return -1;
    }

    if (_recIsInitialized)
    {
        return 0;
    }

    if (QueryPerformanceFrequency(&_perfCounterFreq) == 0)
    {
        return -1;
    }
    _perfCounterFactor = 10000000.0 / (double)_perfCounterFreq.QuadPart;

    if (_ptrDeviceIn == NULL)
    {
        return -1;
    }

    // Initialize the microphone (devices might have been added or removed)
    if (InitMicrophone() == -1)
    {
        WEBRTC_TRACE(kTraceWarning, kTraceAudioDevice, _id, "InitMicrophone() failed");
    }

    // Ensure that the updated capturing endpoint device is valid
    if (_ptrDeviceIn == NULL)
    {
        return -1;
    }

    HRESULT hr = S_OK;
    WAVEFORMATEX* pWfxIn = NULL;
    WAVEFORMATEX Wfx;
    WAVEFORMATEX* pWfxClosestMatch = NULL;


    // Create COM object with IAudioClient interface.
    SAFE_RELEASE(_ptrClientIn);
    hr = _ptrDeviceIn->Activate(
                          __uuidof(IAudioClient),
                          CLSCTX_ALL,
                          NULL,
                          (void**)&_ptrClientIn);
    EXIT_ON_ERROR(hr);


    // Retrieve the stream format that the audio engine uses for its internal
    // processing (mixing) of shared-mode streams.
    hr = _ptrClientIn->GetMixFormat(&pWfxIn);
    if (SUCCEEDED(hr))
    {
        WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, _id, "Audio Engine's current capturing mix format:");
        // format type
        WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, _id, "wFormatTag     : 0x%X (%u)", pWfxIn->wFormatTag, pWfxIn->wFormatTag);
        // number of channels (i.e. mono, stereo...)
        WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, _id, "nChannels      : %d", pWfxIn->nChannels);
        // sample rate
        WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, _id, "nSamplesPerSec : %d", pWfxIn->nSamplesPerSec);
        // for buffer estimation
        WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, _id, "nAvgBytesPerSec: %d", pWfxIn->nAvgBytesPerSec);
        // block size of data
        WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, _id, "nBlockAlign    : %d", pWfxIn->nBlockAlign);
        // number of bits per sample of mono data
        WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, _id, "wBitsPerSample : %d", pWfxIn->wBitsPerSample);
        WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, _id, "cbSize         : %d", pWfxIn->cbSize);
    }

    // Set wave format
    Wfx.wFormatTag = WAVE_FORMAT_PCM;
    Wfx.wBitsPerSample = 16;
    Wfx.cbSize = 0;

    const int freqs[6] = {48000, 44100, 16000, 96000, 32000, 8000};
    hr = S_FALSE;

    // Iterate over frequencies and channels, in order of priority
    for (int freq = 0; freq < sizeof(freqs)/sizeof(freqs[0]); freq++)
    {
        for (int chan = 0; chan < sizeof(_recChannelsPrioList)/sizeof(_recChannelsPrioList[0]); chan++)
        {
            Wfx.nChannels = _recChannelsPrioList[chan];
            Wfx.nSamplesPerSec = freqs[freq];
            Wfx.nBlockAlign = Wfx.nChannels * Wfx.wBitsPerSample / 8;
            Wfx.nAvgBytesPerSec = Wfx.nSamplesPerSec * Wfx.nBlockAlign;
            // If the method succeeds and the audio endpoint device supports the specified stream format,
            // it returns S_OK. If the method succeeds and provides a closest match to the specified format,
            // it returns S_FALSE.
            hr = _ptrClientIn->IsFormatSupported(
                                  AUDCLNT_SHAREMODE_SHARED,
                                  &Wfx,
                                  &pWfxClosestMatch);
            if (hr == S_OK)
            {
                break;
            }
            else
            {
                WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, _id, "nChannels=%d, nSamplesPerSec=%d is not supported",
                    Wfx.nChannels, Wfx.nSamplesPerSec);
            }
        }
        if (hr == S_OK)
            break;
    }

    if (hr == S_OK)
    {
        _recAudioFrameSize = Wfx.nBlockAlign;
        _recSampleRate = Wfx.nSamplesPerSec;
        _recBlockSize = Wfx.nSamplesPerSec/100;
        _recChannels = Wfx.nChannels;
        if (_recBlockSize == 441)
        {
            _recSampleRate = 44000; // we are actually using 44000 Hz and *not* 44100 Hz
            _recBlockSize = 440;    // adjust to size we can handle
        }

        WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, _id, "VoE selected this capturing format:");
        WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, _id, "wFormatTag        : 0x%X (%u)", Wfx.wFormatTag, Wfx.wFormatTag);
        WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, _id, "nChannels         : %d", Wfx.nChannels);
        WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, _id, "nSamplesPerSec    : %d", Wfx.nSamplesPerSec);
        WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, _id, "nAvgBytesPerSec   : %d", Wfx.nAvgBytesPerSec);
        WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, _id, "nBlockAlign       : %d", Wfx.nBlockAlign);
        WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, _id, "wBitsPerSample    : %d", Wfx.wBitsPerSample);
        WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, _id, "cbSize            : %d", Wfx.cbSize);
        WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, _id, "Additional settings:");
        WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, _id, "_recAudioFrameSize: %d", _recAudioFrameSize);
        WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, _id, "_recBlockSize     : %d", _recBlockSize);
        WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, _id, "_recChannels      : %d", _recChannels);
    }

    _Get44kHzDrift();

    // Create a capturing stream.
    hr = _ptrClientIn->Initialize(
                          AUDCLNT_SHAREMODE_SHARED,             // share Audio Engine with other applications
                          AUDCLNT_STREAMFLAGS_EVENTCALLBACK |   // processing of the audio buffer by the client will be event driven
                          AUDCLNT_STREAMFLAGS_NOPERSIST,        // volume and mute settings for an audio session will not persist across system restarts
                          0,                                    // required for event-driven shared mode
                          0,                                    // periodicity
                          &Wfx,                                 // selected wave format
                          NULL);                                // session GUID


    if (hr != S_OK)
    {
        WEBRTC_TRACE(kTraceError, kTraceAudioDevice, _id, "IAudioClient::Initialize() failed:");
        if (pWfxClosestMatch != NULL)
        {
            WEBRTC_TRACE(kTraceError, kTraceAudioDevice, _id, "closest mix format: #channels=%d, samples/sec=%d, bits/sample=%d",
                pWfxClosestMatch->nChannels, pWfxClosestMatch->nSamplesPerSec, pWfxClosestMatch->wBitsPerSample);
        }
        else
        {
            WEBRTC_TRACE(kTraceError, kTraceAudioDevice, _id, "no format suggested");
        }
    }
    EXIT_ON_ERROR(hr);

    if (_ptrAudioBuffer)
    {
        // Update the audio buffer with the selected parameters
        _ptrAudioBuffer->SetRecordingSampleRate(_recSampleRate);
        _ptrAudioBuffer->SetRecordingChannels((WebRtc_UWord8)_recChannels);
    }
    else
    {
        // We can enter this state during CoreAudioIsSupported() when no AudioDeviceImplementation
        // has been created, hence the AudioDeviceBuffer does not exist.
        // It is OK to end up here since we don't initiate any media in CoreAudioIsSupported().
        WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, _id, "AudioDeviceBuffer must be attached before streaming can start");
    }

    // Get the actual size of the shared (endpoint buffer).
    // Typical value is 960 audio frames <=> 20ms @ 48kHz sample rate.
    UINT bufferFrameCount(0);
    hr = _ptrClientIn->GetBufferSize(
                          &bufferFrameCount);
    if (SUCCEEDED(hr))
    {
        WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, _id, "IAudioClient::GetBufferSize() => %u (<=> %u bytes)",
            bufferFrameCount, bufferFrameCount*_recAudioFrameSize);
    }

    // Set the event handle that the system signals when an audio buffer is ready
    // to be processed by the client.
    hr = _ptrClientIn->SetEventHandle(
                          _hCaptureSamplesReadyEvent);
    EXIT_ON_ERROR(hr);

    // Get an IAudioCaptureClient interface.
    SAFE_RELEASE(_ptrCaptureClient);
    hr = _ptrClientIn->GetService(
                          __uuidof(IAudioCaptureClient),
                          (void**)&_ptrCaptureClient);
    EXIT_ON_ERROR(hr);

    // Mark capture side as initialized
    _recIsInitialized = true;

    CoTaskMemFree(pWfxIn);
    CoTaskMemFree(pWfxClosestMatch);

    WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, _id, "capture side is now initialized");
    return 0;

Exit:
    _TraceCOMError(hr);
    CoTaskMemFree(pWfxIn);
    CoTaskMemFree(pWfxClosestMatch);
    SAFE_RELEASE(_ptrClientIn);
    SAFE_RELEASE(_ptrCaptureClient);
    return -1;
}

// ----------------------------------------------------------------------------
//  StartRecording
// ----------------------------------------------------------------------------

WebRtc_Word32 AudioDeviceWindowsCore::StartRecording()
{
    WEBRTC_TRACE(kTraceModuleCall, kTraceAudioDevice, _id, "%s", __FUNCTION__);

    if (!_recIsInitialized)
    {
        return -1;
    }

    if (_hRecThread != NULL)
    {
        return 0;
    }

    if (_recording)
    {
        return 0;
    }

    HRESULT hr = S_OK;

    _Lock();

    // Create thread which will drive the capturing
    _hRecThread = CreateThread(
                    NULL,
                    0,
                    WSAPICaptureThread,
                    this,
                    0,
                    NULL);
    if (_hRecThread == NULL)
    {
        WEBRTC_TRACE(kTraceError, kTraceAudioDevice, _id, 
                     "failed to create the recording thread");
        return -1;
    }

    // Set thread priority to highest possible
    SetThreadPriority(_hRecThread, THREAD_PRIORITY_TIME_CRITICAL);

    _hGetCaptureVolumeThread = CreateThread(NULL,
                                            0,
                                            GetCaptureVolumeThread,
                                            this,
                                            0,
                                            NULL);
    if (_hGetCaptureVolumeThread == NULL)
    {
        WEBRTC_TRACE(kTraceError, kTraceAudioDevice, _id, 
                     "  failed to create the volume getter thread");
        return -1;
    }

    SetThreadPriority(_hGetCaptureVolumeThread, THREAD_PRIORITY_NORMAL);

    _hSetCaptureVolumeThread = CreateThread(NULL,
                                            0,
                                            SetCaptureVolumeThread,
                                            this,
                                            0,
                                            NULL);
    if (_hSetCaptureVolumeThread == NULL)
   {
        WEBRTC_TRACE(kTraceError, kTraceAudioDevice, _id,
                     "  failed to create the volume setter thread");
        return -1;
    }

    SetThreadPriority(_hSetCaptureVolumeThread, THREAD_PRIORITY_NORMAL);

    _UnLock();

    DWORD ret = WaitForSingleObject(_hCaptureStartedEvent, 1000);
    if (ret != WAIT_OBJECT_0)
    {
        WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, _id, "capturing did not start up properly");
        return -1;
    }
    WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, _id, "capture audio stream has now started...");

    _avgCPULoad = 0.0f;
    _playAcc = 0;
    _recording = true;

    return 0;
}

// ----------------------------------------------------------------------------
//  StopRecording
// ----------------------------------------------------------------------------

WebRtc_Word32 AudioDeviceWindowsCore::StopRecording()
{
    WEBRTC_TRACE(kTraceModuleCall, kTraceAudioDevice, _id, "%s", __FUNCTION__);
    WebRtc_Word32 err = 0;

    if (!_recIsInitialized)
    {
        return 0;
    }

    _Lock();

    if (_hRecThread == NULL)
    {
        WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, _id, "no capturing stream is active => close down WASAPI only");
        SAFE_RELEASE(_ptrClientIn);
        SAFE_RELEASE(_ptrCaptureClient);
        _recIsInitialized = false;
        _recording = false;
        _UnLock();
        return 0;
    }

    // Stop the driving thread...
    WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, _id, "closing down the webrtc_core_audio_capture_thread...");
    // Manual-reset event; it will remain signalled to stop all capture threads.
    SetEvent(_hShutdownCaptureEvent);

    _UnLock();
    DWORD ret = WaitForSingleObject(_hRecThread, 2000);
    if (ret != WAIT_OBJECT_0)
    {
        WEBRTC_TRACE(kTraceError, kTraceAudioDevice, _id, "failed to close down webrtc_core_audio_capture_thread");
        err = -1;
    }
    else
    {
        WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, _id, "webrtc_core_audio_capture_thread is now closed");
    }

    ret = WaitForSingleObject(_hGetCaptureVolumeThread, 2000);
    if (ret != WAIT_OBJECT_0)
    {
        // the thread did not stop as it should
        WEBRTC_TRACE(kTraceError, kTraceAudioDevice, _id,
                     "  failed to close down volume getter thread");
        err = -1;
    }
    else
    {
        WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, _id, 
            "  volume getter thread is now closed");
    }

    ret = WaitForSingleObject(_hSetCaptureVolumeThread, 2000);
    if (ret != WAIT_OBJECT_0)
    {
        // the thread did not stop as it should
        WEBRTC_TRACE(kTraceError, kTraceAudioDevice, _id,
                     "  failed to close down volume setter thread");
        err = -1;
    }
    else
    {
        WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, _id, 
            "  volume setter thread is now closed");
    }
    _Lock();

    ResetEvent(_hShutdownCaptureEvent); // Must be manually reset.
    // Ensure that the thread has released these interfaces properly.
    assert(err == -1 || _ptrClientIn == NULL);
    assert(err == -1 || _ptrCaptureClient == NULL);

    _recIsInitialized = false;
    _recording = false;

    // These will create thread leaks in the result of an error,
    // but we can at least resume the call.
    CloseHandle(_hRecThread);
    _hRecThread = NULL;

    CloseHandle(_hGetCaptureVolumeThread);
    _hGetCaptureVolumeThread = NULL;

    CloseHandle(_hSetCaptureVolumeThread);
    _hSetCaptureVolumeThread = NULL;

    _UnLock();

   return err;
}

// ----------------------------------------------------------------------------
//  RecordingIsInitialized
// ----------------------------------------------------------------------------

bool AudioDeviceWindowsCore::RecordingIsInitialized() const
{
    WEBRTC_TRACE(kTraceModuleCall, kTraceAudioDevice, _id, "%s", __FUNCTION__);
    return (_recIsInitialized);
}

// ----------------------------------------------------------------------------
//  Recording
// ----------------------------------------------------------------------------

bool AudioDeviceWindowsCore::Recording() const
{
    WEBRTC_TRACE(kTraceModuleCall, kTraceAudioDevice, _id, "%s", __FUNCTION__);
    return (_recording);
}

// ----------------------------------------------------------------------------
//  PlayoutIsInitialized
// ----------------------------------------------------------------------------

bool AudioDeviceWindowsCore::PlayoutIsInitialized() const
{
    WEBRTC_TRACE(kTraceModuleCall, kTraceAudioDevice, _id, "%s", __FUNCTION__);

    return (_playIsInitialized);
}

// ----------------------------------------------------------------------------
//  StartPlayout
// ----------------------------------------------------------------------------

WebRtc_Word32 AudioDeviceWindowsCore::StartPlayout()
{
    WEBRTC_TRACE(kTraceModuleCall, kTraceAudioDevice, _id, "%s", __FUNCTION__);

    if (!_playIsInitialized)
    {
        return -1;
    }

    if (_hPlayThread != NULL)
    {
        return 0;
    }

    if (_playing)
    {
        return 0;
    }

    HRESULT hr = S_OK;

    _Lock();

    // Create thread which will drive the rendering
    _hPlayThread = CreateThread(
                     NULL,
                     0,
                     WSAPIRenderThread,
                     this,
                     0,
                     NULL);
    if (_hPlayThread == NULL)
    {
        WEBRTC_TRACE(kTraceError, kTraceAudioDevice, _id, "failed to create the playout thread");
        return -1;
    }

    // Set thred priority to highest possible
    SetThreadPriority(_hPlayThread, THREAD_PRIORITY_TIME_CRITICAL);

    _UnLock();

    DWORD ret = WaitForSingleObject(_hRenderStartedEvent, 1000);
    if (ret != WAIT_OBJECT_0)
    {
        WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, _id, "rendering did not start up properly");
        return -1;
    }

    _playing = true;
    WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, _id, "rendering audio stream has now started...");

    return 0;
}

// ----------------------------------------------------------------------------
//  StopPlayout
// ----------------------------------------------------------------------------

WebRtc_Word32 AudioDeviceWindowsCore::StopPlayout()
{
    WEBRTC_TRACE(kTraceModuleCall, kTraceAudioDevice, _id, "%s", __FUNCTION__);

    if (!_playIsInitialized)
    {
        return 0;
    }

    _Lock();

    if (_hPlayThread == NULL)
    {
        WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, _id, "no rending stream is active => close down WASAPI only");
        SAFE_RELEASE(_ptrClientOut);
        SAFE_RELEASE(_ptrRenderClient);
        _playIsInitialized = false;
        _playing = false;
        _UnLock();
        return 0;
    }

    // stop the driving thread...
    WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, _id, "closing down the webrtc_core_audio_render_thread...");
    SetEvent(_hShutdownRenderEvent);

    _UnLock();
    DWORD ret = WaitForSingleObject(_hPlayThread, 2000);
    if (ret != WAIT_OBJECT_0)
    {
        // the thread did not stop as it should
        WEBRTC_TRACE(kTraceError, kTraceAudioDevice, _id, "failed to close down webrtc_core_audio_render_thread");
        CloseHandle(_hPlayThread);
        _hPlayThread = NULL;
        _playIsInitialized = false;
        _playing = false;
        return -1;
    }
    _Lock();
    WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, _id, "webrtc_core_audio_render_thread is now closed");

    // Ensure that the thread has released these interfaces properly
    assert(NULL == _ptrClientOut);
    assert(NULL == _ptrRenderClient);

    _playIsInitialized = false;
    _playing = false;

    CloseHandle(_hPlayThread);
    _hPlayThread = NULL;

    _UnLock();

    return 0;
}

// ----------------------------------------------------------------------------
//  PlayoutDelay
// ----------------------------------------------------------------------------

WebRtc_Word32 AudioDeviceWindowsCore::PlayoutDelay(WebRtc_UWord16& delayMS) const
{
    CriticalSectionScoped lock(_critSect);
    delayMS = (WebRtc_UWord16)_sndCardPlayDelay;
    return 0;
}

// ----------------------------------------------------------------------------
//  RecordingDelay
// ----------------------------------------------------------------------------

WebRtc_Word32 AudioDeviceWindowsCore::RecordingDelay(WebRtc_UWord16& delayMS) const
{
    CriticalSectionScoped lock(_critSect);
    delayMS = (WebRtc_UWord16)_sndCardRecDelay;
    return 0;
}

// ----------------------------------------------------------------------------
//  Playing
// ----------------------------------------------------------------------------

bool AudioDeviceWindowsCore::Playing() const
{
    WEBRTC_TRACE(kTraceModuleCall, kTraceAudioDevice, _id, "%s", __FUNCTION__);
    return (_playing);
}
// ----------------------------------------------------------------------------
//  SetPlayoutBuffer
// ----------------------------------------------------------------------------

WebRtc_Word32 AudioDeviceWindowsCore::SetPlayoutBuffer(const AudioDeviceModule::BufferType type, WebRtc_UWord16 sizeMS)
{
    WEBRTC_TRACE(kTraceModuleCall, kTraceAudioDevice, _id, "AudioDeviceWindowsCore::SetPlayoutBuffer(type=%u, sizeMS=%u)", type, sizeMS);

    CriticalSectionScoped lock(_critSect);

    _playBufType = type;

    if (type == AudioDeviceModule::kFixedBufferSize)
    {
        _playBufDelayFixed = sizeMS;
    }

    return 0;
}

// ----------------------------------------------------------------------------
//  PlayoutBuffer
// ----------------------------------------------------------------------------

WebRtc_Word32 AudioDeviceWindowsCore::PlayoutBuffer(AudioDeviceModule::BufferType& type, WebRtc_UWord16& sizeMS) const
{
    CriticalSectionScoped lock(_critSect);

    type = _playBufType;

    if (type == AudioDeviceModule::kFixedBufferSize)
    {
        sizeMS = _playBufDelayFixed;
    }
    else
    {
        // Use same value as for PlayoutDelay
        sizeMS = (WebRtc_UWord16)_sndCardPlayDelay;
    }

    return 0;
}

// ----------------------------------------------------------------------------
//  CPULoad
// ----------------------------------------------------------------------------

WebRtc_Word32 AudioDeviceWindowsCore::CPULoad(WebRtc_UWord16& load) const
{
    WEBRTC_TRACE(kTraceModuleCall, kTraceAudioDevice, _id, "%s", __FUNCTION__);

    load = static_cast<WebRtc_UWord16> (100*_avgCPULoad);

    return 0;
}

// ----------------------------------------------------------------------------
//  PlayoutWarning
// ----------------------------------------------------------------------------

bool AudioDeviceWindowsCore::PlayoutWarning() const
{
    return ( _playWarning > 0);
}

// ----------------------------------------------------------------------------
//  PlayoutError
// ----------------------------------------------------------------------------

bool AudioDeviceWindowsCore::PlayoutError() const
{
    return ( _playError > 0);
}

// ----------------------------------------------------------------------------
//  RecordingWarning
// ----------------------------------------------------------------------------

bool AudioDeviceWindowsCore::RecordingWarning() const
{
    return ( _recWarning > 0);
}

// ----------------------------------------------------------------------------
//  RecordingError
// ----------------------------------------------------------------------------

bool AudioDeviceWindowsCore::RecordingError() const
{
    return ( _recError > 0);
}

// ----------------------------------------------------------------------------
//  ClearPlayoutWarning
// ----------------------------------------------------------------------------

void AudioDeviceWindowsCore::ClearPlayoutWarning()
{
    _playWarning = 0;
}

// ----------------------------------------------------------------------------
//  ClearPlayoutError
// ----------------------------------------------------------------------------

void AudioDeviceWindowsCore::ClearPlayoutError()
{
    _playError = 0;
}

// ----------------------------------------------------------------------------
//  ClearRecordingWarning
// ----------------------------------------------------------------------------

void AudioDeviceWindowsCore::ClearRecordingWarning()
{
    _recWarning = 0;
}

// ----------------------------------------------------------------------------
//  ClearRecordingError
// ----------------------------------------------------------------------------

void AudioDeviceWindowsCore::ClearRecordingError()
{
    _recError = 0;
}

// ============================================================================
//                                 Private Methods
// ============================================================================

// ----------------------------------------------------------------------------
//  [static] WSAPIRenderThread
// ----------------------------------------------------------------------------

DWORD WINAPI AudioDeviceWindowsCore::WSAPIRenderThread(LPVOID context)
{
    return(((AudioDeviceWindowsCore*)context)->DoRenderThread());
}

// ----------------------------------------------------------------------------
//  [static] WSAPICaptureThread
// ----------------------------------------------------------------------------

DWORD WINAPI AudioDeviceWindowsCore::WSAPICaptureThread(LPVOID context)
{
    return(((AudioDeviceWindowsCore*)context)->DoCaptureThread());
}

DWORD WINAPI AudioDeviceWindowsCore::GetCaptureVolumeThread(LPVOID context)
{
    return(((AudioDeviceWindowsCore*)context)->DoGetCaptureVolumeThread());
}

DWORD WINAPI AudioDeviceWindowsCore::SetCaptureVolumeThread(LPVOID context)
{
    return(((AudioDeviceWindowsCore*)context)->DoSetCaptureVolumeThread());
}

DWORD AudioDeviceWindowsCore::DoGetCaptureVolumeThread()
{
    HANDLE waitObject = _hShutdownCaptureEvent;

    while (1)
    {
        DWORD waitResult = WaitForSingleObject(waitObject, 
                                               GET_MIC_VOLUME_INTERVAL_MS);
        switch (waitResult)
        {
            case WAIT_OBJECT_0: // _hShutdownCaptureEvent
                return 0;
            case WAIT_TIMEOUT:	// timeout notification
                break;
            default:            // unexpected error
                WEBRTC_TRACE(kTraceWarning, kTraceAudioDevice, _id,
                    "  unknown wait termination on get volume thread");
                return -1;
        }

        if (AGC())
        {
            WebRtc_UWord32 currentMicLevel = 0;
            if (MicrophoneVolume(currentMicLevel) == 0)
            {
                // This doesn't set the system volume, just stores it.
                _Lock();
                if (_ptrAudioBuffer)
                {
                    _ptrAudioBuffer->SetCurrentMicLevel(currentMicLevel);				
                }
                _UnLock();
            }
        }
    }
}

DWORD AudioDeviceWindowsCore::DoSetCaptureVolumeThread()
{
    HANDLE waitArray[2] = {_hShutdownCaptureEvent, _hSetCaptureVolumeEvent};

    while (1)
    {
        DWORD waitResult = WaitForMultipleObjects(2, waitArray, FALSE, INFINITE);
        switch (waitResult)
        {
            case WAIT_OBJECT_0:     // _hShutdownCaptureEvent
                return 0;
           case WAIT_OBJECT_0 + 1:  // _hSetCaptureVolumeEvent
                break;
           default:                 // unexpected error
                WEBRTC_TRACE(kTraceWarning, kTraceAudioDevice, _id, 
                    "  unknown wait termination on set volume thread");
                    return -1;
        }

        _Lock();
        WebRtc_UWord32 newMicLevel = _newMicLevel;
        _UnLock();

        if (SetMicrophoneVolume(newMicLevel) == -1)
        {
            WEBRTC_TRACE(kTraceWarning, kTraceAudioDevice, _id, 
                "  the required modification of the microphone volume failed");
        }
    }      
}

// ----------------------------------------------------------------------------
//  DoRenderThread
// ----------------------------------------------------------------------------

DWORD AudioDeviceWindowsCore::DoRenderThread()
{
    WEBRTC_TRACE(kTraceModuleCall, kTraceAudioDevice, _id, "%s", __FUNCTION__);

    bool keepPlaying = true;
    HANDLE waitArray[2] = {_hShutdownRenderEvent, _hRenderSamplesReadyEvent};
    HRESULT hr = S_OK;
    HANDLE hMmTask = NULL;

    LARGE_INTEGER t1;
    LARGE_INTEGER t2;
    WebRtc_Word32 time(0);

    hr = CoInitializeEx(NULL, COM_THREADING_MODEL);
    if (FAILED(hr))
    {
        _TraceCOMError(hr);
        WEBRTC_TRACE(kTraceWarning, kTraceAudioDevice, _id, "unable to initialize COM in render thread");
        return hr;
    }

    _SetThreadName(-1, "webrtc_core_audio_render_thread");

    // Use Multimedia Class Scheduler Service (MMCSS) to boost the thread priority.
    //
    if (_winSupportAvrt)
    {
        DWORD taskIndex(0);
        hMmTask = _PAvSetMmThreadCharacteristicsA("Pro Audio", &taskIndex);
        if (hMmTask)
        {
            if (FALSE == _PAvSetMmThreadPriority(hMmTask, AVRT_PRIORITY_CRITICAL))
            {
                WEBRTC_TRACE(kTraceWarning, kTraceAudioDevice, _id, "failed to boost play-thread using MMCSS");
            }
            WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, _id, "render thread is now registered with MMCSS (taskIndex=%d)", taskIndex);
        }
        else
        {
            WEBRTC_TRACE(kTraceWarning, kTraceAudioDevice, _id, "failed to enable MMCSS on render thread (err=%d)", GetLastError());
            _TraceCOMError(GetLastError());
        }
    }

    _Lock();

    // Get size of rendering buffer (length is expressed as the number of audio frames the buffer can hold).
    // This value is fixed during the rendering session.
    //
    UINT32 bufferLength = 0;
    hr = _ptrClientOut->GetBufferSize(&bufferLength);
    EXIT_ON_ERROR(hr);
    WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, _id, "[REND] size of buffer       : %u", bufferLength);

    // Get maximum latency for the current stream (will not change for the lifetime  of the IAudioClient object).
    //
    REFERENCE_TIME latency;
    _ptrClientOut->GetStreamLatency(&latency);
    WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, _id, "[REND] max stream latency   : %u (%3.2f ms)",
        (DWORD)latency, (double)(latency/10000.0));

    // Get the length of the periodic interval separating successive processing passes by
    // the audio engine on the data in the endpoint buffer.
    //
    // The period between processing passes by the audio engine is fixed for a particular
    // audio endpoint device and represents the smallest processing quantum for the audio engine.
    // This period plus the stream latency between the buffer and endpoint device represents
    // the minimum possible latency that an audio application can achieve.
    // Typical value: 100000 <=> 0.01 sec = 10ms.
    //
    REFERENCE_TIME devPeriod = 0;
    _ptrClientOut->GetDevicePeriod(&devPeriod, NULL);
    WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, _id, "[REND] device period        : %u (%3.2f ms)",
        (DWORD)devPeriod, (double)(devPeriod/10000.0));

    //  The Event Driven renderer will be woken up every defaultDevicePeriod hundred-nano-seconds.
    //  Convert that time into a number of frames.
    //
    double devicePeriodInSeconds = devPeriod / (10000.0*1000.0);
    UINT32 devicePeriodInFrames = static_cast<UINT32>(_playSampleRate * devicePeriodInSeconds + 0.5);

    // Derive inital rendering delay.
    // Example: 10*(960/480) + 15 = 20 + 15 = 35ms
    //
    _sndCardPlayDelay = 10 * (bufferLength / _playBlockSize) + (int)((latency + devPeriod) / 10000);
    _writtenSamples = 0;
    WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, _id, "[REND] initial delay        : %u", _sndCardPlayDelay);

    double endpointBufferSizeMS = 10.0 * ((double)bufferLength / (double)_devicePlayBlockSize);
    WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, _id, "[REND] endpointBufferSizeMS : %3.2f", endpointBufferSizeMS);

    // Before starting the stream, fill the rendering buffer with silence.
    //
    BYTE *pData = NULL;
    hr = _ptrRenderClient->GetBuffer(bufferLength, &pData);
    EXIT_ON_ERROR(hr);

    hr = _ptrRenderClient->ReleaseBuffer(bufferLength, AUDCLNT_BUFFERFLAGS_SILENT);
    EXIT_ON_ERROR(hr);

    _writtenSamples += bufferLength;

    // Start up the rendering audio stream.
    //
    hr = _ptrClientOut->Start();
    EXIT_ON_ERROR(hr);

    _UnLock();

    // Set event which will ensure that the calling thread modifies the playing state to true.
    //
    SetEvent(_hRenderStartedEvent);

    // >> ------------------ THREAD LOOP ------------------

    while (keepPlaying)
    {
        // Wait for a capture notification event or a shutdown event
        DWORD waitResult = WaitForMultipleObjects(2, waitArray, FALSE, 500);
        switch (waitResult)
        {
        case WAIT_OBJECT_0 + 0:     // _hShutdownRenderEvent
            keepPlaying = false;
            break;
        case WAIT_OBJECT_0 + 1:     // _hRenderSamplesReadyEvent
            break;
        case WAIT_TIMEOUT:          // timeout notification
            _ptrClientOut->Stop();
            WEBRTC_TRACE(kTraceWarning, kTraceAudioDevice, _id, "render event timed out after 0.5 seconds");
            goto Exit;
        default:                    // unexpected error
            _ptrClientOut->Stop();
            WEBRTC_TRACE(kTraceWarning, kTraceAudioDevice, _id, "unknown wait termination on render side");
            goto Exit;
        }

        while (keepPlaying)
        {
            _Lock();

            // Get the number of frames of padding (queued up to play) in the endpoint buffer.
            UINT32 padding = 0;
            hr = _ptrClientOut->GetCurrentPadding(&padding);
            EXIT_ON_ERROR(hr);

            // Derive the amount of available space in the output buffer
            WebRtc_UWord32 framesAvailable = bufferLength - padding;
            // WEBRTC_TRACE(kTraceStream, kTraceAudioDevice, _id, "#avaliable audio frames = %u", framesAvailable);

            // Do we have 10 ms available in the render buffer?
            if (framesAvailable < _playBlockSize)
            {
                // Not enough space in render buffer to store next render packet.
                _UnLock();
                break;
            }

            // Write n*10ms buffers to the render buffer
            const WebRtc_UWord32 n10msBuffers = (framesAvailable / _playBlockSize);
            for (WebRtc_UWord32 n = 0; n < n10msBuffers; n++)
            {
                // Get pointer (i.e., grab the buffer) to next space in the shared render buffer.
                hr = _ptrRenderClient->GetBuffer(_playBlockSize, &pData);
                EXIT_ON_ERROR(hr);

                QueryPerformanceCounter(&t1);    // measure time: START

                if (_ptrAudioBuffer)
                {
                    // Request data to be played out (#bytes = _playBlockSize*_audioFrameSize)
                    _UnLock();
                    WebRtc_UWord32 nSamples = _ptrAudioBuffer->RequestPlayoutData(_playBlockSize);
                    _Lock();

                    // Sanity check to ensure that essential states are not modified during the unlocked period
                    if (_ptrRenderClient == NULL || _ptrClientOut == NULL)
                    {
                        _UnLock();
                        WEBRTC_TRACE(kTraceCritical, kTraceAudioDevice, _id, "output state has been modified during unlocked period");
                        goto Exit;
                    }
                    if (nSamples != _playBlockSize)
                    {
                        WEBRTC_TRACE(kTraceWarning, kTraceAudioDevice, _id, "nSamples(%d) != _playBlockSize(%d)", nSamples, _playBlockSize);
                    }

                    // Get the actual (stored) data
                    nSamples = _ptrAudioBuffer->GetPlayoutData((WebRtc_Word8*)pData);
                }

                QueryPerformanceCounter(&t2);    // measure time: STOP
                time = (int)(t2.QuadPart-t1.QuadPart);
                _playAcc += time;

                DWORD dwFlags(0);
                hr = _ptrRenderClient->ReleaseBuffer(_playBlockSize, dwFlags);
                // See http://msdn.microsoft.com/en-us/library/dd316605(VS.85).aspx
                // for more details regarding AUDCLNT_E_DEVICE_INVALIDATED.
                EXIT_ON_ERROR(hr);

                _writtenSamples += _playBlockSize;
            }

            _UnLock();
        }
    }

    // ------------------ THREAD LOOP ------------------ <<

    Sleep(static_cast<DWORD>(endpointBufferSizeMS+0.5));
    hr = _ptrClientOut->Stop();

Exit:
    if (FAILED(hr))
    {
        _UnLock();
        _ptrClientOut->Stop();
        _TraceCOMError(hr);
    }

    if (_winSupportAvrt)
    {
        if (NULL != hMmTask)
        {
            _PAvRevertMmThreadCharacteristics(hMmTask);
        }
    }

    if (keepPlaying)
    {
        hr = _ptrClientOut->Stop();
        if (FAILED(hr))
        {
            _TraceCOMError(hr);
        }
        hr = _ptrClientOut->Reset();
        if (FAILED(hr))
        {
            _TraceCOMError(hr);
        }

        // Trigger callback from module process thread
        _playError = 1;
        WEBRTC_TRACE(kTraceError, kTraceUtility, _id, "kPlayoutError message posted: rendering thread has ended pre-maturely");
    }
    else
    {
        WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, _id, "_Rendering thread is now terminated properly");
    }

    SAFE_RELEASE(_ptrClientOut);
    SAFE_RELEASE(_ptrRenderClient);

    CoUninitialize();
    return (DWORD)hr;
}

// ----------------------------------------------------------------------------
//  DoCaptureThread
// ----------------------------------------------------------------------------

DWORD AudioDeviceWindowsCore::DoCaptureThread()
{
    WEBRTC_TRACE(kTraceModuleCall, kTraceAudioDevice, _id, "%s", __FUNCTION__);

    bool keepRecording = true;
    HANDLE waitArray[2] = {_hShutdownCaptureEvent, _hCaptureSamplesReadyEvent};
    HRESULT hr = S_OK;
    HANDLE hMmTask = NULL;


    LARGE_INTEGER t1;
    LARGE_INTEGER t2;
    WebRtc_Word32 time(0);

    BYTE* syncBuffer = NULL;
    UINT32 syncBufIndex = 0;

    WebRtc_UWord32 newMicLevel(0);
    WebRtc_UWord32 currentMicLevel(0);

    _readSamples = 0;

    hr = CoInitializeEx(NULL, COM_THREADING_MODEL);
    if (FAILED(hr))
    {
        _TraceCOMError(hr);
        WEBRTC_TRACE(kTraceWarning, kTraceAudioDevice, _id, "unable to initialize COM in capture thread");
        return hr;
    }

    _SetThreadName(-1, "webrtc_core_audio_capture_thread");

    // Use Multimedia Class Scheduler Service (MMCSS) to boost the thread priority.
    //
    if (_winSupportAvrt)
    {
        DWORD taskIndex(0);
        hMmTask = _PAvSetMmThreadCharacteristicsA("Pro Audio", &taskIndex);
        if (hMmTask)
        {
            if (FALSE == _PAvSetMmThreadPriority(hMmTask, AVRT_PRIORITY_CRITICAL))
            {
                WEBRTC_TRACE(kTraceWarning, kTraceAudioDevice, _id, "failed to boost rec-thread using MMCSS");
            }
            WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, _id, "capture thread is now registered with MMCSS (taskIndex=%d)", taskIndex);
        }
        else
        {
            WEBRTC_TRACE(kTraceWarning, kTraceAudioDevice, _id, "failed to enable MMCSS on capture thread (err=%d)", GetLastError());
            _TraceCOMError(GetLastError());
        }
    }

    _Lock();

    // Get size of capturing buffer (length is expressed as the number of audio frames the buffer can hold).
    // This value is fixed during the capturing session.
    //
    UINT32 bufferLength = 0;
    hr = _ptrClientIn->GetBufferSize(&bufferLength);
    EXIT_ON_ERROR(hr);
    WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, _id, "[CAPT] size of buffer       : %u", bufferLength);

    // Allocate memory for sync buffer.
    // It is used for compensation between native 44.1 and internal 44.0 and
    // for cases when the capture buffer is larger than 10ms.
    //
    const UINT32 syncBufferSize = 2*(bufferLength * _recAudioFrameSize);
    syncBuffer = new BYTE[syncBufferSize];
    if (syncBuffer == NULL)
    {
        return E_POINTER;
    }
    WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, _id, "[CAPT] size of sync buffer  : %u [bytes]", syncBufferSize);

    // Get maximum latency for the current stream (will not change for the lifetime of the IAudioClient object).
    //
    REFERENCE_TIME latency;
    _ptrClientIn->GetStreamLatency(&latency);
    WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, _id, "[CAPT] max stream latency   : %u (%3.2f ms)",
        (DWORD)latency, (double)(latency / 10000.0));

    // Get the length of the periodic interval separating successive processing passes by
    // the audio engine on the data in the endpoint buffer.
    //
    REFERENCE_TIME devPeriod = 0;
    _ptrClientIn->GetDevicePeriod(&devPeriod, NULL);
    WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, _id, "[CAPT] device period        : %u (%3.2f ms)",
        (DWORD)devPeriod, (double)(devPeriod / 10000.0));

    double extraDelayMS = (double)((latency + devPeriod) / 10000.0);
    WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, _id, "[CAPT] extraDelayMS         : %3.2f", extraDelayMS);

    double endpointBufferSizeMS = 10.0 * ((double)bufferLength / (double)_recBlockSize);
    WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, _id, "[CAPT] endpointBufferSizeMS : %3.2f", endpointBufferSizeMS);

    // Start up the capturing stream.
    //
    hr = _ptrClientIn->Start();
    EXIT_ON_ERROR(hr);

    _UnLock();

    // Set event which will ensure that the calling thread modifies the recording state to true.
    //
    SetEvent(_hCaptureStartedEvent);

    // >> ---------------------------- THREAD LOOP ----------------------------

    while (keepRecording)
    {
        // Wait for a capture notification event or a shutdown event
        DWORD waitResult = WaitForMultipleObjects(2, waitArray, FALSE, 500);
        switch (waitResult)
        {
        case WAIT_OBJECT_0 + 0:        // _hShutdownCaptureEvent
            keepRecording = false;
            break;
        case WAIT_OBJECT_0 + 1:        // _hCaptureSamplesReadyEvent
            break;
        case WAIT_TIMEOUT:            // timeout notification
            _ptrClientIn->Stop();
            WEBRTC_TRACE(kTraceWarning, kTraceAudioDevice, _id, "capture event timed out after 0.5 seconds");
            goto Exit;
        default:                    // unexpected error
            _ptrClientIn->Stop();
            WEBRTC_TRACE(kTraceWarning, kTraceAudioDevice, _id, "unknown wait termination on capture side");
            goto Exit;
        }

        while (keepRecording)
        {
            BYTE *pData = 0;
            UINT32 framesAvailable = 0;
            DWORD flags = 0;
            UINT64 recTime = 0;
            UINT64 recPos = 0;

            _Lock();

            //  Find out how much capture data is available
            //
            hr = _ptrCaptureClient->GetBuffer(&pData,           // packet which is ready to be read by used
                                              &framesAvailable, // #frames in the captured packet (can be zero)
                                              &flags,           // support flags (check)
                                              &recPos,          // device position of first audio frame in data packet
                                              &recTime);        // value of performance counter at the time of recording the first audio frame

            if (SUCCEEDED(hr))
            {
                if (AUDCLNT_S_BUFFER_EMPTY == hr)
                {
                    // Buffer was empty => start waiting for a new capture notification event
                    _UnLock();
                    break;
                }

                if (flags & AUDCLNT_BUFFERFLAGS_SILENT)
                {
                    // Treat all of the data in the packet as silence and ignore the actual data values.
                    WEBRTC_TRACE(kTraceWarning, kTraceAudioDevice, _id, "AUDCLNT_BUFFERFLAGS_SILENT");
                    pData = NULL;
                }

                assert(framesAvailable != 0);

                if (pData)
                {
                    CopyMemory(&syncBuffer[syncBufIndex*_recAudioFrameSize], pData, framesAvailable*_recAudioFrameSize);
                }
                else
                {
                    ZeroMemory(&syncBuffer[syncBufIndex*_recAudioFrameSize], framesAvailable*_recAudioFrameSize);
                }
                assert(syncBufferSize >= (syncBufIndex*_recAudioFrameSize)+framesAvailable*_recAudioFrameSize);

                // Release the capture buffer
                //
                hr = _ptrCaptureClient->ReleaseBuffer(framesAvailable);
                EXIT_ON_ERROR(hr);

                _readSamples += framesAvailable;
                syncBufIndex += framesAvailable;

                QueryPerformanceCounter(&t1);

                // Check the current delay on the recording side
                //
                _sndCardRecDelay = (WebRtc_UWord32)((((UINT64)t1.QuadPart * _perfCounterFactor) - recTime) / 10000) + (10*syncBufIndex) / _recBlockSize - 10;

                // Check the current delay on the playout side
                //
                if (_ptrClientOut)
                {
                    IAudioClock* clock = NULL;
                    UINT64 pos = 0;
                    UINT64 freq = 1;
                    hr = _ptrClientOut->GetService(__uuidof(IAudioClock), (void**)&clock);
                    EXIT_ON_ERROR(hr);
                    clock->GetPosition(&pos, NULL);
                    clock->GetFrequency(&freq);
                    _sndCardPlayDelay = ROUND((double(_writtenSamples) / _devicePlaySampleRate - double(pos) / freq) * 1000.0);
                    clock->Release();
                }

                // Send the captured data to the registered consumer
                //
                WebRtc_UWord32 sndCardRecDelay = _sndCardRecDelay;  // avoid modifying the "correct" delay
                while (syncBufIndex >= _recBlockSize)
                {
                    if (_ptrAudioBuffer)
                    {
                        _ptrAudioBuffer->SetRecordedBuffer((const WebRtc_Word8*)syncBuffer, _recBlockSize);

                        _driftAccumulator += _sampleDriftAt48kHz;
                        const WebRtc_Word32 clockDrift = static_cast<WebRtc_Word32>(_driftAccumulator);
                        _driftAccumulator -= clockDrift;

                        _ptrAudioBuffer->SetVQEData(_sndCardPlayDelay, sndCardRecDelay, clockDrift);

                        QueryPerformanceCounter(&t1);    // measure time: START

                        _UnLock();  // release lock while making the callback
                        _ptrAudioBuffer->DeliverRecordedData();
                        _Lock();    // restore the lock

                        QueryPerformanceCounter(&t2);    // measure time: STOP

                        // Measure "average CPU load".
                        // Basically what we do here is to measure how many percent of our 10ms period
                        // is used for encoding and decoding. This value shuld be used as a warning indicator
                        // only and not seen as an absolute value. Running at ~100% will lead to bad QoS.
                        time = (int)(t2.QuadPart - t1.QuadPart);
                        _avgCPULoad = (float)(_avgCPULoad*.99 + (time + _playAcc) / (double)(_perfCounterFreq.QuadPart));
                        _playAcc = 0;

                        // Sanity check to ensure that essential states are not modified during the unlocked period
                        if (_ptrCaptureClient == NULL || _ptrClientIn == NULL)
                        {
                            _UnLock();
                            WEBRTC_TRACE(kTraceCritical, kTraceAudioDevice, _id, "input state has been modified during unlocked period");
                            goto Exit;
                        }
                    }

                    // store remaining data which was not able to deliver as 10ms segment
                    MoveMemory(&syncBuffer[0], &syncBuffer[_recBlockSize*_recAudioFrameSize], (syncBufIndex-_recBlockSize)*_recAudioFrameSize);
                    syncBufIndex -= _recBlockSize;
                    sndCardRecDelay -= 10;
                }

                if (_AGC)
                {
                    WebRtc_UWord32 newMicLevel = _ptrAudioBuffer->NewMicLevel();
                    if (newMicLevel != 0)
                    {
                        // The VQE will only deliver non-zero microphone levels when a change is needed.
                        // Set this new mic level (received from the observer as return value in the callback).
                        WEBRTC_TRACE(kTraceStream, kTraceAudioDevice, _id, "AGC change of volume: new=%u",  newMicLevel);
                        // We store this outside of the audio buffer to avoid 
                        // having it overwritten by the getter thread.
                        _newMicLevel = newMicLevel;
                        SetEvent(_hSetCaptureVolumeEvent);
                    }
                }
            }
            else
            {
                // If GetBuffer returns AUDCLNT_E_BUFFER_ERROR, the thread consuming the audio samples
                // must wait for the next processing pass. The client might benefit from keeping a count
                // of the failed GetBuffer calls. If GetBuffer returns this error repeatedly, the client
                // can start a new processing loop after shutting down the current client by calling
                // IAudioClient::Stop, IAudioClient::Reset, and releasing the audio client.
                WEBRTC_TRACE(kTraceError, kTraceAudioDevice, _id,
                    "IAudioCaptureClient::GetBuffer returned AUDCLNT_E_BUFFER_ERROR, hr = 0x%08X",  hr);
                goto Exit;
            }

            _UnLock();
        }
    }

    // ---------------------------- THREAD LOOP ---------------------------- <<

    hr = _ptrClientIn->Stop();

Exit:
    if (FAILED(hr))
    {
        _UnLock();
        _ptrClientIn->Stop();
        _TraceCOMError(hr);
    }

    if (_winSupportAvrt)
    {
        if (NULL != hMmTask)
        {
            _PAvRevertMmThreadCharacteristics(hMmTask);
        }
    }

    if (keepRecording)
    {
        hr = _ptrClientIn->Stop();
        if (FAILED(hr))
        {
            _TraceCOMError(hr);
        }
        hr = _ptrClientIn->Reset();
        if (FAILED(hr))
        {
            _TraceCOMError(hr);
        }

        // Trigger callback from module process thread
        _recError = 1;
        WEBRTC_TRACE(kTraceError, kTraceUtility, _id, "kRecordingError message posted: capturing thread has ended pre-maturely");
    }
    else
    {
        WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, _id, "_Capturing thread is now terminated properly");
    }

    SAFE_RELEASE(_ptrClientIn);
    SAFE_RELEASE(_ptrCaptureClient);

    if (syncBuffer)
    {
        delete [] syncBuffer;
    }

    CoUninitialize();
    return (DWORD)hr;
}

// ----------------------------------------------------------------------------
//  _RefreshDeviceList
//
//  Creates a new list of endpoint rendering or capture devices after
//  deleting any previously created (and possibly out-of-date) list of
//  such devices.
// ----------------------------------------------------------------------------

WebRtc_Word32 AudioDeviceWindowsCore::_RefreshDeviceList(EDataFlow dir)
{
    WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, _id, "%s", __FUNCTION__);

    HRESULT hr = S_OK;
    IMMDeviceCollection *pCollection = NULL;

    assert(dir == eRender || dir == eCapture);
    assert(_ptrEnumerator != NULL);

    // Create a fresh list of devices using the specified direction
    hr = _ptrEnumerator->EnumAudioEndpoints(
                           dir,
                           DEVICE_STATE_ACTIVE,
                           &pCollection);
    if (FAILED(hr))
    {
        _TraceCOMError(hr);
        SAFE_RELEASE(pCollection);
        return -1;
    }

    if (dir == eRender)
    {
        SAFE_RELEASE(_ptrRenderCollection);
        _ptrRenderCollection = pCollection;
    }
    else
    {
        SAFE_RELEASE(_ptrCaptureCollection);
        _ptrCaptureCollection = pCollection;
    }

    return 0;
}

// ----------------------------------------------------------------------------
//  _DeviceListCount
//
//  Gets a count of the endpoint rendering or capture devices in the
//  current list of such devices.
// ----------------------------------------------------------------------------

WebRtc_Word16 AudioDeviceWindowsCore::_DeviceListCount(EDataFlow dir)
{
    WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, _id, "%s", __FUNCTION__);

    HRESULT hr = S_OK;
    UINT count = 0;

    assert(eRender == dir || eCapture == dir);

    if (eRender == dir && NULL != _ptrRenderCollection)
    {
        hr = _ptrRenderCollection->GetCount(&count);
    }
    else if (NULL != _ptrCaptureCollection)
    {
        hr = _ptrCaptureCollection->GetCount(&count);
    }

    if (FAILED(hr))
    {
        _TraceCOMError(hr);
        return -1;
    }

    return static_cast<WebRtc_Word16> (count);
}

// ----------------------------------------------------------------------------
//  _GetListDeviceName
//
//  Gets the friendly name of an endpoint rendering or capture device
//  from the current list of such devices. The caller uses an index
//  into the list to identify the device.
//
//  Uses: _ptrRenderCollection or _ptrCaptureCollection which is updated
//  in _RefreshDeviceList().
// ----------------------------------------------------------------------------

WebRtc_Word32 AudioDeviceWindowsCore::_GetListDeviceName(EDataFlow dir, int index, LPWSTR szBuffer, int bufferLen)
{
    WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, _id, "%s", __FUNCTION__);

    HRESULT hr = S_OK;
    IMMDevice *pDevice = NULL;

    assert(dir == eRender || dir == eCapture);

    if (eRender == dir && NULL != _ptrRenderCollection)
    {
        hr = _ptrRenderCollection->Item(index, &pDevice);
    }
    else if (NULL != _ptrCaptureCollection)
    {
        hr = _ptrCaptureCollection->Item(index, &pDevice);
    }

    if (FAILED(hr))
    {
        _TraceCOMError(hr);
        SAFE_RELEASE(pDevice);
        return -1;
    }

    WebRtc_Word32 res = _GetDeviceName(pDevice, szBuffer, bufferLen);
    SAFE_RELEASE(pDevice);
    return res;
}

// ----------------------------------------------------------------------------
//  _GetDefaultDeviceName
//
//  Gets the friendly name of an endpoint rendering or capture device
//  given a specified device role.
//
//  Uses: _ptrEnumerator
// ----------------------------------------------------------------------------

WebRtc_Word32 AudioDeviceWindowsCore::_GetDefaultDeviceName(EDataFlow dir, ERole role, LPWSTR szBuffer, int bufferLen)
{
    WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, _id, "%s", __FUNCTION__);

    HRESULT hr = S_OK;
    IMMDevice *pDevice = NULL;

    assert(dir == eRender || dir == eCapture);
    assert(role == eConsole || role == eCommunications);
    assert(_ptrEnumerator != NULL);

    hr = _ptrEnumerator->GetDefaultAudioEndpoint(
                           dir,
                           role,
                           &pDevice);

    if (FAILED(hr))
    {
        _TraceCOMError(hr);
        SAFE_RELEASE(pDevice);
        return -1;
    }

    WebRtc_Word32 res = _GetDeviceName(pDevice, szBuffer, bufferLen);
    SAFE_RELEASE(pDevice);
    return res;
}

// ----------------------------------------------------------------------------
//  _GetListDeviceID
//
//  Gets the unique ID string of an endpoint rendering or capture device
//  from the current list of such devices. The caller uses an index
//  into the list to identify the device.
//
//  Uses: _ptrRenderCollection or _ptrCaptureCollection which is updated
//  in _RefreshDeviceList().
// ----------------------------------------------------------------------------

WebRtc_Word32 AudioDeviceWindowsCore::_GetListDeviceID(EDataFlow dir, int index, LPWSTR szBuffer, int bufferLen)
{
    WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, _id, "%s", __FUNCTION__);

    HRESULT hr = S_OK;
    IMMDevice *pDevice = NULL;

    assert(dir == eRender || dir == eCapture);

    if (eRender == dir && NULL != _ptrRenderCollection)
    {
        hr = _ptrRenderCollection->Item(index, &pDevice);
    }
    else if (NULL != _ptrCaptureCollection)
    {
        hr = _ptrCaptureCollection->Item(index, &pDevice);
    }

    if (FAILED(hr))
    {
        _TraceCOMError(hr);
        SAFE_RELEASE(pDevice);
        return -1;
    }

    WebRtc_Word32 res = _GetDeviceID(pDevice, szBuffer, bufferLen);
    SAFE_RELEASE(pDevice);
    return res;
}

// ----------------------------------------------------------------------------
//  _GetDefaultDeviceID
//
//  Gets the uniqe device ID of an endpoint rendering or capture device
//  given a specified device role.
//
//  Uses: _ptrEnumerator
// ----------------------------------------------------------------------------

WebRtc_Word32 AudioDeviceWindowsCore::_GetDefaultDeviceID(EDataFlow dir, ERole role, LPWSTR szBuffer, int bufferLen)
{
    WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, _id, "%s", __FUNCTION__);

    HRESULT hr = S_OK;
    IMMDevice *pDevice = NULL;

    assert(dir == eRender || dir == eCapture);
    assert(role == eConsole || role == eCommunications);
    assert(_ptrEnumerator != NULL);

    hr = _ptrEnumerator->GetDefaultAudioEndpoint(
                           dir,
                           role,
                           &pDevice);

    if (FAILED(hr))
    {
        _TraceCOMError(hr);
        SAFE_RELEASE(pDevice);
        return -1;
    }

    WebRtc_Word32 res = _GetDeviceID(pDevice, szBuffer, bufferLen);
    SAFE_RELEASE(pDevice);
    return res;
}

// ----------------------------------------------------------------------------
//  _GetDeviceName
// ----------------------------------------------------------------------------

WebRtc_Word32 AudioDeviceWindowsCore::_GetDeviceName(IMMDevice* pDevice,
                                                     LPWSTR pszBuffer,
                                                     int bufferLen)
{
    WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, _id, "%s", __FUNCTION__);

    static const WCHAR szDefault[] = L"<Device not available>";

    HRESULT hr = E_FAIL;
    IPropertyStore *pProps = NULL;
    PROPVARIANT varName;

    assert(pszBuffer != NULL);
    assert(bufferLen > 0);

    if (pDevice != NULL)
    {
        hr = pDevice->OpenPropertyStore(STGM_READ, &pProps);
        if (FAILED(hr))
        {
            WEBRTC_TRACE(kTraceError, kTraceAudioDevice, _id,
                "IMMDevice::OpenPropertyStore failed, hr = 0x%08X", hr);
        }
    }

    // Initialize container for property value.
    PropVariantInit(&varName);

    if (SUCCEEDED(hr))
    {
        // Get the endpoint device's friendly-name property.
        hr = pProps->GetValue(PKEY_Device_FriendlyName, &varName);
        if (FAILED(hr))
        {
            WEBRTC_TRACE(kTraceError, kTraceAudioDevice, _id,
                "IPropertyStore::GetValue failed, hr = 0x%08X", hr);
        }
    }

    if ((SUCCEEDED(hr)) && (VT_EMPTY == varName.vt))
    {
        hr = E_FAIL;
        WEBRTC_TRACE(kTraceError, kTraceAudioDevice, _id,
            "IPropertyStore::GetValue returned no value, hr = 0x%08X", hr);
    }

    if ((SUCCEEDED(hr)) && (VT_LPWSTR != varName.vt))
    {
        // The returned value is not a wide null terminated string.
        hr = E_UNEXPECTED;
        WEBRTC_TRACE(kTraceError, kTraceAudioDevice, _id,
            "IPropertyStore::GetValue returned unexpected type, hr = 0x%08X", hr);
    }

    if (SUCCEEDED(hr) && (varName.pwszVal != NULL))
    {
        // Copy the valid device name to the provided ouput buffer.
        wcsncpy_s(pszBuffer, bufferLen, varName.pwszVal, _TRUNCATE);
    }
    else
    {
        // Failed to find the device name.
        wcsncpy_s(pszBuffer, bufferLen, szDefault, _TRUNCATE);
    }

    PropVariantClear(&varName);
    SAFE_RELEASE(pProps);

    return 0;
}

// ----------------------------------------------------------------------------
//  _GetDeviceID
// ----------------------------------------------------------------------------

WebRtc_Word32 AudioDeviceWindowsCore::_GetDeviceID(IMMDevice* pDevice, LPWSTR pszBuffer, int bufferLen)
{
    WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, _id, "%s", __FUNCTION__);

    static const WCHAR szDefault[] = L"<Device not available>";

    HRESULT hr = E_FAIL;
    LPWSTR pwszID = NULL;

    assert(pszBuffer != NULL);
    assert(bufferLen > 0);

    if (pDevice != NULL)
    {
        hr = pDevice->GetId(&pwszID);
    }

    if (hr == S_OK)
    {
        // Found the device ID.
        wcsncpy_s(pszBuffer, bufferLen, pwszID, _TRUNCATE);
    }
    else
    {
        // Failed to find the device ID.
        wcsncpy_s(pszBuffer, bufferLen, szDefault, _TRUNCATE);
    }

    CoTaskMemFree(pwszID);
    return 0;
}

// ----------------------------------------------------------------------------
//  _GetDefaultDevice
// ----------------------------------------------------------------------------

WebRtc_Word32 AudioDeviceWindowsCore::_GetDefaultDevice(EDataFlow dir, ERole role, IMMDevice** ppDevice)
{
    WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, _id, "%s", __FUNCTION__);

    HRESULT hr(S_OK);

    assert(_ptrEnumerator != NULL);

    hr = _ptrEnumerator->GetDefaultAudioEndpoint(
                                   dir,
                                   role,
                                   ppDevice);
    if (FAILED(hr))
    {
        _TraceCOMError(hr);
        return -1;
    }

    return 0;
}

// ----------------------------------------------------------------------------
//  _GetListDevice
// ----------------------------------------------------------------------------

WebRtc_Word32 AudioDeviceWindowsCore::_GetListDevice(EDataFlow dir, int index, IMMDevice** ppDevice)
{
    HRESULT hr(S_OK);

    assert(_ptrEnumerator != NULL);

    IMMDeviceCollection *pCollection = NULL;

    hr = _ptrEnumerator->EnumAudioEndpoints(
                               dir,
                               DEVICE_STATE_ACTIVE,        // only active endpoints are OK
                               &pCollection);
    if (FAILED(hr))
    {
        _TraceCOMError(hr);
        SAFE_RELEASE(pCollection);
        return -1;
    }

    hr = pCollection->Item(
                        index,
                        ppDevice);
    if (FAILED(hr))
    {
        _TraceCOMError(hr);
        SAFE_RELEASE(pCollection);
        return -1;
    }

    return 0;
}

// ----------------------------------------------------------------------------
//  _EnumerateEndpointDevicesAll
// ----------------------------------------------------------------------------

WebRtc_Word32 AudioDeviceWindowsCore::_EnumerateEndpointDevicesAll(EDataFlow dataFlow) const
{
    WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, _id, "%s", __FUNCTION__);

    assert(_ptrEnumerator != NULL);

    HRESULT hr = S_OK;
    IMMDeviceCollection *pCollection = NULL;

    // Generate a collection of audio endpoint devices in the system.
    // Get states for *all* endpoint devices.
    // Output: IMMDeviceCollection interface.
    hr = _ptrEnumerator->EnumAudioEndpoints(
                                 dataFlow,            // data-flow direction (input parameter)
                                 DEVICE_STATE_ACTIVE | DEVICE_STATE_DISABLED | DEVICE_STATE_NOTPRESENT | DEVICE_STATE_UNPLUGGED,
                                 &pCollection);        // release interface when done

    EXIT_ON_ERROR(hr);

    // use the IMMDeviceCollection interface...

    UINT count;
    IMMDevice *pEndpoint = NULL;
    IPropertyStore *pProps = NULL;
    IAudioEndpointVolume* pEndpointVolume = NULL;
    LPWSTR pwszID = NULL;

    // Retrieve a count of the devices in the device collection.
    hr = pCollection->GetCount(&count);
    EXIT_ON_ERROR(hr);
    if (dataFlow == eRender)
        WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, _id, "#rendering endpoint devices (counting all): %u", count);
    else if (dataFlow == eCapture)
        WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, _id, "#capturing endpoint devices (counting all): %u", count);

    if (count == 0)
    {
        return 0;
    }

    // Each loop prints the name of an endpoint device.
    for (ULONG i = 0; i < count; i++)
    {
        WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, _id, "Endpoint %d:", i);

        // Get pointer to endpoint number i.
        // Output: IMMDevice interface.
        hr = pCollection->Item(
                            i,
                            &pEndpoint);
        EXIT_ON_ERROR(hr);

        // use the IMMDevice interface of the specified endpoint device...

        // Get the endpoint ID string (uniquely identifies the device among all audio endpoint devices)
        hr = pEndpoint->GetId(&pwszID);
        EXIT_ON_ERROR(hr);
        WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, _id, "ID string    : %S", pwszID);

        // Retrieve an interface to the device's property store.
        // Output: IPropertyStore interface.
        hr = pEndpoint->OpenPropertyStore(
                          STGM_READ,
                          &pProps);
        EXIT_ON_ERROR(hr);

        // use the IPropertyStore interface...

        PROPVARIANT varName;
        // Initialize container for property value.
        PropVariantInit(&varName);

        // Get the endpoint's friendly-name property.
        // Example: "Speakers (Realtek High Definition Audio)"
        hr = pProps->GetValue(
                       PKEY_Device_FriendlyName,
                       &varName);
        EXIT_ON_ERROR(hr);
        WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, _id, "friendly name: \"%S\"", varName.pwszVal);

        // Get the endpoint's current device state
        DWORD dwState;
        hr = pEndpoint->GetState(&dwState);
        EXIT_ON_ERROR(hr);
        if (dwState & DEVICE_STATE_ACTIVE)
            WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, _id, "state (0x%x)  : *ACTIVE*", dwState);
        if (dwState & DEVICE_STATE_DISABLED)
            WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, _id, "state (0x%x)  : DISABLED", dwState);
        if (dwState & DEVICE_STATE_NOTPRESENT)
            WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, _id, "state (0x%x)  : NOTPRESENT", dwState);
        if (dwState & DEVICE_STATE_UNPLUGGED)
            WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, _id, "state (0x%x)  : UNPLUGGED", dwState);

        // Check the hardware volume capabilities.
        DWORD dwHwSupportMask = 0;
        hr = pEndpoint->Activate(__uuidof(IAudioEndpointVolume), CLSCTX_ALL,
                               NULL, (void**)&pEndpointVolume);
        EXIT_ON_ERROR(hr);
        hr = pEndpointVolume->QueryHardwareSupport(&dwHwSupportMask);
        EXIT_ON_ERROR(hr);
        if (dwHwSupportMask & ENDPOINT_HARDWARE_SUPPORT_VOLUME)
            // The audio endpoint device supports a hardware volume control
            WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, _id, "hwmask (0x%x) : HARDWARE_SUPPORT_VOLUME", dwHwSupportMask);
        if (dwHwSupportMask & ENDPOINT_HARDWARE_SUPPORT_MUTE)
            // The audio endpoint device supports a hardware mute control
            WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, _id, "hwmask (0x%x) : HARDWARE_SUPPORT_MUTE", dwHwSupportMask);
        if (dwHwSupportMask & ENDPOINT_HARDWARE_SUPPORT_METER)
            // The audio endpoint device supports a hardware peak meter
            WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, _id, "hwmask (0x%x) : HARDWARE_SUPPORT_METER", dwHwSupportMask);

        // Check the channel count (#channels in the audio stream that enters or leaves the audio endpoint device)
        UINT nChannelCount(0);
        hr = pEndpointVolume->GetChannelCount(
                                &nChannelCount);
        EXIT_ON_ERROR(hr);
        WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, _id, "#channels    : %u", nChannelCount);

        if (dwHwSupportMask & ENDPOINT_HARDWARE_SUPPORT_VOLUME)
        {
            // Get the volume range.
            float fLevelMinDB(0.0);
            float fLevelMaxDB(0.0);
            float fVolumeIncrementDB(0.0);
            hr = pEndpointVolume->GetVolumeRange(
                                    &fLevelMinDB,
                                    &fLevelMaxDB,
                                    &fVolumeIncrementDB);
            EXIT_ON_ERROR(hr);
            WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, _id, "volume range : %4.2f (min), %4.2f (max), %4.2f (inc) [dB]",
                fLevelMinDB, fLevelMaxDB, fVolumeIncrementDB);

            // The volume range from vmin = fLevelMinDB to vmax = fLevelMaxDB is divided
            // into n uniform intervals of size vinc = fVolumeIncrementDB, where
            // n = (vmax ?vmin) / vinc.
            // The values vmin, vmax, and vinc are measured in decibels. The client can set
            // the volume level to one of n + 1 discrete values in the range from vmin to vmax.
            int n = (int)((fLevelMaxDB-fLevelMinDB)/fVolumeIncrementDB);
            WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, _id, "#intervals   : %d", n);

            // Get information about the current step in the volume range.
            // This method represents the volume level of the audio stream that enters or leaves
            // the audio endpoint device as an index or "step" in a range of discrete volume levels.
            // Output value nStepCount is the number of steps in the range. Output value nStep
            // is the step index of the current volume level. If the number of steps is n = nStepCount,
            // then step index nStep can assume values from 0 (minimum volume) to n ?1 (maximum volume).
            UINT nStep(0);
            UINT nStepCount(0);
            hr = pEndpointVolume->GetVolumeStepInfo(
                                    &nStep,
                                    &nStepCount);
            EXIT_ON_ERROR(hr);
            WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, _id, "volume steps : %d (nStep), %d (nStepCount)", nStep, nStepCount);
        }

        CoTaskMemFree(pwszID);
        pwszID = NULL;
        PropVariantClear(&varName);
        SAFE_RELEASE(pProps);
        SAFE_RELEASE(pEndpoint);
        SAFE_RELEASE(pEndpointVolume);
    }
    SAFE_RELEASE(pCollection);
    return 0;

Exit:
    _TraceCOMError(hr);
    CoTaskMemFree(pwszID);
    pwszID = NULL;
    SAFE_RELEASE(pCollection);
    SAFE_RELEASE(pEndpoint);
    SAFE_RELEASE(pEndpointVolume);
    SAFE_RELEASE(pProps);
    return -1;
}

// ----------------------------------------------------------------------------
//  _TraceCOMError
// ----------------------------------------------------------------------------

void AudioDeviceWindowsCore::_TraceCOMError(HRESULT hr) const
{
    TCHAR buf[MAXERRORLENGTH];
    LPCTSTR errorText;

    _com_error error(hr);
    errorText = error.ErrorMessage();

    WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, _id, "Core Audio method failed (hr=0x%x)", hr);
    StringCchPrintf(buf, MAXERRORLENGTH, TEXT("Error details: "));
    StringCchCat(buf, MAXERRORLENGTH, errorText);
    WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, _id, "%s", WideToUTF8(buf));
}

// ----------------------------------------------------------------------------
//  _SetThreadName
// ----------------------------------------------------------------------------

void AudioDeviceWindowsCore::_SetThreadName(DWORD dwThreadID, LPCSTR szThreadName)
{
    // See http://msdn.microsoft.com/en-us/library/xcb2z8hs(VS.71).aspx for details on the code
    // in this function. Name of article is "Setting a Thread Name (Unmanaged)".

    THREADNAME_INFO info;
    info.dwType = 0x1000;
    info.szName = szThreadName;
    info.dwThreadID = dwThreadID;
    info.dwFlags = 0;

    __try
    {
        RaiseException( 0x406D1388, 0, sizeof(info)/sizeof(DWORD), (ULONG_PTR *)&info );
    }
    __except (EXCEPTION_CONTINUE_EXECUTION)
    {
    }
}

// ----------------------------------------------------------------------------
//  _Get44kHzDrift
// ----------------------------------------------------------------------------

void AudioDeviceWindowsCore::_Get44kHzDrift()
{
    // We aren't able to resample at 44.1 kHz. Instead we run at 44 kHz and push/pull
    // from the engine faster to compensate. If only one direction is set to 44.1 kHz
    // the result is indistinguishable from clock drift to the AEC. We can compensate
    // internally if we inform the AEC about the drift.
    _sampleDriftAt48kHz = 0;
    _driftAccumulator = 0;

    if (_playSampleRate == 44000 && _recSampleRate != 44000)
    {
        _sampleDriftAt48kHz = 480.0f/440;
    }
    else if(_playSampleRate != 44000 && _recSampleRate == 44000)
    {
        _sampleDriftAt48kHz = -480.0f/441;
    }
}

// ----------------------------------------------------------------------------
//  WideToUTF8
// ----------------------------------------------------------------------------

char* AudioDeviceWindowsCore::WideToUTF8(const TCHAR* src) const {
#ifdef UNICODE
    const size_t kStrLen = sizeof(_str);
    memset(_str, 0, kStrLen);
    // Get required size (in bytes) to be able to complete the conversion.
    int required_size = WideCharToMultiByte(CP_UTF8, 0, src, -1, _str, 0, 0, 0);
    if (required_size <= kStrLen)
    {
        // Process the entire input string, including the terminating null char.
        if (WideCharToMultiByte(CP_UTF8, 0, src, -1, _str, kStrLen, 0, 0) == 0)
            memset(_str, 0, kStrLen);
    }
    return _str;
#else
    return const_cast<char*>(src);
#endif
}

}  // namespace webrtc

#endif  // WEBRTC_WINDOWS_CORE_AUDIO_BUILD
