/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "vie_autotest.h"

#include "codec_primitives.h"
#include "common_types.h"
#include "general_primitives.h"
#include "tb_capture_device.h"
#include "tb_I420_codec.h"
#include "tb_interfaces.h"
#include "tb_video_channel.h"
#include "vie_autotest_defines.h"

int ViEAutoTest::ViECodecStandardTest()
{
    ViETest::Log(" ");
    ViETest::Log("========================================");
    ViETest::Log(" ViECodec Standard Test\n");

    int number_of_errors = 0;

    TbInterfaces interfaces = TbInterfaces("ViECodecStandardTest",
                                           number_of_errors);

    TbCaptureDevice capture_device =
        TbCaptureDevice(interfaces, number_of_errors);

    int video_channel = -1;

    int error = interfaces.base->CreateChannel(video_channel);
    ViETest::TestError(error == 0, "ERROR: %s at line %d",
                       __FUNCTION__, __LINE__);
    error = interfaces.capture->ConnectCaptureDevice(capture_device.captureId,
                                                     video_channel);
    ViETest::TestError(error == 0, "ERROR: %s at line %d",
                       __FUNCTION__, __LINE__);

    ConfigureRtpRtcp(interfaces.rtp_rtcp,
                     &number_of_errors,
                     video_channel);

    RenderInWindow(interfaces.render, &number_of_errors,
                   capture_device.captureId, _window1, 0);
    RenderInWindow(interfaces.render, &number_of_errors,
                   video_channel, _window2, 1);

    TestCodecs(interfaces, number_of_errors, capture_device.captureId,
               video_channel, kDoNotForceResolution, kDoNotForceResolution);

    if (number_of_errors > 0)
    {
        // Test failed
        ViETest::Log(" ");
        ViETest::Log(" ERROR ViECodec Standard Test FAILED!");
        ViETest::Log(" Number of errors: %d", number_of_errors);
        ViETest::Log("========================================");
        ViETest::Log(" ");
        return number_of_errors;
    }

    ViETest::Log(" ");
    ViETest::Log(" ViECodec Standard Test PASSED!");
    ViETest::Log("========================================");
    ViETest::Log(" ");
    return 0;
}

