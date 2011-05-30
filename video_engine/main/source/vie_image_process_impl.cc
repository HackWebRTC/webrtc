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
 * vie_image_process_impl.cpp
 */
#include "vie_image_process_impl.h"

// Defines
#include "vie_defines.h"

#include "trace.h"
#include "vie_errors.h"
#include "vie_impl.h"
#include "vie_channel.h"
#include "vie_channel_manager.h"
#include "vie_encoder.h"
#include "vie_input_manager.h"
#include "vie_capturer.h"

namespace webrtc
{

// ----------------------------------------------------------------------------
// GetInterface
// ----------------------------------------------------------------------------

ViEImageProcess* ViEImageProcess::GetInterface(VideoEngine* videoEngine)
{
#ifdef WEBRTC_VIDEO_ENGINE_IMAGE_PROCESS_API
    if (videoEngine == NULL)
    {
        return NULL;
    }
    VideoEngineImpl* vieImpl = reinterpret_cast<VideoEngineImpl*> (videoEngine);
    ViEImageProcessImpl* vieImageProcessImpl = vieImpl;
    (*vieImageProcessImpl)++; // Increase ref count

    return vieImageProcessImpl;
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

int ViEImageProcessImpl::Release()
{
    WEBRTC_TRACE(webrtc::kTraceApiCall, webrtc::kTraceVideo, _instanceId,
               "ViEImageProcess::Release()");
    (*this)--; // Decrease ref count

    WebRtc_Word32 refCount = GetCount();
    if (refCount < 0)
    {
        WEBRTC_TRACE(webrtc::kTraceWarning, webrtc::kTraceVideo, _instanceId,
                   "ViEImageProcess release too many times");
        SetLastError(kViEAPIDoesNotExist);
        return -1;
    }
    WEBRTC_TRACE(webrtc::kTraceInfo, webrtc::kTraceVideo, _instanceId,
               "ViEImageProcess reference count: %d", refCount);
    return refCount;
}

// ----------------------------------------------------------------------------
// Constructor
// ----------------------------------------------------------------------------

ViEImageProcessImpl::ViEImageProcessImpl()
{
    WEBRTC_TRACE(webrtc::kTraceMemory, webrtc::kTraceVideo, _instanceId,
               "ViEImageProcessImpl::ViEImageProcessImpl() Ctor");
}

// ----------------------------------------------------------------------------
// Destructor
// ----------------------------------------------------------------------------

ViEImageProcessImpl::~ViEImageProcessImpl()
{
    WEBRTC_TRACE(webrtc::kTraceMemory, webrtc::kTraceVideo, _instanceId,
               "ViEImageProcessImpl::~ViEImageProcessImpl() Dtor");
}

// ============================================================================
// Effect filter
// ============================================================================

// ----------------------------------------------------------------------------
// RegisterCaptureEffectFilter
//
// Registers an effect filter for a capture device
// ----------------------------------------------------------------------------

int ViEImageProcessImpl::RegisterCaptureEffectFilter(
    const int captureId, ViEEffectFilter& captureFilter)
{
    WEBRTC_TRACE(webrtc::kTraceApiCall, webrtc::kTraceVideo, ViEId(_instanceId),
               "%s(captureId: %d)", __FUNCTION__, captureId);
    if (!IsInitialized())
    {
        SetLastError(kViENotInitialized);
        WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideo, ViEId(_instanceId),
                   "%s - ViE instance %d not initialized", __FUNCTION__,
                   _instanceId);
        return -1;
    }

    ViEInputManagerScoped is(_inputManager);
    ViECapturer* vieCapture = is.Capture(captureId);
    if (vieCapture == NULL)
    {
        WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideo, ViEId(_instanceId),
                   "%s: Capture device %d doesn't exist", __FUNCTION__,
                   captureId);
        SetLastError(kViEImageProcessInvalidCaptureId);
        return -1;
    }

    if (vieCapture->RegisterEffectFilter(&captureFilter) != 0)
    {
        SetLastError(kViEImageProcessFilterExists);
        return -1;
    }
    return 0;
}

// ----------------------------------------------------------------------------
// DeregisterCaptureEffectFilter
//
// Deregisters a previously set fffect filter
// ----------------------------------------------------------------------------

