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
 * vie_capture_impl.cc
 */

#include "vie_capture_impl.h"

// Defines
#include "vie_defines.h"

#include "trace.h"
#include "vie_capturer.h"
#include "vie_channel.h"
#include "vie_channel_manager.h"
#include "vie_encoder.h"
#include "vie_impl.h"
#include "vie_input_manager.h"
#include "vie_errors.h"

namespace webrtc
{

// ----------------------------------------------------------------------------
// GetInterface
// ----------------------------------------------------------------------------

ViECapture* ViECapture::GetInterface(VideoEngine* videoEngine)
{
#ifdef WEBRTC_VIDEO_ENGINE_CAPTURE_API
    if (videoEngine == NULL)
    {
        return NULL;
    }
    VideoEngineImpl* vieImpl = reinterpret_cast<VideoEngineImpl*> (videoEngine);
    ViECaptureImpl* vieCaptureImpl = vieImpl;
    (*vieCaptureImpl)++; // Increase ref count

    return vieCaptureImpl;
#else
    return NULL;
#endif
}

// ----------------------------------------------------------------------------
// Release
//
// Releases the interface, i.e. reduces the reference counter. The number of
// remaining references is returned, -1 if released too many times.
// ----------------------------------------------------------------------------

int ViECaptureImpl::Release()
{
    WEBRTC_TRACE(webrtc::kTraceApiCall, webrtc::kTraceVideo, instance_id_,
                 "ViECapture::Release()");
    (*this)--; // Decrease ref count

    WebRtc_Word32 refCount = GetCount();
    if (refCount < 0)
    {
        WEBRTC_TRACE(webrtc::kTraceWarning, webrtc::kTraceVideo, instance_id_,
                     "ViECapture release too many times");
        SetLastError(kViEAPIDoesNotExist);
        return -1;
    }
    WEBRTC_TRACE(webrtc::kTraceInfo, webrtc::kTraceVideo, instance_id_,
                 "ViECapture reference count: %d", refCount);
    return refCount;
}

// ----------------------------------------------------------------------------
// Constructor
// ----------------------------------------------------------------------------

ViECaptureImpl::ViECaptureImpl()
{
    WEBRTC_TRACE(webrtc::kTraceMemory, webrtc::kTraceVideo, instance_id_,
                 "ViECaptureImpl::ViECaptureImpl() Ctor");
}

// ----------------------------------------------------------------------------
// Destructor
// ----------------------------------------------------------------------------

ViECaptureImpl::~ViECaptureImpl()
{
    WEBRTC_TRACE(webrtc::kTraceMemory, webrtc::kTraceVideo, instance_id_,
                 "ViECaptureImpl::~ViECaptureImpl() Dtor");
}

// ============================================================================
// Available devices
// ============================================================================

// ----------------------------------------------------------------------------
// NumberOfCaptureDevices
//
// Returns the number of available devices
// ----------------------------------------------------------------------------
int ViECaptureImpl::NumberOfCaptureDevices()
{
    WEBRTC_TRACE(webrtc::kTraceApiCall, webrtc::kTraceVideo, ViEId(instance_id_), "%s",
                 __FUNCTION__);
    if (!Initialized())
    {
        SetLastError(kViENotInitialized);
        WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideo, ViEId(instance_id_),
                     "%s - ViE instance %d not initialized", __FUNCTION__,
                     instance_id_);
        return -1;
    }
    return  input_manager_.NumberOfCaptureDevices();
}

// ----------------------------------------------------------------------------
// GetCaptureDevice
//
// Gets capture device listNumber, both name and unique id if available
// ----------------------------------------------------------------------------
int ViECaptureImpl::GetCaptureDevice(unsigned int listNumber,
                                     char* deviceNameUTF8,
                                     unsigned int deviceNameUTF8Length,
                                     char* uniqueIdUTF8,
                                     unsigned int uniqueIdUTF8Length)
{
    WEBRTC_TRACE(webrtc::kTraceApiCall, webrtc::kTraceVideo, ViEId(instance_id_),
                 "%s(listNumber: %d)", __FUNCTION__, listNumber);
    if (!Initialized())
    {
        SetLastError(kViENotInitialized);
        WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideo, ViEId(instance_id_),
                     "%s - ViE instance %d not initialized", __FUNCTION__,
                     instance_id_);
        return -1;
    }
    return input_manager_.GetDeviceName(listNumber,
                                       (WebRtc_UWord8*) deviceNameUTF8,
                                       deviceNameUTF8Length,
                                       (WebRtc_UWord8*) uniqueIdUTF8,
                                       uniqueIdUTF8Length);
}

