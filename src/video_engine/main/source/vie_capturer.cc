/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

/*
 * vie_capturer.cc
 */

#include "vie_capturer.h"
#include "vie_defines.h"

#include "critical_section_wrapper.h"
#include "event_wrapper.h"
#include "module_common_types.h"
#include "video_capture_factory.h"
#include "video_processing.h"
#include "video_render_defines.h"
#include "thread_wrapper.h"
#include "vie_image_process.h"
#include "vie_encoder.h"
#include "process_thread.h"
#include "trace.h"

namespace webrtc {

// ----------------------------------------------------------------------------
// Constructor
// ----------------------------------------------------------------------------

ViECapturer::ViECapturer(int captureId,
                       int engineId,
                       ProcessThread& moduleProcessThread)
    :    ViEFrameProviderBase(captureId, engineId),
        _captureCritsect(*CriticalSectionWrapper::CreateCriticalSection()),
        _deliverCritsect(*CriticalSectionWrapper::CreateCriticalSection()),
        _captureModule(NULL),
        _externalCaptureModule(NULL),
        _moduleProcessThread(moduleProcessThread),
        _captureId(captureId),
        _vieCaptureThread(*ThreadWrapper::CreateThread(ViECaptureThreadFunction,
                                                    this, kHighPriority,
                                                    "ViECaptureThread")),
        _vieCaptureEvent(*EventWrapper::Create()),
        _vieDeliverEvent(*EventWrapper::Create()),
        _capturedFrame(),
        _deliverFrame(),
        _encodedFrame(),
        _effectFilter(NULL),
        _imageProcModule(NULL),
        _imageProcModuleRefCounter(0),
        _deflickerFrameStats(NULL),
        _brightnessFrameStats(NULL),
        _currentBrightnessLevel(Normal),
        _reportedBrightnessLevel(Normal),
        _denoisingEnabled(false),
        _observerCritsect(*CriticalSectionWrapper::CreateCriticalSection()),
        _observer(NULL),
        _encodingCritsect(*CriticalSectionWrapper::CreateCriticalSection()),
        _captureEncoder(NULL),
        _encodeCompleteCallback(NULL),
        _vieEncoder(NULL),
        _vcm(NULL),
        _decodeBuffer(),
        _decoderInitialized(false),
        _requestedCapability()
{
    WEBRTC_TRACE(webrtc::kTraceMemory, webrtc::kTraceVideo, ViEId(engineId, captureId),
               "ViECapturer::ViECapturer(captureId: %d, engineId: %d) - "
               "Constructor", captureId, engineId);

    unsigned int tId = 0;
    if (_vieCaptureThread.Start(tId))
    {
        WEBRTC_TRACE(webrtc::kTraceInfo, webrtc::kTraceVideo, ViEId(engineId, captureId),
                   "%s: thread started: %u", __FUNCTION__, tId);
    } else
    {
        assert(false);
    }
}

// ----------------------------------------------------------------------------
// Destructor
// ----------------------------------------------------------------------------

ViECapturer::~ViECapturer()
{
    WEBRTC_TRACE(webrtc::kTraceMemory, webrtc::kTraceVideo,
               ViEId(_engineId, _captureId),
               "ViECapturer Destructor, captureId: %d, engineId: %d",
               _captureId, _engineId);

    // Stop the thread
    _deliverCritsect.Enter();
    _captureCritsect.Enter();
    _vieCaptureThread.SetNotAlive();
    _vieCaptureEvent.Set();
    _captureCritsect.Leave();
    _deliverCritsect.Leave();

    _providerCritSect.Enter();
    if (_vieEncoder)
    {
        _vieEncoder->DeRegisterExternalEncoder(_codec.plType);
    }
    _providerCritSect.Leave();

    // Stop the camera input
    if (_captureModule)
    {
        _moduleProcessThread.DeRegisterModule(_captureModule);
        _captureModule->DeRegisterCaptureDataCallback();
        _captureModule->Release();
        _captureModule = NULL;
    }
    if (_vieCaptureThread.Stop())
    {
        // Thread stopped
        delete &_vieCaptureThread;
        delete &_vieCaptureEvent;
        delete &_vieDeliverEvent;
    } else
    {
        assert(false);
        WEBRTC_TRACE(webrtc::kTraceMemory, webrtc::kTraceVideoRenderer, ViEId(_engineId, _captureId),
                   "%s: Not able to stop capture thread for device %d, leaking",
                   __FUNCTION__, _captureId);
        // Not possible to stop the thread, leak it...
    }

    if (_imageProcModule)
    {
        VideoProcessingModule::Destroy(_imageProcModule);
    }
    if (_deflickerFrameStats)
    {
        delete _deflickerFrameStats;
        _deflickerFrameStats = NULL;
    }
    delete _brightnessFrameStats;
    if (_vcm)
    {
        delete _vcm;
    }
    delete &_captureCritsect;
    delete &_deliverCritsect;
    delete &_encodingCritsect;
    delete &_observerCritsect;
}

// ----------------------------------------------------------------------------
// Static factory class
// ----------------------------------------------------------------------------
ViECapturer* ViECapturer::CreateViECapture(int  captureId,
                                         int engineId,
                                         VideoCaptureModule& captureModule,
                                         ProcessThread& moduleProcessThread)
{
    ViECapturer* capture = new ViECapturer(captureId, engineId,
                                         moduleProcessThread);
    if (!capture || capture->Init(captureModule) != 0)
    {
        delete capture;
        capture = NULL;
    }
    return capture;
}

WebRtc_Word32 ViECapturer::Init(VideoCaptureModule& captureModule)
{
    assert(_captureModule == NULL);
    _captureModule = &captureModule;
    _captureModule->RegisterCaptureDataCallback(*this);
    _captureModule->AddRef();
    if (_moduleProcessThread.RegisterModule(_captureModule) != 0)
    {
        return -1;
    }
    return 0;
}

ViECapturer* ViECapturer::CreateViECapture(int captureId,
                                         int engineId,
                                         const WebRtc_UWord8* deviceUniqueIdUTF8,
                                         const WebRtc_UWord32 deviceUniqueIdUTF8Length,
                                         ProcessThread& moduleProcessThread)
{
    ViECapturer* capture = new ViECapturer(captureId, engineId, moduleProcessThread);
    if (!capture ||
        capture->Init(deviceUniqueIdUTF8, deviceUniqueIdUTF8Length) != 0)
    {
        delete capture;
        capture = NULL;
    }
    return capture;
}
WebRtc_Word32 ViECapturer::Init(const WebRtc_UWord8* deviceUniqueIdUTF8,
                                const WebRtc_UWord32 deviceUniqueIdUTF8Length)
{
    assert(_captureModule == NULL);
#ifndef WEBRTC_VIDEO_EXTERNAL_CAPTURE_AND_RENDER
    if (deviceUniqueIdUTF8 == NULL)
    {
        _captureModule  = VideoCaptureFactory::Create(
            ViEModuleId(_engineId, _captureId), _externalCaptureModule);
    } else
    {
        _captureModule = VideoCaptureFactory::Create(
            ViEModuleId(_engineId, _captureId), deviceUniqueIdUTF8);
    }
#endif
    if (!_captureModule)
        return -1;
    _captureModule->AddRef();
    _captureModule->RegisterCaptureDataCallback(*this);
    if (_moduleProcessThread.RegisterModule(_captureModule) != 0)
    {
        return -1;
    }
    return 0;
}

// ----------------------------------------------------------------------------
// FrameCallbackChanged
// ----------------------------------------------------------------------------
int ViECapturer::FrameCallbackChanged()
{
    if (Started() && !EncoderActive() && !CaptureCapabilityFixed()) // Reconfigure the camera if a new size is required and the capture device does not provide encoded frames.
    {
        int bestWidth;
        int bestHeight;
        int bestFrameRate;
        VideoCaptureCapability captureSettings;
        _captureModule->CaptureSettings(captureSettings);
        GetBestFormat(bestWidth, bestHeight, bestFrameRate);
        if (bestWidth != 0 && bestHeight != 0 && bestFrameRate != 0)
        {
            if (bestWidth != captureSettings.width ||
                bestHeight != captureSettings.height ||
                bestFrameRate != captureSettings.maxFPS ||
                captureSettings.codecType != kVideoCodecUnknown)
            {
                Stop();
                Start(_requestedCapability);
            }
        }
    }
    return 0;
}

// ----------------------------------------------------------------------------
// Start
//
// Starts the capture device.
// ----------------------------------------------------------------------------

WebRtc_Word32 ViECapturer::Start(const CaptureCapability captureCapability)
{
    WEBRTC_TRACE(webrtc::kTraceInfo, webrtc::kTraceVideo, ViEId(_engineId, _captureId),
               "%s", __FUNCTION__);

    int width;
    int height;
    int frameRate;
    VideoCaptureCapability capability;
    _requestedCapability = captureCapability;
    if (EncoderActive())
    {
        CriticalSectionScoped cs(_encodingCritsect);
        capability.width = _codec.width;
        capability.height = _codec.height;
        capability.maxFPS = _codec.maxFramerate;
        capability.codecType = _codec.codecType;
        capability.rawType = kVideoI420;

    } else if (!CaptureCapabilityFixed())
    {
        GetBestFormat(width, height, frameRate); // Ask the observers for best size
        if (width == 0)
        {
            width = kViECaptureDefaultWidth;
        }
        if (height == 0)
        {
            height = kViECaptureDefaultHeight;
        }
        if (frameRate == 0)
        {
            frameRate = kViECaptureDefaultFramerate;
        }
        capability.height = height;
        capability.width = width;
        capability.maxFPS = frameRate;
        capability.rawType = kVideoI420;
        capability.codecType = kVideoCodecUnknown;
    } else // Width and heigh and type specified with call to Start - not set by observers.
    {
        capability.width = _requestedCapability.width;
        capability.height = _requestedCapability.height;
        capability.maxFPS = _requestedCapability.maxFPS;
        capability.rawType = _requestedCapability.rawType;
        capability.interlaced = _requestedCapability.interlaced;
    }
    return _captureModule->StartCapture(capability);
}

// ----------------------------------------------------------------------------
// Stop
//
// Stops the capture device
// ----------------------------------------------------------------------------

WebRtc_Word32 ViECapturer::Stop()
{
    WEBRTC_TRACE(webrtc::kTraceInfo, webrtc::kTraceVideo, ViEId(_engineId, _captureId),
               "%s", __FUNCTION__);
    _requestedCapability = CaptureCapability();
    return _captureModule->StopCapture();
}

// ----------------------------------------------------------------------------
// Started
//
// Returns true if the capture device is started, false otherwise
// ----------------------------------------------------------------------------

bool ViECapturer::Started()
{
    WEBRTC_TRACE(webrtc::kTraceInfo, webrtc::kTraceVideo, ViEId(_engineId, _captureId),
               "%s", __FUNCTION__);
    return _captureModule->CaptureStarted();
}

const WebRtc_UWord8* ViECapturer::CurrentDeviceName() const
{
    return _captureModule->CurrentDeviceName();
}

// ----------------------------------------------------------------------------
// SetCaptureDelay
//
// Overrides the capture delay
// ----------------------------------------------------------------------------
WebRtc_Word32 ViECapturer::SetCaptureDelay(WebRtc_Word32 delayMs)
{
    return _captureModule->SetCaptureDelay(delayMs);
}

// ----------------------------------------------------------------------------
// SetCapturedFrameRotation
//
// Tell the capture module whether or not to rotate a frame when it is captured
// ----------------------------------------------------------------------------
WebRtc_Word32 ViECapturer::SetRotateCapturedFrames(
                                    const RotateCapturedFrame rotation)
{
    VideoCaptureRotation convertedRotation = kCameraRotate0;
    switch (rotation)
    {
        case RotateCapturedFrame_0:
            convertedRotation = kCameraRotate0;
            break;
        case RotateCapturedFrame_90:
            convertedRotation = kCameraRotate90;
            break;
        case RotateCapturedFrame_180:
            convertedRotation = kCameraRotate180;
            break;
        case RotateCapturedFrame_270:
            convertedRotation = kCameraRotate270;
            break;
        default:
            break;
    }
    return _captureModule->SetCaptureRotation(convertedRotation);
}

// ----------------------------------------------------------------------------
// IncomingFrame
//
// Inherited from ExternalCapture, will deliver a new captured
// frame to the capture module.
// ----------------------------------------------------------------------------

int ViECapturer::IncomingFrame(unsigned char* videoFrame,
                              unsigned int videoFrameLength,
                              unsigned short width, unsigned short height,
                              RawVideoType videoType,
                              unsigned long long captureTime)
{
    WEBRTC_TRACE(webrtc::kTraceInfo, webrtc::kTraceVideo, ViEId(_engineId, _captureId),
               "%ExternalCapture::IncomingFrame width %d, height %d, captureTime %u",
               width, height, captureTime);

    if (!_externalCaptureModule)
    {
        return -1;
    }
    VideoCaptureCapability capability;
    capability.width = width;
    capability.height = height;
    capability.rawType = videoType;
    return _externalCaptureModule->IncomingFrame(videoFrame, videoFrameLength,
                                                 capability, captureTime);
}
// ----------------------------------------------------------------------------
// OnIncomingCapturedFrame
//
// Inherited from VideoCaptureDataCallback, will deliver a new captured
// frame
// ----------------------------------------------------------------------------

void ViECapturer::OnIncomingCapturedFrame(const WebRtc_Word32 captureId,
                                         VideoFrame& videoFrame,
                                         VideoCodecType codecType)
{
    WEBRTC_TRACE(webrtc::kTraceStream, webrtc::kTraceVideo, ViEId(_engineId, _captureId),
               "%s(captureId: %d)", __FUNCTION__, captureId);

    CriticalSectionScoped cs(_captureCritsect);
    if (codecType != kVideoCodecUnknown)
    {
        if (_encodedFrame.Length() != 0) // The last encoded frame has not been sent yet. Need to wait
        {
            _vieDeliverEvent.Reset();
            WEBRTC_TRACE(webrtc::kTraceWarning, webrtc::kTraceVideo, ViEId(_engineId, _captureId),
                       "%s(captureId: %d) Last encoded frame not yet delivered.",
                       __FUNCTION__, captureId);
            _captureCritsect.Leave();
            _vieDeliverEvent.Wait(500); // Wait 500ms for the coded frame to be sent before unblocking this.
            assert(_encodedFrame.Length()==0);
            _captureCritsect.Enter();
        }
        _encodedFrame.SwapFrame(videoFrame);
    } else
    {
        _capturedFrame.SwapFrame(videoFrame);
    }
    _vieCaptureEvent.Set();
    return;
}

void ViECapturer::OnCaptureDelayChanged(const WebRtc_Word32 id,
                                       const WebRtc_Word32 delay)
{
    WEBRTC_TRACE(webrtc::kTraceStream, webrtc::kTraceVideo,
               ViEId(_engineId, _captureId),
               "%s(captureId: %d) delay %d", __FUNCTION__, _captureId,
               delay);

    // Deliver the network delay to all registered callbacks
    ViEFrameProviderBase::SetFrameDelay(delay);
    CriticalSectionScoped cs(_encodingCritsect);
    if (_vieEncoder)
    {
        _vieEncoder->DelayChanged(id, delay);
    }
}

WebRtc_Word32 ViECapturer::RegisterEffectFilter(ViEEffectFilter* effectFilter)
{
    CriticalSectionScoped cs(_deliverCritsect);

    if (effectFilter == NULL)
    {
        if (_effectFilter == NULL)
        {
            WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideo, ViEId(_engineId, _captureId),
                       "%s: no effect filter added for capture device %d",
                       __FUNCTION__, _captureId);
            return -1;
        }
        WEBRTC_TRACE(webrtc::kTraceInfo, webrtc::kTraceVideo, ViEId(_engineId,_captureId),
                   "%s: deregister effect filter for device %d", __FUNCTION__,
                   _captureId);
    } else
    {
        if (_effectFilter)
        {
            WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideo, ViEId(_engineId,_captureId),
                       "%s: effect filter already added for capture device %d",
                       __FUNCTION__, _captureId);
            return -1;
        }
        WEBRTC_TRACE(webrtc::kTraceInfo, webrtc::kTraceVideo, ViEId(_engineId, _captureId),
                   "%s: register effect filter for device %d", __FUNCTION__,
                   _captureId);
    }
    _effectFilter = effectFilter;
    return 0;
}