int ViEAutoTest::ViECodecExtendedTest()
{
    int error = 0;
    int numberOfErrors = 0;

    {
        ViETest::Log(" ");
        ViETest::Log("========================================");
        ViETest::Log(" ViECodec Extended Test\n");

        numberOfErrors = ViECodecAPITest();
        numberOfErrors += ViECodecStandardTest();
        numberOfErrors += ViECodecExternalCodecTest();

        TbInterfaces interfaces = TbInterfaces("ViECodecExtendedTest",
                                               numberOfErrors);
        webrtc::ViEBase* ptrViEBase = interfaces.base;
        webrtc::ViECapture* ptrViECapture = interfaces.capture;
        webrtc::ViERender* ptrViERender = interfaces.render;
        webrtc::ViECodec* ptrViECodec = interfaces.codec;
        webrtc::ViERTP_RTCP* ptrViERtpRtcp = interfaces.rtp_rtcp;
        webrtc::ViENetwork* ptrViENetwork = interfaces.network;

        TbCaptureDevice captureDevice = TbCaptureDevice(interfaces,
                                                        numberOfErrors);
        int captureId = captureDevice.captureId;

        int videoChannel = -1;
        error = ptrViEBase->CreateChannel(videoChannel);
        numberOfErrors += ViETest::TestError(error == 0, "ERROR: %s at line %d",
                                             __FUNCTION__, __LINE__);

        error = ptrViECapture->ConnectCaptureDevice(captureId, videoChannel);
        numberOfErrors += ViETest::TestError(error == 0, "ERROR: %s at line %d",
                                             __FUNCTION__, __LINE__);

        error = ptrViERtpRtcp->SetRTCPStatus(videoChannel,
                                             webrtc::kRtcpCompound_RFC4585);
        numberOfErrors += ViETest::TestError(error == 0, "ERROR: %s at line %d",
                                             __FUNCTION__, __LINE__);

        error = ptrViERtpRtcp->SetKeyFrameRequestMethod(
            videoChannel, webrtc::kViEKeyFrameRequestPliRtcp);
        numberOfErrors += ViETest::TestError(error == 0, "ERROR: %s at line %d",
                                             __FUNCTION__, __LINE__);

        error = ptrViERtpRtcp->SetTMMBRStatus(videoChannel, true);
        numberOfErrors += ViETest::TestError(error == 0, "ERROR: %s at line %d",
                                             __FUNCTION__, __LINE__);

        error = ptrViERender->AddRenderer(captureId, _window1, 0, 0.0, 0.0,
                                          1.0, 1.0);
        numberOfErrors += ViETest::TestError(error == 0, "ERROR: %s at line %d",
                                             __FUNCTION__, __LINE__);

        error = ptrViERender->AddRenderer(videoChannel, _window2, 1, 0.0, 0.0,
                                          1.0, 1.0);
        numberOfErrors += ViETest::TestError(error == 0, "ERROR: %s at line %d",
                                             __FUNCTION__, __LINE__);

        error = ptrViERender->StartRender(captureId);
        numberOfErrors += ViETest::TestError(error == 0,
                                             "ERROR: %s at line %d",
                                             __FUNCTION__, __LINE__);

        error = ptrViERender->StartRender(videoChannel);
        numberOfErrors += ViETest::TestError(error == 0, "ERROR: %s at line %d",
                                             __FUNCTION__, __LINE__);

        webrtc::VideoCodec videoCodec;
        memset(&videoCodec, 0, sizeof(webrtc::VideoCodec));
        for (int idx = 0; idx < ptrViECodec->NumberOfCodecs(); idx++)
        {
            error = ptrViECodec->GetCodec(idx, videoCodec);
            numberOfErrors += ViETest::TestError(error == 0,
                                                 "ERROR: %s at line %d",
                                                 __FUNCTION__, __LINE__);

            if (videoCodec.codecType != webrtc::kVideoCodecH263
                && videoCodec.codecType != webrtc::kVideoCodecI420)
            {
                videoCodec.width = 640;
                videoCodec.height = 480;
            }
            error = ptrViECodec->SetReceiveCodec(videoChannel, videoCodec);
            numberOfErrors += ViETest::TestError(error == 0,
                                                 "ERROR: %s at line %d",
                                                 __FUNCTION__, __LINE__);
        }

        const char* ipAddress = "127.0.0.1";
        const unsigned short rtpPort = 6000;
        error = ptrViENetwork->SetLocalReceiver(videoChannel, rtpPort);
        numberOfErrors += ViETest::TestError(error == 0, "ERROR: %s at line %d",
                                             __FUNCTION__, __LINE__);

        error = ptrViEBase->StartReceive(videoChannel);
        numberOfErrors += ViETest::TestError(error == 0, "ERROR: %s at line %d",
                                             __FUNCTION__, __LINE__);

        error = ptrViENetwork->SetSendDestination(videoChannel, ipAddress,
                                                  rtpPort);
        numberOfErrors += ViETest::TestError(error == 0, "ERROR: %s at line %d",
                                             __FUNCTION__, __LINE__);

        error = ptrViEBase->StartSend(videoChannel);
        numberOfErrors += ViETest::TestError(error == 0, "ERROR: %s at line %d",
                                             __FUNCTION__, __LINE__);

        //
        // Codec specific tests
        //
        memset(&videoCodec, 0, sizeof(webrtc::VideoCodec));
        error = ptrViEBase->StopSend(videoChannel);
        numberOfErrors += ViETest::TestError(error == 0, "ERROR: %s at line %d",
                                             __FUNCTION__, __LINE__);

        ViEAutotestCodecObserver codecObserver;
        error = ptrViECodec->RegisterEncoderObserver(videoChannel,
                                                     codecObserver);
        numberOfErrors += ViETest::TestError(error == 0, "ERROR: %s at line %d",
                                             __FUNCTION__, __LINE__);
        error = ptrViECodec->RegisterDecoderObserver(videoChannel,
                                                     codecObserver);
        numberOfErrors += ViETest::TestError(error == 0, "ERROR: %s at line %d",
                                             __FUNCTION__, __LINE__);

        //***************************************************************
        //	Testing finished. Tear down Video Engine
        //***************************************************************

        error = ptrViEBase->StopReceive(videoChannel);
        numberOfErrors += ViETest::TestError(error == 0, "ERROR: %s at line %d",
                                             __FUNCTION__, __LINE__);

        error = ptrViEBase->StopSend(videoChannel); // Already stopped
        numberOfErrors += ViETest::TestError(error == -1,
                                             "ERROR: %s at line %d",
                                             __FUNCTION__, __LINE__);

        error = ptrViERender->StopRender(videoChannel);
        numberOfErrors += ViETest::TestError(error == 0, "ERROR: %s at line %d",
                                             __FUNCTION__, __LINE__);

        error = ptrViERender->RemoveRenderer(captureId);
        numberOfErrors += ViETest::TestError(error == 0, "ERROR: %s at line %d",
                                             __FUNCTION__, __LINE__);

        error = ptrViERender->RemoveRenderer(videoChannel);
        numberOfErrors += ViETest::TestError(error == 0, "ERROR: %s at line %d",
                                             __FUNCTION__, __LINE__);

        error = ptrViECapture->DisconnectCaptureDevice(videoChannel);
        numberOfErrors += ViETest::TestError(error == 0, "ERROR: %s at line %d",
                                             __FUNCTION__, __LINE__);

        error = ptrViEBase->DeleteChannel(videoChannel);
        numberOfErrors += ViETest::TestError(error == 0, "ERROR: %s at line %d",
                                             __FUNCTION__, __LINE__);
    }

    //
    // Default channel
    //
    {
        // Create VIE
        TbInterfaces ViE("ViECodecExtendedTest2", numberOfErrors);
        // Create a capture device
        TbCaptureDevice tbCapture(ViE, numberOfErrors);

        // Create channel 1
        int videoChannel1 = -1;
        error = ViE.base->CreateChannel(videoChannel1);
        numberOfErrors += ViETest::TestError(error == 0, "ERROR: %s at line %d",
                                             __FUNCTION__, __LINE__);

        unsigned short rtpPort1 = 12000;
        error = ViE.network->SetLocalReceiver(videoChannel1,
                                              rtpPort1);
        numberOfErrors += ViETest::TestError(error == 0, "ERROR: %s at line %d",
                                             __FUNCTION__, __LINE__);
        error = ViE.network->SetSendDestination(videoChannel1,
                                                "127.0.0.1", rtpPort1);
        numberOfErrors += ViETest::TestError(error == 0, "ERROR: %s at line %d",
                                             __FUNCTION__, __LINE__);

        tbCapture.ConnectTo(videoChannel1);

        error = ViE.rtp_rtcp->SetKeyFrameRequestMethod(
            videoChannel1, webrtc::kViEKeyFrameRequestPliRtcp);
        numberOfErrors += ViETest::TestError(error == 0, "ERROR: %s at line %d",
                                             __FUNCTION__, __LINE__);

        error = ViE.render->AddRenderer(videoChannel1, _window1,
                                        0, 0.0, 0.0, 1.0, 1.0);
        numberOfErrors += ViETest::TestError(error == 0, "ERROR: %s at line %d",
                                             __FUNCTION__, __LINE__);
        error = ViE.render->StartRender(videoChannel1);
        numberOfErrors += ViETest::TestError(error == 0, "ERROR: %s at line %d",
                                             __FUNCTION__, __LINE__);

        ViEAutotestCodecObserver codecObserver1;
        error = ViE.codec->RegisterEncoderObserver(videoChannel1,
                                                   codecObserver1);
        numberOfErrors += ViETest::TestError(error == 0, "ERROR: %s at line %d",
                                             __FUNCTION__, __LINE__);
        error = ViE.codec->RegisterDecoderObserver(videoChannel1,
                                                   codecObserver1);
        numberOfErrors += ViETest::TestError(error == 0, "ERROR: %s at line %d",
                                             __FUNCTION__, __LINE__);

        // Set Send codec
        unsigned short codecWidth = 176;
        unsigned short codecHeight = 144;
        bool codecSet = false;
        webrtc::VideoCodec videoCodec;
        for (int idx = 0; idx < ViE.codec->NumberOfCodecs(); idx++)
        {
            error = ViE.codec->GetCodec(idx, videoCodec);
            numberOfErrors += ViETest::TestError(error == 0,
                                                 "ERROR: %s at line %d",
                                                 __FUNCTION__, __LINE__);
            error = ViE.codec->SetReceiveCodec(videoChannel1, videoCodec);
            numberOfErrors += ViETest::TestError(error == 0,
                                                 "ERROR: %s at line %d",
                                                 __FUNCTION__, __LINE__);
            if (videoCodec.codecType == webrtc::kVideoCodecVP8)
            {
                videoCodec.width = codecWidth;
                videoCodec.height = codecHeight;
                videoCodec.startBitrate = 200;
                videoCodec.maxBitrate = 300;
                error = ViE.codec->SetSendCodec(videoChannel1, videoCodec);
                numberOfErrors += ViETest::TestError(error == 0,
                                                     "ERROR: %s at line %d",
                                                     __FUNCTION__, __LINE__);
                codecSet = true;
                break;
            }
        }
        numberOfErrors += ViETest::TestError(codecSet, "ERROR: %s at line %d",
                                             __FUNCTION__, __LINE__);

        error = ViE.base->StartSend(videoChannel1);
        numberOfErrors += ViETest::TestError(error == 0, "ERROR: %s at line %d",
                                             __FUNCTION__, __LINE__);
        error = ViE.base->StartReceive(videoChannel1);
        numberOfErrors += ViETest::TestError(error == 0, "ERROR: %s at line %d",
                                             __FUNCTION__, __LINE__);

        // Create channel 2, based on channel 1
        int videoChannel2 = -1;
        error = ViE.base->CreateChannel(videoChannel2, videoChannel1);
        numberOfErrors += ViETest::TestError(error == 0, "ERROR: %s at line %d",
                                             __FUNCTION__, __LINE__);
        numberOfErrors += ViETest::TestError(videoChannel1 != videoChannel2,
                                             "ERROR: %s at line %d",
                                             __FUNCTION__, __LINE__);

        error = ViE.rtp_rtcp->SetKeyFrameRequestMethod(
            videoChannel2, webrtc::kViEKeyFrameRequestPliRtcp);
        numberOfErrors += ViETest::TestError(error == 0, "ERROR: %s at line %d",
                                             __FUNCTION__, __LINE__);

        // Prepare receive codecs
        for (int idx = 0; idx < ViE.codec->NumberOfCodecs(); idx++)
        {
            error = ViE.codec->GetCodec(idx, videoCodec);
            numberOfErrors += ViETest::TestError(error == 0,
                                                 "ERROR: %s at line %d",
                                                 __FUNCTION__, __LINE__);
            error = ViE.codec->SetReceiveCodec(videoChannel2, videoCodec);
            numberOfErrors += ViETest::TestError(error == 0,
                                                 "ERROR: %s at line %d",
                                                 __FUNCTION__, __LINE__);
        }

        ViEAutotestCodecObserver codecObserver2;
        error = ViE.codec->RegisterDecoderObserver(videoChannel2,
                                                   codecObserver2);
        numberOfErrors += ViETest::TestError(error == 0, "ERROR: %s at line %d",
                                             __FUNCTION__, __LINE__);

        error = ViE.render->AddRenderer(videoChannel2, _window2,
                                        0, 0.0, 0.0, 1.0, 1.0);
        numberOfErrors += ViETest::TestError(error == 0, "ERROR: %s at line %d",
                                             __FUNCTION__, __LINE__);
        error = ViE.render->StartRender(videoChannel2);
        numberOfErrors += ViETest::TestError(error == 0, "ERROR: %s at line %d",
                                             __FUNCTION__, __LINE__);

        unsigned short rtpPort2 = 13000;
        error = ViE.network->SetLocalReceiver(videoChannel2,
                                              rtpPort2);
        numberOfErrors += ViETest::TestError(error == 0, "ERROR: %s at line %d",
                                             __FUNCTION__, __LINE__);
        error = ViE.network->SetSendDestination(videoChannel2,
                                                "127.0.0.1", rtpPort2);
        numberOfErrors += ViETest::TestError(error == 0, "ERROR: %s at line %d",
                                             __FUNCTION__, __LINE__);

        error = ViE.base->StartReceive(videoChannel2);
        numberOfErrors += ViETest::TestError(error == 0, "ERROR: %s at line %d",
                                             __FUNCTION__, __LINE__);
        error = ViE.base->StartSend(videoChannel2);
        numberOfErrors += ViETest::TestError(error == 0, "ERROR: %s at line %d",
                                             __FUNCTION__, __LINE__);

        ViETest::Log("\nTest using one encoder on several channels");
        ViETest::Log("Channel 1 is rendered in Window1, channel 2 in Window 2."
                     "\nSending VP8 on both channels");

        AutoTestSleep(KAutoTestSleepTimeMs);

        // Check that we received H.263 on both channels
        numberOfErrors += ViETest::TestError(
            codecObserver1.incomingCodec.codecType == webrtc::kVideoCodecVP8
            && codecObserver1.incomingCodec.width == 176,
            "ERROR: %s at line %d", __FUNCTION__, __LINE__);
        numberOfErrors += ViETest::TestError(
            codecObserver2.incomingCodec.codecType ==
                webrtc::kVideoCodecVP8
            && codecObserver2.incomingCodec.width == 176,
            "ERROR: %s at line %d", __FUNCTION__, __LINE__);

        // Delete the first channel and keep the second
        error = ViE.base->DeleteChannel(videoChannel1);
        numberOfErrors += ViETest::TestError(error == 0, "ERROR: %s at line %d",
                                             __FUNCTION__, __LINE__);

        ViETest::Log("Channel 1 deleted, you should only see video in Window "
                     "2");

        AutoTestSleep(KAutoTestSleepTimeMs);

        // Create another channel
        int videoChannel3 = -1;
        error = ViE.base->CreateChannel(videoChannel3, videoChannel2);
        numberOfErrors += ViETest::TestError(error == 0, "ERROR: %s at line %d",
                                             __FUNCTION__, __LINE__);
        numberOfErrors += ViETest::TestError(videoChannel3 != videoChannel2,
                                             "ERROR: %s at line %d",
                                             __FUNCTION__, __LINE__);

        error = ViE.rtp_rtcp->SetKeyFrameRequestMethod(
            videoChannel3, webrtc::kViEKeyFrameRequestPliRtcp);
        numberOfErrors += ViETest::TestError(error == 0, "ERROR: %s at line %d",
                                             __FUNCTION__, __LINE__);

        // Prepare receive codecs
        for (int idx = 0; idx < ViE.codec->NumberOfCodecs(); idx++)
        {
            error = ViE.codec->GetCodec(idx, videoCodec);
            numberOfErrors += ViETest::TestError(error == 0,
                                                 "ERROR: %s at line %d",
                                                 __FUNCTION__, __LINE__);
            error = ViE.codec->SetReceiveCodec(videoChannel3,
                                               videoCodec);
            numberOfErrors += ViETest::TestError(error == 0,
                                                 "ERROR: %s at line %d",
                                                 __FUNCTION__, __LINE__);
        }

        ViEAutotestCodecObserver codecObserver3;
        error = ViE.codec->RegisterDecoderObserver(videoChannel3,
                                                   codecObserver3);
        numberOfErrors += ViETest::TestError(error == 0, "ERROR: %s at line %d",
                                             __FUNCTION__, __LINE__);

        error = ViE.render->AddRenderer(videoChannel3, _window1, 0, 0.0,
                                        0.0, 1.0, 1.0);
        numberOfErrors += ViETest::TestError(error == 0, "ERROR: %s at line %d",
                                             __FUNCTION__, __LINE__);
        error = ViE.render->StartRender(videoChannel3);
        numberOfErrors += ViETest::TestError(error == 0, "ERROR: %s at line %d",
                                             __FUNCTION__, __LINE__);

        unsigned short rtpPort3 = 14000;
        error = ViE.network->SetLocalReceiver(videoChannel3, rtpPort3);
        numberOfErrors += ViETest::TestError(error == 0, "ERROR: %s at line %d",
                                             __FUNCTION__, __LINE__);
        error = ViE.network->SetSendDestination(videoChannel3,
                                                "127.0.0.1", rtpPort3);
        numberOfErrors += ViETest::TestError(error == 0, "ERROR: %s at line %d",
                                             __FUNCTION__, __LINE__);

        error = ViE.base->StartReceive(videoChannel3);
        numberOfErrors += ViETest::TestError(error == 0, "ERROR: %s at line %d",
                                             __FUNCTION__, __LINE__);
        error = ViE.base->StartSend(videoChannel3);
        numberOfErrors += ViETest::TestError(error == 0, "ERROR: %s at line %d",
                                             __FUNCTION__, __LINE__);

        error = ViE.base->DeleteChannel(videoChannel2);
        numberOfErrors += ViETest::TestError(error == 0, "ERROR: %s at line %d",
                                             __FUNCTION__, __LINE__);

        ViETest::Log("A third channel created and rendered in Window 1,\n"
            "channel 2 is deleted and you should only see video in Window 1");

        AutoTestSleep(KAutoTestSleepTimeMs);

        error = ViE.base->DeleteChannel(videoChannel3);
        numberOfErrors += ViETest::TestError(error == 0, "ERROR: %s at line %d",
                                             __FUNCTION__, __LINE__);
    }

    // SetKeyFrameRequestCallbackStatus
    // Check callback

    // SetPacketLossBitrateAdaptationStatus
    // Check bitrate changes/doesn't change

    // GetAvailableBandwidth

    // SendKeyFrame

    if (numberOfErrors > 0)
    {
        // Test failed
        ViETest::Log(" ");
        ViETest::Log(" ERROR ViECodec Extended Test FAILED!");
        ViETest::Log(" Number of errors: %d", numberOfErrors);
        ViETest::Log("========================================");
        ViETest::Log(" ");
        return numberOfErrors;
    }

    ViETest::Log(" ");
    ViETest::Log(" ViECodec Extended Test PASSED!");
    ViETest::Log("========================================");
    ViETest::Log(" ");
    return 0;
}

