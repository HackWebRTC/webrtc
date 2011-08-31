/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

//
// vie_autotest_render.cc
//

#include "vie_autotest_defines.h"
#include "vie_autotest.h"
#include "engine_configurations.h"

#include "video_render.h"

#include "tb_interfaces.h"
#include "tb_video_channel.h"
#include "tb_capture_device.h"

#if defined(WIN32)
#include <windows.h>
#include <ddraw.h>
#include <tchar.h>
#elif defined(WEBRTC_LINUX)
    //From windgi.h
    #undef RGB
    #define RGB(r,g,b)          ((unsigned long)(((unsigned char)(r)|((unsigned short)((unsigned char)(g))<<8))|(((unsigned long)(unsigned char)(b))<<16)))
    //From ddraw.h
/*    typedef struct _DDCOLORKEY
 {
 DWORD       dwColorSpaceLowValue;   // low boundary of color space that is to
 DWORD       dwColorSpaceHighValue;  // high boundary of color space that is
 } DDCOLORKEY;*/
#elif defined(WEBRTC_MAC)
#endif

class ViEAutoTestExternalRenderer: public ExternalRenderer
{
public:
    ViEAutoTestExternalRenderer() :
        _width(0),
        _height(0)
    {
    }
    virtual int FrameSizeChange(unsigned int width, unsigned int height,
                                unsigned int numberOfStreams)
    {
        _width = width;
        _height = height;
        return 0;
    }

    virtual int DeliverFrame(unsigned char* buffer, int bufferSize,
                             unsigned int time_stamp)
    {
        if (bufferSize != _width * _height * 3 / 2)
        {
            ViETest::Log("incorrect render buffer received, of length = %d\n",
                         bufferSize);
            return 0;
        }
        ViETest::Log("callback DeliverFrame is good\n");
        return 0;
    }

public:
    virtual ~ViEAutoTestExternalRenderer()
    {
    }
private:
    int _width, _height;
};