// ============================================================================
// Allocate capture device
// ============================================================================


// ----------------------------------------------------------------------------
// AllocateCaptureDevice
//
// Allocates the capture device
// ----------------------------------------------------------------------------
int ViECaptureImpl::AllocateCaptureDevice(
                                          const char* uniqueIdUTF8,
                                          const unsigned int uniqueIdUTF8Length,
                                          int& captureId)
{
    WEBRTC_TRACE(webrtc::kTraceApiCall, webrtc::kTraceVideo, ViEId(instance_id_),
                 "%s(uniqueIdUTF8: %s)", __FUNCTION__, uniqueIdUTF8);
    if (!Initialized())
    {
        SetLastError(kViENotInitialized);
        WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideo, ViEId(instance_id_),
                     "%s - ViE instance %d not initialized", __FUNCTION__,
                     instance_id_);
        return -1;
    }
    const WebRtc_Word32
        result =
            input_manager_.CreateCaptureDevice(
                (const WebRtc_UWord8*) uniqueIdUTF8,
                (const WebRtc_UWord32) uniqueIdUTF8Length, captureId);
    if (result != 0)
    {
        SetLastError(result);
        return -1;
    }
    return 0;
}

// ----------------------------------------------------------------------------
// AllocateExternalCaptureDevice
//
// Register a customer implemented capture device. callback should be called
// for all new captured images once the the capture device is started
// ----------------------------------------------------------------------------
int ViECaptureImpl::AllocateExternalCaptureDevice(
    int& captureId, ViEExternalCapture*& externalCapture)
{
    WEBRTC_TRACE(webrtc::kTraceApiCall, webrtc::kTraceVideo, ViEId(instance_id_), "%s",
                 __FUNCTION__);

    if (!Initialized())
    {
        SetLastError(kViENotInitialized);
        WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideo, ViEId(instance_id_),
                     "%s - ViE instance %d not initialized", __FUNCTION__,
                     instance_id_);
        return -1;
    }
    const WebRtc_Word32 result =
        input_manager_.CreateExternalCaptureDevice(externalCapture, captureId);

    if (result != 0)
    {
        SetLastError(result);
        return -1;
    }
    return 0;
}

// ----------------------------------------------------------------------------
// AllocateCaptureDevice
//
// Allocates the capture device, the capture module to attach
// must be associated with the unique ID.
// ----------------------------------------------------------------------------
int ViECaptureImpl::AllocateCaptureDevice(VideoCaptureModule& captureModule,
                                          int& captureId)
{
    WEBRTC_TRACE(webrtc::kTraceApiCall, webrtc::kTraceVideo, ViEId(instance_id_), "%s",
                 __FUNCTION__);

    if (!Initialized())
    {
        SetLastError(kViENotInitialized);
        WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideo, ViEId(instance_id_),
                     "%s - ViE instance %d not initialized", __FUNCTION__,
                     instance_id_);
        return -1;
    }
    const WebRtc_Word32 result =
        input_manager_.CreateCaptureDevice(captureModule, captureId);
    if (result != 0)
    {
        SetLastError(result);
        return -1;
    }
    return 0;
}

// ----------------------------------------------------------------------------
// ReleaseCaptureDevice
//
// Releases an allocated capture device
// ----------------------------------------------------------------------------
int ViECaptureImpl::ReleaseCaptureDevice(const int captureId)
{
    WEBRTC_TRACE(webrtc::kTraceApiCall, webrtc::kTraceVideo, ViEId(instance_id_),
                 "%s(captureId: %d)", __FUNCTION__, captureId);

    {
        ViEInputManagerScoped is(input_manager_);
        ViECapturer* ptrViECapture = is.Capture(captureId);
        if (ptrViECapture == NULL)
        {
            WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideo, ViEId(instance_id_),
                         "%s: Capture device %d doesn't exist", __FUNCTION__,
                         captureId);
            SetLastError(kViECaptureDeviceDoesNotExist);
            return -1;
        }

    }

    // Destroy the capture device
    return input_manager_.DestroyCaptureDevice(captureId);
}

// ============================================================================
// Pair capture device and channel
// ============================================================================