//------------------------------------------------------------------------------------------------
//
// IncImageProcRefCount
// Help function used for keeping track of VideoImageProcesingModule. Creates the module if it is needed.
// Return 0 on success and guarantee that the image proc module exist.
//------------------------------------------------------------------------------------------------

WebRtc_Word32 ViECapturer::IncImageProcRefCount()
{
    if (!_imageProcModule)
    {
        assert(_imageProcModuleRefCounter==0);
        _imageProcModule = VideoProcessingModule::Create(ViEModuleId(_engineId, _captureId));
        if (_imageProcModule == NULL)
        {
            WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideo, ViEId(_engineId, _captureId),
                       "%s: could not create video processing module",
                       __FUNCTION__);
            return -1;
        }
    }
    _imageProcModuleRefCounter++;
    return 0;
}
WebRtc_Word32 ViECapturer::DecImageProcRefCount()
{
    _imageProcModuleRefCounter--;
    // Destroy module
    if (_imageProcModuleRefCounter == 0)
    {
        VideoProcessingModule::Destroy(_imageProcModule);
        _imageProcModule = NULL;
    }
    return 0;
}

WebRtc_Word32 ViECapturer::EnableDenoising(bool enable)
{
    WEBRTC_TRACE(webrtc::kTraceInfo, webrtc::kTraceVideo, ViEId(_engineId, _captureId),
               "%s(captureDeviceId: %d, enable: %d)", __FUNCTION__,
               _captureId, enable);

    CriticalSectionScoped cs(_deliverCritsect);
    if (enable)
    {
        // Sanity check
        if (_denoisingEnabled)
        {
            WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideo, ViEId(_engineId, _captureId),
                       "%s: denoising already enabled", __FUNCTION__);
            return -1;
        }
        _denoisingEnabled = true;
        if (IncImageProcRefCount() != 0)
        {
            return -1;
        }
    } else
    {
        // Sanity check
        if (_denoisingEnabled == false)
        {
            WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideo, ViEId(_engineId, _captureId),
                       "%s: denoising not enabled", __FUNCTION__);
            return -1;
        }
        _denoisingEnabled = false;
        DecImageProcRefCount();
    }

    return 0;
}

