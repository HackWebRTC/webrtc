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
 * ViERenderManager.cpp
 */

#include "vie_render_manager.h"
#include "engine_configurations.h"
#include "vie_defines.h"

#include "critical_section_wrapper.h"
#include "video_render.h"
#include "video_render_defines.h"
#include "rw_lock_wrapper.h"
#include "trace.h"

namespace webrtc {

// ----------------------------------------------------------------------------
// Constructor
// ----------------------------------------------------------------------------



ViERenderManagerScoped::ViERenderManagerScoped(const ViERenderManager& vieRenderManager)                                 
    :
    ViEManagerScopedBase(vieRenderManager)    
{

}


// ----------------------------------------------------------------------------
// Renderer()
//
// Returns a pointer to the ViERender object
// ----------------------------------------------------------------------------

ViERenderer* ViERenderManagerScoped::Renderer(WebRtc_Word32 renderId) const
{
    return static_cast<const ViERenderManager*> (vie_manager_)->ViERenderPtr(
        renderId);
}


// ----------------------------------------------------------------------------
// Constructor
// ----------------------------------------------------------------------------

ViERenderManager::ViERenderManager(WebRtc_Word32 engineId) :
    _listCritsect(*CriticalSectionWrapper::CreateCriticalSection()),    
    _engineId(engineId),
    _streamToViERenderer(),
    _renderList(),
    _useExternalRenderModule(false)
{
    WEBRTC_TRACE(webrtc::kTraceMemory, webrtc::kTraceVideo, ViEId(engineId),
        "ViERenderManager::ViERenderManager(engineId: %d) - Constructor", engineId);
}

// ----------------------------------------------------------------------------
// Destructor
// ----------------------------------------------------------------------------

ViERenderManager::~ViERenderManager()
{
    WEBRTC_TRACE(webrtc::kTraceMemory, webrtc::kTraceVideo, ViEId(_engineId),
        "ViERenderManager Destructor, engineId: %d", _engineId);

    while(_streamToViERenderer.Size()!=0)
    {
        MapItem* item=_streamToViERenderer.First();
        assert(item);
        const WebRtc_Word32 renderId=item->GetId();
        item=NULL;// Deleted be RemoveRenderStream;
        RemoveRenderStream(renderId);        
    }    
    delete &_listCritsect;
    
}

// ----------------------------------------------------------------------------
// RegisterVideoRenderModule
// ----------------------------------------------------------------------------

WebRtc_Word32 ViERenderManager::RegisterVideoRenderModule(VideoRender& renderModule)
{
    // See if there is already a render module registered for the window that
    // the registrant render module is associated with
    VideoRender* ptrCurrentModule = FindRenderModule(renderModule.Window());
    if (ptrCurrentModule)
    {
        WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideo, ViEId(_engineId),
            "A module is already registered for this window (window=%p, current module=%p, registrant module=%p",
            renderModule.Window(), ptrCurrentModule, &renderModule);
        return -1;
    }

    // Register module
    _renderList.PushBack(static_cast<void*>(&renderModule));
    _useExternalRenderModule=true;

    return 0;
}

// ----------------------------------------------------------------------------
// DeRegisterVideoRenderModule
// ----------------------------------------------------------------------------

WebRtc_Word32 ViERenderManager::DeRegisterVideoRenderModule(VideoRender& renderModule)
{
    // Check if there are streams in the module
    WebRtc_UWord32 nStreams = renderModule.GetNumIncomingRenderStreams();
    if (nStreams != 0)
    {
        WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideo, ViEId(_engineId),
            "There are still %d streams in this module, cannot de-register", nStreams);
        return -1;
    }

    // Erase the render module from the map
    ListItem* listItem = NULL;
    bool found = false;
    for (listItem = _renderList.First(); listItem != NULL; listItem = _renderList.Next(listItem))
    {
        if (&renderModule == static_cast<VideoRender*>(listItem->GetItem()))
        {
            // We've found our renderer
            _renderList.Erase(listItem);
            found = true;
            break;
        }
    }
    if (!found)
    {
        WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideo, ViEId(_engineId), "Module not registered");
        return -1;
    }

    return 0;
}

// ----------------------------------------------------------------------------
// AddRenderStream
// ----------------------------------------------------------------------------

