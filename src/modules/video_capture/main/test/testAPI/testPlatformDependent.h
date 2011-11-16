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

#include "testDefines.h"
#include "video_capture_factory.h"
#include "Logger.h"

//#define RENDER_PREVIEW //Does not work properly on Linux

#ifdef RENDER_PREVIEW
    #include "Renderer.h"
#else
    typedef void* Renderer;
#endif

namespace webrtc
{

struct CaptureSetting
{
    WebRtc_Word32 settingID;
    WebRtc_UWord8 captureName[256];
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

    WebRtc_Word64 firstCapturedFrameTime;

    VideoCaptureModule* captureModule;
    

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
        
    }
    void ResetAll()
    {
        ResetSettings();
		
        settingID = -1;        
        captureModule=0;    
        initStartTime=0;
        initStopTime=0;
        stopStartTime=0;
        stopStopTime=0;
    }

};

class testPlatformDependent: public VideoCaptureDataCallback
{
public:
    testPlatformDependent(void);
    ~testPlatformDependent(void);

    
    int DoTest();

    void SetRenderer(Renderer* renderer);

    // from VideoCaptureDataCallback
    virtual void OnIncomingCapturedFrame(const WebRtc_Word32 id,                                                
                                         VideoFrame&  videoFrame,
                                         webrtc::VideoCodecType codecType);

    virtual void OnCaptureDelayChanged(const WebRtc_Word32 id,
                                       const WebRtc_Word32 delay);

private:
    // Test multiple create delete start stop of one module
    WebRtc_Word32 testCreateDelete(const WebRtc_UWord8* uniqueID);
    WebRtc_Word32 testCapabilities(const WebRtc_UWord8* uniqueID);
    WebRtc_Word32 testMultipleCameras();
    WebRtc_Word32 testRotation(const WebRtc_UWord8* uniqueID);


    void VerifyResultFrame(const WebRtc_Word32 id,const VideoFrame&  videoFrame);
    void EvaluateTestResult(CaptureSetting& captureResult);

    VideoCaptureModule* _captureModule;
    VideoCaptureModule::DeviceInfo* _captureInfo;

    CaptureSetting _captureSettings[4];
    WebRtc_UWord32 _noOfCameras;

#ifdef RENDER_PREVIEW
    Renderer*_renderer[4];
#endif
    Logger _logger;

};
} // namespace webrtc