// ----------------------------------------------------------------------------
// ConnectCaptureDevice
//
// Connects a capture device with a channel, i.e. the capture video from this
// device will be sent to that channel. Serveral channels can be connectet to
// the same capture device.
// ----------------------------------------------------------------------------
int ViECaptureImpl::ConnectCaptureDevice(const int captureId,
                                         const int videoChannel)
{
    WEBRTC_TRACE(webrtc::kTraceApiCall, webrtc::kTraceVideo,
                 ViEId(instance_id_, videoChannel),
                 "%s(captureId: %d, videoChannel: %d)", __FUNCTION__, captureId,
                 videoChannel);

    ViEInputManagerScoped is(input_manager_);
    ViECapturer* ptrViECapture = is.Capture(captureId);
    if (ptrViECapture == NULL)
    {
        WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideo,
                     ViEId(instance_id_, videoChannel),
                     "%s: Capture device %d doesn't exist", __FUNCTION__,
                     captureId);
        SetLastError(kViECaptureDeviceDoesNotExist);
        return -1;
    }

    ViEChannelManagerScoped cs(channel_manager_);
    ViEEncoder* ptrViEEncoder = cs.Encoder(videoChannel);
    if (ptrViEEncoder == NULL)
    {
        WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideo,
                     ViEId(instance_id_, videoChannel),
                     "%s: Channel %d doesn't exist", __FUNCTION__,
                     videoChannel);
        SetLastError(kViECaptureDeviceInvalidChannelId);
        return -1;
    }
    //  Check if the encoder already has a connected frame provider
    if (is.FrameProvider(ptrViEEncoder) != NULL)
    {
        WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideo,
                     ViEId(instance_id_, videoChannel),
                     "%s: Channel %d already connected to a capture device.",
                     __FUNCTION__, videoChannel);
        SetLastError(kViECaptureDeviceAlreadyConnected);
        return -1;
    }
    VideoCodec codec;
    bool useHardwareEncoder = false;
    if (ptrViEEncoder->GetEncoder(codec) == 0)
    { // try to provide the encoder with preencoded frames if possible
        if (ptrViECapture->PreEncodeToViEEncoder(codec, *ptrViEEncoder,
                                                 videoChannel) == 0)
        {
            useHardwareEncoder = true;
        }
    }
    // If we don't use the camera as hardware encoder we register the vieEncoder
    // for callbacks
    if (!useHardwareEncoder
        && ptrViECapture->RegisterFrameCallback(videoChannel, ptrViEEncoder)
            != 0)
    {
        SetLastError(kViECaptureDeviceUnknownError);
        return -1;
    }
    return 0;
}

// ----------------------------------------------------------------------------
// DisconnectCaptureDevice
//
// Disconnects a capture device from a connected channel.
// ----------------------------------------------------------------------------
int ViECaptureImpl::DisconnectCaptureDevice(const int videoChannel)
{
    WEBRTC_TRACE(webrtc::kTraceApiCall, webrtc::kTraceVideo,
                 ViEId(instance_id_, videoChannel), "%s(videoChannel: %d)",
                 __FUNCTION__, videoChannel);

    ViEChannelManagerScoped cs(channel_manager_);
    ViEEncoder* ptrViEEncoder = cs.Encoder(videoChannel);
    if (ptrViEEncoder == NULL)
    {
        WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideo, ViEId(instance_id_),
                     "%s: Channel %d doesn't exist", __FUNCTION__,
                     videoChannel);
        SetLastError(kViECaptureDeviceInvalidChannelId);
        return -1;
    }

    ViEInputManagerScoped is(input_manager_);
    ViEFrameProviderBase* frameProvider = is.FrameProvider(ptrViEEncoder);
    if (!frameProvider)
    {
        WEBRTC_TRACE(webrtc::kTraceWarning, webrtc::kTraceVideo, ViEId(instance_id_),
                     "%s: No capture device connected to channel %d",
                     __FUNCTION__, videoChannel);
        SetLastError(kViECaptureDeviceNotConnected);
        return -1;
    }
    if (frameProvider->Id() < kViECaptureIdBase
        || frameProvider->Id() > kViECaptureIdMax)
    {
        WEBRTC_TRACE(webrtc::kTraceWarning, webrtc::kTraceVideo, ViEId(instance_id_),
                     "%s: No capture device connected to channel %d",
                     __FUNCTION__, videoChannel);
        SetLastError(kViECaptureDeviceNotConnected);
        return -1;
    }

    if (frameProvider->DeregisterFrameCallback(ptrViEEncoder) != 0)
    {
        SetLastError(kViECaptureDeviceUnknownError);
        return -1;
    }

    return 0;

}

