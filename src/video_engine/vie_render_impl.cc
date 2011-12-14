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
 * vie_render_impl.cc
 */

#include "vie_render_impl.h"

// Defines
#include "engine_configurations.h"
#include "vie_defines.h"

#include "trace.h"
#include "video_render.h"
#include "video_render_defines.h"
#include "vie_errors.h"
#include "vie_impl.h"
#include "vie_capturer.h"
#include "vie_channel.h"
#include "vie_frame_provider_base.h"
#include "vie_channel_manager.h"
#include "vie_input_manager.h"
#include "vie_render_manager.h"
#include "video_engine/vie_renderer.h"

namespace webrtc
{

// ----------------------------------------------------------------------------
// GetInterface
// ----------------------------------------------------------------------------

ViERender* ViERender::GetInterface(VideoEngine* videoEngine)
{
#ifdef WEBRTC_VIDEO_ENGINE_RENDER_API
    if (videoEngine == NULL)
    {
        return NULL;
    }
    VideoEngineImpl* vieImpl = reinterpret_cast<VideoEngineImpl*> (videoEngine);
    ViERenderImpl* vieRenderImpl = vieImpl;
    (*vieRenderImpl)++; // Increase ref count

    return vieRenderImpl;
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

int ViERenderImpl::Release()
{
    WEBRTC_TRACE(webrtc::kTraceApiCall, webrtc::kTraceVideo, instance_id_,
                 "ViERender::Release()");
    (*this)--; // Decrease ref count

    WebRtc_Word32 refCount = GetCount();
    if (refCount < 0)
    {
        WEBRTC_TRACE(webrtc::kTraceWarning, webrtc::kTraceVideo, instance_id_,
                     "ViERender release too many times");
        // SetLastError()
        return -1;
    }
    WEBRTC_TRACE(webrtc::kTraceInfo, webrtc::kTraceVideo, instance_id_,
                 "ViERender reference count: %d", refCount);
    return refCount;
}

// ----------------------------------------------------------------------------
// Constructor
// ----------------------------------------------------------------------------

ViERenderImpl::ViERenderImpl()
{
    WEBRTC_TRACE(webrtc::kTraceMemory, webrtc::kTraceVideo, instance_id_,
                 "ViERenderImpl::ViERenderImpl() Ctor");
}

// ----------------------------------------------------------------------------
// Destructor
// ----------------------------------------------------------------------------

ViERenderImpl::~ViERenderImpl()
{
    WEBRTC_TRACE(webrtc::kTraceMemory, webrtc::kTraceVideo, instance_id_,
                 "ViERenderImpl::~ViERenderImpl() Dtor");
}

// ============================================================================
// Registration of render module
// ============================================================================

// ----------------------------------------------------------------------------
// RegisterVideoRenderModule
//
// Registers a video render module, must be called before
// AddRenderer is called for an input stream associated
// with the same window as the module.
// ----------------------------------------------------------------------------

int ViERenderImpl::RegisterVideoRenderModule(
    VideoRender& renderModule)
{
    WEBRTC_TRACE(webrtc::kTraceApiCall, webrtc::kTraceVideo, ViEId(instance_id_),
                 "%s (&renderModule: %p)", __FUNCTION__, &renderModule);

    if (render_manager_.RegisterVideoRenderModule(renderModule) != 0)
    {
        // Error logging is done in RegisterVideoRenderModule
        SetLastError(kViERenderUnknownError);
        return -1;
    }

    return 0;
}

// ----------------------------------------------------------------------------
// DeRegisterVideoRenderModule
//
// De-registers a video render module, must be called after
// RemoveRenderer has been called for all input streams associated
// with the same window as the module.
// ----------------------------------------------------------------------------

int ViERenderImpl::DeRegisterVideoRenderModule(
    VideoRender& renderModule)
{
    WEBRTC_TRACE(webrtc::kTraceApiCall, webrtc::kTraceVideo, ViEId(instance_id_),
                 "%s (&renderModule: %p)", __FUNCTION__, &renderModule);
    if (render_manager_.DeRegisterVideoRenderModule(renderModule) != 0)
    {
        // Error logging is done in DeRegisterVideoRenderModule
        SetLastError(kViERenderUnknownError);
        return -1;
    }

    return 0;
}

// ============================================================================
// Add renderer
// ============================================================================

int ViERenderImpl::AddRenderer(const int renderId, void* window,
                               const unsigned int zOrder, const float left,
                               const float top, const float right,
                               const float bottom)
{
    WEBRTC_TRACE(webrtc::kTraceApiCall, webrtc::kTraceVideo, ViEId(instance_id_),
                 "%s (renderId: %d,  window: 0x%p, zOrder: %u, left: %f, "
                 "top: %f, right: %f, bottom: %f)",
                 __FUNCTION__, renderId, window, zOrder, left, top, right,
                 bottom);
    if (!Initialized())
    {
        SetLastError(kViENotInitialized);
        WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideo, ViEId(instance_id_),
                     "%s - ViE instance %d not initialized", __FUNCTION__,
                     instance_id_);
        return -1;
    }

    { // Check if the renderer exist already
        ViERenderManagerScoped rs(render_manager_);
        if (rs.Renderer(renderId) != NULL)
        {
            WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideo, ViEId(instance_id_),
                         "%s - Renderer already exist %d.", __FUNCTION__,
                         renderId);
            SetLastError(kViERenderAlreadyExists);
            return -1;
        }
    }

