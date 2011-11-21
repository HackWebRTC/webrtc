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
 * vie_render_manager.h
 */

#ifndef WEBRTC_VIDEO_ENGINE_MAIN_SOURCE_VIE_RENDER_MANAGER_H_
#define WEBRTC_VIDEO_ENGINE_MAIN_SOURCE_VIE_RENDER_MANAGER_H_

// Defines
#include "engine_configurations.h"
#include "vie_defines.h"

#include "typedefs.h"
#include "list_wrapper.h"
#include "map_wrapper.h"

#include "vie_manager_base.h"

#include "vie_renderer.h"

namespace webrtc {

class CriticalSectionWrapper;
class RWLockWrapper;
class VideoRender;
class VideoRenderCallback;

class ViERenderManager: private ViEManagerBase
{
    friend class ViERenderManagerScoped;
public:
    ViERenderManager(WebRtc_Word32 engineId);
    ~ViERenderManager();

    WebRtc_Word32 RegisterVideoRenderModule(VideoRender& renderModule);
    WebRtc_Word32 DeRegisterVideoRenderModule(VideoRender& renderModule);

    ViERenderer* AddRenderStream(const WebRtc_Word32 renderId,                                             
                                             void* window,                                             
                                             const WebRtc_UWord32 zOrder,
                                             const float left,
                                             const float top,
                                             const float right,
                                             const float bottom);

    WebRtc_Word32 RemoveRenderStream(WebRtc_Word32 renderId);
    
    VideoRender* FindRenderModule(void* window);

private:

    // Methods used by ViERenderScoped
    ViERenderer* ViERenderPtr(WebRtc_Word32 renderId) const;

    // Members
    CriticalSectionWrapper&    _listCritsect;    

    WebRtc_Word32             _engineId;
    MapWrapper                 _streamToViERenderer; // Protected by ViEManagerBase
    ListWrapper                _renderList;
    bool                       _useExternalRenderModule;
};


// ------------------------------------------------------------------
// ViERenderManagerScoped
// ------------------------------------------------------------------
class ViERenderManagerScoped: private ViEManagerScopedBase
{
public:    
    ViERenderManagerScoped(const ViERenderManager& vieRenderManager);
    ViERenderer* Renderer(WebRtc_Word32 renderId) const;
};

} //namespace webrtc

#endif    // WEBRTC_VIDEO_ENGINE_MAIN_SOURCE_VIE_RENDER_MANAGER_H_