int ViEImageProcessImpl::DeregisterCaptureEffectFilter(const int captureId)
{
    WEBRTC_TRACE(webrtc::kTraceApiCall, webrtc::kTraceVideo, ViEId(_instanceId),
               "%s(captureId: %d)", __FUNCTION__, captureId);

    ViEInputManagerScoped is(_inputManager);
    ViECapturer* vieCapture = is.Capture(captureId);
    if (vieCapture == NULL)
    {
        WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideo, ViEId(_instanceId),
                   "%s: Capture device %d doesn't exist", __FUNCTION__,
                   captureId);
        SetLastError(kViEImageProcessInvalidCaptureId);
        return -1;
    }
    if (vieCapture->RegisterEffectFilter(NULL) != 0)
    {
        SetLastError(kViEImageProcessFilterDoesNotExist);
        return -1;
    }
    return 0;
}

// ----------------------------------------------------------------------------
// RegisterSendEffectFilter
//
// Registers an effect filter for a channel
// ----------------------------------------------------------------------------

int ViEImageProcessImpl::RegisterSendEffectFilter(const int videoChannel,
                                                  ViEEffectFilter& sendFilter)
{
    WEBRTC_TRACE(webrtc::kTraceApiCall, webrtc::kTraceVideo, ViEId(_instanceId),
               "%s(videoChannel: %d)", __FUNCTION__, videoChannel);

    ViEChannelManagerScoped cs(_channelManager);
    ViEEncoder* vieEncoder = cs.Encoder(videoChannel);
    if (vieEncoder == NULL)
    {
        WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideo, ViEId(_instanceId),
                   "%s: Channel %d doesn't exist", __FUNCTION__, videoChannel);
        SetLastError(kViEImageProcessInvalidChannelId);
        return -1;
    }

    if (vieEncoder->RegisterEffectFilter(&sendFilter) != 0)
    {
        SetLastError(kViEImageProcessFilterExists);
        return -1;
    }
    return 0;
}

// ----------------------------------------------------------------------------
// DeregisterSendEffectFilter
//
// Deregisters a previously set effect filter
// ----------------------------------------------------------------------------

int ViEImageProcessImpl::DeregisterSendEffectFilter(const int videoChannel)
{
    WEBRTC_TRACE(webrtc::kTraceApiCall, webrtc::kTraceVideo, ViEId(_instanceId),
               "%s(videoChannel: %d)", __FUNCTION__, videoChannel);

    ViEChannelManagerScoped cs(_channelManager);
    ViEEncoder* vieEncoder = cs.Encoder(videoChannel);
    if (vieEncoder == NULL)
    {
        WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideo, ViEId(_instanceId),
                   "%s: Channel %d doesn't exist", __FUNCTION__, videoChannel);
        SetLastError(kViEImageProcessInvalidChannelId);
        return -1;
    }
    if (vieEncoder->RegisterEffectFilter(NULL) != 0)
    {
        SetLastError(kViEImageProcessFilterDoesNotExist);
        return -1;
    }
    return 0;
}

// ----------------------------------------------------------------------------
// RegisterRenderEffectFilter
//
// Registers an effect filter for an incoming decoded stream
// ----------------------------------------------------------------------------

int ViEImageProcessImpl::RegisterRenderEffectFilter(
    const int videoChannel, ViEEffectFilter& renderFilter)
{
    WEBRTC_TRACE(webrtc::kTraceApiCall, webrtc::kTraceVideo, ViEId(_instanceId),
               "%s(videoChannel: %d)", __FUNCTION__, videoChannel);

    ViEChannelManagerScoped cs(_channelManager);
    ViEChannel* vieChannel = cs.Channel(videoChannel);
    if (vieChannel == NULL)
    {
        WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideo, ViEId(_instanceId),
                   "%s: Channel %d doesn't exist", __FUNCTION__, videoChannel);
        SetLastError(kViEImageProcessInvalidChannelId);
        return -1;
    }

    if (vieChannel->RegisterEffectFilter(&renderFilter) != 0)
    {
        SetLastError(kViEImageProcessFilterExists);
        return -1;
    }
    return 0;
}

// ----------------------------------------------------------------------------
// DeregisterRenderEffectFilter
//
// ----------------------------------------------------------------------------

