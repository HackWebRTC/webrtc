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
 * vie_frame_provider_base.h
 */

#ifndef WEBRTC_VIDEO_ENGINE_MAIN_SOURCE_VIE_FRAME_PROVIDER_BASE_H_
#define WEBRTC_VIDEO_ENGINE_MAIN_SOURCE_VIE_FRAME_PROVIDER_BASE_H_

// Defines
#include "typedefs.h"
#include "module_common_types.h"
#include "map_wrapper.h"

namespace webrtc {
class CriticalSectionWrapper;
class VideoEncoder;

class ViEFrameCallback
{
public:
    virtual void DeliverFrame(int id, VideoFrame& videoFrame, int numCSRCs = 0,
                              const WebRtc_UWord32 CSRC[kRtpCsrcSize] = NULL) = 0;
    /*
     * Delay has changed from the provider.
     * frameDelay new capture delay in Ms.
     */
    virtual void DelayChanged(int id, int frameDelay)=0;

    /*
     Fetch the width, height and frame rate preferred by this observer.
     return 0 on success, -1 otherwise.
     */
    virtual int GetPreferedFrameSettings(int &width, int &height,
                                         int &frameRate)=0;

    virtual void ProviderDestroyed(int id) = 0;

protected:
    virtual ~ViEFrameCallback()
    {
    }
    ;
};

class ViEFrameProviderBase
{
public:
    ViEFrameProviderBase(int Id, int engineId);
    virtual ~ViEFrameProviderBase();
    int Id();

    // Register frame callbacks, i.e. a receiver of the captured frame.
    virtual int RegisterFrameCallback(int observerId,
                                      ViEFrameCallback* callbackObject);
    virtual int
    DeregisterFrameCallback(const ViEFrameCallback* callbackObject);
    virtual bool
    IsFrameCallbackRegistered(const ViEFrameCallback* callbackObject);

    int NumberOfRegistersFrameCallbacks();

    // FrameCallbackChanged
    // Inherited classes should check for new frameSettings and reconfigure output if possible.
    // Return 0 on success, -1 otherwise.
    virtual int FrameCallbackChanged() = 0;

protected:
    void DeliverFrame(VideoFrame& videoFrame, int numCSRCs = 0,
                      const WebRtc_UWord32 CSRC[kRtpCsrcSize] = NULL);
    void SetFrameDelay(int frameDelay);
    int FrameDelay();
    int GetBestFormat(int& bestWidth, int& bestHeight, int& bestFrameRate);

    int _id;
    int _engineId;

protected:
    // Frame callbacks
    MapWrapper _frameCallbackMap;
    CriticalSectionWrapper& _providerCritSect;
private:

    VideoFrame* _ptrExtraFrame;

    //Members
    int _frameDelay;

};

} //namespace webrtc
#endif  // WEBRTC_VIDEO_ENGINE_MAIN_SOURCE_VIE_FRAME_PROVIDER_BASE_H_