WebRtc_Word32 ViECapturer::EnableDeflickering(bool enable)
{
    WEBRTC_TRACE(webrtc::kTraceInfo, webrtc::kTraceVideo, ViEId(_engineId, _captureId),
               "%s(captureDeviceId: %d, enable: %d)", __FUNCTION__,
               _captureId, enable);

    CriticalSectionScoped cs(_deliverCritsect);
    if (enable)
    {
        // Sanity check
        if (_deflickerFrameStats)
        {
            WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideo, ViEId(_engineId, _captureId),
                       "%s: deflickering already enabled", __FUNCTION__);
            return -1;
        }
        // Create the module
        if (IncImageProcRefCount() != 0)
        {
            return -1;
        }
        _deflickerFrameStats = new VideoProcessingModule::FrameStats();
    } else
    {
        // Sanity check
        if (_deflickerFrameStats == NULL)
        {
            WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideo, ViEId(_engineId, _captureId),
                       "%s: deflickering not enabled", __FUNCTION__);
            return -1;
        }
        // Destroy module
        DecImageProcRefCount();
        delete _deflickerFrameStats;
        _deflickerFrameStats = NULL;
    }
    return 0;
}
WebRtc_Word32 ViECapturer::EnableBrightnessAlarm(bool enable)
{
    WEBRTC_TRACE(webrtc::kTraceInfo, webrtc::kTraceVideo, ViEId(_engineId, _captureId),
               "%s(captureDeviceId: %d, enable: %d)", __FUNCTION__,
               _captureId, enable);

    CriticalSectionScoped cs(_deliverCritsect);
    if (enable)
    {
        // Sanity check
        if (_brightnessFrameStats)
        {
            WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideo, ViEId(_engineId, _captureId),
                       "%s: BrightnessAlarm already enabled", __FUNCTION__);
            return -1;
        }
        if (IncImageProcRefCount() != 0)
        {
            return -1;
        }
        _brightnessFrameStats = new VideoProcessingModule::FrameStats();
    } else
    {
        DecImageProcRefCount();
        // Sanity check
        if (_brightnessFrameStats == NULL)
        {
            WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideo, ViEId(_engineId, _captureId),
                       "%s: deflickering not enabled", __FUNCTION__);
            return -1;
        }
        delete _brightnessFrameStats;
        _brightnessFrameStats = NULL;
    }
    return 0;
}