ViERenderer* ViERenderManager::AddRenderStream(const WebRtc_Word32 renderId,                                                           
                                                           void* window,                                                           
                                                           const WebRtc_UWord32 zOrder,
                                                           const float left,
                                                           const float top,
                                                           const float right,
                                                           const float bottom)
{
    CriticalSectionScoped cs(_listCritsect);

    if (_streamToViERenderer.Find(renderId) != NULL)
    {
        // This stream is already added to a renderer, not allowed!
        WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideo, ViEId(_engineId), "Render stream already exists");
        return NULL;
    }
    
    // Get the render module for this window
    VideoRender* ptrRenderer = FindRenderModule(window);
    if (ptrRenderer == NULL)
    {
        // No render module for this window, create a new one
       ptrRenderer = VideoRender::CreateVideoRender(ViEModuleId(_engineId, -1), window, false);
       if (!ptrRenderer)
       {
           WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideo, ViEId(_engineId), "Could not create new render module");
           return NULL;
       }
       _renderList.PushBack((void*) ptrRenderer);
    }
    
    ViERenderer* vieRenderer= ViERenderer::CreateViERenderer(renderId,_engineId,*ptrRenderer,*this,zOrder,left,top,right,bottom);
    if(!vieRenderer)
    {
        WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideo, ViEId(_engineId,renderId), "Could not create new render stream");
        return NULL;
    }

    _streamToViERenderer.Insert(renderId, vieRenderer);

    return vieRenderer;
}

// ----------------------------------------------------------------------------
// RemoveRenderStream
// ----------------------------------------------------------------------------

WebRtc_Word32 ViERenderManager::RemoveRenderStream(const WebRtc_Word32 renderId)
{
    // We need exclusive right to the items in the rendermanager to delete a stream
    ViEManagerWriteScoped(*this);

    // Protect the list/map
    CriticalSectionScoped cs(_listCritsect);

    MapItem* mapItem = _streamToViERenderer.Find(renderId);
    if (mapItem == NULL)
    {
        // No such stream
        WEBRTC_TRACE(webrtc::kTraceWarning, webrtc::kTraceVideo, ViEId(_engineId), "No renderer for this stream found, channelId");
        return 0;
    }    

    // Get the vieRender object. 
    ViERenderer* ptrViERenderer = static_cast<ViERenderer*>(mapItem->GetItem());
    assert(ptrViERenderer);

    // Get the render module pointer for this vieRender object
    VideoRender& renderer=ptrViERenderer->RenderModule();

    // Delete the vieRender.
    // This deletes the stream in the render module.
    delete ptrViERenderer;
    
    // Remove from the stream map
    _streamToViERenderer.Erase(mapItem);

    // Check if there are other streams in the module
    if (!_useExternalRenderModule && renderer.GetNumIncomingRenderStreams() == 0)
    {
        // Erase the render module from the map
        ListItem* listItem = NULL;
        for (listItem = _renderList.First(); listItem != NULL; listItem = _renderList.Next(listItem))
        {
            if (&renderer == static_cast<VideoRender*>(listItem->GetItem()))
            {
                // We've found our renderer
                _renderList.Erase(listItem);
                break;
            }
        }
        // Destroy the module
        VideoRender::DestroyVideoRender(&renderer);
    }

    return 0;
}

// ----------------------------------------------------------------------------
// FindRenderModule
//
// Returns a pointer to the render module if it exists in the render list.
// Assumed protected
// ----------------------------------------------------------------------------

VideoRender* ViERenderManager::FindRenderModule(void* window)
{
    VideoRender* ptrRenderer = NULL;
    ListItem* listItem = NULL;
    for (listItem = _renderList.First(); listItem != NULL; listItem = _renderList.Next(listItem))
    {
        ptrRenderer = static_cast<VideoRender*>(listItem->GetItem());
        if (ptrRenderer == NULL)
        {
            break;
        }
        if (ptrRenderer->Window() == window)
        {
            // We've found the render module
            break;
        }
        ptrRenderer = NULL;
    }
    return ptrRenderer;
}

ViERenderer* ViERenderManager::ViERenderPtr(WebRtc_Word32 renderId) const
{
    ViERenderer* ptrRenderer = NULL;

    MapItem* mapItem = _streamToViERenderer.Find(renderId);
    if (mapItem == NULL)
    {
        // No such stream in any renderer
        return NULL;
    }
    ptrRenderer = static_cast<ViERenderer*>(mapItem->GetItem());
    
    return ptrRenderer;
}

} //namespace webrtc