int ViEAutoTest::ViERenderStandardTest()
{
    ViETest::Log(" ");
    ViETest::Log("========================================");
    ViETest::Log(" ViERender Standard Test\n");

    //***************************************************************
    //	Begin create/initialize WebRTC Video Engine for testing
    //***************************************************************


    int error = 0;
    int numberOfErrors = 0;
    int rtpPort = 6000;

    tbInterfaces ViE("ViERender", numberOfErrors);

    // Create a video channel
    tbVideoChannel tbChannel(ViE, numberOfErrors, webrtc::kVideoCodecVP8);
    tbCaptureDevice tbCapture(ViE, numberOfErrors); // Create a capture device
    tbCapture.ConnectTo(tbChannel.videoChannel);
    tbChannel.StartReceive(rtpPort);
    tbChannel.StartSend(rtpPort);

    error = ViE.ptrViERender->RegisterVideoRenderModule(*_vrm1);
    numberOfErrors += ViETest::TestError(error == 0, "ERROR: %s at line %d",
                                         __FUNCTION__, __LINE__);

    error = ViE.ptrViERender->AddRenderer(tbCapture.captureId, _window1, 0,
                                          0.0, 0.0, 1.0, 1.0);
    numberOfErrors += ViETest::TestError(error == 0, "ERROR: %s at line %d",
                                         __FUNCTION__, __LINE__);

    error = ViE.ptrViERender->StartRender(tbCapture.captureId);
    numberOfErrors += ViETest::TestError(error == 0, "ERROR: %s at line %d",
                                         __FUNCTION__, __LINE__);

    error = ViE.ptrViERender->RegisterVideoRenderModule(*_vrm2);
    numberOfErrors += ViETest::TestError(error == 0, "ERROR: %s at line %d",
                                         __FUNCTION__, __LINE__);

    error = ViE.ptrViERender->AddRenderer(tbChannel.videoChannel, _window2, 1,
                                          0.0, 0.0, 1.0, 1.0);
    numberOfErrors += ViETest::TestError(error == 0, "ERROR: %s at line %d",
                                         __FUNCTION__, __LINE__);

    error = ViE.ptrViERender->StartRender(tbChannel.videoChannel);
    numberOfErrors += ViETest::TestError(error == 0, "ERROR: %s at line %d",
                                         __FUNCTION__, __LINE__);

    ViETest::Log("\nCapture device is renderered in Window 1");
    ViETest::Log("Remote stream is renderered in Window 2");
    AutoTestSleep(KAutoTestSleepTimeMs);

    error = ViE.ptrViERender->StopRender(tbCapture.captureId);
    numberOfErrors += ViETest::TestError(error == 0, "ERROR: %s at line %d",
                                         __FUNCTION__, __LINE__);

    error = ViE.ptrViERender->RemoveRenderer(tbCapture.captureId);
    numberOfErrors += ViETest::TestError(error == 0, "ERROR: %s at line %d",
                                         __FUNCTION__, __LINE__);

    // PIP and full screen rendering is not supported on Android
#ifndef WEBRTC_ANDROID
    error = ViE.ptrViERender->DeRegisterVideoRenderModule(*_vrm1);
    numberOfErrors += ViETest::TestError(error == 0, "ERROR: %s at line %d",
                                         __FUNCTION__, __LINE__);

    error = ViE.ptrViERender->AddRenderer(tbCapture.captureId, _window2, 0,
                                          0.75, 0.75, 1.0, 1.0);
    numberOfErrors += ViETest::TestError(error == 0, "ERROR: %s at line %d",
                                         __FUNCTION__, __LINE__);

    error = ViE.ptrViERender->StartRender(tbCapture.captureId);
    numberOfErrors += ViETest::TestError(error == 0, "ERROR: %s at line %d",
                                         __FUNCTION__, __LINE__);

    ViETest::Log("\nCapture device is now rendered in Window 2, PiP.");
    ViETest::Log("Switching to full screen rendering in %d seconds.\n",
                 KAutoTestSleepTimeMs / 1000);
    AutoTestSleep(KAutoTestSleepTimeMs);

    error = ViE.ptrViERender->RemoveRenderer(tbCapture.captureId);
    numberOfErrors += ViETest::TestError(error == 0, "ERROR: %s at line %d",
                                         __FUNCTION__, __LINE__);
    error = ViE.ptrViERender->RemoveRenderer(tbChannel.videoChannel);
    numberOfErrors += ViETest::TestError(error == 0, "ERROR: %s at line %d",
                                         __FUNCTION__, __LINE__);
    error = ViE.ptrViERender->DeRegisterVideoRenderModule(*_vrm2);
    numberOfErrors += ViETest::TestError(error == 0, "ERROR: %s at line %d",
                                         __FUNCTION__, __LINE__);

    // Destroy render module and create new in full screen mode
    VideoRender::DestroyVideoRender(_vrm1);
    _vrm1 = NULL;
    _vrm1 = VideoRender::CreateVideoRender(4563, _window1, true, _renderType);
    numberOfErrors += ViETest::TestError(_vrm1, "ERROR: %s at line %d",
                                         __FUNCTION__, __LINE__);

    error = ViE.ptrViERender->RegisterVideoRenderModule(*_vrm1);
    numberOfErrors += ViETest::TestError(error == 0, "ERROR: %s at line %d",
                                         __FUNCTION__, __LINE__);
    error = ViE.ptrViERender->AddRenderer(tbCapture.captureId, _window1, 0,
                                          0.75f, 0.75f, 1.0f, 1.0f);
    numberOfErrors += ViETest::TestError(error == 0, "ERROR: %s at line %d",
                                         __FUNCTION__, __LINE__);
    error = ViE.ptrViERender->StartRender(tbCapture.captureId);
    numberOfErrors += ViETest::TestError(error == 0, "ERROR: %s at line %d",
                                         __FUNCTION__, __LINE__);
    error = ViE.ptrViERender->AddRenderer(tbChannel.videoChannel, _window1, 1,
                                          0.0, 0.0, 1.0, 1.0);
    numberOfErrors += ViETest::TestError(error == 0, "ERROR: %s at line %d",
                                         __FUNCTION__, __LINE__);
    error = ViE.ptrViERender->StartRender(tbChannel.videoChannel);
    numberOfErrors += ViETest::TestError(error == 0, "ERROR: %s at line %d",
                                         __FUNCTION__, __LINE__);

    AutoTestSleep(KAutoTestSleepTimeMs);

    error = ViE.ptrViERender->RemoveRenderer(tbCapture.captureId);
    numberOfErrors += ViETest::TestError(error == 0, "ERROR: %s at line %d",
                                         __FUNCTION__, __LINE__);

    error = ViE.ptrViERender->RemoveRenderer(tbChannel.videoChannel);
    numberOfErrors += ViETest::TestError(error == 0, "ERROR: %s at line %d",
                                         __FUNCTION__, __LINE__);
    error = ViE.ptrViERender->DeRegisterVideoRenderModule(*_vrm1);
    numberOfErrors += ViETest::TestError(error == 0, "ERROR: %s at line %d",
                                         __FUNCTION__, __LINE__);

    // Destroy full screen render module and create new in normal mode
    VideoRender::DestroyVideoRender(_vrm1);
    _vrm1 = NULL;
    _vrm1 = VideoRender::CreateVideoRender(4561, _window1, false, _renderType);
    numberOfErrors += ViETest::TestError(_vrm1, "ERROR: %s at line %d",
                                         __FUNCTION__, __LINE__);
#endif

    //***************************************************************
    //	Engine ready. Begin testing class
    //***************************************************************


    //***************************************************************
    //	Testing finished. Tear down Video Engine
    //***************************************************************

    tbCapture.Disconnect(tbChannel.videoChannel);

    if (numberOfErrors > 0)
    {
        // Test failed
        ViETest::Log(" ");
        ViETest::Log(" ERROR ViERender Standard Test FAILED!");
        ViETest::Log(" Number of errors: %d", numberOfErrors);
        ViETest::Log("========================================");
        ViETest::Log(" ");
        return numberOfErrors;
    }

    ViETest::Log(" ");
    ViETest::Log(" ViERender Standard Test PASSED!");
    ViETest::Log("========================================");
    ViETest::Log(" ");

    return 0;
}