// ============================================================================
// Start/stop
// ============================================================================

// ----------------------------------------------------------------------------
// StartCapture
//
// Starts an allocated capture device, i.e. will start output captured frame
// ----------------------------------------------------------------------------
int ViECaptureImpl::StartCapture(const int captureId,
                                 const CaptureCapability captureCapability)
{
    WEBRTC_TRACE(webrtc::kTraceApiCall, webrtc::kTraceVideo, ViEId(instance_id_),
                 "%s(captureId: %d)", __FUNCTION__, captureId);

    ViEInputManagerScoped is(input_manager_);
    ViECapturer* ptrViECapture = is.Capture(captureId);
    if (ptrViECapture == NULL)
    {
        WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideo,
                     ViEId(instance_id_, captureId),
                     "%s: Capture device %d doesn't exist", __FUNCTION__,
                     captureId);
        SetLastError(kViECaptureDeviceDoesNotExist);
        return -1;
    }
    if (ptrViECapture->Started())
    {
        SetLastError(kViECaptureDeviceAlreadyStarted);
        return -1;
    }
    if (ptrViECapture->Start(captureCapability) != 0)
    {
        SetLastError(kViECaptureDeviceUnknownError);
        return -1;
    }
    return 0;
}

// ----------------------------------------------------------------------------
// StopCapture
//
// Stops a started capture device
// ----------------------------------------------------------------------------
int ViECaptureImpl::StopCapture(const int captureId)
{
    WEBRTC_TRACE(webrtc::kTraceApiCall, webrtc::kTraceVideo, ViEId(instance_id_),
                 "%s(captureId: %d)", __FUNCTION__, captureId);

    ViEInputManagerScoped is(input_manager_);
    ViECapturer* ptrViECapture = is.Capture(captureId);
    if (ptrViECapture == NULL)
    {
        WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideo,
                     ViEId(instance_id_, captureId),
                     "%s: Capture device %d doesn't exist", __FUNCTION__,
                     captureId);
        SetLastError(kViECaptureDeviceDoesNotExist);
        return -1;
    }
    if (!ptrViECapture->Started())
    {
        SetLastError(kViECaptureDeviceNotStarted);
        return -1;
    }
    if (ptrViECapture->Stop() != 0)
    {
        SetLastError(kViECaptureDeviceUnknownError);
        return -1;
    }

    return 0;
}

// ----------------------------------------------------------------------------
// RotateCapturedFrames
//
// Rotates a frame as soon as it's delivered from the capture device.
// This will apply to mobile devices with accelerometers or other rotation
// detection abilities.
// ----------------------------------------------------------------------------
int ViECaptureImpl::SetRotateCapturedFrames(const int captureId,
                                            const RotateCapturedFrame rotation)
{
    int iRotation = -1;
    switch (rotation)
    {
        case RotateCapturedFrame_0:
            iRotation = 0;
            break;
        case RotateCapturedFrame_90:
            iRotation = 90;
            break;
        case RotateCapturedFrame_180:
            iRotation = 180;
            break;
        case RotateCapturedFrame_270:
            iRotation = 270;
            break;

    }
    WEBRTC_TRACE(webrtc::kTraceApiCall, webrtc::kTraceVideo, ViEId(instance_id_),
                 "%s(rotation: %d)", __FUNCTION__, iRotation);

    ViEInputManagerScoped is(input_manager_);
    ViECapturer* ptrViECapture = is.Capture(captureId);
    if (ptrViECapture == NULL)
    {
        WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideo,
                     ViEId(instance_id_, captureId),
                     "%s: Capture device %d doesn't exist", __FUNCTION__,
                     captureId);
        SetLastError(kViECaptureDeviceDoesNotExist);
        return -1;
    }
    if (ptrViECapture->SetRotateCapturedFrames(rotation) != 0)
    {
        SetLastError(kViECaptureDeviceUnknownError);
        return -1;
    }

    return 0;

}