int ViEAutoTest::ViECodecAPITest()
{
    ViETest::Log(" ");
    ViETest::Log("========================================");
    ViETest::Log(" ViECodec API Test\n");

    // ***************************************************************
    // Begin create/initialize WebRTC Video Engine for testing
    // ***************************************************************

    int error = 0;
    int numberOfErrors = 0;

    webrtc::VideoEngine* ptrViE = NULL;
    ptrViE = webrtc::VideoEngine::Create();
    numberOfErrors += ViETest::TestError(ptrViE != NULL, "ERROR: %s at line %d",
                                         __FUNCTION__, __LINE__);

    webrtc::ViEBase* ptrViEBase = webrtc::ViEBase::GetInterface(ptrViE);
    numberOfErrors += ViETest::TestError(ptrViEBase != NULL,
                                         "ERROR: %s at line %d", __FUNCTION__,
                                         __LINE__);

    error = ptrViEBase->Init();
    numberOfErrors += ViETest::TestError(error == 0, "ERROR: %s at line %d",
                                         __FUNCTION__, __LINE__);

    int videoChannel = -1;
    error = ptrViEBase->CreateChannel(videoChannel);
    numberOfErrors += ViETest::TestError(error == 0, "ERROR: %s at line %d",
                                         __FUNCTION__, __LINE__);

    webrtc::ViECodec* ptrViECodec = webrtc::ViECodec::GetInterface(ptrViE);
    numberOfErrors += ViETest::TestError(ptrViECodec != NULL,
                                         "ERROR: %s at line %d", __FUNCTION__,
                                         __LINE__);

    //***************************************************************
    //	Engine ready. Begin testing class
    //***************************************************************

    //
    // SendCodec
    //
    webrtc::VideoCodec videoCodec;
    memset(&videoCodec, 0, sizeof(webrtc::VideoCodec));

    const int numberOfCodecs = ptrViECodec->NumberOfCodecs();
    numberOfErrors += ViETest::TestError(numberOfCodecs > 0,
                                         "ERROR: %s at line %d", __FUNCTION__,
                                         __LINE__);

    SetSendCodec(webrtc::kVideoCodecVP8, ptrViECodec, videoChannel,
                 &numberOfErrors, kDoNotForceResolution, kDoNotForceResolution);

    memset(&videoCodec, 0, sizeof(videoCodec));
    error = ptrViECodec->GetSendCodec(videoChannel, videoCodec);
    assert(videoCodec.codecType == webrtc::kVideoCodecVP8);

    SetSendCodec(webrtc::kVideoCodecI420, ptrViECodec, videoChannel,
                 &numberOfErrors, kDoNotForceResolution, kDoNotForceResolution);
    memset(&videoCodec, 0, sizeof(videoCodec));
    error = ptrViECodec->GetSendCodec(videoChannel, videoCodec);
    assert(videoCodec.codecType == webrtc::kVideoCodecI420);

    //***************************************************************
    //	Testing finished. Tear down Video Engine
    //***************************************************************

    error = ptrViEBase->DeleteChannel(videoChannel);
    numberOfErrors += ViETest::TestError(error == 0, "ERROR: %s at line %d",
                                         __FUNCTION__, __LINE__);

    int remainingInterfaces = 0;
    remainingInterfaces = ptrViECodec->Release();
    numberOfErrors += ViETest::TestError(remainingInterfaces == 0,
                                         "ERROR: %s at line %d", __FUNCTION__,
                                         __LINE__);

    remainingInterfaces = ptrViEBase->Release();
    numberOfErrors += ViETest::TestError(remainingInterfaces == 0,
                                         "ERROR: %s at line %d", __FUNCTION__,
                                         __LINE__);

    bool deleted = webrtc::VideoEngine::Delete(ptrViE);
    numberOfErrors += ViETest::TestError(deleted == true,
                                         "ERROR: %s at line %d", __FUNCTION__,
                                         __LINE__);

    if (numberOfErrors > 0)
    {
        // Test failed
        ViETest::Log(" ");
        ViETest::Log(" ERROR ViECodec API Test FAILED!");
        ViETest::Log(" Number of errors: %d", numberOfErrors);
        ViETest::Log("========================================");
        ViETest::Log(" ");
        return numberOfErrors;
    }

    ViETest::Log(" ");
    ViETest::Log(" ViECodec API Test PASSED!");
    ViETest::Log("========================================");
    ViETest::Log(" ");
    return 0;
}