int ViEAutoTest::ViERenderExtendedTest()
{
    ViETest::Log(" ");
    ViETest::Log("========================================");
    ViETest::Log(" ViERender Extended Test\n");

    int error = 0;
    int numberOfErrors = 0;
    int rtpPort = 6000;

    tbInterfaces ViE("ViERender_API", numberOfErrors);

    // Create a video channel
    tbVideoChannel tbChannel(ViE, numberOfErrors, webrtc::kVideoCodecVP8);
    tbCaptureDevice tbCapture(ViE, numberOfErrors); // Create a capture device
    tbCapture.ConnectTo(tbChannel.videoChannel);
    tbChannel.StartReceive(rtpPort);
    tbChannel.StartSend(rtpPort);

    error = ViE.ptrViERender->RegisterVideoRenderModule(*_vrm1);
    numberOfErrors += ViETest::TestError(error == 0, "ERROR: %s at line %d",
                                         __FUNCTION__, __LINE__);

    error = ViE.ptrViERender->AddRenderer(tbCapture.captureId, _window1, 0,
                                          0.0, 0.0, 1.0, 1.0);
    numberOfErrors += ViETest::TestError(error == 0, "ERROR: %s at line %d",
                                         __FUNCTION__, __LINE__);

    error = ViE.ptrViERender->StartRender(tbCapture.captureId);
    numberOfErrors += ViETest::TestError(error == 0, "ERROR: %s at line %d",
                                         __FUNCTION__, __LINE__);

    error = ViE.ptrViERender->RegisterVideoRenderModule(*_vrm2);
    numberOfErrors += ViETest::TestError(error == 0, "ERROR: %s at line %d",
                                         __FUNCTION__, __LINE__);

    error = ViE.ptrViERender->AddRenderer(tbChannel.videoChannel, _window2, 1,
                                          0.0, 0.0, 1.0, 1.0);
    numberOfErrors += ViETest::TestError(error == 0, "ERROR: %s at line %d",
                                         __FUNCTION__, __LINE__);

    error = ViE.ptrViERender->StartRender(tbChannel.videoChannel);
    numberOfErrors += ViETest::TestError(error == 0, "ERROR: %s at line %d",
                                         __FUNCTION__, __LINE__);

    ViETest::Log("\nCapture device is renderered in Window 1");
    ViETest::Log("Remote stream is renderered in Window 2");
    AutoTestSleep(KAutoTestSleepTimeMs);

#ifdef _WIN32
    ViETest::Log("\nConfiguring Window2");
    ViETest::Log("you will see video only in first quadrant");
    error = ViE.ptrViERender->ConfigureRender(tbChannel.videoChannel, 0, 0.0f,
                                              0.0f, 0.5f, 0.5f);
    numberOfErrors += ViETest::TestError(error == 0, "ERROR: %s at line %d",
                                         __FUNCTION__, __LINE__);
    AutoTestSleep(KAutoTestSleepTimeMs);

    ViETest::Log("you will see video only in fourth quadrant");
    error = ViE.ptrViERender->ConfigureRender(tbChannel.videoChannel, 0, 0.5f,
                                              0.5f, 1.0f, 1.0f);
    numberOfErrors += ViETest::TestError(error == 0, "ERROR: %s at line %d",
                                         __FUNCTION__, __LINE__);
    AutoTestSleep(KAutoTestSleepTimeMs);

    ViETest::Log("normal video on Window2");
    error = ViE.ptrViERender->ConfigureRender(tbChannel.videoChannel, 0, 0.0f,
                                              0.0f, 1.0f, 1.0f);
    numberOfErrors += ViETest::TestError(error == 0, "ERROR: %s at line %d",
                                         __FUNCTION__, __LINE__);
    AutoTestSleep(KAutoTestSleepTimeMs);
#endif

    ViETest::Log("Mirroring Local Preview (Window1) Left-Right");
    error = ViE.ptrViERender->MirrorRenderStream(tbCapture.captureId, true,
                                                 false, true);
    numberOfErrors += ViETest::TestError(error == 0, "ERROR: %s at line %d",
                                         __FUNCTION__, __LINE__);
    AutoTestSleep(KAutoTestSleepTimeMs);

    ViETest::Log("\nMirroring Local Preview (Window1) Left-Right and Up-Down");
    error = ViE.ptrViERender->MirrorRenderStream(tbCapture.captureId, true,
                                                 true, true);
    numberOfErrors += ViETest::TestError(error == 0, "ERROR: %s at line %d",
                                         __FUNCTION__, __LINE__);
    AutoTestSleep(KAutoTestSleepTimeMs);

    ViETest::Log("\nMirroring Remote Window(Window2) Up-Down");
    error = ViE.ptrViERender->MirrorRenderStream(tbChannel.videoChannel, true,
                                                 true, false);
    numberOfErrors += ViETest::TestError(error == 0, "ERROR: %s at line %d",
                                         __FUNCTION__, __LINE__);
    AutoTestSleep(KAutoTestSleepTimeMs);

    ViETest::Log("Disabling Mirroing on Window1 and Window2");
    error = ViE.ptrViERender->MirrorRenderStream(tbCapture.captureId, false,
                                                 false, false);
    numberOfErrors += ViETest::TestError(error == 0, "ERROR: %s at line %d",
                                         __FUNCTION__, __LINE__);
    AutoTestSleep(KAutoTestSleepTimeMs);
    error = ViE.ptrViERender->MirrorRenderStream(tbChannel.videoChannel, false,
                                                 false, false);
    numberOfErrors += ViETest::TestError(error == 0, "ERROR: %s at line %d",
                                         __FUNCTION__, __LINE__);
    AutoTestSleep(KAutoTestSleepTimeMs);

    ViETest::Log("\nEnabling Full Screen render in 5 sec");

    error = ViE.ptrViERender->RemoveRenderer(tbCapture.captureId);
    numberOfErrors += ViETest::TestError(error == 0, "ERROR: %s at line %d",
                                         __FUNCTION__, __LINE__);
    error = ViE.ptrViERender->DeRegisterVideoRenderModule(*_vrm1);
    numberOfErrors += ViETest::TestError(error == 0, "ERROR: %s at line %d",
                                         __FUNCTION__, __LINE__);
    error = ViE.ptrViERender->RemoveRenderer(tbChannel.videoChannel);
    numberOfErrors += ViETest::TestError(error == 0, "ERROR: %s at line %d",
                                         __FUNCTION__, __LINE__);
    error = ViE.ptrViERender->DeRegisterVideoRenderModule(*_vrm2);
    numberOfErrors += ViETest::TestError(error == 0, "ERROR: %s at line %d",
                                         __FUNCTION__, __LINE__);

    // Destroy render module and create new in full screen mode
    VideoRender::DestroyVideoRender(_vrm1);
    _vrm1 = NULL;
    _vrm1 = VideoRender::CreateVideoRender(4563, _window1, true, _renderType);
    numberOfErrors += ViETest::TestError(_vrm1, "ERROR: %s at line %d",
                                         __FUNCTION__, __LINE__);

    error = ViE.ptrViERender->RegisterVideoRenderModule(*_vrm1);
    numberOfErrors += ViETest::TestError(error == 0, "ERROR: %s at line %d",
                                         __FUNCTION__, __LINE__);
    error = ViE.ptrViERender->AddRenderer(tbCapture.captureId, _window1, 0,
                                          0.0f, 0.0f, 1.0f, 1.0f);
    numberOfErrors += ViETest::TestError(error == 0, "ERROR: %s at line %d",
                                         __FUNCTION__, __LINE__);
    error = ViE.ptrViERender->StartRender(tbCapture.captureId);
    numberOfErrors += ViETest::TestError(error == 0, "ERROR: %s at line %d",
                                         __FUNCTION__, __LINE__);
    AutoTestSleep(KAutoTestSleepTimeMs);

    error = ViE.ptrViERender->StopRender(tbCapture.captureId);
    numberOfErrors += ViETest::TestError(error == 0, "ERROR: %s at line %d",
                                         __FUNCTION__, __LINE__);

    /* error = ViE.ptrViERender->StopRender(tbChannel.videoChannel);
     numberOfErrors += ViETest::TestError(error == 0, "ERROR: %s at line %d", __FUNCTION__, __LINE__);
     */
    ViETest::Log("\nStop renderer");

    error = ViE.ptrViERender->RemoveRenderer(tbCapture.captureId);
    numberOfErrors += ViETest::TestError(error == 0, "ERROR: %s at line %d",
                                         __FUNCTION__, __LINE__);

    /* error = ViE.ptrViERender->RemoveRenderer(tbChannel.videoChannel);
     numberOfErrors += ViETest::TestError(error == 0, "ERROR: %s at line %d", __FUNCTION__, __LINE__);
     */
    ViETest::Log("\nRemove renderer");

    error = ViE.ptrViERender->DeRegisterVideoRenderModule(*_vrm1);
    numberOfErrors += ViETest::TestError(error == 0, "ERROR: %s at line %d",
                                         __FUNCTION__, __LINE__);

    // Destroy full screen render module and create new for external rendering
    VideoRender::DestroyVideoRender(_vrm1);
    _vrm1 = NULL;
    _vrm1 = VideoRender::CreateVideoRender(4564, NULL, false, _renderType);
    numberOfErrors += ViETest::TestError(_vrm1, "ERROR: %s at line %d",
                                         __FUNCTION__, __LINE__);

    error = ViE.ptrViERender->RegisterVideoRenderModule(*_vrm1);
    numberOfErrors += ViETest::TestError(error == 0, "ERROR: %s at line %d",
                                         __FUNCTION__, __LINE__);

    ViETest::Log("\nExternal Render Test");
    ViEAutoTestExternalRenderer externalRenderObj;
    error = ViE.ptrViERender->AddRenderer(tbCapture.captureId,
                                          webrtc::kVideoI420,
                                          &externalRenderObj);
    numberOfErrors += ViETest::TestError(error == 0, "ERROR: %s at line %d",
                                         __FUNCTION__, __LINE__);
    error = ViE.ptrViERender->StartRender(tbCapture.captureId);
    numberOfErrors += ViETest::TestError(error == 0, "ERROR: %s at line %d",
                                         __FUNCTION__, __LINE__);
    AutoTestSleep(KAutoTestSleepTimeMs);

    error = ViE.ptrViERender->StopRender(tbCapture.captureId);
    numberOfErrors += ViETest::TestError(error == 0, "ERROR: %s at line %d",
                                         __FUNCTION__, __LINE__);

    error = ViE.ptrViERender->RemoveRenderer(tbCapture.captureId);
    numberOfErrors += ViETest::TestError(error == 0, "ERROR: %s at line %d",
                                         __FUNCTION__, __LINE__);
    error = ViE.ptrViERender->DeRegisterVideoRenderModule(*_vrm1);
    numberOfErrors += ViETest::TestError(error == 0, "ERROR: %s at line %d",
                                         __FUNCTION__, __LINE__);

    // Destroy render module for external rendering and create new in normal
    // mode
    VideoRender::DestroyVideoRender(_vrm1);
    _vrm1 = NULL;
    _vrm1 = VideoRender::CreateVideoRender(4561, _window1, false, _renderType);
    numberOfErrors += ViETest::TestError(_vrm1, "ERROR: %s at line %d",
                                         __FUNCTION__, __LINE__);

    tbCapture.Disconnect(tbChannel.videoChannel);

    if (numberOfErrors > 0)
    {
        // Test failed
        ViETest::Log(" ");
        ViETest::Log(" ERROR ViERender Extended Test FAILED!");
        ViETest::Log(" Number of errors: %d", numberOfErrors);
        ViETest::Log("========================================");
        ViETest::Log(" ");
        return numberOfErrors;
    }

    ViETest::Log(" ");
    ViETest::Log(" ViERender Extended Test PASSED!");
    ViETest::Log("========================================");
    ViETest::Log(" ");

    return 0;
}

int ViEAutoTest::ViERenderAPITest()
{
    ViETest::Log(" ");
    ViETest::Log("========================================");
    ViETest::Log(" ViERender API Test\n");

    int numberOfErrors = 0;

    //TODO add the real tests cases

    if (numberOfErrors > 0)
    {
        // Test failed
        ViETest::Log(" ");
        ViETest::Log(" ERROR ViERender API Test FAILED!");
        ViETest::Log(" Number of errors: %d", numberOfErrors);
        ViETest::Log("========================================");
        ViETest::Log(" ");
        return numberOfErrors;
    }

    ViETest::Log(" ");
    ViETest::Log(" ViERender API Test PASSED!");
    ViETest::Log("========================================");
    ViETest::Log(" ");

    return 0;
}
