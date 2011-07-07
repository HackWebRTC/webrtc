/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "vie_frame_provider_base.h"
#include "critical_section_wrapper.h"
#include "tick_util.h"
#include "trace.h"
#include "vie_defines.h"

namespace webrtc {

ViEFrameProviderBase::ViEFrameProviderBase(int Id, int engineId):
_id(Id),
_engineId(engineId),
_frameCallbackMap(),
_providerCritSect(*CriticalSectionWrapper::CreateCriticalSection()),
_ptrExtraFrame(NULL),
_frameDelay(0)
{
}

ViEFrameProviderBase::~ViEFrameProviderBase()
{
    if(_frameCallbackMap.Size()>0)
    {
        WEBRTC_TRACE(webrtc::kTraceWarning, webrtc::kTraceVideo, ViEId(_engineId,_id), "FramCallbacks still exist when Provider deleted %d",_frameCallbackMap.Size());
    }
    for(MapItem* item=_frameCallbackMap.First();item!=NULL;item=_frameCallbackMap.Next(item))
    {
        static_cast<ViEFrameCallback*>(item->GetItem())->ProviderDestroyed(_id);
    }

    while(_frameCallbackMap.Erase(_frameCallbackMap.First()) == 0)
        ;

    delete &_providerCritSect;
    delete _ptrExtraFrame;
}

int ViEFrameProviderBase::Id()
{
    return _id;
}

void ViEFrameProviderBase::DeliverFrame(webrtc::VideoFrame& videoFrame,int numCSRCs,
                                const WebRtc_UWord32 CSRC[kRtpCsrcSize])
{
#ifdef _DEBUG
    const TickTime startProcessTime=TickTime::Now();
#endif
    CriticalSectionScoped cs(_providerCritSect);

    // Deliver the frame to all registered callbacks
    if (_frameCallbackMap.Size() > 0)
    {
        if(_frameCallbackMap.Size()==1)
        {
            ViEFrameCallback* frameObserver = static_cast<ViEFrameCallback*>(_frameCallbackMap.First()->GetItem());
            frameObserver->DeliverFrame(_id,videoFrame,numCSRCs,CSRC);
        }
        else
        {
            // Make a copy of the frame for all callbacks
            for (MapItem* mapItem = _frameCallbackMap.First();
                mapItem != NULL;
                mapItem = _frameCallbackMap.Next(mapItem))
            {
                if (_ptrExtraFrame == NULL)
                {
                    _ptrExtraFrame = new webrtc::VideoFrame();
                }
                if (mapItem != NULL)
                {
                    ViEFrameCallback* frameObserver = static_cast<ViEFrameCallback*>(mapItem->GetItem());
                    if (frameObserver != NULL)
                    {
                        // We must copy the frame each time since the previous receiver might swap it...
                        _ptrExtraFrame->CopyFrame(videoFrame);
                        frameObserver->DeliverFrame(_id, *_ptrExtraFrame,numCSRCs,CSRC);
                    }
                }
            }
        }
    }

#ifdef _DEBUG
    const int processTime=(int) (TickTime::Now()-startProcessTime).Milliseconds();
    if(processTime>25) // Warn If the delivery time is too long.
    {
        WEBRTC_TRACE(webrtc::kTraceWarning, webrtc::kTraceVideo, ViEId(_engineId,_id), "%s Too long time: %ums",__FUNCTION__,processTime);
    }
#endif
}

void ViEFrameProviderBase::SetFrameDelay(int frameDelay)
{

    CriticalSectionScoped cs(_providerCritSect);
    _frameDelay=frameDelay;

    for (MapItem* mapItem = _frameCallbackMap.First();
                mapItem != NULL;
                mapItem = _frameCallbackMap.Next(mapItem))
    {
        ViEFrameCallback* frameObserver = static_cast<ViEFrameCallback*>(mapItem->GetItem());
        assert(frameObserver);
        frameObserver->DelayChanged(_id,frameDelay);
    }

}

int ViEFrameProviderBase::FrameDelay()
{
    return _frameDelay;
}

int ViEFrameProviderBase::GetBestFormat(int& bestWidth,
                                              int& bestHeight,
                                              int& bestFrameRate)
{

    int largestWidth = 0;
    int largestHeight = 0;
    int highestFrameRate = 0;

    CriticalSectionScoped cs(_providerCritSect);

    // Check if this one already exists...
    for (MapItem* mapItem = _frameCallbackMap.First();
         mapItem != NULL;
         mapItem = _frameCallbackMap.Next(mapItem))
    {


        int preferedWidth=0;
        int preferedHeight=0;
        int preferedFrameRate=0;

        ViEFrameCallback* callbackObject = static_cast<ViEFrameCallback*>(mapItem->GetItem());
        assert(callbackObject);
        if(callbackObject->GetPreferedFrameSettings(preferedWidth,preferedHeight,preferedFrameRate)==0)
        {
            if (preferedWidth > largestWidth)
            {
                largestWidth = preferedWidth;
            }
            if (preferedHeight > largestHeight)
            {
                largestHeight = preferedHeight;
            }
            if (preferedFrameRate > highestFrameRate)
            {
                highestFrameRate = preferedFrameRate;
            }
        }
    }

    bestWidth = largestWidth;
    bestHeight = largestHeight;
    bestFrameRate = highestFrameRate;

    return 0;
}

int ViEFrameProviderBase::RegisterFrameCallback(int observerId,ViEFrameCallback* callbackObject)
{
    if (callbackObject == NULL)
    {
        WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideo, ViEId(_engineId, _id),
            "%s: No argument", __FUNCTION__);
        return -1;
    }
    WEBRTC_TRACE(webrtc::kTraceInfo, webrtc::kTraceVideo, ViEId(_engineId, _id),
        "%s(0x%p)", callbackObject);