// ----------------------------------------------------------------------------
// SetCaptureDelay
//
// Defines the capture delay for an external capture device.
// This call will also override a the capture delay value for a  capture
// device.
// ----------------------------------------------------------------------------
int ViECaptureImpl::SetCaptureDelay(const int captureId,
                                    const unsigned int captureDelayMs)
{
    WEBRTC_TRACE(webrtc::kTraceApiCall, webrtc::kTraceVideo, ViEId(instance_id_),
                 "%s(captureId: %d, captureDelayMs %u)", __FUNCTION__,
                 captureId, captureDelayMs);

    ViEInputManagerScoped is(input_manager_);
    ViECapturer* ptrViECapture = is.Capture(captureId);
    if (ptrViECapture == NULL)
    {
        WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideo,
                     ViEId(instance_id_, captureId),
                     "%s: Capture device %d doesn't exist", __FUNCTION__,
                     captureId);
        SetLastError(kViECaptureDeviceDoesNotExist);
        return -1;
    }

    if (ptrViECapture->SetCaptureDelay(captureDelayMs) != 0)
    {
        SetLastError(kViECaptureDeviceUnknownError);
        return -1;
    }
    return 0;

}

// ============================================================================
// Capture capabilities
// ============================================================================

// ----------------------------------------------------------------------------
// NumberOfCapabilities
//
// Returns the number of capabilities fot the specified device
// ----------------------------------------------------------------------------
int ViECaptureImpl::NumberOfCapabilities(const char* uniqueIdUTF8,
                                         const unsigned int uniqueIdUTF8Length)
{
    WEBRTC_TRACE(webrtc::kTraceApiCall, webrtc::kTraceVideo, ViEId(instance_id_),
                 "%s(captureDeviceName: %s)", __FUNCTION__, uniqueIdUTF8);

#if defined(WEBRTC_MAC_INTEL)
    // TODO: Move to capture module!
    // QTKit framework handles all capabilites and capture settings
    // automatically (mandatory).
    // Thus this function cannot be supported on the Mac platform.
    SetLastError(kViECaptureDeviceMacQtkitNotSupported);
    WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideo, ViEId(instance_id_),
                 "%s This API is not supported on Mac OS", __FUNCTION__,
                 instance_id_);
    return -1;
#endif

    if (!Initialized())
    {
        SetLastError(kViENotInitialized);
        WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideo, ViEId(instance_id_),
                     "%s - ViE instance %d not initialized", __FUNCTION__,
                     instance_id_);
        return -1;
    }

    return (int) input_manager_.NumberOfCaptureCapabilities(
        (WebRtc_UWord8*) uniqueIdUTF8);
}

// ----------------------------------------------------------------------------
// GetCaptureCapability
//
// Gets a capture capability for the specified capture device
// ----------------------------------------------------------------------------
int ViECaptureImpl::GetCaptureCapability(const char* uniqueIdUTF8,
                                         const unsigned int uniqueIdUTF8Length,
                                         const unsigned int capabilityNumber,
                                         CaptureCapability& capability)
{
    WEBRTC_TRACE(webrtc::kTraceApiCall, webrtc::kTraceVideo, ViEId(instance_id_),
                 "%s(captureDeviceName: %s)", __FUNCTION__, uniqueIdUTF8);

#if defined(WEBRTC_MAC_INTEL)
    // TODO: Move to capture module!
    // QTKit framework handles all capabilites and capture settings
    // automatically (mandatory).
    // Thus this function cannot be supported on the Mac platform.
    SetLastError(kViECaptureDeviceMacQtkitNotSupported);
    WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideo, ViEId(instance_id_),
                 "%s This API is not supported on Mac OS", __FUNCTION__,
                 instance_id_);
    return -1;
#endif

    if (!Initialized())
    {
        SetLastError(kViENotInitialized);
        WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideo, ViEId(instance_id_),
                     "%s - ViE instance %d not initialized", __FUNCTION__,
                     instance_id_);
        return -1;
    }
    if (input_manager_.GetCaptureCapability((WebRtc_UWord8*) uniqueIdUTF8,
                                           capabilityNumber, capability) != 0)
    {
        SetLastError(kViECaptureDeviceUnknownError);
        return -1;
    }
    return 0;
}

int ViECaptureImpl::ShowCaptureSettingsDialogBox(
    const char* uniqueIdUTF8, const unsigned int uniqueIdUTF8Length,
    const char* dialogTitle, void* parentWindow /*= NULL*/,
    const unsigned int x/*=200*/, const unsigned int y/*=200*/)
{
#if defined(WEBRTC_MAC_INTEL)
    // TODO: Move to capture module
    // QTKit framework handles all capabilites and capture settings
    // automatically (mandatory).
    // Thus this function cannot be supported on the Mac platform.
    SetLastError(kViECaptureDeviceMacQtkitNotSupported);
    WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideo, ViEId(instance_id_),
                 "%s This API is not supported on Mac OS", __FUNCTION__,
                 instance_id_);
    return -1;