bool ViECapturer::ViECaptureThreadFunction(void* obj)
{
    return static_cast<ViECapturer*> (obj)->ViECaptureProcess();
}

bool ViECapturer::ViECaptureProcess()
{
    if (_vieCaptureEvent.Wait(kThreadWaitTimeMs) == kEventSignaled)
    {
        _deliverCritsect.Enter();
        if (_capturedFrame.Length() > 0) // New I420 frame
        {
            _captureCritsect.Enter();
            _deliverFrame.SwapFrame(_capturedFrame);
            _capturedFrame.SetLength(0);
            _captureCritsect.Leave();
            DeliverI420Frame(_deliverFrame);
        }
        if (_encodedFrame.Length() > 0)
        {
            _captureCritsect.Enter();
            _deliverFrame.SwapFrame(_encodedFrame);
            _encodedFrame.SetLength(0);
            _vieDeliverEvent.Set();
            _captureCritsect.Leave();
            DeliverCodedFrame(_deliverFrame);
        }
        _deliverCritsect.Leave();
        if (_currentBrightnessLevel != _reportedBrightnessLevel)
        {
            CriticalSectionScoped cs(_observerCritsect);
            if (_observer)
            {
                _observer->BrightnessAlarm(_id, _currentBrightnessLevel);
                _reportedBrightnessLevel = _currentBrightnessLevel;
            }
        }
    }
    // We're done!
    return true;
}

