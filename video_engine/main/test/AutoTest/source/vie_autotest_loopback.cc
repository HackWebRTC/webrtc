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
// vie_autotest_loopback.cc
//
// This code is also used as sample code for ViE 3.0
//

#include "vie_autotest_defines.h"
#include "vie_autotest.h"

// ===================================================================
//
// BEGIN: VideoEngine 3.0 Sample Code
//

#include "common_types.h"
#include "voe_base.h"
#include "vie_base.h"
#include "vie_capture.h"
#include "vie_codec.h"
#include "vie_network.h"
#include "vie_render.h"
#include "vie_rtp_rtcp.h"

int VideoEngineSampleCode(void* window1, void* window2)
{
    //********************************************************
    //  Begin create/initialize Video Engine for testing
    //********************************************************

    int error = 0;
    bool succeeded = true;
    int numberOfErrors = 0;

    //
    // Create a VideoEngine instance
    //
    VideoEngine* ptrViE = NULL;
    ptrViE = VideoEngine::Create();
    if (ptrViE == NULL)
    {
        printf("ERROR in VideoEngine::Create\n");
        return -1;
    }

    error = ptrViE->SetTraceFilter(webrtc::kTraceAll);
    if (error == -1)
    {
        printf("ERROR in VideoEngine::SetTraceLevel\n");
        return -1;
    }

#ifdef ANDROID
    error = ptrViE->SetTraceFile("/sdcard/ViETrace.txt");
    if (error == -1)
    {
        printf("ERROR in VideoEngine::SetTraceFile\n");
        return -1;
    }

    error = ptrViE->SetTraceFile("/sdcard/ViEEncryptedTrace.txt");
    if (error == -1)
    {
        printf("ERROR in VideoEngine::SetTraceFile\n");
        return -1;
    }
#else
    error = ptrViE->SetTraceFile("ViETrace.txt");
    if (error == -1)
    {
        printf("ERROR in VideoEngine::SetTraceFile\n");
        return -1;
    }

#endif

    //
    // Init VideoEngine and create a channel
    //
    ViEBase* ptrViEBase = ViEBase::GetInterface(ptrViE);
    if (ptrViEBase == NULL)
    {
        printf("ERROR in ViEBase::GetInterface\n");
        return -1;
    }

    error = ptrViEBase->Init();
    if (error == -1)
    {
        printf("ERROR in ViEBase::Init\n");
        return -1;
    }

    int videoChannel = -1;
    error = ptrViEBase->CreateChannel(videoChannel);
    if (error == -1)
    {
        printf("ERROR in ViEBase::CreateChannel\n");
        return -1;
    }

    //
    // List available capture devices, allocate and connect.
    //
    ViECapture* ptrViECapture = ViECapture::GetInterface(ptrViE);
    if (ptrViEBase == NULL)
    {
        printf("ERROR in ViECapture::GetInterface\n");
        return -1;
    }

    const unsigned int KMaxDeviceNameLength = 128;
    const unsigned int KMaxUniqueIdLength = 256;
    char deviceName[KMaxDeviceNameLength];
    memset(deviceName, 0, KMaxDeviceNameLength);
    char uniqueId[KMaxUniqueIdLength];
    memset(uniqueId, 0, KMaxUniqueIdLength);

    printf("Available capture devices:\n");
    int captureIdx = 0;
    for (captureIdx = 0;
         captureIdx < ptrViECapture->NumberOfCaptureDevices();
         captureIdx++)
    {
        memset(deviceName, 0, KMaxDeviceNameLength);
        memset(uniqueId, 0, KMaxUniqueIdLength);

        error = ptrViECapture->GetCaptureDevice(captureIdx, deviceName,
                                                KMaxDeviceNameLength, uniqueId,
                                                KMaxUniqueIdLength);
        if (error == -1)
        {
            printf("ERROR in ViECapture::GetCaptureDevice\n");
            return -1;
        }
        printf("\t %d. %s\n", captureIdx + 1, deviceName);
    }
    printf("\nChoose capture device: ");
#ifdef ANDROID
    captureIdx = 0;
    printf("0\n");
#else
    int dummy = scanf("%d", &captureIdx);
    getchar();
    captureIdx = captureIdx - 1; // Compensate for idx start at 1.
#endif
    error = ptrViECapture->GetCaptureDevice(captureIdx, deviceName,
                                            KMaxDeviceNameLength, uniqueId,
                                            KMaxUniqueIdLength);
    if (error == -1)
    {
        printf("ERROR in ViECapture::GetCaptureDevice\n");
        return -1;
    }

    int captureId = 0;
    error = ptrViECapture->AllocateCaptureDevice(uniqueId, KMaxUniqueIdLength,
                                                 captureId);
    if (error == -1)
    {
        printf("ERROR in ViECapture::AllocateCaptureDevice\n");
        return -1;
    }

    error = ptrViECapture->ConnectCaptureDevice(captureId, videoChannel);
    if (error == -1)
    {
        printf("ERROR in ViECapture::ConnectCaptureDevice\n");
        return -1;
    }

    error = ptrViECapture->StartCapture(captureId);
    if (error == -1)
    {
        printf("ERROR in ViECapture::StartCapture\n");
        return -1;
    }

    //
    // RTP/RTCP settings
    //
    ViERTP_RTCP* ptrViERtpRtcp = ViERTP_RTCP::GetInterface(ptrViE);
    if (ptrViERtpRtcp == NULL)
    {
        printf("ERROR in ViERTP_RTCP::GetInterface\n");
        return -1;
    }

    error = ptrViERtpRtcp->SetRTCPStatus(videoChannel, kRtcpCompound_RFC4585);
    if (error == -1)
    {
        printf("ERROR in ViERTP_RTCP::SetRTCPStatus\n");
        return -1;
    }

    error = ptrViERtpRtcp->SetKeyFrameRequestMethod(videoChannel,
                                                    kViEKeyFrameRequestPliRtcp);
    if (error == -1)
    {
        printf("ERROR in ViERTP_RTCP::SetKeyFrameRequestMethod\n");
        return -1;
    }

    error = ptrViERtpRtcp->SetTMMBRStatus(videoChannel, true);
    if (error == -1)
    {
        printf("ERROR in ViERTP_RTCP::SetTMMBRStatus\n");
        return -1;
    }

    //
    // Set up rendering
    //
    ViERender* ptrViERender = ViERender::GetInterface(ptrViE);
    if (ptrViERender == NULL)
    {
        printf("ERROR in ViERender::GetInterface\n");
        return -1;
    }

    error
        = ptrViERender->AddRenderer(captureId, window1, 0, 0.0, 0.0, 1.0, 1.0);
    if (error == -1)
    {
        printf("ERROR in ViERender::AddRenderer\n");
        return -1;
    }

    error = ptrViERender->StartRender(captureId);
    if (error == -1)
    {
        printf("ERROR in ViERender::StartRender\n");
        return -1;
    }

    error = ptrViERender->AddRenderer(videoChannel, window2, 1, 0.0, 0.0, 1.0,
                                      1.0);
    if (error == -1)
    {
        printf("ERROR in ViERender::AddRenderer\n");
        return -1;
    }

    error = ptrViERender->StartRender(videoChannel);
    if (error == -1)
    {
        printf("ERROR in ViERender::StartRender\n");
        return -1;
    }

    //
    // Setup codecs
    //
    ViECodec* ptrViECodec = ViECodec::GetInterface(ptrViE);
    if (ptrViECodec == NULL)
    {
        printf("ERROR in ViECodec::GetInterface\n");
        return -1;
    }

    // Check available codecs and prepare receive codecs
    printf("\nAvailable codecs:\n");
    webrtc::VideoCodec videoCodec;
    memset(&videoCodec, 0, sizeof(webrtc::VideoCodec));
    int codecIdx = 0;
    for (codecIdx = 0; codecIdx < ptrViECodec->NumberOfCodecs(); codecIdx++)
    {
        error = ptrViECodec->GetCodec(codecIdx, videoCodec);
        if (error == -1)
        {
            printf("ERROR in ViECodec::GetCodec\n");
            return -1;
        }

        // try to keep the test frame size small when I420
        if (videoCodec.codecType == webrtc::kVideoCodecI420)
        {
            videoCodec.width = 176;
            videoCodec.height = 144;
        }

        error = ptrViECodec->SetReceiveCodec(videoChannel, videoCodec);
        if (error == -1)
        {
            printf("ERROR in ViECodec::SetReceiveCodec\n");
            return -1;
        }
        if (videoCodec.codecType != webrtc::kVideoCodecRED
            && videoCodec.codecType != webrtc::kVideoCodecULPFEC)
        {
            printf("\t %d. %s\n", codecIdx + 1, videoCodec.plName);
        }
    }
    printf("Choose codec: ");
#ifdef ANDROID
    codecIdx = 0;
    printf("0\n");
#else
    dummy = scanf("%d", &codecIdx);
    getchar();
    codecIdx = codecIdx - 1; // Compensate for idx start at 1.
#endif

    error = ptrViECodec->GetCodec(codecIdx, videoCodec);
    if (error == -1)
    {
        printf("ERROR in ViECodec::GetCodec\n");
        return -1;
    }

    // try to keep the test frame size small when I420
    if (videoCodec.codecType == webrtc::kVideoCodecI420)
    {
        videoCodec.width = 176;
        videoCodec.height = 144;
    }

    error = ptrViECodec->SetSendCodec(videoChannel, videoCodec);
    if (error == -1)
    {
        printf("ERROR in ViECodec::SetSendCodec\n");
        return -1;
    }

    //
    // Address settings
    //
    ViENetwork* ptrViENetwork = ViENetwork::GetInterface(ptrViE);
    if (ptrViENetwork == NULL)
    {
        printf("ERROR in ViENetwork::GetInterface\n");
        return -1;
    }

    const char* ipAddress = "127.0.0.1";
    const unsigned short rtpPort = 6000;
    error = ptrViENetwork->SetLocalReceiver(videoChannel, rtpPort);
    if (error == -1)
    {
        printf("ERROR in ViENetwork::SetLocalReceiver\n");
        return -1;
    }

    error = ptrViEBase->StartReceive(videoChannel);
    if (error == -1)
    {
        printf("ERROR in ViENetwork::StartReceive\n");
        return -1;
    }

    error = ptrViENetwork->SetSendDestination(videoChannel, ipAddress, rtpPort);
    if (error == -1)
    {
        printf("ERROR in ViENetwork::SetSendDestination\n");
        return -1;
    }

    error = ptrViEBase->StartSend(videoChannel);
    if (error == -1)
    {
        printf("ERROR in ViENetwork::StartSend\n");
        return -1;
    }

    //********************************************************
    //  Engine started
    //********************************************************


    // Call started
    printf("\nLoopback call started\n\n");
    printf("Press enter to stop...");
    while ((getchar()) != '\n')
        ;

    //********************************************************
    //  Testing finished. Tear down Video Engine
    //********************************************************

    error = ptrViEBase->StopReceive(videoChannel);
    if (error == -1)
    {
        printf("ERROR in ViEBase::StopReceive\n");
        return -1;
    }

    error = ptrViEBase->StopSend(videoChannel);
    if (error == -1)
    {
        printf("ERROR in ViEBase::StopSend\n");
        return -1;
    }

    error = ptrViERender->StopRender(captureId);
    if (error == -1)
    {
        printf("ERROR in ViERender::StopRender\n");
        return -1;
    }

    error = ptrViERender->RemoveRenderer(captureId);
    if (error == -1)
    {
        printf("ERROR in ViERender::RemoveRenderer\n");
        return -1;
    }

    error = ptrViERender->StopRender(videoChannel);
    if (error == -1)
    {
        printf("ERROR in ViERender::StopRender\n");
        return -1;
    }

    error = ptrViERender->RemoveRenderer(videoChannel);
    if (error == -1)
    {
        printf("ERROR in ViERender::RemoveRenderer\n");
        return -1;
    }

    error = ptrViECapture->StopCapture(captureId);
    if (error == -1)
    {
        printf("ERROR in ViECapture::StopCapture\n");
        return -1;
    }

    error = ptrViECapture->DisconnectCaptureDevice(videoChannel);
    if (error == -1)
    {
        printf("ERROR in ViECapture::DisconnectCaptureDevice\n");
        return -1;
    }

    error = ptrViECapture->ReleaseCaptureDevice(captureId);
    if (error == -1)
    {
        printf("ERROR in ViECapture::ReleaseCaptureDevice\n");
        return -1;
    }

    error = ptrViEBase->DeleteChannel(videoChannel);
    if (error == -1)
    {
        printf("ERROR in ViEBase::DeleteChannel\n");
        return -1;
    }

    int remainingInterfaces = 0;
    remainingInterfaces = ptrViECodec->Release();
    remainingInterfaces += ptrViECapture->Release();
    remainingInterfaces += ptrViERtpRtcp->Release();
    remainingInterfaces += ptrViERender->Release();
    remainingInterfaces += ptrViENetwork->Release();
    remainingInterfaces += ptrViEBase->Release();
    if (remainingInterfaces > 0)
    {
        printf("ERROR: Could not release all interfaces\n");
        return -1;
    }

    bool deleted = VideoEngine::Delete(ptrViE);
    if (deleted == false)
    {
        printf("ERROR in VideoEngine::Delete\n");
        return -1;
    }

    return 0;

    //
    // END:  VideoEngine 3.0 Sample Code
    //
    // ===================================================================
}

int ViEAutoTest::ViELoopbackCall()
{
    ViETest::Log(" ");
    ViETest::Log("========================================");
    ViETest::Log(" ViE Autotest Loopback Call\n");

    if (VideoEngineSampleCode(_window1, _window2) == 0)
    {
        ViETest::Log(" ");
        ViETest::Log(" ViE Autotest Loopback Call Done");
        ViETest::Log("========================================");
        ViETest::Log(" ");

        return 0;
    }

    ViETest::Log(" ");
    ViETest::Log(" ViE Autotest Loopback Call Failed");
    ViETest::Log("========================================");
    ViETest::Log(" ");
    return 1;

}