int ViEImageProcessImpl::DeregisterRenderEffectFilter(const int videoChannel)
{
    WEBRTC_TRACE(webrtc::kTraceApiCall, webrtc::kTraceVideo, ViEId(_instanceId),
               "%s(videoChannel: %d)", __FUNCTION__, videoChannel);

    ViEChannelManagerScoped cs(_channelManager);
    ViEChannel* vieChannel = cs.Channel(videoChannel);
    if (vieChannel == NULL)
    {
        WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideo, ViEId(_instanceId),
                   "%s: Channel %d doesn't exist", __FUNCTION__, videoChannel);
        SetLastError(kViEImageProcessInvalidChannelId);
        return -1;
    }

    if (vieChannel->RegisterEffectFilter(NULL) != 0)
    {
        SetLastError(kViEImageProcessFilterDoesNotExist);
        return -1;
    }
    return 0;
}

// ============================================================================
// Image enhancement
// ============================================================================

// ----------------------------------------------------------------------------
// EnableDeflickering
//
// Enables/disables deflickering of the captured image.
// ----------------------------------------------------------------------------

int ViEImageProcessImpl::EnableDeflickering(const int captureId,
                                            const bool enable)
{
    WEBRTC_TRACE(webrtc::kTraceApiCall, webrtc::kTraceVideo, ViEId(_instanceId),
               "%s(captureId: %d, enable: %d)", __FUNCTION__, captureId, enable);

    ViEInputManagerScoped is(_inputManager);
    ViECapturer* vieCapture = is.Capture(captureId);
    if (vieCapture == NULL)
    {
        WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideo, ViEId(_instanceId),
                   "%s: Capture device %d doesn't exist", __FUNCTION__,
                   captureId);
        SetLastError(kViEImageProcessInvalidChannelId);
        return -1;
    }

    if (vieCapture->EnableDeflickering(enable) != 0)
    {
        if (enable)
            SetLastError(kViEImageProcessAlreadyEnabled);
        else
        {
            SetLastError(kViEImageProcessAlreadyDisabled);
        }
        return -1;
    }
    return 0;
}

// ----------------------------------------------------------------------------
// EnableDenoising
//
// Enables/disables denoising of the captured image.
// ----------------------------------------------------------------------------

int ViEImageProcessImpl::EnableDenoising(const int captureId, const bool enable)
{
    WEBRTC_TRACE(webrtc::kTraceApiCall, webrtc::kTraceVideo, ViEId(_instanceId),
               "%s(captureId: %d, enable: %d)", __FUNCTION__, captureId, enable);

    ViEInputManagerScoped is(_inputManager);
    ViECapturer* vieCapture = is.Capture(captureId);
    if (vieCapture == NULL)
    {
        WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideo, ViEId(_instanceId),
                   "%s: Capture device %d doesn't exist", __FUNCTION__,
                   captureId);
        SetLastError(kViEImageProcessInvalidCaptureId);
        return -1;
    }

    if (vieCapture->EnableDenoising(enable) != 0)
    {
        if (enable)
            SetLastError(kViEImageProcessAlreadyEnabled);
        else
        {
            SetLastError(kViEImageProcessAlreadyDisabled);
        }
        return -1;
    }
    return 0;

}

// ----------------------------------------------------------------------------
// EnableColorEnhancement
//
// Enables coloe enhancement for decoded images
// ----------------------------------------------------------------------------

int ViEImageProcessImpl::EnableColorEnhancement(const int videoChannel,
                                                const bool enable)
{
    WEBRTC_TRACE(webrtc::kTraceApiCall, webrtc::kTraceVideo, ViEId(_instanceId),
               "%s(videoChannel: %d, enable: %d)", __FUNCTION__, videoChannel,
               enable);

    ViEChannelManagerScoped cs(_channelManager);
    ViEChannel* vieChannel = cs.Channel(videoChannel);
    if (vieChannel == NULL)
    {
        WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideo, ViEId(_instanceId),
                   "%s: Channel %d doesn't exist", __FUNCTION__, videoChannel);
        SetLastError(kViEImageProcessInvalidChannelId);
        return -1;
    }
    if (vieChannel->EnableColorEnhancement(enable) != 0)
    {
        if (enable)
            SetLastError(kViEImageProcessAlreadyEnabled);
        else
        {
            SetLastError(kViEImageProcessAlreadyDisabled);
        }
        return -1;
    }
    return 0;
}
} // namespace webrtc