#endif
    WEBRTC_TRACE(webrtc::kTraceApiCall, webrtc::kTraceVideo, ViEId(instance_id_),
                 "%s captureId (captureDeviceName: %s)", __FUNCTION__,
                 uniqueIdUTF8);

    return input_manager_.DisplayCaptureSettingsDialogBox(
        (WebRtc_UWord8*) uniqueIdUTF8, (WebRtc_UWord8*) dialogTitle,
        parentWindow, x, y);
}

int ViECaptureImpl::GetOrientation(const char* uniqueIdUTF8,
                                   RotateCapturedFrame &orientation)
{
    WEBRTC_TRACE(webrtc::kTraceApiCall, webrtc::kTraceVideo, ViEId(instance_id_),
                 "%s (captureDeviceName: %s)", __FUNCTION__, uniqueIdUTF8);

    if (!Initialized())
    {
        SetLastError(kViENotInitialized);
        WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideo, ViEId(instance_id_),
                     "%s - ViE instance %d not initialized", __FUNCTION__,
                     instance_id_);
        return -1;
    }
    if (input_manager_.GetOrientation((WebRtc_UWord8*) uniqueIdUTF8,
                                     orientation) != 0)
    {
        SetLastError(kViECaptureDeviceUnknownError);
        return -1;
    }
    return 0;
}

// ============================================================================
// Callbacks
// ============================================================================

// ----------------------------------------------------------------------------
// EnableBrightnessAlarm
//
// Enables brightness alarm callback for a specified capture device
// ----------------------------------------------------------------------------
int ViECaptureImpl::EnableBrightnessAlarm(const int captureId,
                                          const bool enable)
{
    ViEInputManagerScoped is(input_manager_);
    ViECapturer* ptrViECapture = is.Capture(captureId);
    if (ptrViECapture == NULL)
    {
        WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideo,
                     ViEId(instance_id_, captureId),
                     "%s: Capture device %d doesn't exist", __FUNCTION__,
                     captureId);
        SetLastError(kViECaptureDeviceDoesNotExist);
        return -1;
    }
    if (ptrViECapture->EnableBrightnessAlarm(enable) != 0)
    {
        SetLastError(kViECaptureDeviceUnknownError);
        return -1;
    }
    return 0;
}

// ----------------------------------------------------------------------------
// RegisterObserver
//
// Register the customer implemented observer for capture callbacks
// ----------------------------------------------------------------------------
int ViECaptureImpl::RegisterObserver(const int captureId,
                                     ViECaptureObserver& observer)
{
    ViEInputManagerScoped is(input_manager_);
    ViECapturer* ptrViECapture = is.Capture(captureId);
    if (ptrViECapture == NULL)
    {
        WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideo,
                     ViEId(instance_id_, captureId),
                     "%s: Capture device %d doesn't exist", __FUNCTION__,
                     captureId);
        SetLastError(kViECaptureDeviceDoesNotExist);
        return -1;
    }
    if (ptrViECapture->IsObserverRegistered())
    {
        WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideo,
                     ViEId(instance_id_, captureId),
                     "%s: Observer already registered", __FUNCTION__);
        SetLastError(kViECaptureObserverAlreadyRegistered);
        return -1;
    }

    if (ptrViECapture->RegisterObserver(observer) != 0)
    {
        SetLastError(kViECaptureDeviceUnknownError);
        return -1;
    }
    return 0;
}

// ----------------------------------------------------------------------------
// DeregisterObserver
//
// Removes the previously registered observer
// ----------------------------------------------------------------------------
int ViECaptureImpl::DeregisterObserver(const int captureId)
{
    ViEInputManagerScoped is(input_manager_);
    ViECapturer* ptrViECapture = is.Capture(captureId);
    if (ptrViECapture == NULL)
    {
        WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideo,
                     ViEId(instance_id_, captureId),
                     "%s: Capture device %d doesn't exist", __FUNCTION__,
                     captureId);
        SetLastError(kViECaptureDeviceDoesNotExist);
        return -1;
    }
    if (!ptrViECapture->IsObserverRegistered())
    {
        SetLastError(kViECaptureDeviceObserverNotRegistered);
        return -1;
    }

    if (ptrViECapture->DeRegisterObserver() != 0)
    {
        SetLastError(kViECaptureDeviceUnknownError);
        return -1;
    }
    return 0;
}
} // namespace webrtc