    {
        CriticalSectionScoped cs(_providerCritSect);

        // Check if this one already exists...
        for (MapItem* mapItem = _frameCallbackMap.First();
             mapItem != NULL;
             mapItem = _frameCallbackMap.Next(mapItem))
        {
            const ViEFrameCallback* observer=static_cast<ViEFrameCallback*> (mapItem->GetItem());
            if (observer == callbackObject)
            {
                // This callback is already registered
                WEBRTC_TRACE(webrtc::kTraceWarning, webrtc::kTraceVideo, ViEId(_engineId, _id),
                    "%s 0x%p already registered", __FUNCTION__, callbackObject);

                assert("!frameObserver already registered");
                return -1;
            }
        }

        if (_frameCallbackMap.Insert(observerId,callbackObject) != 0)
        {
            WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideo, ViEId(_engineId, _id),
                "%s: Could not add 0x%p to list", __FUNCTION__, callbackObject);
            return -1;
        }
    }
    // Report current capture delay
    callbackObject->DelayChanged(_id,_frameDelay);

    FrameCallbackChanged(); // Notify implementer of this class that the callback list have changed
    return 0;


}


// ----------------------------------------------------------------------------
// DeregisterFrameCallback
// ----------------------------------------------------------------------------

int ViEFrameProviderBase::DeregisterFrameCallback(const ViEFrameCallback* callbackObject)
{
    if (callbackObject == NULL)
    {
        WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideo, ViEId(_engineId, _id),
            "%s: No argument", __FUNCTION__);
        return -1;
    }
    WEBRTC_TRACE(webrtc::kTraceInfo, webrtc::kTraceVideo, ViEId(_engineId, _id),
        "%s(0x%p)", callbackObject);


    {
        CriticalSectionScoped cs(_providerCritSect);
        bool itemFound=false;


        // Try to find the callback in our list
        for (MapItem* mapItem = _frameCallbackMap.First();
             mapItem != NULL;
             mapItem = _frameCallbackMap.Next(mapItem))
        {
            const ViEFrameCallback* observer=static_cast<ViEFrameCallback*> (mapItem->GetItem());
            if (observer == callbackObject)
            {
                // We found it, remove it!
                _frameCallbackMap.Erase(mapItem);
                WEBRTC_TRACE(webrtc::kTraceInfo, webrtc::kTraceVideo, ViEId(_engineId, _id),
                    "%s 0x%p deregistered", __FUNCTION__, callbackObject);
                itemFound=true;
                break;
            }
        }
        if(!itemFound)
        {
            WEBRTC_TRACE(webrtc::kTraceWarning, webrtc::kTraceVideo, ViEId(_engineId, _id),
                    "%s 0x%p not found", __FUNCTION__, callbackObject);
            return -1;
        }
    }

    FrameCallbackChanged(); // Notify implementer of this class that the callback list have changed
    return 0;
}

// ----------------------------------------------------------------------------
// IsFrameCallbackRegistered
// ----------------------------------------------------------------------------

bool ViEFrameProviderBase::IsFrameCallbackRegistered(const ViEFrameCallback* callbackObject)
{
    if (callbackObject == NULL)
    {
        WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideo, ViEId(_engineId, _id),
            "%s: No argument", __FUNCTION__);
        return false;
    }
    WEBRTC_TRACE(webrtc::kTraceInfo, webrtc::kTraceVideo, ViEId(_engineId, _id),
        "%s(0x%p)", callbackObject);

    for (MapItem* mapItem = _frameCallbackMap.First();
         mapItem != NULL;
         mapItem = _frameCallbackMap.Next(mapItem))
    {
        if (callbackObject == mapItem->GetItem())
        {
            // We found the callback
            WEBRTC_TRACE(webrtc::kTraceInfo, webrtc::kTraceVideo, ViEId(_engineId, _id),
                "%s 0x%p is registered", __FUNCTION__, callbackObject);
            return true;
        }
    }
    WEBRTC_TRACE(webrtc::kTraceInfo, webrtc::kTraceVideo, ViEId(_engineId, _id),
        "%s 0x%p not registered", __FUNCTION__, callbackObject);
    return false;
}

// ----------------------------------------------------------------------------
// NumberOfRegistersFrameCallbacks
// ----------------------------------------------------------------------------

int ViEFrameProviderBase::NumberOfRegistersFrameCallbacks()
{
    CriticalSectionScoped cs(_providerCritSect);
    return _frameCallbackMap.Size();
}
} // namespac webrtc