void ViECapturer::DeliverI420Frame(VideoFrame& videoFrame)
{
    // Apply image enhancement and effect filter
    if (_deflickerFrameStats)
    {
        if (_imageProcModule->GetFrameStats(*_deflickerFrameStats, videoFrame) == 0)
        {
            _imageProcModule->Deflickering(videoFrame, *_deflickerFrameStats);
        } else
        {
            WEBRTC_TRACE(webrtc::kTraceStream, webrtc::kTraceVideo, ViEId(_engineId, _captureId),
                       "%s: could not get frame stats for captured frame", __FUNCTION__);
        }
    }
    if (_denoisingEnabled)
    {
        _imageProcModule->Denoising(videoFrame);
    }
    if (_brightnessFrameStats)
    {
        if (_imageProcModule->GetFrameStats (*_brightnessFrameStats, videoFrame) == 0)
        {
            WebRtc_Word32 brightness = _imageProcModule->BrightnessDetection(
                                                        videoFrame,
                                                        *_brightnessFrameStats);
            switch (brightness)
            {
                case VideoProcessingModule::kNoWarning:
                    _currentBrightnessLevel = Normal;
                    break;
                case VideoProcessingModule::kDarkWarning:
                    _currentBrightnessLevel = Dark;
                    break;
                case VideoProcessingModule::kBrightWarning:
                    _currentBrightnessLevel = Bright;
                    break;
                default:
                    WEBRTC_TRACE(webrtc::kTraceInfo, webrtc::kTraceVideo,
                               ViEId(_engineId, _captureId),
                               "%s: Brightness detection failed", __FUNCTION__);
            }
        }
    }
    if (_effectFilter)
    {
        _effectFilter->Transform(videoFrame.Length(), videoFrame.Buffer(),
                                 videoFrame.TimeStamp(), videoFrame.Width(),
                                 videoFrame.Height());
    }
    // Deliver the captured frame to all observers (channels,renderer or file)
    ViEFrameProviderBase::DeliverFrame(videoFrame);
}