    if (renderId >= kViEChannelIdBase && renderId <= kViEChannelIdMax)
    {
        // This is a channel
        ViEChannelManagerScoped cm(channel_manager_);
        ViEFrameProviderBase* frameProvider = cm.Channel(renderId);
        if (frameProvider == NULL)
        {
            WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideo, ViEId(instance_id_),
                         "%s: FrameProvider id %d doesn't exist", __FUNCTION__,
                         renderId);
            SetLastError(kViERenderInvalidRenderId);
            return -1;
        }
        ViERenderer* renderer = render_manager_.AddRenderStream(renderId,
                                                               window, zOrder,
                                                               left, top,
                                                               right, bottom);
        if (renderer == NULL)
        {
            SetLastError(kViERenderUnknownError);
            return -1;
        }
        return frameProvider->RegisterFrameCallback(renderId, renderer);
    }
    else // camera or file
    {
        ViEInputManagerScoped is(input_manager_);
        ViEFrameProviderBase* frameProvider = is.FrameProvider(renderId);
        if (frameProvider == NULL)
        {
            WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideo, ViEId(instance_id_),
                         "%s: FrameProvider id %d doesn't exist", __FUNCTION__,
                         renderId);
            SetLastError(kViERenderInvalidRenderId);
            return -1;
        }
        ViERenderer* renderer = render_manager_.AddRenderStream(renderId,
                                                               window, zOrder,
                                                               left, top,
                                                               right, bottom);
        if (renderer == NULL)
        {
            SetLastError(kViERenderUnknownError);
            return -1;
        }
        return frameProvider->RegisterFrameCallback(renderId, renderer);
    }
    SetLastError(kViERenderInvalidRenderId);
    return -1;

}

int ViERenderImpl::RemoveRenderer(const int renderId)
{
    WEBRTC_TRACE(webrtc::kTraceApiCall, webrtc::kTraceVideo, ViEId(instance_id_),
                 "%s(renderId: %d)", __FUNCTION__, renderId);
    if (!Initialized())
    {
        SetLastError(kViENotInitialized);
        WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideo, ViEId(instance_id_),
                     "%s - ViE instance %d not initialized", __FUNCTION__,
                     instance_id_);
        return -1;
    }

    ViERenderer* renderer = NULL;
    {
        ViERenderManagerScoped rs(render_manager_);
        renderer = rs.Renderer(renderId);
        if (!renderer)
        {
            WEBRTC_TRACE(webrtc::kTraceWarning, webrtc::kTraceVideo, ViEId(instance_id_),
                         "%s No render exist with renderId: %d", __FUNCTION__,
                         renderId);
            SetLastError(kViERenderInvalidRenderId);
            return -1;
        }
    } // Leave the scope lock since we don't want to lock two managers
      // simultanousely

    if (renderId >= kViEChannelIdBase && renderId <= kViEChannelIdMax)
    {
        // This is a channel
        ViEChannelManagerScoped cm(channel_manager_);
        ViEChannel* channel = cm.Channel(renderId);
        if (!channel)
        {
            WEBRTC_TRACE(webrtc::kTraceWarning, webrtc::kTraceVideo, ViEId(instance_id_),
                         "%s: no channel with id %d exists ", __FUNCTION__,
                         renderId);
            SetLastError(kViERenderInvalidRenderId);
            return -1;
        }
        channel->DeregisterFrameCallback(renderer);
    }
    else //Provider owned by inputmanager - ie file or capture device
    {
        ViEInputManagerScoped is(input_manager_);
        ViEFrameProviderBase* provider = is.FrameProvider(renderId);
        if (!provider)
        {
            WEBRTC_TRACE(webrtc::kTraceWarning, webrtc::kTraceVideo, ViEId(instance_id_),
                         "%s: no provider with id %d exists ", __FUNCTION__,
                         renderId);
            SetLastError(kViERenderInvalidRenderId);
            return -1;
        }
        provider->DeregisterFrameCallback(renderer);

    }
    if (render_manager_.RemoveRenderStream(renderId) != 0)
    {
        SetLastError(kViERenderUnknownError);
        return -1;
    }
    return 0;
}

