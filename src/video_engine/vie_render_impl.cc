/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "video_engine/vie_render_impl.h"

#include "engine_configurations.h"
#include "modules/video_render/main/interface/video_render.h"
#include "modules/video_render/main/interface/video_render_defines.h"
#include "system_wrappers/interface/trace.h"
#include "video_engine/main/interface/vie_errors.h"
#include "video_engine/vie_capturer.h"
#include "video_engine/vie_channel.h"
#include "video_engine/vie_channel_manager.h"
#include "video_engine/vie_defines.h"
#include "video_engine/vie_frame_provider_base.h"
#include "video_engine/vie_impl.h"
#include "video_engine/vie_input_manager.h"
#include "video_engine/vie_render_manager.h"
#include "video_engine/vie_renderer.h"

namespace webrtc {

ViERender* ViERender::GetInterface(VideoEngine* video_engine) {
#ifdef WEBRTC_VIDEO_ENGINE_RENDER_API
  if (!video_engine) {
    return NULL;
  }
  VideoEngineImpl* vie_impl = reinterpret_cast<VideoEngineImpl*>(video_engine);
  ViERenderImpl* vie_render_impl = vie_impl;
  // Increase ref count.
  (*vie_render_impl)++;
  return vie_render_impl;
#else
  return NULL;
#endif
}

int ViERenderImpl::Release() {
  WEBRTC_TRACE(kTraceApiCall, kTraceVideo, instance_id_,
               "ViERender::Release()");
  // Decrease ref count
  (*this)--;
  WebRtc_Word32 ref_count = GetCount();
  if (ref_count < 0) {
    WEBRTC_TRACE(kTraceWarning, kTraceVideo, instance_id_,
                 "ViERender release too many times");
    return -1;
  }
  WEBRTC_TRACE(kTraceInfo, kTraceVideo, instance_id_,
               "ViERender reference count: %d", ref_count);
  return ref_count;
}

ViERenderImpl::ViERenderImpl() {
  WEBRTC_TRACE(kTraceMemory, kTraceVideo, instance_id_,
               "ViERenderImpl::ViERenderImpl() Ctor");
}

ViERenderImpl::~ViERenderImpl() {
  WEBRTC_TRACE(kTraceMemory, kTraceVideo, instance_id_,
               "ViERenderImpl::~ViERenderImpl() Dtor");
}

int ViERenderImpl::RegisterVideoRenderModule(
  VideoRender& render_module) {
  WEBRTC_TRACE(kTraceApiCall, kTraceVideo, ViEId(instance_id_),
               "%s (&render_module: %p)", __FUNCTION__, &render_module);
  if (render_manager_.RegisterVideoRenderModule(render_module) != 0) {
    SetLastError(kViERenderUnknownError);
    return -1;
  }
  return 0;
}

int ViERenderImpl::DeRegisterVideoRenderModule(
  VideoRender& render_module) {
  WEBRTC_TRACE(kTraceApiCall, kTraceVideo, ViEId(instance_id_),
               "%s (&render_module: %p)", __FUNCTION__, &render_module);
  if (render_manager_.DeRegisterVideoRenderModule(render_module) != 0) {
    // Error logging is done in ViERenderManager::DeRegisterVideoRenderModule.
    SetLastError(kViERenderUnknownError);
    return -1;
  }
  return 0;
}

int ViERenderImpl::AddRenderer(const int render_id, void* window,
                               const unsigned int z_order, const float left,
                               const float top, const float right,
                               const float bottom) {
  WEBRTC_TRACE(kTraceApiCall, kTraceVideo, ViEId(instance_id_),
               "%s (render_id: %d,  window: 0x%p, z_order: %u, left: %f, "
               "top: %f, right: %f, bottom: %f)",
               __FUNCTION__, render_id, window, z_order, left, top, right,
               bottom);
  if (!Initialized()) {
    SetLastError(kViENotInitialized);
    WEBRTC_TRACE(kTraceError, kTraceVideo, ViEId(instance_id_),
                 "%s - ViE instance %d not initialized", __FUNCTION__,
                 instance_id_);
    return -1;
  }
  {
    ViERenderManagerScoped rs(render_manager_);
    if (rs.Renderer(render_id)) {
      WEBRTC_TRACE(kTraceError, kTraceVideo, ViEId(instance_id_),
                   "%s - Renderer already exist %d.", __FUNCTION__,
                   render_id);
      SetLastError(kViERenderAlreadyExists);
      return -1;
    }
  }
  if (render_id >= kViEChannelIdBase && render_id <= kViEChannelIdMax) {
    // This is a channel.
    ViEChannelManagerScoped cm(channel_manager_);
    ViEFrameProviderBase* frame_provider = cm.Channel(render_id);
    if (!frame_provider) {
      WEBRTC_TRACE(kTraceError, kTraceVideo, ViEId(instance_id_),
                   "%s: FrameProvider id %d doesn't exist", __FUNCTION__,
                   render_id);
      SetLastError(kViERenderInvalidRenderId);
      return -1;
    }
    ViERenderer* renderer = render_manager_.AddRenderStream(render_id,
                                                            window, z_order,
                                                            left, top,
                                                            right, bottom);
    if (!renderer) {
      SetLastError(kViERenderUnknownError);
      return -1;
    }
    return frame_provider->RegisterFrameCallback(render_id, renderer);
  } else {
    // Camera or file.
    ViEInputManagerScoped is(input_manager_);
    ViEFrameProviderBase* frame_provider = is.FrameProvider(render_id);
    if (!frame_provider) {
      WEBRTC_TRACE(kTraceError, kTraceVideo, ViEId(instance_id_),
                   "%s: FrameProvider id %d doesn't exist", __FUNCTION__,
                   render_id);
      SetLastError(kViERenderInvalidRenderId);
      return -1;
    }
    ViERenderer* renderer = render_manager_.AddRenderStream(render_id,
                                                            window, z_order,
                                                            left, top,
                                                            right, bottom);
    if (!renderer) {
      SetLastError(kViERenderUnknownError);
      return -1;
    }
    return frame_provider->RegisterFrameCallback(render_id, renderer);
  }
  SetLastError(kViERenderInvalidRenderId);
  return -1;
}

int ViERenderImpl::RemoveRenderer(const int render_id) {
  WEBRTC_TRACE(kTraceApiCall, kTraceVideo, ViEId(instance_id_),
               "%s(render_id: %d)", __FUNCTION__, render_id);
  if (!Initialized()) {
    SetLastError(kViENotInitialized);
    WEBRTC_TRACE(kTraceError, kTraceVideo, ViEId(instance_id_),
                 "%s - ViE instance %d not initialized", __FUNCTION__,
                 instance_id_);
    return -1;
  }

  ViERenderer* renderer = NULL;
  {
    ViERenderManagerScoped rs(render_manager_);
    renderer = rs.Renderer(render_id);
    if (!renderer) {
      WEBRTC_TRACE(kTraceWarning, kTraceVideo, ViEId(instance_id_),
                   "%s No render exist with render_id: %d", __FUNCTION__,
                   render_id);
      SetLastError(kViERenderInvalidRenderId);
      return -1;
    }
    // Leave the scope lock since we don't want to lock two managers
    // simultanousely.
  }
  if (render_id >= kViEChannelIdBase && render_id <= kViEChannelIdMax) {
    // This is a channel.
    ViEChannelManagerScoped cm(channel_manager_);
    ViEChannel* channel = cm.Channel(render_id);
    if (!channel) {
      WEBRTC_TRACE(kTraceWarning, kTraceVideo, ViEId(instance_id_),
                   "%s: no channel with id %d exists ", __FUNCTION__,
                   render_id);
      SetLastError(kViERenderInvalidRenderId);
      return -1;
    }
    channel->DeregisterFrameCallback(renderer);
  } else {
    // Provider owned by inputmanager, i.e. file or capture device.
    ViEInputManagerScoped is(input_manager_);
    ViEFrameProviderBase* provider = is.FrameProvider(render_id);
    if (!provider) {
      WEBRTC_TRACE(kTraceWarning, kTraceVideo, ViEId(instance_id_),
                   "%s: no provider with id %d exists ", __FUNCTION__,
                   render_id);
      SetLastError(kViERenderInvalidRenderId);
      return -1;
    }
    provider->DeregisterFrameCallback(renderer);
  }
  if (render_manager_.RemoveRenderStream(render_id) != 0) {
    SetLastError(kViERenderUnknownError);
    return -1;
  }
  return 0;
}

int ViERenderImpl::StartRender(const int render_id) {
  WEBRTC_TRACE(kTraceApiCall, kTraceVideo, ViEId(instance_id_, render_id),
               "%s(channel: %d)", __FUNCTION__, render_id);
  ViERenderManagerScoped rs(render_manager_);
  ViERenderer* renderer = rs.Renderer(render_id);
  if (!renderer) {
    WEBRTC_TRACE(kTraceError, kTraceVideo, ViEId(instance_id_, render_id),
                 "%s: No renderer with render Id %d exist.", __FUNCTION__,
                 render_id);
    SetLastError(kViERenderInvalidRenderId);
    return -1;
  }
  if (renderer->StartRender() != 0) {
    SetLastError(kViERenderUnknownError);
    return -1;
  }
  return 0;
}

int ViERenderImpl::StopRender(const int render_id) {
  WEBRTC_TRACE(kTraceApiCall, kTraceVideo, ViEId(instance_id_, render_id),
               "%s(channel: %d)", __FUNCTION__, render_id);
  ViERenderManagerScoped rs(render_manager_);
  ViERenderer* renderer = rs.Renderer(render_id);
  if (!renderer) {
    WEBRTC_TRACE(kTraceError, kTraceVideo, ViEId(instance_id_, render_id),
                 "%s: No renderer with render_id %d exist.", __FUNCTION__,
                 render_id);
    SetLastError(kViERenderInvalidRenderId);
    return -1;
  }
  if (renderer->StopRender() != 0) {
    SetLastError(kViERenderUnknownError);
    return -1;
  }
  return 0;
}

int ViERenderImpl::ConfigureRender(int render_id, const unsigned int z_order,
                                   const float left, const float top,
                                   const float right, const float bottom) {
  WEBRTC_TRACE(kTraceApiCall, kTraceVideo, ViEId(instance_id_, render_id),
               "%s(channel: %d)", __FUNCTION__, render_id);
  ViERenderManagerScoped rs(render_manager_);
  ViERenderer* renderer = rs.Renderer(render_id);
  if (!renderer) {
    WEBRTC_TRACE(kTraceError, kTraceVideo, ViEId(instance_id_, render_id),
                 "%s: No renderer with render_id %d exist.", __FUNCTION__,
                 render_id);
    SetLastError(kViERenderInvalidRenderId);
    return -1;
  }

  if (renderer->ConfigureRenderer(z_order, left, top, right, bottom) != 0) {
    SetLastError(kViERenderUnknownError);
    return -1;
  }
  return 0;
}

int ViERenderImpl::MirrorRenderStream(const int render_id, const bool enable,
                                      const bool mirror_xaxis,
                                      const bool mirror_yaxis) {
  ViERenderManagerScoped rs(render_manager_);
  ViERenderer* renderer = rs.Renderer(render_id);
  if (!renderer) {
    WEBRTC_TRACE(kTraceError, kTraceVideo, ViEId(instance_id_, render_id),
                 "%s: No renderer with render_id %d exist.", __FUNCTION__,
                 render_id);
    SetLastError(kViERenderInvalidRenderId);
    return -1;
  }
  if (renderer->EnableMirroring(render_id, enable, mirror_xaxis, mirror_yaxis)
      != 0) {
    SetLastError(kViERenderUnknownError);
    return -1;
  }
  return 0;
}

int ViERenderImpl::AddRenderer(const int render_id,
                               RawVideoType video_input_format,
                               ExternalRenderer* external_renderer) {
  // Check if the client requested a format that we can convert the frames to.
  if (video_input_format != kVideoI420 &&
      video_input_format != kVideoYV12 &&
      video_input_format != kVideoYUY2 &&
      video_input_format != kVideoUYVY &&
      video_input_format != kVideoARGB &&
      video_input_format != kVideoRGB24 &&
      video_input_format != kVideoRGB565 &&
      video_input_format != kVideoARGB4444 &&
      video_input_format != kVideoARGB1555) {
    WEBRTC_TRACE(kTraceError, kTraceVideo, ViEId(instance_id_, render_id),
                 "%s: Unsupported video frame format requested",
                 __FUNCTION__, render_id);
    SetLastError(kViERenderInvalidFrameFormat);
    return -1;
  }
  if (!Initialized()) {
    SetLastError(kViENotInitialized);
    WEBRTC_TRACE(kTraceError, kTraceVideo, ViEId(instance_id_),
                 "%s - ViE instance %d not initialized", __FUNCTION__,
                 instance_id_);
    return -1;
  }
  {
    // Verify the renderer doesn't exist.
    ViERenderManagerScoped rs(render_manager_);
    if (rs.Renderer(render_id)) {
      WEBRTC_TRACE(kTraceError, kTraceVideo, ViEId(instance_id_),
                   "%s - Renderer already exist %d.", __FUNCTION__,
                   render_id);
      SetLastError(kViERenderAlreadyExists);
      return -1;
    }
  }
  if (render_id >= kViEChannelIdBase && render_id <= kViEChannelIdMax) {
    // This is a channel.
    ViEChannelManagerScoped cm(channel_manager_);
    ViEFrameProviderBase* frame_provider = cm.Channel(render_id);
    if (!frame_provider) {
      WEBRTC_TRACE(kTraceError, kTraceVideo, ViEId(instance_id_),
                   "%s: FrameProvider id %d doesn't exist", __FUNCTION__,
                   render_id);
      SetLastError(kViERenderInvalidRenderId);
      return -1;
    }
    ViERenderer* renderer = render_manager_.AddRenderStream(render_id, NULL,
                                                            0, 0.0f, 0.0f,
                                                            1.0f, 1.0f);
    if (!renderer) {
      SetLastError(kViERenderUnknownError);
      return -1;
    }
    if (renderer->SetExternalRenderer(render_id, video_input_format,
                                      external_renderer) == -1) {
      SetLastError(kViERenderUnknownError);
      return -1;
    }

    return frame_provider->RegisterFrameCallback(render_id, renderer);
  } else {
    // Camera or file.
    ViEInputManagerScoped is(input_manager_);
    ViEFrameProviderBase* frame_provider = is.FrameProvider(render_id);
    if (!frame_provider) {
      WEBRTC_TRACE(kTraceError, kTraceVideo, ViEId(instance_id_),
                   "%s: FrameProvider id %d doesn't exist", __FUNCTION__,
                   render_id);
      SetLastError(kViERenderInvalidRenderId);
      return -1;
    }
    ViERenderer* renderer = render_manager_.AddRenderStream(render_id, NULL,
                                                            0, 0.0f, 0.0f,
                                                            1.0f, 1.0f);
    if (!renderer) {
      SetLastError(kViERenderUnknownError);
      return -1;
    }
    if (renderer->SetExternalRenderer(render_id, video_input_format,
                                      external_renderer) == -1) {
      SetLastError(kViERenderUnknownError);
      return -1;
    }
    return frame_provider->RegisterFrameCallback(render_id, renderer);
  }
  SetLastError(kViERenderInvalidRenderId);
  return -1;
}

}  // namespace webrtc
