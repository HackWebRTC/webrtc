/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#pragma once

#include "video_capture.h"

//#define RENDER_PREVIEW

#ifdef RENDER_PREVIEW
    #include "Renderer.h"
    #include "video_coding.h"
    #include "module_common_types.h"
#endif

#if defined (WEBRTC_MAC_INTEL) || defined (WEBRTC_LINUX)
	#include "Logger.h"
#else
	#include "Logger.h"
#endif

#include "testDefines.h"

namespace webrtc
{

class testCameraEncoder: private VideoCaptureDataCallback
#ifdef RENDER_PREVIEW
    ,VCMReceiveCallback
#endif
{
public:
    testCameraEncoder(void);
    ~testCameraEncoder(void);

    int DoTest();


private:
    
    int testCapability(VideoCaptureCapability& capability);

        // Implement VideoCaptureDataCallback
    virtual void OnIncomingCapturedFrame(const WebRtc_Word32 id,                                                
                                         VideoFrame&  videoFrame,
                                         webrtc::VideoCodecType codecType);

    virtual void OnCaptureDelayChanged(const WebRtc_Word32 id,
                                       const WebRtc_Word32 delay);

    void EvaluateTestResult();

    

#ifdef RENDER_PREVIEW
    //Implements webrtc::VCMReceiveCallback
    virtual WebRtc_Word32 FrameToRender(VideoFrame& videoFrame);
#endif

    VideoCaptureModule* _captureModule;
    VideoCaptureModule::DeviceInfo* _captureInfo;
    VideoCaptureModule::VideoCaptureEncodeInterface* _encodeInterface;

#ifdef RENDER_PREVIEW
    Renderer*_renderer;
    webrtc::VideoCodingModule* _videoCoding;
#endif

    struct CaptureSetting
    { 
        VideoCaptureCapability capability;     
        WebRtc_Word32 captureDelay;
        WebRtc_Word64 lastRenderTimeMS;

        WebRtc_Word32 incomingFrames;
        WebRtc_Word32 timingWarnings;
        WebRtc_Word64 startTime;
        WebRtc_Word64 stopTime;
        WebRtc_Word64 initStartTime;
        WebRtc_Word64 initStopTime;
        WebRtc_Word64 stopStartTime;
        WebRtc_Word64 stopStopTime;
        WebRtc_Word64 bitrateMeasureTime;
        WebRtc_Word32 noOfBytes;
        WebRtc_Word32 idrFrames;

        WebRtc_Word64 firstCapturedFrameTime;

        CaptureSetting()
        {
            ResetAll();
            
            
        }
        void ResetSettings()
        {
            
            capability.width=0;
            capability.height=0;
            capability.maxFPS=0;    
            captureDelay=0;
            lastRenderTimeMS=0;
            incomingFrames=0;
            timingWarnings=0;
            startTime=0;
            stopTime=0;
            firstCapturedFrameTime=0;
            noOfBytes=0;
            idrFrames=0;
            bitrateMeasureTime=0;
            
        }
        void ResetAll()
        {
            ResetSettings();
    		   
            initStartTime=0;
            initStopTime=0;
            stopStartTime=0;
            stopStopTime=0;
        }

    };

    Logger _logger;    
    CaptureSetting _captureSettings;
};
} // namespace webrtc