void ViECapturer::DeliverCodedFrame(VideoFrame& videoFrame)
{
    if (_encodeCompleteCallback)
    {
        EncodedImage encodedImage(videoFrame.Buffer(),
                                          videoFrame.Length(),
                                          videoFrame.Size());
        encodedImage._timeStamp = 90*(WebRtc_UWord32) videoFrame.RenderTimeMs();
        _encodeCompleteCallback->Encoded(encodedImage);
    }

    if (NumberOfRegistersFrameCallbacks() > 0 && _decoderInitialized)
    {
        videoFrame.Swap(_decodeBuffer.payloadData, _decodeBuffer.bufferSize,
                        _decodeBuffer.payloadSize);
        _decodeBuffer.encodedHeight = videoFrame.Height();
        _decodeBuffer.encodedWidth = videoFrame.Width();
        _decodeBuffer.renderTimeMs = videoFrame.RenderTimeMs();
        _decodeBuffer.timeStamp = 90*(WebRtc_UWord32) videoFrame.RenderTimeMs();
        _decodeBuffer.payloadType = _codec.plType;
        _vcm->DecodeFromStorage(_decodeBuffer);
    }

}
/*
 * DeregisterFrameCallback - Overrides ViEFrameProvider
 */
int ViECapturer::DeregisterFrameCallback(const ViEFrameCallback* callbackObject)
{

    _providerCritSect.Enter();
    if (callbackObject == _vieEncoder) //Don't use this camera as encoder anymore. Need to tell the ViEEncoder.
    {
        ViEEncoder* vieEncoder = NULL;
        vieEncoder = _vieEncoder;
        _vieEncoder = NULL;
        _providerCritSect.Leave();
        _deliverCritsect.Enter(); //Need to take this here in order to avoid deadlock with VCM. The reason is that VCM will call ::Release and a deadlock can occure.
        vieEncoder->DeRegisterExternalEncoder(_codec.plType);
        _deliverCritsect.Leave();
        return 0;
    }
    _providerCritSect.Leave();
    return ViEFrameProviderBase::DeregisterFrameCallback(callbackObject);
}

/*
 * IsFrameCallbackRegistered - Overrides ViEFrameProvider
 */
bool ViECapturer::IsFrameCallbackRegistered(const ViEFrameCallback* callbackObject)
{
    CriticalSectionScoped cs(_providerCritSect);
    if (callbackObject == _vieEncoder)
    {
        return true;
    }
    return ViEFrameProviderBase::IsFrameCallbackRegistered(callbackObject);

}

// ----------------------------------------------------------------------------
// External encoder using attached capture device.
// Implements VideoEncoderInterface
// ----------------------------------------------------------------------------

WebRtc_Word32 ViECapturer::PreEncodeToViEEncoder(const VideoCodec& codec,
                                                ViEEncoder& vieEncoder,
                                                WebRtc_Word32 vieEncoderId)
{
    WEBRTC_TRACE(webrtc::kTraceInfo, webrtc::kTraceVideo, ViEId(_engineId, _captureId),
               "%s(captureDeviceId: %d)", __FUNCTION__, _captureId);
    {
        if (_vieEncoder && &vieEncoder != _vieEncoder)
        {
            WEBRTC_TRACE( webrtc::kTraceInfo, webrtc::kTraceVideo, ViEId(_engineId, _captureId),
                       "%s(captureDeviceId: %d Capture device already encoding)",
                       __FUNCTION__, _captureId);
            return -1;
        }
    }
    CriticalSectionScoped cs(_encodingCritsect);
    VideoCaptureModule::VideoCaptureEncodeInterface* captureEncoder =
                                      _captureModule->GetEncodeInterface(codec);
    if (!captureEncoder)
        return -1; // Encoding not supported?

    _captureEncoder = captureEncoder;
    // Create VCM module used for decoding frames if needed.
    if (!_vcm)
    {
        _vcm = VideoCodingModule::Create(_captureId);
    }

    if (vieEncoder.RegisterExternalEncoder(this, codec.plType) != 0)
    {
        return -1;
    }
    if (vieEncoder.SetEncoder(codec) != 0)
    {
        vieEncoder.DeRegisterExternalEncoder(codec.plType);
        return -1;
    }
    // Make sure the encoder is not an I420 observer.
    ViEFrameProviderBase::DeregisterFrameCallback(&vieEncoder);
    _vieEncoder = &vieEncoder; // Store the vieEncoder that is using this capture device.
    _vieEncoderId = vieEncoderId;
    memcpy(&_codec, &codec, sizeof(webrtc::VideoCodec));
    return 0;
}