#ifdef WEBRTC_VIDEO_ENGINE_EXTERNAL_CODEC_API
#include "vie_external_codec.h"
#endif
int ViEAutoTest::ViECodecExternalCodecTest()
{
    ViETest::Log(" ");
    ViETest::Log("========================================");
    ViETest::Log(" ViEExternalCodec Test\n");

    // ***************************************************************
    // Begin create/initialize WebRTC Video Engine for testing
    // ***************************************************************


    // ***************************************************************
    // Engine ready. Begin testing class
    // ***************************************************************

#ifdef WEBRTC_VIDEO_ENGINE_EXTERNAL_CODEC_API
    int numberOfErrors=0;
    {
        int error=0;
        TbInterfaces ViE("ViEExternalCodec", numberOfErrors);
        TbCaptureDevice captureDevice(ViE, numberOfErrors);
        tbVideoChannel channel(ViE, numberOfErrors, webrtc::kVideoCodecI420,
                               352,288,30,(352*288*3*8*30)/(2*1000));

        captureDevice.ConnectTo(channel.videoChannel);

        error = ViE.render->AddRenderer(channel.videoChannel, _window1, 0,
                                              0.0, 0.0, 1.0, 1.0);
        numberOfErrors += ViETest::TestError(error == 0, "ERROR: %s at line %d",
                                             __FUNCTION__, __LINE__);
        error = ViE.render->StartRender(channel.videoChannel);
        numberOfErrors += ViETest::TestError(error == 0, "ERROR: %s at line %d",
                                             __FUNCTION__, __LINE__);

        channel.StartReceive();
        channel.StartSend();

        ViETest::Log("Using internal I420 codec");
        AutoTestSleep(KAutoTestSleepTimeMs/2);

        ViEExternalCodec* ptrViEExtCodec =
            ViEExternalCodec::GetInterface(ViE.video_engine);
        numberOfErrors += ViETest::TestError(ptrViEExtCodec != NULL,
                                             "ERROR: %s at line %d",
                                             __FUNCTION__, __LINE__);

        webrtc::VideoCodec codecStruct;

        error=ViE.codec->GetSendCodec(channel.videoChannel,codecStruct);
        numberOfErrors += ViETest::TestError(ptrViEExtCodec != NULL,
                                             "ERROR: %s at line %d",
                                             __FUNCTION__, __LINE__);

        // Use external encoder instead
        {
            tbI420Encoder extEncoder;

            // Test to register on wrong channel
            error = ptrViEExtCodec->RegisterExternalSendCodec(
                channel.videoChannel+5,codecStruct.plType,&extEncoder);
            numberOfErrors += ViETest::TestError(error == -1,
                                                 "ERROR: %s at line %d",
                                                 __FUNCTION__, __LINE__);
            numberOfErrors += ViETest::TestError(
                ViE.LastError() == kViECodecInvalidArgument,
                "ERROR: %s at line %d", __FUNCTION__, __LINE__);

            error = ptrViEExtCodec->RegisterExternalSendCodec(
                channel.videoChannel,codecStruct.plType,&extEncoder);
            numberOfErrors += ViETest::TestError(error == 0,
                                                 "ERROR: %s at line %d",
                                                 __FUNCTION__, __LINE__);

            // Use new external encoder
            error = ViE.codec->SetSendCodec(channel.videoChannel,
                                            codecStruct);
            numberOfErrors += ViETest::TestError(error == 0,
                                                 "ERROR: %s at line %d",
                                                 __FUNCTION__, __LINE__);

            tbI420Decoder extDecoder;
            error = ptrViEExtCodec->RegisterExternalReceiveCodec(
                channel.videoChannel,codecStruct.plType,&extDecoder);
            numberOfErrors += ViETest::TestError(error == 0,
                                                 "ERROR: %s at line %d",
                                                 __FUNCTION__, __LINE__);

            error = ViE.codec->SetReceiveCodec(channel.videoChannel,
                                               codecStruct);
            numberOfErrors += ViETest::TestError(error == 0,
                                                 "ERROR: %s at line %d",
                                                 __FUNCTION__, __LINE__);

            ViETest::Log("Using external I420 codec");
            AutoTestSleep(KAutoTestSleepTimeMs);

            // Test to deregister on wrong channel
            error = ptrViEExtCodec->DeRegisterExternalSendCodec(
                channel.videoChannel+5,codecStruct.plType);
            numberOfErrors += ViETest::TestError(error == -1,
                                                 "ERROR: %s at line %d",
                                                 __FUNCTION__, __LINE__);
            numberOfErrors += ViETest::TestError(
                ViE.LastError() == kViECodecInvalidArgument,
                "ERROR: %s at line %d", __FUNCTION__, __LINE__);

            // Test to deregister wrong payload type.
            error = ptrViEExtCodec->DeRegisterExternalSendCodec(
                channel.videoChannel,codecStruct.plType-1);
            numberOfErrors += ViETest::TestError(error == -1,
                                                 "ERROR: %s at line %d",
                                                 __FUNCTION__, __LINE__);

            // Deregister external send codec
            error = ptrViEExtCodec->DeRegisterExternalSendCodec(
                channel.videoChannel,codecStruct.plType);
            numberOfErrors += ViETest::TestError(error == 0,
                                                 "ERROR: %s at line %d",
                                                 __FUNCTION__, __LINE__);

            error = ptrViEExtCodec->DeRegisterExternalReceiveCodec(
                channel.videoChannel,codecStruct.plType);
            numberOfErrors += ViETest::TestError(error == 0,
                                                 "ERROR: %s at line %d",
                                                 __FUNCTION__, __LINE__);

            // Verify that the encoder and decoder has been used
            tbI420Encoder::FunctionCalls encodeCalls =
                extEncoder.GetFunctionCalls();
            numberOfErrors += ViETest::TestError(encodeCalls.InitEncode == 1,
                                                 "ERROR: %s at line %d",
                                                 __FUNCTION__, __LINE__);
            numberOfErrors += ViETest::TestError(encodeCalls.Release == 1,
                                                 "ERROR: %s at line %d",
                                                 __FUNCTION__, __LINE__);
            numberOfErrors += ViETest::TestError(encodeCalls.Encode > 30,
                                                 "ERROR: %s at line %d",
                                                 __FUNCTION__, __LINE__);
            numberOfErrors += ViETest::TestError(
                encodeCalls.RegisterEncodeCompleteCallback ==1,
                "ERROR: %s at line %d", __FUNCTION__, __LINE__);
            numberOfErrors += ViETest::TestError(encodeCalls.SetRates > 1,
                                                 "ERROR: %s at line %d",
                                                 __FUNCTION__, __LINE__);
            numberOfErrors += ViETest::TestError(encodeCalls.SetPacketLoss > 1,
                                                 "ERROR: %s at line %d",
                                                 __FUNCTION__, __LINE__);

            tbI420Decoder::FunctionCalls decodeCalls =
                extDecoder.GetFunctionCalls();
            numberOfErrors += ViETest::TestError(decodeCalls.InitDecode == 1,
                                                 "ERROR: %s at line %d",
                                                 __FUNCTION__, __LINE__);
            numberOfErrors += ViETest::TestError(decodeCalls.Release == 1,
                                                 "ERROR: %s at line %d",
                                                 __FUNCTION__, __LINE__);
            numberOfErrors += ViETest::TestError(decodeCalls.Decode > 30,
                                                 "ERROR: %s at line %d",
                                                 __FUNCTION__, __LINE__);
            numberOfErrors += ViETest::TestError(
                decodeCalls.RegisterDecodeCompleteCallback ==1,
                "ERROR: %s at line %d", __FUNCTION__, __LINE__);

            ViETest::Log("Changing payload type Using external I420 codec");

            codecStruct.plType=codecStruct.plType-1;
            error = ptrViEExtCodec->RegisterExternalReceiveCodec(
                channel.videoChannel, codecStruct.plType, &extDecoder);
            numberOfErrors += ViETest::TestError(error == 0,
                                                 "ERROR: %s at line %d",
                                                 __FUNCTION__, __LINE__);

            error = ViE.codec->SetReceiveCodec(channel.videoChannel,
                                               codecStruct);
            numberOfErrors += ViETest::TestError(error == 0,
                                                 "ERROR: %s at line %d",
                                                 __FUNCTION__, __LINE__);

            error = ptrViEExtCodec->RegisterExternalSendCodec(
                channel.videoChannel, codecStruct.plType, &extEncoder);
            numberOfErrors += ViETest::TestError(error == 0,
                                                 "ERROR: %s at line %d",
                                                 __FUNCTION__, __LINE__);

            // Use new external encoder
            error = ViE.codec->SetSendCodec(channel.videoChannel,
                                            codecStruct);
            numberOfErrors += ViETest::TestError(error == 0,
                                                 "ERROR: %s at line %d",
                                                 __FUNCTION__, __LINE__);

            AutoTestSleep(KAutoTestSleepTimeMs/2);

            //***************************************************************
            //	Testing finished. Tear down Video Engine
            //***************************************************************


            error = ptrViEExtCodec->DeRegisterExternalSendCodec(
                channel.videoChannel,codecStruct.plType);
            numberOfErrors += ViETest::TestError(error == 0,
                                                 "ERROR: %s at line %d",
                                                 __FUNCTION__, __LINE__);
            error = ptrViEExtCodec->DeRegisterExternalReceiveCodec(
                channel.videoChannel,codecStruct.plType);
            numberOfErrors += ViETest::TestError(error == 0,
                                                 "ERROR: %s at line %d",
                                                 __FUNCTION__, __LINE__);

            // Verify that the encoder and decoder has been used
            encodeCalls = extEncoder.GetFunctionCalls();
            numberOfErrors += ViETest::TestError(encodeCalls.InitEncode == 2,
                                                 "ERROR: %s at line %d",
                                                 __FUNCTION__, __LINE__);
            numberOfErrors += ViETest::TestError(encodeCalls.Release == 2,
                                                 "ERROR: %s at line %d",
                                                 __FUNCTION__, __LINE__);
            numberOfErrors += ViETest::TestError(encodeCalls.Encode > 30,
                                                 "ERROR: %s at line %d",
                                                 __FUNCTION__, __LINE__);
            numberOfErrors += ViETest::TestError(
                encodeCalls.RegisterEncodeCompleteCallback == 2,
                "ERROR: %s at line %d", __FUNCTION__, __LINE__);
            numberOfErrors += ViETest::TestError(encodeCalls.SetRates > 1,
                                                 "ERROR: %s at line %d",
                                                 __FUNCTION__, __LINE__);
            numberOfErrors += ViETest::TestError(encodeCalls.SetPacketLoss > 1,
                                                 "ERROR: %s at line %d",
                                                 __FUNCTION__, __LINE__);

            decodeCalls = extDecoder.GetFunctionCalls();
            numberOfErrors += ViETest::TestError(decodeCalls.InitDecode == 2,
                                                 "ERROR: %s at line %d",
                                                 __FUNCTION__, __LINE__);
            numberOfErrors += ViETest::TestError(decodeCalls.Release == 2,
                                                 "ERROR: %s at line %d",
                                                 __FUNCTION__, __LINE__);
            numberOfErrors += ViETest::TestError(decodeCalls.Decode > 30,
                                                 "ERROR: %s at line %d",
                                                 __FUNCTION__, __LINE__);
            numberOfErrors += ViETest::TestError(
                decodeCalls.RegisterDecodeCompleteCallback == 2,
                "ERROR: %s at line %d", __FUNCTION__, __LINE__);

            int remainingInterfaces = ptrViEExtCodec->Release();
            numberOfErrors += ViETest::TestError(remainingInterfaces == 0,
                                                 "ERROR: %s at line %d",
                                                 __FUNCTION__, __LINE__);
        } // tbI420Encoder and extDecoder goes out of scope

        ViETest::Log("Using internal I420 codec");
        AutoTestSleep(KAutoTestSleepTimeMs/2);

    }
    if (numberOfErrors > 0)
    {
        // Test failed
        ViETest::Log(" ");
        ViETest::Log(" ERROR ViEExternalCodec Test FAILED!");
        ViETest::Log(" Number of errors: %d", numberOfErrors);
        ViETest::Log("========================================");
        ViETest::Log(" ");
        return numberOfErrors;
    }

    ViETest::Log(" ");
    ViETest::Log(" ViEExternalCodec Test PASSED!");
    ViETest::Log("========================================");
    ViETest::Log(" ");
    return 0;

#else
    ViETest::Log(" ViEExternalCodec not enabled\n");
    return 0;
#endif
}
