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
// vie_autotest_image_process.cc
//

// Settings
#include "vie_autotest_defines.h"
#include "vie_autotest.h"
#include "engine_configurations.h"

#include "tb_interfaces.h"
#include "tb_video_channel.h"
#include "tb_capture_device.h"

class MyEffectFilter: public ViEEffectFilter
{
public:
    MyEffectFilter() {}

    ~MyEffectFilter() {}

    virtual int Transform(int size, unsigned char* frameBuffer,
                          unsigned int timeStamp90KHz, unsigned int width,
                          unsigned int height)
    {
        // Black and white
        memset(frameBuffer + (2 * size) / 3, 0x7f, size / 3);
        return 0;
    }
};

int ViEAutoTest::ViEImageProcessStandardTest()
{
    ViETest::Log(" ");
    ViETest::Log("========================================");
    ViETest::Log(" ViEImageProcess Standard Test\n");

    //***************************************************************
    //	Begin create/initialize WebRTC Video Engine for testing
    //***************************************************************


    int error = 0;
    bool succeeded = true;
    int numberOfErrors = 0;

    int rtpPort = 6000;
    // Create VIE
    tbInterfaces ViE("ViEImageProcessAPITest", numberOfErrors);
    // Create a video channel
    tbVideoChannel tbChannel(ViE, numberOfErrors, webrtc::kVideoCodecVP8);
    // Create a capture device
    tbCaptureDevice tbCapture(ViE, numberOfErrors);

    tbCapture.ConnectTo(tbChannel.videoChannel);
    tbChannel.StartReceive(rtpPort);
    tbChannel.StartSend(rtpPort);

    MyEffectFilter effectFilter;

    error = ViE.ptrViERender->AddRenderer(tbCapture.captureId, _window1, 0,
                                          0.0, 0.0, 1.0, 1.0);
    numberOfErrors += ViETest::TestError(error == 0, "ERROR: %s at line %d",
                                         __FUNCTION__, __LINE__);

    error = ViE.ptrViERender->StartRender(tbCapture.captureId);
    numberOfErrors += ViETest::TestError(error == 0, "ERROR: %s at line %d",
                                         __FUNCTION__, __LINE__);

    error = ViE.ptrViERender->AddRenderer(tbChannel.videoChannel, _window2, 1,
                                          0.0, 0.0, 1.0, 1.0);
    numberOfErrors += ViETest::TestError(error == 0, "ERROR: %s at line %d",
                                         __FUNCTION__, __LINE__);

    error = ViE.ptrViERender->StartRender(tbChannel.videoChannel);
    numberOfErrors += ViETest::TestError(error == 0, "ERROR: %s at line %d",
                                         __FUNCTION__, __LINE__);

    ViETest::Log("Capture device is renderered in Window 1");
    ViETest::Log("Remote stream is renderered in Window 2");
    AutoTestSleep(KAutoTestSleepTimeMs);

    //***************************************************************
    //	Engine ready. Begin testing class
    //***************************************************************


    error
        = ViE.ptrViEImageProcess->RegisterCaptureEffectFilter(
            tbCapture.captureId, effectFilter);
    numberOfErrors += ViETest::TestError(error == 0, "ERROR: %s at line %d",
                                         __FUNCTION__, __LINE__);

    ViETest::Log("Black and white filter registered for capture device, "
                 "affects both windows");
    AutoTestSleep(KAutoTestSleepTimeMs);

    error = ViE.ptrViEImageProcess->DeregisterCaptureEffectFilter(
        tbCapture.captureId);
    numberOfErrors += ViETest::TestError(error == 0, "ERROR: %s at line %d",
                                         __FUNCTION__, __LINE__);

    error = ViE.ptrViEImageProcess->RegisterRenderEffectFilter(
        tbChannel.videoChannel, effectFilter);
    numberOfErrors += ViETest::TestError(error == 0, "ERROR: %s at line %d",
                                         __FUNCTION__, __LINE__);

    ViETest::Log("Remove capture effect filter, adding filter for incoming "
                 "stream");
    ViETest::Log("Only Window 2 should be black and white");
    AutoTestSleep(KAutoTestSleepTimeMs);

    error = ViE.ptrViERender->StopRender(tbCapture.captureId);
    numberOfErrors += ViETest::TestError(error == 0, "ERROR: %s at line %d",
                                         __FUNCTION__, __LINE__);

    error = ViE.ptrViERender->RemoveRenderer(tbCapture.captureId);
    numberOfErrors += ViETest::TestError(error == 0, "ERROR: %s at line %d",
                                         __FUNCTION__, __LINE__);

    int rtpPort2 = rtpPort + 100;
    // Create a video channel
    tbVideoChannel tbChannel2(ViE, numberOfErrors, webrtc::kVideoCodecVP8);

    tbCapture.ConnectTo(tbChannel2.videoChannel);
    tbChannel2.StartReceive(rtpPort2);
    tbChannel2.StartSend(rtpPort2);

    error = ViE.ptrViERender->AddRenderer(tbChannel2.videoChannel, _window1, 1,
                                          0.0, 0.0, 1.0, 1.0);
    numberOfErrors += ViETest::TestError(error == 0, "ERROR: %s at line %d",
                                         __FUNCTION__, __LINE__);

    error = ViE.ptrViERender->StartRender(tbChannel2.videoChannel);
    numberOfErrors += ViETest::TestError(error == 0, "ERROR: %s at line %d",
                                         __FUNCTION__, __LINE__);

    error = ViE.ptrViEImageProcess->DeregisterRenderEffectFilter(
        tbChannel.videoChannel);
    numberOfErrors += ViETest::TestError(error == 0, "ERROR: %s at line %d",
                                         __FUNCTION__, __LINE__);

    ViETest::Log("Local renderer removed, added new channel and rendering in "
                 "Window1.");

    error = ViE.ptrViEImageProcess->RegisterCaptureEffectFilter(
        tbCapture.captureId, effectFilter);
    numberOfErrors += ViETest::TestError(error == 0, "ERROR: %s at line %d",
                                         __FUNCTION__, __LINE__);

    ViETest::Log("Black and white filter registered for capture device, "
                 "affects both windows");
    AutoTestSleep(KAutoTestSleepTimeMs);

    error = ViE.ptrViEImageProcess->DeregisterCaptureEffectFilter(
        tbCapture.captureId);
    numberOfErrors += ViETest::TestError(error == 0, "ERROR: %s at line %d",
                                         __FUNCTION__, __LINE__);

    error = ViE.ptrViEImageProcess->RegisterSendEffectFilter(
        tbChannel.videoChannel, effectFilter);
    numberOfErrors += ViETest::TestError(error == 0, "ERROR: %s at line %d",
                                         __FUNCTION__, __LINE__);

    ViETest::Log("Capture filter removed.");
    ViETest::Log("Black and white filter registered for one channel, Window2 "
                 "should be black and white");
    AutoTestSleep(KAutoTestSleepTimeMs);

    error = ViE.ptrViEImageProcess->DeregisterSendEffectFilter(
        tbChannel.videoChannel);
    numberOfErrors += ViETest::TestError(error == 0, "ERROR: %s at line %d",
                                         __FUNCTION__, __LINE__);

    //***************************************************************
    //	Testing finished. Tear down Video Engine
    //***************************************************************

    if (numberOfErrors > 0)
    {
        // Test failed
        ViETest::Log(" ");
        ViETest::Log(" ERROR ViEImageProcess Standard Test FAILED!");
        ViETest::Log(" Number of errors: %d", numberOfErrors);
        ViETest::Log("========================================");
        ViETest::Log(" ");
        return numberOfErrors;
    }

    ViETest::Log(" ");
    ViETest::Log(" ViEImageProcess Standard Test PASSED!");
    ViETest::Log("========================================");
    ViETest::Log(" ");
    return 0;
}