// ============================================================================
// Start/stop
// ============================================================================

// ----------------------------------------------------------------------------
// StartRender
//
// Starts rendering the stream from the channel
// ----------------------------------------------------------------------------

int ViERenderImpl::StartRender(const int renderId)
{
    WEBRTC_TRACE(webrtc::kTraceApiCall, webrtc::kTraceVideo,
                 ViEId(instance_id_, renderId), "%s(channel: %d)", __FUNCTION__,
                 renderId);

    ViERenderManagerScoped rs(render_manager_);
    ViERenderer* ptrRender = rs.Renderer(renderId);
    if (ptrRender == NULL)
    {
        // No renderer for this channel
        WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideo,
                     ViEId(instance_id_, renderId),
                     "%s: No renderer with render Id %d exist.", __FUNCTION__,
                     renderId);
        SetLastError(kViERenderInvalidRenderId);
        return -1;
    }

    if (ptrRender->StartRender() != 0)
    {
        SetLastError(kViERenderUnknownError);
        return -1;
    }
    return 0;
}

// ----------------------------------------------------------------------------
// StopRender
//
// Stop rendering a stream
// ----------------------------------------------------------------------------

int ViERenderImpl::StopRender(const int renderId)
{
    WEBRTC_TRACE(webrtc::kTraceApiCall, webrtc::kTraceVideo,
                 ViEId(instance_id_, renderId), "%s(channel: %d)", __FUNCTION__,
                 renderId);

    ViERenderManagerScoped rs(render_manager_);
    ViERenderer* ptrRender = rs.Renderer(renderId);
    if (ptrRender == NULL)
    {
        // No renderer for this channel
        WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideo,
                     ViEId(instance_id_, renderId),
                     "%s: No renderer with renderId %d exist.", __FUNCTION__,
                     renderId);
        SetLastError(kViERenderInvalidRenderId);
        return -1;
    }
    if (ptrRender->StopRender() != 0)
    {
        SetLastError(kViERenderUnknownError);
        return -1;
    }
    return 0;
}

// ============================================================================
// Stream configurations
// ============================================================================

// ----------------------------------------------------------------------------
// ConfigureRender
//
// Reconfigures an already added render stream
// ----------------------------------------------------------------------------

int ViERenderImpl::ConfigureRender(int renderId, const unsigned int zOrder,
                                   const float left, const float top,
                                   const float right, const float bottom)
{
    WEBRTC_TRACE(webrtc::kTraceApiCall, webrtc::kTraceVideo, ViEId(instance_id_, renderId),
                 "%s(channel: %d)", __FUNCTION__, renderId);

    ViERenderManagerScoped rs(render_manager_);
    ViERenderer* ptrRender = rs.Renderer(renderId);
    if (ptrRender == NULL)
    {
        // No renderer for this channel
        WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideo,
                     ViEId(instance_id_, renderId),
                     "%s: No renderer with renderId %d exist.", __FUNCTION__,
                     renderId);
        SetLastError(kViERenderInvalidRenderId);
        return -1;
    }

    if (ptrRender->ConfigureRenderer(zOrder, left, top, right, bottom) != 0)
    {
        SetLastError(kViERenderUnknownError);
        return -1;
    }
    return 0;
}

// ----------------------------------------------------------------------------
// MirrorRenderStream
//
// Enables mirror rendering
// ----------------------------------------------------------------------------