bool ViECapturer::EncoderActive()
{
    return _vieEncoder != NULL;
}

/*
 * CaptureCapabilityFixed
 * Returns true if width, height and framerate was specified when the Start() was called.
 */
bool ViECapturer::CaptureCapabilityFixed()
{
    return _requestedCapability.width != 0 && _requestedCapability.height != 0
            && _requestedCapability.maxFPS != 0;
}

// ----------------------------------------------------------------------------
// Implements VideoEncoder
//
// ----------------------------------------------------------------------------
WebRtc_Word32 ViECapturer::Version(WebRtc_Word8 *version, WebRtc_Word32 length) const
{
    return 0;
}

WebRtc_Word32 ViECapturer::InitEncode(const VideoCodec* codecSettings,
                                     WebRtc_Word32 numberOfCores,
                                     WebRtc_UWord32 maxPayloadSize)
{
    WEBRTC_TRACE(webrtc::kTraceInfo, webrtc::kTraceVideo, ViEId(_engineId, _captureId),
               "%s(captureDeviceId: %d)", __FUNCTION__, _captureId);

    CriticalSectionScoped cs(_encodingCritsect);
    if (!_captureEncoder || !codecSettings)
        return WEBRTC_VIDEO_CODEC_ERROR;

    if (_vcm) // Initialize VCM to be able to decode frames if needed.
    {
        if (_vcm->InitializeReceiver() == 0)
        {
            if (_vcm->RegisterReceiveCallback(this) == 0)
            {
                if (_vcm->RegisterReceiveCodec(codecSettings, numberOfCores,
                                                                   false) == 0)
                {
                    _decoderInitialized = true;
                    WEBRTC_TRACE(webrtc::kTraceInfo, webrtc::kTraceVideo, ViEId(_engineId, _captureId),
                               "%s(captureDeviceId: %d) VCM Decoder initialized",
                               __FUNCTION__, _captureId);
                }
            }
        }
    }
    return _captureEncoder->ConfigureEncoder(*codecSettings, maxPayloadSize);
}

/*
 * Encode
 * Orders the Capture device to create a certain frame type.
 */
WebRtc_Word32
ViECapturer::Encode(const RawImage& inputImage,
                    const CodecSpecificInfo* codecSpecificInfo,
                    const VideoFrameType* frameTypes)
{

    CriticalSectionScoped cs(_encodingCritsect);

    if (!_captureEncoder)
        return WEBRTC_VIDEO_CODEC_UNINITIALIZED;

    if (*frameTypes == kKeyFrame)
        return _captureEncoder->EncodeFrameType(kVideoFrameKey);

    if (*frameTypes == kSkipFrame)
        return _captureEncoder->EncodeFrameType(kFrameEmpty);

    return WEBRTC_VIDEO_CODEC_ERR_PARAMETER;
}

WebRtc_Word32 ViECapturer::RegisterEncodeCompleteCallback( EncodedImageCallback* callback)
{
    WEBRTC_TRACE(webrtc::kTraceInfo, webrtc::kTraceVideo, ViEId(_engineId, _captureId),
               "%s(captureDeviceId: %d)", __FUNCTION__, _captureId);

    CriticalSectionScoped cs(_deliverCritsect);
    if (!_captureEncoder)
        return WEBRTC_VIDEO_CODEC_UNINITIALIZED;

    _encodeCompleteCallback = callback;
    return 0;

}
WebRtc_Word32 ViECapturer::Release()
{
    WEBRTC_TRACE(webrtc::kTraceInfo, webrtc::kTraceVideo, ViEId(_engineId, _captureId),
               "%s(captureDeviceId: %d)", __FUNCTION__, _captureId);

    {
        CriticalSectionScoped cs(_deliverCritsect);
        _encodeCompleteCallback = NULL;
    }

    {
        CriticalSectionScoped cs(_encodingCritsect);

        _decoderInitialized = false;
        _codec.codecType = kVideoCodecUnknown;
        _captureEncoder->ConfigureEncoder(_codec, 0); // Reset the camera to output I420.

        if (_vieEncoder) // Need to add the encoder as an observer of I420.
        {
            ViEFrameProviderBase::RegisterFrameCallback(_vieEncoderId,
                                                        _vieEncoder);
        }
        _vieEncoder = NULL;
    }
    return 0;
}