int ViEAutoTest::ViEImageProcessExtendedTest()
{
    ViETest::Log(" ");
    ViETest::Log("========================================");
    ViETest::Log(" ViEImageProcess Extended Test\n");

    int error = 0;
    bool succeeded = true;
    int numberOfErrors = 0;

    numberOfErrors = ViEImageProcessStandardTest();

    if (numberOfErrors > 0)
    {
        // Test failed
        ViETest::Log(" ");
        ViETest::Log(" ERROR ViEImageProcess Extended Test FAILED!");
        ViETest::Log(" Number of errors: %d", numberOfErrors);
        ViETest::Log("========================================");
        ViETest::Log(" ");
        return numberOfErrors;
    }

    ViETest::Log(" ");
    ViETest::Log(" ViEImageProcess Extended Test PASSED!");
    ViETest::Log("========================================");
    ViETest::Log(" ");
    return 0;
}

int ViEAutoTest::ViEImageProcessAPITest()
{
    ViETest::Log(" ");
    ViETest::Log("========================================");
    ViETest::Log(" ViEImageProcess API Test\n");

    int error = 0;
    bool succeeded = true;
    int numberOfErrors = 0;

    int rtpPort = 6000;
    tbInterfaces ViE("ViEImageProcessAPITest", numberOfErrors);
    tbVideoChannel tbChannel(ViE, numberOfErrors, webrtc::kVideoCodecVP8);
    tbCaptureDevice tbCapture(ViE, numberOfErrors);

    tbCapture.ConnectTo(tbChannel.videoChannel);

    MyEffectFilter effectFilter;

    //
    // Capture effect filter
    //
    // Add effect filter
    error = ViE.ptrViEImageProcess->RegisterCaptureEffectFilter(
        tbCapture.captureId, effectFilter);
    numberOfErrors += ViETest::TestError(error == 0, "ERROR: %s at line %d",
                                         __FUNCTION__, __LINE__);
    // Add again -> error
    error = ViE.ptrViEImageProcess->RegisterCaptureEffectFilter(
        tbCapture.captureId, effectFilter);
    numberOfErrors += ViETest::TestError(error != 0, "ERROR: %s at line %d",
                                         __FUNCTION__, __LINE__);
    error = ViE.ptrViEImageProcess->DeregisterCaptureEffectFilter(
        tbCapture.captureId);
    numberOfErrors += ViETest::TestError(error == 0, "ERROR: %s at line %d",
                                         __FUNCTION__, __LINE__);
    // Double deregister
    error = ViE.ptrViEImageProcess->DeregisterCaptureEffectFilter(
        tbCapture.captureId);
    numberOfErrors += ViETest::TestError(error != 0, "ERROR: %s at line %d",
                                         __FUNCTION__, __LINE__);
    // Non-existing capture device
    error = ViE.ptrViEImageProcess->RegisterCaptureEffectFilter(
        tbChannel.videoChannel, effectFilter);
    numberOfErrors += ViETest::TestError(error != 0, "ERROR: %s at line %d",
                                         __FUNCTION__, __LINE__);

    //
    // Render effect filter
    //
    error = ViE.ptrViEImageProcess->RegisterRenderEffectFilter(
        tbChannel.videoChannel, effectFilter);
    numberOfErrors += ViETest::TestError(error == 0, "ERROR: %s at line %d",
                                         __FUNCTION__, __LINE__);
    error = ViE.ptrViEImageProcess->RegisterRenderEffectFilter(
        tbChannel.videoChannel, effectFilter);
    numberOfErrors += ViETest::TestError(error != 0, "ERROR: %s at line %d",
                                         __FUNCTION__, __LINE__);
    error = ViE.ptrViEImageProcess->DeregisterRenderEffectFilter(
        tbChannel.videoChannel);
    numberOfErrors += ViETest::TestError(error == 0, "ERROR: %s at line %d",
                                         __FUNCTION__, __LINE__);
    error = ViE.ptrViEImageProcess->DeregisterRenderEffectFilter(
        tbChannel.videoChannel);
    numberOfErrors += ViETest::TestError(error != 0, "ERROR: %s at line %d",
                                         __FUNCTION__, __LINE__);
    // Non-existing channel id
    error = ViE.ptrViEImageProcess->RegisterRenderEffectFilter(
        tbCapture.captureId, effectFilter);
    numberOfErrors += ViETest::TestError(error != 0, "ERROR: %s at line %d",
                                         __FUNCTION__, __LINE__);

    //
    // Send effect filter
    //
    error = ViE.ptrViEImageProcess->RegisterSendEffectFilter(
        tbChannel.videoChannel, effectFilter);
    numberOfErrors += ViETest::TestError(error == 0, "ERROR: %s at line %d",
                                         __FUNCTION__, __LINE__);
    error = ViE.ptrViEImageProcess->RegisterSendEffectFilter(
        tbChannel.videoChannel, effectFilter);
    numberOfErrors += ViETest::TestError(error != 0, "ERROR: %s at line %d",
                                         __FUNCTION__, __LINE__);
    error = ViE.ptrViEImageProcess->DeregisterSendEffectFilter(
        tbChannel.videoChannel);
    numberOfErrors += ViETest::TestError(error == 0, "ERROR: %s at line %d",
                                         __FUNCTION__, __LINE__);
    error = ViE.ptrViEImageProcess->DeregisterSendEffectFilter(
        tbChannel.videoChannel);
    numberOfErrors += ViETest::TestError(error != 0, "ERROR: %s at line %d",
                                         __FUNCTION__, __LINE__);
    error = ViE.ptrViEImageProcess->RegisterSendEffectFilter(
        tbCapture.captureId, effectFilter);
    numberOfErrors += ViETest::TestError(error != 0, "ERROR: %s at line %d",
                                         __FUNCTION__, __LINE__);

    //
    // Denoising
    //
    error = ViE.ptrViEImageProcess->EnableDenoising(tbCapture.captureId, true);
    numberOfErrors += ViETest::TestError(error == 0, "ERROR: %s at line %d",
                                         __FUNCTION__, __LINE__);
    error = ViE.ptrViEImageProcess->EnableDenoising(tbCapture.captureId, true);
    numberOfErrors += ViETest::TestError(error != 0, "ERROR: %s at line %d",
                                         __FUNCTION__, __LINE__);
    error = ViE.ptrViEImageProcess->EnableDenoising(tbCapture.captureId, false);
    numberOfErrors += ViETest::TestError(error == 0, "ERROR: %s at line %d",
                                         __FUNCTION__, __LINE__);
    error = ViE.ptrViEImageProcess->EnableDenoising(tbCapture.captureId, false);
    numberOfErrors += ViETest::TestError(error != 0, "ERROR: %s at line %d",
                                         __FUNCTION__, __LINE__);
    error = ViE.ptrViEImageProcess->EnableDenoising(tbChannel.videoChannel,
                                                    true);
    numberOfErrors += ViETest::TestError(error != 0, "ERROR: %s at line %d",
                                         __FUNCTION__, __LINE__);

    //
    // Deflickering
    //
    error = ViE.ptrViEImageProcess->EnableDeflickering(tbCapture.captureId,
                                                       true);
    numberOfErrors += ViETest::TestError(error == 0, "ERROR: %s at line %d",
                                         __FUNCTION__, __LINE__);
    error = ViE.ptrViEImageProcess->EnableDeflickering(tbCapture.captureId,
                                                       true);
    numberOfErrors += ViETest::TestError(error != 0, "ERROR: %s at line %d",
                                         __FUNCTION__, __LINE__);
    error = ViE.ptrViEImageProcess->EnableDeflickering(tbCapture.captureId,
                                                       false);
    numberOfErrors += ViETest::TestError(error == 0, "ERROR: %s at line %d",
                                         __FUNCTION__, __LINE__);
    error = ViE.ptrViEImageProcess->EnableDeflickering(tbCapture.captureId,
                                                       false);
    numberOfErrors += ViETest::TestError(error != 0, "ERROR: %s at line %d",
                                         __FUNCTION__, __LINE__);
    error = ViE.ptrViEImageProcess->EnableDeflickering(tbChannel.videoChannel,
                                                       true);
    numberOfErrors += ViETest::TestError(error != 0, "ERROR: %s at line %d",
                                         __FUNCTION__, __LINE__);

    //
    // Color enhancement
    //
    error = ViE.ptrViEImageProcess->EnableColorEnhancement(
        tbChannel.videoChannel, false);
    numberOfErrors += ViETest::TestError(error == 0, "ERROR: %s at line %d",
                                         __FUNCTION__, __LINE__);
    error = ViE.ptrViEImageProcess->EnableColorEnhancement(
        tbChannel.videoChannel, true);
    numberOfErrors += ViETest::TestError(error == 0, "ERROR: %s at line %d",
                                         __FUNCTION__, __LINE__);
    error = ViE.ptrViEImageProcess->EnableColorEnhancement(
        tbChannel.videoChannel, true);
    numberOfErrors += ViETest::TestError(error != 0, "ERROR: %s at line %d",
                                         __FUNCTION__, __LINE__);
    error = ViE.ptrViEImageProcess->EnableColorEnhancement(
        tbChannel.videoChannel, false);
    numberOfErrors += ViETest::TestError(error == 0, "ERROR: %s at line %d",
                                         __FUNCTION__, __LINE__);
    error = ViE.ptrViEImageProcess->EnableColorEnhancement(
        tbChannel.videoChannel, false);
    numberOfErrors += ViETest::TestError(error != 0, "ERROR: %s at line %d",
                                         __FUNCTION__, __LINE__);
    error = ViE.ptrViEImageProcess->EnableColorEnhancement(tbCapture.captureId,
                                                           true);
    numberOfErrors += ViETest::TestError(error != 0, "ERROR: %s at line %d",
                                         __FUNCTION__, __LINE__);

    if (numberOfErrors > 0)
    {
        // Test failed
        ViETest::Log(" ");
        ViETest::Log(" ERROR ViEImageProcess Extended Test FAILED!");
        ViETest::Log(" Number of errors: %d", numberOfErrors);
        ViETest::Log("========================================");
        ViETest::Log(" ");
        return numberOfErrors;
    }

    ViETest::Log(" ");
    ViETest::Log(" ViEImageProcess Extended Test PASSED!");
    ViETest::Log("========================================");
    ViETest::Log(" ");
    return 0;
}