int ViERenderImpl::MirrorRenderStream(const int renderId, const bool enable,
                                      const bool mirrorXAxis,
                                      const bool mirrorYAxis)
{
    ViERenderManagerScoped rs(render_manager_);
    ViERenderer* ptrRender = rs.Renderer(renderId);
    if (ptrRender == NULL)
    {
        // No renderer for this channel
        WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideo,
                     ViEId(instance_id_, renderId),
                     "%s: No renderer with renderId %d exist.", __FUNCTION__,
                     renderId);
        SetLastError(kViERenderInvalidRenderId);
        return -1;
    }
    if (ptrRender->EnableMirroring(renderId, enable, mirrorXAxis, mirrorYAxis)
        != 0)
    {
        SetLastError(kViERenderUnknownError);
        return -1;
    }
    return 0;
}

// ============================================================================
// External render
// ============================================================================


// ----------------------------------------------------------------------------
// 
//
// AddRenderer
// ----------------------------------------------------------------------------

int ViERenderImpl::AddRenderer(const int renderId,
                               webrtc::RawVideoType videoInputFormat,
                               ExternalRenderer* externalRenderer)
{
    // check if the client requested a format that we can convert the frames to
    if (videoInputFormat != webrtc::kVideoI420
        && videoInputFormat != webrtc::kVideoYV12
        && videoInputFormat != webrtc::kVideoYUY2
        && videoInputFormat != webrtc::kVideoUYVY
        && videoInputFormat != webrtc::kVideoARGB
        && videoInputFormat != webrtc::kVideoRGB24
        && videoInputFormat != webrtc::kVideoRGB565
        && videoInputFormat != webrtc::kVideoARGB4444
        && videoInputFormat != webrtc::kVideoARGB1555)
    {
        WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideo,
                     ViEId(instance_id_, renderId),
                     "%s: Unsupported video frame format requested",
                     __FUNCTION__, renderId);
        SetLastError(kViERenderInvalidFrameFormat);
        return -1;
    }

    if (!Initialized())
    {
        SetLastError(kViENotInitialized);
        WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideo, ViEId(instance_id_),
                     "%s - ViE instance %d not initialized", __FUNCTION__,
                     instance_id_);
        return -1;
    }

    { // Check if the renderer exist already
        ViERenderManagerScoped rs(render_manager_);
        if (rs.Renderer(renderId) != NULL)
        {
            WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideo, ViEId(instance_id_),
                         "%s - Renderer already exist %d.", __FUNCTION__,
                         renderId);
            SetLastError(kViERenderAlreadyExists);
            return -1;
        }
    }

    if (renderId >= kViEChannelIdBase && renderId <= kViEChannelIdMax)
    {
        // This is a channel
        ViEChannelManagerScoped cm(channel_manager_);
        ViEFrameProviderBase* frameProvider = cm.Channel(renderId);
        if (frameProvider == NULL)
        {
            WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideo, ViEId(instance_id_),
                         "%s: FrameProvider id %d doesn't exist", __FUNCTION__,
                         renderId);
            SetLastError(kViERenderInvalidRenderId);
            return -1;
        }
        ViERenderer* ptrRender = render_manager_.AddRenderStream(renderId, NULL,
                                                                0, 0.0f, 0.0f,
                                                                1.0f, 1.0f);
        if (ptrRender == NULL)
        {
            SetLastError(kViERenderUnknownError);
            return -1;
        }
        if (-1 == ptrRender->SetExternalRenderer(renderId, videoInputFormat,
                                                 externalRenderer))
        {
            SetLastError(kViERenderUnknownError);
            return -1;
        }

        return frameProvider->RegisterFrameCallback(renderId, ptrRender);
    }
    else // camera or file
    {
        ViEInputManagerScoped is(input_manager_);
        ViEFrameProviderBase* frameProvider = is.FrameProvider(renderId);
        if (frameProvider == NULL)
        {
            WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideo, ViEId(instance_id_),
                         "%s: FrameProvider id %d doesn't exist", __FUNCTION__,
                         renderId);
            SetLastError(kViERenderInvalidRenderId);
            return -1;
        }
        ViERenderer* ptrRender = render_manager_.AddRenderStream(renderId, NULL,
                                                                0, 0.0f, 0.0f,
                                                                1.0f, 1.0f);
        if (ptrRender == NULL)
        {
            SetLastError(kViERenderUnknownError);
            return -1;
        }
        if (-1 == ptrRender->SetExternalRenderer(renderId, videoInputFormat,
                                                 externalRenderer))
        {
            SetLastError(kViERenderUnknownError);
            return -1;
        }
        return frameProvider->RegisterFrameCallback(renderId, ptrRender);
    }
    SetLastError(kViERenderInvalidRenderId);
    return -1;

}

} // namespace webrtc