/*
 * Reset
 * Should reset the capture device to the state it was in after the InitEncode function.
 * Current implementation do nothing.
 */
WebRtc_Word32 ViECapturer::Reset()
{

    WEBRTC_TRACE(webrtc::kTraceInfo, webrtc::kTraceVideo, ViEId(_engineId, _captureId),
               "%s(captureDeviceId: %d)", __FUNCTION__, _captureId);
    return 0;

}
WebRtc_Word32 ViECapturer::SetPacketLoss(WebRtc_UWord32 packetLoss)
{
    WEBRTC_TRACE(webrtc::kTraceInfo, webrtc::kTraceVideo, ViEId(_engineId, _captureId),
               "%s(captureDeviceId: %d)", __FUNCTION__, _captureId);

    CriticalSectionScoped cs(_encodingCritsect);
    if (!_captureEncoder)
        return WEBRTC_VIDEO_CODEC_UNINITIALIZED;

    return _captureEncoder->SetPacketLoss(packetLoss);
}

WebRtc_Word32 ViECapturer::SetRates(WebRtc_UWord32 newBitRate,
                                   WebRtc_UWord32 frameRate)
{
    WEBRTC_TRACE(webrtc::kTraceInfo, webrtc::kTraceVideo, ViEId(_engineId, _captureId),
               "%s(captureDeviceId: %d)", __FUNCTION__, _captureId);

    CriticalSectionScoped cs(_encodingCritsect);
    if (!_captureEncoder)
        return WEBRTC_VIDEO_CODEC_UNINITIALIZED;

    return _captureEncoder->SetRates(newBitRate, frameRate);
}

/*
 * FrameToRender - implements VCMReceiveCallback.
 * (VCM decode callback) Used in order to be able to provide I420 frames to renderer etc.
 */

WebRtc_Word32 ViECapturer::FrameToRender(VideoFrame& videoFrame)
{
    _deliverCritsect.Enter();
    DeliverI420Frame(videoFrame);
    _deliverCritsect.Leave();
    return 0;
}

/******************************************************************************/

//
// Statistics Observer functions
//
WebRtc_Word32 ViECapturer::RegisterObserver(ViECaptureObserver& observer)
{
    if (_observer)
    {
        WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideo, ViEId(_engineId,
                                                         _captureId),
                   "%s Observer already registered", __FUNCTION__,
                   _captureId);
        return -1;
    }
    if (_captureModule->RegisterCaptureCallback(*this) != 0)
    {
        return -1;
    }
    _captureModule->EnableFrameRateCallback(true);
    _captureModule->EnableNoPictureAlarm(true);
    _observer = &observer;
    return 0;
}

WebRtc_Word32 ViECapturer::DeRegisterObserver()
{
    CriticalSectionScoped cs(_observerCritsect);
    if (!_observer)
    {
        WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideo, ViEId(_engineId, _captureId),
                   "%s No observer registered", __FUNCTION__, _captureId);
        return -1;
    }
    _captureModule->EnableFrameRateCallback(false);
    _captureModule->EnableNoPictureAlarm(false);
    _captureModule->DeRegisterCaptureCallback();
    _observer = NULL;
    return 0;

}
bool ViECapturer::IsObserverRegistered()
{
    CriticalSectionScoped cs(_observerCritsect);
    return _observer != NULL;
}
// ----------------------------------------------------------------------------
// Implements VideoCaptureFeedBack
//
// ----------------------------------------------------------------------------
void ViECapturer::OnCaptureFrameRate(const WebRtc_Word32 id,
                                    const WebRtc_UWord32 frameRate)
{
    WEBRTC_TRACE(webrtc::kTraceStream, webrtc::kTraceVideo,
               ViEId(_engineId, _captureId), "OnCaptureFrameRate %d",
               frameRate);

    CriticalSectionScoped cs(_observerCritsect);
    _observer->CapturedFrameRate(_id, (WebRtc_UWord8) frameRate);
}

void ViECapturer::OnNoPictureAlarm(const WebRtc_Word32 id,
                                  const VideoCaptureAlarm alarm)
{
    WEBRTC_TRACE(webrtc::kTraceStream, webrtc::kTraceVideo,
               ViEId(_engineId, _captureId), "OnNoPictureAlarm %d", alarm);

    CriticalSectionScoped cs(_observerCritsect);
    CaptureAlarm vieAlarm = (alarm == Raised) ? AlarmRaised : AlarmCleared;
    _observer->NoPictureAlarm(id, vieAlarm);
}
// ----------------------------------------------------------------------------

WebRtc_Word32 ViECapturer::SetCaptureDeviceImage(const VideoFrame& captureDeviceImage)
{
    return _captureModule->StartSendImage(captureDeviceImage, 10);
}

} // namespace webrtc
