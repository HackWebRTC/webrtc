/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "testPlatformDependent.h"
#include <stdio.h>
#include "trace.h"
#include "tick_util.h"

namespace webrtc
{
static int testPlatformDependentResult = 0;

#ifdef _WIN32
#include <Windows.h>
#endif

#if defined( _DEBUG) && defined (_WIN32)
//#include "vld.h"
#endif

#ifdef NDEBUG
#if defined(WEBRTC_MAC_INTEL)

#else
#undef assert
#define assert(p) if(!(p)){LOG("Error line %d\n",__LINE__);testPlatformDependentResult=-1;}
#endif
#endif

testPlatformDependent::testPlatformDependent(void) :
    _captureModule(NULL), _noOfCameras(0)

{
    Trace::CreateTrace();
    Trace::SetLevelFilter(webrtc::kTraceAll);
    Trace::SetTraceFile("testPlatformDependent.txt");
    _captureInfo = VideoCaptureModule::CreateDeviceInfo(5);
#ifdef RENDER_PREVIEW
    memset(_renderer, 0, sizeof(_renderer));
#endif
}

testPlatformDependent::~testPlatformDependent(void)
{
    VideoCaptureModule::DestroyDeviceInfo(_captureInfo);

#ifdef RENDER_PREVIEW
    if (_renderer[0])
        delete _renderer[0];

    if (_renderer[1])
        delete _renderer[1];
    if (_renderer[2])
        delete _renderer[2];

    if (_renderer[3])
        delete _renderer[3];

#endif
    Trace::ReturnTrace();
}

//FILE* file=NULL;

void testPlatformDependent::OnIncomingCapturedFrame(const WebRtc_Word32 id,
                                                    VideoFrame& videoFrame,
                                                    VideoCodecType /*codecType*/)
{
    VerifyResultFrame(id, videoFrame);

    //LOG("OnIncomingCapturedFrame, width %d height %d id %d length %d\n",
    //      videoFrame.Width(), videoFrame.Height(),id,videoFrame.Length());

    /*  if(file==NULL)
     {
     file = fopen("/sdcard/testPlatform.yuv","wb");
     LOG("\nOnIncomingCapturedFrame, open file\n");
     }
     if(file)
     {
     fwrite(videoFrame.Buffer(),videoFrame.Length(),1,file);
     fflush(file);
     }*/
#ifdef RENDER_PREVIEW
    if (id < 4 && _renderer[id])
    {
        _renderer[id]->RenderFrame(videoFrame);
    }
#endif

}

void testPlatformDependent::OnCaptureDelayChanged(
                                                  const WebRtc_Word32 settingID,
                                                  const WebRtc_Word32 delay)
{

    bool found = false;

    for (WebRtc_UWord32 i = 0; i < _noOfCameras; ++i)
    {

        if (settingID == _captureSettings[i].settingID)
        {
            found = true;
            _captureSettings[0].captureDelay = delay;
        }
    }
    assert(found);
}
void testPlatformDependent::VerifyResultFrame(const WebRtc_Word32 settingID,
                                              const VideoFrame& videoFrame)
{
    bool found = false;
    for (WebRtc_UWord32 i = 0; i < _noOfCameras; ++i)
    {
        if (settingID == _captureSettings[i].settingID)
        {
            found = true;

            assert(videoFrame.Height()==_captureSettings[i].capability.height);
            assert(videoFrame.Width()==_captureSettings[i].capability.width);
            assert(videoFrame.RenderTimeMs()>=TickTime::MillisecondTimestamp()-30); // RenderTimstamp should be the time now
            if ((videoFrame.RenderTimeMs()
                > _captureSettings[i].lastRenderTimeMS + (1000 * 1.1)
                    / _captureSettings[i].capability.maxFPS
                && _captureSettings[i].lastRenderTimeMS > 0)
                || (videoFrame.RenderTimeMs()
                    < _captureSettings[i].lastRenderTimeMS + (1000 * 0.9)
                        / _captureSettings[i].capability.maxFPS
                    && _captureSettings[i].lastRenderTimeMS > 0))
            {
                _captureSettings[i].timingWarnings++;
            }

            if (_captureSettings[i].lastRenderTimeMS == 0)
            {
                _captureSettings[i].firstCapturedFrameTime
                    = TickTime::MillisecondTimestamp();
            }
            _captureSettings[i].incomingFrames++;
            _captureSettings[i].lastRenderTimeMS = videoFrame.RenderTimeMs();
        }
    }
    assert(found);
}

WebRtc_Word32 testPlatformDependent::testCreateDelete(
    const WebRtc_UWord8* uniqueID)
{
    WebRtc_Word32 testTime = 8000;
    WebRtc_Word32 numOfLoops = 7;
    LOG("\n\nTesting create /delete - start stop of camera %s\n",(char*)uniqueID);
    for (WebRtc_Word32 i = 0; i < numOfLoops; ++i)
    {
        LOG("Loop %d of %d\n",(int) i, (int) numOfLoops);
        _captureSettings[0].settingID = 0;
#ifndef WEBRTC_MAC
        _captureInfo->GetCapability(uniqueID, 0, _captureSettings[0].capability);
#else
        _captureSettings[0].capability.width = 352;
        _captureSettings[0].capability.height = 288;
        _captureSettings[0].capability.maxFPS = 30;
        _captureSettings[0].capability.rawType = kVideoUnknown;
#endif
        _captureSettings[0].startTime = TickTime::MillisecondTimestamp();
        _captureSettings[0].initStartTime = TickTime::MillisecondTimestamp();
        _captureSettings[0].captureModule = VideoCaptureModule::Create(0,
                                                                       uniqueID);
        assert(!_captureSettings[0].captureModule->CaptureStarted());
        assert(_captureSettings[0].captureModule); // Test that it is created
        assert(!_captureSettings[0].captureModule->RegisterCaptureDataCallback(*this));

        VideoCaptureCapability capability;
        assert(_captureSettings[0].captureModule->CaptureSettings(capability)==0);

        assert(_captureSettings[0].captureModule->StartCapture(
                                            _captureSettings[0].capability) ==0);
        assert(_captureSettings[0].captureModule->CaptureStarted());
        assert(_captureSettings[0].captureModule->CaptureSettings(
                                                               capability) ==0);
        _captureSettings[0].initStopTime = TickTime::MillisecondTimestamp();

        assert(capability==_captureSettings[0].capability);

        WebRtc_Word64 timeNow = TickTime::MillisecondTimestamp();
        while (_captureSettings[0].incomingFrames <= 5
                && testTime > timeNow - _captureSettings[0].startTime)
        {
            SLEEP(100);
            timeNow = TickTime::MillisecondTimestamp();
        }
        _captureSettings[0].stopTime = TickTime::MillisecondTimestamp();
        _captureSettings[0].stopStartTime = TickTime::MillisecondTimestamp();
        assert(_captureSettings[0].captureModule->StopCapture()==0);
        assert(!_captureSettings[0].captureModule->CaptureStarted());

        VideoCaptureModule::Destroy(_captureSettings[0].captureModule);
        _captureSettings[0].stopStopTime = TickTime::MillisecondTimestamp();

        assert((_captureSettings[0].incomingFrames >= 5)); // Make sure at least 5 frames has been captured
        EvaluateTestResult(_captureSettings[0]);
        _captureSettings[0].ResetAll();
    }
    LOG("Test Done\n");
    return testPlatformDependentResult;
}

WebRtc_Word32 testPlatformDependent::testCapabilities(
                                                  const WebRtc_UWord8* uniqueID)
{
#ifndef WEBRTC_MAC
    LOG("\n\nTesting capture capabilities\n");

    _captureSettings[0].captureModule = VideoCaptureModule::Create(0, uniqueID);
    assert(_captureSettings[0].captureModule); // Test that it is created

    assert(!_captureSettings[0].captureModule->RegisterCaptureDataCallback(*this));

    WebRtc_Word32 numOfCapabilities =
            _captureInfo->NumberOfCapabilities(uniqueID);

    assert(numOfCapabilities);
    bool oneValidCap = false;
    WebRtc_Word32 testTime = 4000;

    for (WebRtc_Word32 j = 0; j < numOfCapabilities; ++j)
    {
        VideoCaptureCapability capability;
        int b = (_captureInfo->GetCapability(uniqueID, j, capability) == 0);

        assert(b);
        assert(capability.width);
        assert(capability.height);
        assert(capability.maxFPS);
        assert(capability.expectedCaptureDelay);
        oneValidCap = true;
        _captureSettings[0].lastRenderTimeMS = 0;
        _captureSettings[0].settingID = 0;
        _captureSettings[0].captureDelay = 50;
        _captureSettings[0].capability = capability;

        LOG("\n\n  Starting camera: capability %d, width %u, height %u,"
            " framerate %u, color %d.\n",
            (int) j,(unsigned int)_captureSettings[0].capability.width,
            (unsigned int)_captureSettings[0].capability.height,
            (unsigned int) _captureSettings[0].capability.maxFPS,(int) capability.rawType);
        _captureSettings[0].initStartTime = TickTime::MillisecondTimestamp();
        assert(_captureSettings[0].captureModule->StartCapture(_captureSettings[0].capability)==0);
        _captureSettings[0].startTime = TickTime::MillisecondTimestamp();
        _captureSettings[0].initStopTime = TickTime::MillisecondTimestamp();

        while (TickTime::MillisecondTimestamp() - _captureSettings[0].startTime
            < testTime && _captureSettings[0].incomingFrames < 600)
        {
            SLEEP(200);
        }

        _captureSettings[0].stopTime = TickTime::MillisecondTimestamp();
        _captureSettings[0].stopStartTime = TickTime::MillisecondTimestamp();
        assert(_captureSettings[0].captureModule->StopCapture()==0);
        _captureSettings[0].stopStopTime = TickTime::MillisecondTimestamp();

        EvaluateTestResult(_captureSettings[0]);
    }
    assert(oneValidCap); // Make sure the camera support at least one capability
    VideoCaptureModule::Destroy(_captureSettings[0].captureModule);
    _captureSettings[0].ResetAll();
    return testPlatformDependentResult;
#else
    // GetCapability() not support on Mac
    return 0;
#endif

}

WebRtc_Word32 testPlatformDependent::testMultipleCameras()
{
    // Test multiple cameras
    LOG("\n\nTesting all cameras simultanously\n");
    _noOfCameras = _captureInfo->NumberOfDevices();
    WebRtc_Word32 testTime = 20000;
    for (WebRtc_UWord32 i = 0; i < _noOfCameras; ++i)
    {
#ifdef RENDER_PREVIEW
        if (!_renderer[i])
        {
            _renderer[i] = new Renderer(true);
        }
#endif
        WebRtc_UWord8 id[256];
        _captureInfo->GetDeviceName(i, _captureSettings[i].captureName, 256,
                                    id, 256);
        WebRtc_UWord8* name = _captureSettings[i].captureName;
        LOG("\n\n  Found capture device %u\n  name %s\n  unique name %s\n"
            ,(unsigned int) i,(char*) name, (char*)id);
        _captureSettings[i].captureModule = VideoCaptureModule::Create(i, id);
        assert(_captureSettings[i].captureModule); // Test that it is created
        assert(!_captureSettings[i].captureModule->RegisterCaptureDataCallback(*this));

        _captureSettings[i].lastRenderTimeMS = 0;
        _captureSettings[i].settingID = i;
        _captureSettings[i].captureDelay = 0;
        _captureSettings[i].capability.maxFPS = 30;
        _captureSettings[i].capability.width = 640;
        _captureSettings[i].capability.height = 480;

        LOG("\n\n  Starting camera %s.\n",name);
        _captureSettings[i].captureModule->StartCapture(
                                                        _captureSettings[i].capability);
        _captureSettings[i].startTime = TickTime::MillisecondTimestamp();

    }

    SLEEP(testTime);
    for (WebRtc_UWord32 i = 0; i < _noOfCameras; ++i)
    {
        _captureSettings[i].stopTime = TickTime::MillisecondTimestamp();
        _captureSettings[i].captureModule->StopCapture();

        EvaluateTestResult(_captureSettings[i]);
        VideoCaptureModule::Destroy(_captureSettings[i].captureModule);
        _captureSettings[i].ResetAll();
    }
    return testPlatformDependentResult;
}

void testPlatformDependent::SetRenderer(Renderer* renderer)
{
    LOG("\ntestPlatformDependent::SetRenderer()\n");
#ifdef RENDER_PREVIEW
    _renderer[0] = renderer;
#endif
}

WebRtc_Word32 testPlatformDependent::testRotation(const WebRtc_UWord8* uniqueID)
{
    LOG("\n\nTesting capture Rotation\n");

    _captureSettings[0].captureModule = VideoCaptureModule::Create(0, uniqueID);
    assert(_captureSettings[0].captureModule); // Test that it is created

    assert(!_captureSettings[0].captureModule->RegisterCaptureDataCallback(*this));
#ifndef WEBRTC_MAC
    assert(_captureInfo->GetCapability(uniqueID,0,_captureSettings[0].capability)==0);
#else
    // GetCapability not supported on Mac
    _captureSettings[0].capability.width = 352;
    _captureSettings[0].capability.height = 288;
    _captureSettings[0].capability.maxFPS = 30;
    _captureSettings[0].capability.rawType = kVideoUnknown;
#endif

    WebRtc_Word32 testTime = 4000;

    _captureSettings[0].lastRenderTimeMS = 0;
    _captureSettings[0].settingID = 0;
    _captureSettings[0].captureDelay = 50;

    LOG("\n\n  Starting camera: width %u, height %u, framerate %u, color %d.\n",
        (unsigned int)_captureSettings[0].capability.width,
        (unsigned int)_captureSettings[0].capability.height,
        (unsigned int) _captureSettings[0].capability.maxFPS,
        (int) _captureSettings[0].capability.rawType);

    _captureSettings[0].initStartTime = TickTime::MillisecondTimestamp();
    assert(_captureSettings[0].captureModule->StartCapture(_captureSettings[0].capability)==0);
    _captureSettings[0].startTime = TickTime::MillisecondTimestamp();
    _captureSettings[0].initStopTime = TickTime::MillisecondTimestamp();

    LOG("\nSetting capture rotation 0\n");
    assert(_captureSettings[0].captureModule->SetCaptureRotation(kCameraRotate0)==0);
    while (TickTime::MillisecondTimestamp() - _captureSettings[0].startTime
        < testTime)
    {
        SLEEP(200);
    }
    LOG("\nSetting capture rotation 90\n");
    assert(_captureSettings[0].captureModule->SetCaptureRotation(kCameraRotate90)==0);
    while (TickTime::MillisecondTimestamp() - _captureSettings[0].startTime
        < testTime * 2)
    {
        SLEEP(200);
    }
    LOG("\nSetting capture rotation 180\n");
    assert(_captureSettings[0].captureModule->SetCaptureRotation(kCameraRotate180)==0);
    while (TickTime::MillisecondTimestamp() - _captureSettings[0].startTime
        < testTime * 3)
    {
        SLEEP(200);
    }
    LOG("\nSetting capture rotation 270\n");
    assert(_captureSettings[0].captureModule->SetCaptureRotation(kCameraRotate270)==0);
    while (TickTime::MillisecondTimestamp() - _captureSettings[0].startTime
        < testTime * 4)
    {
        SLEEP(200);
    }

    _captureSettings[0].stopTime = TickTime::MillisecondTimestamp();
    _captureSettings[0].stopStartTime = TickTime::MillisecondTimestamp();
    assert(_captureSettings[0].captureModule->StopCapture()==0);
    _captureSettings[0].stopStopTime = TickTime::MillisecondTimestamp();

    EvaluateTestResult(_captureSettings[0]);

    VideoCaptureModule::Destroy(_captureSettings[0].captureModule);
    _captureSettings[0].ResetAll();

    return testPlatformDependentResult;

}
int testPlatformDependent::DoTest()
{
    LOG("\ntestPlatformDependent::DoTest()\n");
#ifdef RENDER_PREVIEW
    if (!_renderer[0])
    {
        _renderer[0] = new Renderer(true);
    }
#endif

    // Test one camera at the time
    LOG("\n\nTesting one camera at the time\n");
    _noOfCameras = _captureInfo->NumberOfDevices();

    for (WebRtc_UWord32 i = 0; i < _noOfCameras; ++i)
    {
        WebRtc_UWord8 name[256];
        WebRtc_UWord8 uniqueID[256];
        WebRtc_UWord8 productId[256];
        memset(productId, 0, sizeof(productId));
        _captureInfo->GetDeviceName(i, name, 256, uniqueID, 256, productId, 256);

        char logFileName[512];
        SPRINTF(logFileName,512,"testPlatformDependent%s_%s.txt",(char*)name,(char*)productId);
        _logger.SetFileName(logFileName);

        WebRtc_Word32 cap = _captureInfo->NumberOfCapabilities(uniqueID);
        LOG("\n\n  Found capture device %u\n "
            " name %s\n Capabilities %d, unique name %s \n",
            (unsigned int) i,name,(int) cap,(char*) uniqueID);
        testCreateDelete(uniqueID);
        testCapabilities(uniqueID);
        testRotation(uniqueID);

    }
#ifndef ANDROID
    _logger.SetFileName("testPlatformDependent_multipleCameras.txt");
    testMultipleCameras();
#endif
    LOG("\n\ntestPlatformDependent done\n");
    return 0;
}
void testPlatformDependent::EvaluateTestResult(CaptureSetting& captureResult)
{
    WebRtc_UWord64 timeToFirstFrame = captureResult.firstCapturedFrameTime
                                      - captureResult.startTime;
    WebRtc_UWord64 timeToStart = captureResult.initStopTime
                                - captureResult.initStartTime;
    WebRtc_UWord64 timeToStop = captureResult.stopStopTime
                                - captureResult.stopStartTime;

    assert(timeToStart<4000);
    assert(timeToStop<3000);

    assert((timeToFirstFrame<3500) && (timeToFirstFrame>0)); // Assert if it takes more than 3500ms to start.
    WebRtc_Word64 expectedNumberOfFrames = ((captureResult.stopTime
                                - captureResult.startTime - timeToFirstFrame)
                                * captureResult.capability.maxFPS) / 1000;
    assert(captureResult.incomingFrames>0.50*expectedNumberOfFrames); // Make sure at least 50% of the expected frames have been received from the camera

    LOG("  Test result.\n  No Captured %d,expected %d, \n  timingWarnings %d,"
        " time to first %lu\n  time to start %lu, time to stop %lu\n",
        (int) captureResult.incomingFrames,(int)(expectedNumberOfFrames),
        (int) captureResult.timingWarnings,(long) timeToFirstFrame,
        (long) timeToStart,(long) timeToStop);
    captureResult.ResetSettings();
}
} // namespace webrtc
