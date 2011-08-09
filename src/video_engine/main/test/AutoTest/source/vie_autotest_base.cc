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
// vie_autotest_base.cc
//

#include "vie_autotest_defines.h"
#include "vie_autotest.h"
#include "engine_configurations.h"
#include "video_capture.h"

int ViEAutoTest::ViEBaseStandardTest()
{
    ViETest::Log(" ");
    ViETest::Log("========================================");
    ViETest::Log(" ViEBase Standard Test");

    //***************************************************************
    //	Begin create/initialize WebRTC Video Engine for testing
    //***************************************************************

    int error = 0;
    bool succeeded = true;
    int numberOfErrors = 0;

    ViETest::Log("Starting a loopback call...");

    VideoEngine* ptrViE = NULL;
    ptrViE = VideoEngine::Create();
    numberOfErrors += ViETest::TestError(ptrViE != NULL,
                                         "ERROR: %s at line %d", __FUNCTION__,
                                         __LINE__);
#ifdef WEBRTC_ANDROID
    error = ptrViE->SetTraceFile("/sdcard/ViEBaseStandardTest.txt");
    numberOfErrors += ViETest::TestError(error == 0,
                                         "ERROR: %s at line %d", __FUNCTION__,
                                         __LINE__);
#else
    error = ptrViE->SetTraceFile("ViEBaseStandardTest.txt");
    numberOfErrors += ViETest::TestError(error == 0, "ERROR: %s at line %d",
                                         __FUNCTION__, __LINE__);
#endif
    ViEBase* ptrViEBase = ViEBase::GetInterface(ptrViE);
    numberOfErrors += ViETest::TestError(ptrViEBase != NULL,
                                         "ERROR: %s at line %d", __FUNCTION__,
                                         __LINE__);

    error = ptrViEBase->Init();
    numberOfErrors += ViETest::TestError(error == 0, "ERROR: %s at line %d",
                                         __FUNCTION__, __LINE__);

    //***************************************************************
    //	Engine ready. Begin testing class
    //***************************************************************

    int videoChannel = -1;
    error = ptrViEBase->CreateChannel(videoChannel);
    numberOfErrors += ViETest::TestError(error == 0, "ERROR: %s at line %d",
                                         __FUNCTION__, __LINE__);

    ViECapture* ptrViECapture = ViECapture::GetInterface(ptrViE);
    numberOfErrors += ViETest::TestError(ptrViECapture != NULL,
                                         "ERROR: %s at line %d", __FUNCTION__,
                                         __LINE__);

    VideoCaptureModule* vcpm(NULL);

    const unsigned int KMaxDeviceNameLength = 128;
    const unsigned int KMaxUniqueIdLength = 256;
    WebRtc_UWord8 deviceName[KMaxDeviceNameLength];
    memset(deviceName, 0, KMaxDeviceNameLength);
    WebRtc_UWord8 uniqueId[KMaxUniqueIdLength];
    memset(uniqueId, 0, KMaxUniqueIdLength);

    bool captureDeviceSet = false;
    int captureId = 0;
    VideoCaptureModule::DeviceInfo* devInfo =
        VideoCaptureModule::CreateDeviceInfo(0);

    for (unsigned int captureIdx = 0;
         captureIdx < devInfo->NumberOfDevices();
         captureIdx++)
    {
        error = devInfo->GetDeviceName(captureIdx, deviceName,
                                       KMaxDeviceNameLength, uniqueId,
                                       KMaxUniqueIdLength);
        numberOfErrors += ViETest::TestError(error == 0,
                                             "ERROR: %s at line %d",
                                             __FUNCTION__, __LINE__);

        vcpm = VideoCaptureModule::Create(4571, uniqueId);
        numberOfErrors += ViETest::TestError(vcpm != NULL,
                                             "ERROR: %s at line %d",
                                             __FUNCTION__, __LINE__);

        error = ptrViECapture->AllocateCaptureDevice(*vcpm, captureId);
        if (error == 0)
        {
            ViETest::Log("Using capture device: %s, captureId: %d.",
                         deviceName, captureId);
            captureDeviceSet = true;
            break;
        }
        else
        {
            VideoCaptureModule::Destroy(vcpm);
            vcpm = NULL;
        }
    }
    VideoCaptureModule::DestroyDeviceInfo(devInfo);

    numberOfErrors+= ViETest::TestError(
        captureDeviceSet,
        "ERROR: %s at line %d - Could not set capture device",
        __FUNCTION__, __LINE__);

    error = ptrViECapture->ConnectCaptureDevice(captureId, videoChannel);
    numberOfErrors += ViETest::TestError(error == 0, "ERROR: %s at line %d",
                                         __FUNCTION__, __LINE__);

    error = ptrViECapture->StartCapture(captureId);
    numberOfErrors += ViETest::TestError(error == 0, "ERROR: %s at line %d",
                                         __FUNCTION__, __LINE__);

    ViERTP_RTCP* ptrViERtpRtcp = ViERTP_RTCP::GetInterface(ptrViE);
    numberOfErrors += ViETest::TestError(ptrViE != NULL,
                                         "ERROR: %s at line %d", __FUNCTION__,
                                         __LINE__);

    error = ptrViERtpRtcp->SetRTCPStatus(videoChannel, kRtcpCompound_RFC4585);
    numberOfErrors += ViETest::TestError(error == 0, "ERROR: %s at line %d",
                                         __FUNCTION__, __LINE__);

    error = ptrViERtpRtcp->SetKeyFrameRequestMethod(videoChannel,
                                                    kViEKeyFrameRequestPliRtcp);
    numberOfErrors += ViETest::TestError(error == 0, "ERROR: %s at line %d",
                                         __FUNCTION__, __LINE__);

    error = ptrViERtpRtcp->SetTMMBRStatus(videoChannel, true);
    numberOfErrors += ViETest::TestError(error == 0, "ERROR: %s at line %d",
                                         __FUNCTION__, __LINE__);

    ViERender* ptrViERender = ViERender::GetInterface(ptrViE);
    numberOfErrors += ViETest::TestError(ptrViERender != NULL,
                                         "ERROR: %s at line %d", __FUNCTION__,
                                         __LINE__);

    error = ptrViERender->RegisterVideoRenderModule(*_vrm1);
    numberOfErrors += ViETest::TestError(error == 0, "ERROR: %s at line %d",
                                         __FUNCTION__, __LINE__);

    error = ptrViERender->AddRenderer(captureId, _window1, 0, 0.0, 0.0, 1.0,
                                      1.0);
    numberOfErrors += ViETest::TestError(error == 0, "ERROR: %s at line %d",
                                         __FUNCTION__, __LINE__);

    error = ptrViERender->StartRender(captureId);
    numberOfErrors += ViETest::TestError(error == 0, "ERROR: %s at line %d",
                                         __FUNCTION__, __LINE__);


    error = ptrViERender->RegisterVideoRenderModule(*_vrm2);
    numberOfErrors += ViETest::TestError(error == 0, "ERROR: %s at line %d",
                                         __FUNCTION__, __LINE__);

    error = ptrViERender->AddRenderer(videoChannel, _window2, 1, 0.0, 0.0, 1.0,
                                      1.0);
    numberOfErrors += ViETest::TestError(error == 0, "ERROR: %s at line %d",
                                         __FUNCTION__, __LINE__);

    error = ptrViERender->StartRender(videoChannel);
    numberOfErrors += ViETest::TestError(error == 0, "ERROR: %s at line %d",
                                         __FUNCTION__, __LINE__);

    ViECodec* ptrViECodec = ViECodec::GetInterface(ptrViE);
    numberOfErrors += ViETest::TestError(ptrViECodec != NULL,
                                         "ERROR: %s at line %d", __FUNCTION__,
                                         __LINE__);

    webrtc::VideoCodec videoCodec;
    memset(&videoCodec, 0, sizeof(webrtc::VideoCodec));
    for (int idx = 0; idx < ptrViECodec->NumberOfCodecs(); idx++)
    {
        error = ptrViECodec->GetCodec(idx, videoCodec);
        numberOfErrors += ViETest::TestError(error == 0,
                                             "ERROR: %s at line %d",
                                             __FUNCTION__, __LINE__);

        // try to keep the test frame size small when I420
        if (videoCodec.codecType == webrtc::kVideoCodecI420)
        {
            videoCodec.width = 176;
            videoCodec.height = 144;
            error = ptrViECodec->SetSendCodec(videoChannel, videoCodec);
            numberOfErrors += ViETest::TestError(error == 0,
                                                 "ERROR: %s at line %d",
                                                 __FUNCTION__, __LINE__);
        }

        error = ptrViECodec->SetReceiveCodec(videoChannel, videoCodec);
        numberOfErrors += ViETest::TestError(error == 0,
                                             "ERROR: %s at line %d",
                                             __FUNCTION__, __LINE__);
    }

    ViENetwork* ptrViENetwork = ViENetwork::GetInterface(ptrViE);
    numberOfErrors += ViETest::TestError(ptrViENetwork != NULL,
                                         "ERROR: %s at line %d", __FUNCTION__,
                                         __LINE__);

    char version[1024] = "";
    int versionLength = 1024;
    error = ptrViEBase->GetVersion(version);

    ViETest::Log("\nUsing WebRTC Video Engine version: %s", version);
    numberOfErrors += ViETest::TestError(error == 0, "ERROR: %s at line %d",
                                         __FUNCTION__, __LINE__);

    const char* ipAddress = "127.0.0.1";
    unsigned short rtpPortListen = 6000;
    unsigned short rtpPortSend = 6000;


    rtpPortListen = 6100;
    rtpPortSend = 6100;

    error = ptrViENetwork->SetLocalReceiver(videoChannel, rtpPortListen);
    numberOfErrors += ViETest::TestError(error == 0, "ERROR: %s at line %d",
                                         __FUNCTION__, __LINE__);

    error = ptrViEBase->StartReceive(videoChannel);
    numberOfErrors += ViETest::TestError(error == 0, "ERROR: %s at line %d",
                                         __FUNCTION__, __LINE__);

    error = ptrViENetwork->SetSendDestination(videoChannel, ipAddress,
                                              rtpPortSend);
    numberOfErrors += ViETest::TestError(error == 0, "ERROR: %s at line %d",
                                         __FUNCTION__, __LINE__);

    error = ptrViEBase->StartSend(videoChannel);
    numberOfErrors += ViETest::TestError(error == 0, "ERROR: %s at line %d",
                                         __FUNCTION__, __LINE__);


    error = ptrViERender->MirrorRenderStream(captureId, true, false, true);
    numberOfErrors += ViETest::TestError(error == 0, "ERROR: %s at line %d",
                                         __FUNCTION__, __LINE__);

    // Call started
    ViETest::Log("Call started");
    ViETest::Log("You should see a mirrored local preview from camera %s in "
                 "window 1 and the remote video in window 2.",
                 deviceName);

    //***************************************************************
    //	Finished initializing engine. Begin testing
    //***************************************************************

    AutoTestSleep(KAutoTestSleepTimeMs);

    //***************************************************************
    //	Testing finished. Tear down Video Engine
    //***************************************************************

    // Shut down
    error = ptrViEBase->StopReceive(videoChannel);
    numberOfErrors += ViETest::TestError(error == 0, "ERROR: %s at line %d",
                                         __FUNCTION__, __LINE__);

    error = ptrViERender->StopRender(videoChannel);
    numberOfErrors += ViETest::TestError(error == 0, "ERROR: %s at line %d",
                                         __FUNCTION__, __LINE__);

    error = ptrViERender->RemoveRenderer(videoChannel);
    numberOfErrors += ViETest::TestError(error == 0, "ERROR: %s at line %d",
                                         __FUNCTION__, __LINE__);

    error = ptrViERender->DeRegisterVideoRenderModule(*_vrm2);
    numberOfErrors += ViETest::TestError(error == 0, "ERROR: %s at line %d",
                                         __FUNCTION__, __LINE__);

    int remainingInterfaces = 0;

    error = ptrViEBase->StopSend(videoChannel);
    numberOfErrors += ViETest::TestError(error == 0, "ERROR: %s at line %d",
                                         __FUNCTION__, __LINE__);

    error = ptrViERender->RemoveRenderer(captureId);
    numberOfErrors += ViETest::TestError(error == 0, "ERROR: %s at line %d",
                                         __FUNCTION__, __LINE__);

    error = ptrViERender->DeRegisterVideoRenderModule(*_vrm1);
    numberOfErrors += ViETest::TestError(error == 0, "ERROR: %s at line %d",
                                         __FUNCTION__, __LINE__);

    error = ptrViECapture->StopCapture(captureId);
    numberOfErrors += ViETest::TestError(error == 0, "ERROR: %s at line %d",
                                         __FUNCTION__, __LINE__);

    error = ptrViECapture->DisconnectCaptureDevice(videoChannel);
    numberOfErrors += ViETest::TestError(error == 0, "ERROR: %s at line %d",
                                         __FUNCTION__, __LINE__);

    error = ptrViECapture->ReleaseCaptureDevice(captureId);
    numberOfErrors += ViETest::TestError(error == 0, "ERROR: %s at line %d",
                                         __FUNCTION__, __LINE__);

    VideoCaptureModule::Destroy(vcpm);
    vcpm = NULL;

    remainingInterfaces = ptrViECapture->Release();
    numberOfErrors += ViETest::TestError(remainingInterfaces == 0,
                                         "ERROR: %s at line %d", __FUNCTION__,
                                         __LINE__);

    error = ptrViEBase->DeleteChannel(videoChannel);
    numberOfErrors += ViETest::TestError(error == 0, "ERROR: %s at line %d",
                                         __FUNCTION__, __LINE__);

    remainingInterfaces = ptrViECodec->Release();
    numberOfErrors += ViETest::TestError(remainingInterfaces == 0,
                                         "ERROR: %s at line %d", __FUNCTION__,
                                         __LINE__);

    remainingInterfaces = ptrViERtpRtcp->Release();
    numberOfErrors += ViETest::TestError(remainingInterfaces == 0,
                                         "ERROR: %s at line %d", __FUNCTION__,
                                         __LINE__);

    remainingInterfaces = ptrViERender->Release();
    numberOfErrors += ViETest::TestError(remainingInterfaces == 0,
                                         "ERROR: %s at line %d", __FUNCTION__,
                                         __LINE__);

    remainingInterfaces = ptrViENetwork->Release();
    numberOfErrors += ViETest::TestError(remainingInterfaces == 0,
                                         "ERROR: %s at line %d", __FUNCTION__,
                                         __LINE__);

    remainingInterfaces = ptrViEBase->Release();
    numberOfErrors += ViETest::TestError(remainingInterfaces == 0,
                                         "ERROR: %s at line %d", __FUNCTION__,
                                         __LINE__);

    bool deleted = VideoEngine::Delete(ptrViE);
    numberOfErrors += ViETest::TestError(deleted == true,
                                         "ERROR: %s at line %d", __FUNCTION__,
                                         __LINE__);

    if (numberOfErrors > 0)
    {
        // Test failed
        ViETest::Log(" ");
        ViETest::Log(" ERROR ViEBase Standard Test FAILED!");
        ViETest::Log(" Number of errors: %d", numberOfErrors);
        ViETest::Log("========================================");
        ViETest::Log(" ");
        return numberOfErrors;
    }

    ViETest::Log(" ");
    ViETest::Log(" ViEBase Standard Test PASSED!");
    ViETest::Log("========================================");
    ViETest::Log(" ");
    return 0;
}

int ViEAutoTest::ViEBaseExtendedTest()
{
    // Start with standard test
    ViEBaseAPITest();
    ViEBaseStandardTest();

    ViETest::Log(" ");
    ViETest::Log("========================================");
    ViETest::Log(" ViEBase Extended Test");

    ViETest::Log(" ");
    ViETest::Log(" ViEBase Extended Test PASSED!");
    ViETest::Log("========================================");
    ViETest::Log(" ");

    return 0;
}

int ViEAutoTest::ViEBaseAPITest()
{
    ViETest::Log(" ");
    ViETest::Log("========================================");
    ViETest::Log(" ViEBase API Test");


    //***************************************************************
    //	Begin create/initialize WebRTC Video Engine for testing
    //***************************************************************

    int error = 0;
    bool succeeded = true;
    int numberOfErrors = 0;

    VideoEngine* ptrViE = NULL;
    ViEBase* ptrViEBase = NULL;

    // Get the ViEBase API
    ptrViEBase = ViEBase::GetInterface(ptrViE);
    numberOfErrors += ViETest::TestError(ptrViEBase == NULL);

    ptrViE = VideoEngine::Create();
    numberOfErrors += ViETest::TestError(ptrViE != NULL, "VideoEngine::Create");

#ifdef WEBRTC_ANDROID
    error = ptrViE->SetTraceFile("/sdcard/WebRTC/ViESampleCodeTrace.txt");
    numberOfErrors += ViETest::TestError(error == 0, "SetTraceFile error");

    error = ptrViE->SetTraceFile(
        "/sdcard/WebRTC/ViESampleCodeTraceEncrypted.txt");
    numberOfErrors += ViETest::TestError(error == 0, "SetTraceFile");
#else
    error = ptrViE->SetTraceFile("WebRTCViESampleCodeTrace.txt");
    numberOfErrors += ViETest::TestError(error == 0, "SetTraceFile error");
#endif

    ptrViEBase = ViEBase::GetInterface(ptrViE);
    numberOfErrors += ViETest::TestError(ptrViEBase != NULL);

    //***************************************************************
    //	Engine ready. Begin testing class
    //***************************************************************

    char version[1024] = "";
    error = ptrViEBase->GetVersion(version);
    numberOfErrors += ViETest::TestError(error == 0, "ERROR: %s at line %d",
                                         __FUNCTION__, __LINE__);

    error = ptrViEBase->LastError();
    numberOfErrors += ViETest::TestError(error == 0, "ERROR: %s at line %d",
                                         __FUNCTION__, __LINE__);

    // Create without init
    int videoChannel = -1;
    error = ptrViEBase->CreateChannel(videoChannel);
    numberOfErrors += ViETest::TestError(error != 0, "ERROR: %s at line %d",
                                         __FUNCTION__, __LINE__);

    error = ptrViEBase->Init();
    numberOfErrors += ViETest::TestError(error == 0, "ERROR: %s at line %d",
                                         __FUNCTION__, __LINE__);

    error = ptrViEBase->CreateChannel(videoChannel);
    numberOfErrors += ViETest::TestError(error == 0, "ERROR: %s at line %d",
                                         __FUNCTION__, __LINE__);

    int videoChannel2 = -1;
    error = ptrViEBase->CreateChannel(videoChannel2);
    numberOfErrors += ViETest::TestError(error == 0, "ERROR: %s at line %d",
                                         __FUNCTION__, __LINE__);
    numberOfErrors += ViETest::TestError(videoChannel != videoChannel2,
                                         "ERROR: %s at line %d", __FUNCTION__,
                                         __LINE__);

    error = ptrViEBase->DeleteChannel(videoChannel2);
    numberOfErrors += ViETest::TestError(error == 0, "ERROR: %s at line %d",
                                         __FUNCTION__, __LINE__);

    // Channel doesn't exist
    error = ptrViEBase->CreateChannel(videoChannel2, videoChannel + 1);
    numberOfErrors += ViETest::TestError(error == -1, "ERROR: %s at line %d",
                                         __FUNCTION__, __LINE__);

    // Channel doesn't exist
    error = ptrViEBase->CreateChannel(videoChannel2, videoChannel);
    numberOfErrors += ViETest::TestError(error == 0, "ERROR: %s at line %d",
                                         __FUNCTION__, __LINE__);

    // VoiceEngine
    VoiceEngine* ptrVoE = NULL;
    VoEBase* ptrVoEBase = NULL;
    int audioChannel = -1;

    ptrVoE = VoiceEngine::Create();
    numberOfErrors += ViETest::TestError(ptrVoE != NULL,
                                         "ERROR: %s at line %d", __FUNCTION__,
                                         __LINE__);

    ptrVoEBase = VoEBase::GetInterface(ptrVoE);
    numberOfErrors += ViETest::TestError(ptrVoEBase != NULL,
                                         "ERROR: %s at line %d", __FUNCTION__,
                                         __LINE__);

    error = ptrVoEBase->Init();
    numberOfErrors += ViETest::TestError(error == 0, "ERROR: %s at line %d",
                                         __FUNCTION__, __LINE__);

    audioChannel = ptrVoEBase->CreateChannel();
    numberOfErrors += ViETest::TestError(audioChannel != -1,
                                         "ERROR: %s at line %d", __FUNCTION__,
                                         __LINE__);

    // Connect before setting VoE
    error = ptrViEBase->ConnectAudioChannel(videoChannel, audioChannel);
    numberOfErrors += ViETest::TestError(error != 0, "ERROR: %s at line %d",
                                         __FUNCTION__, __LINE__);

    error = ptrViEBase->SetVoiceEngine(ptrVoE);
    numberOfErrors += ViETest::TestError(error == 0, "ERROR: %s at line %d",
                                         __FUNCTION__, __LINE__);

    error = ptrViEBase->ConnectAudioChannel(videoChannel, audioChannel);
    numberOfErrors += ViETest::TestError(error == 0, "ERROR: %s at line %d",
                                         __FUNCTION__, __LINE__);

    //***************************************************************
    //	Testing finished. Tear down Video Engine
    //***************************************************************

    error = ptrViEBase->DisconnectAudioChannel(videoChannel + 5);
    numberOfErrors += ViETest::TestError(error != 0, "ERROR: %s at line %d",
                                         __FUNCTION__, __LINE__);

    error = ptrViEBase->DisconnectAudioChannel(videoChannel);
    numberOfErrors += ViETest::TestError(error == 0, "ERROR: %s at line %d",
                                         __FUNCTION__, __LINE__);

    error = ptrViEBase->SetVoiceEngine(NULL);
    numberOfErrors += ViETest::TestError(error == 0, "ERROR: %s at line %d",
                                         __FUNCTION__, __LINE__);

    ViEBase* ptrViEBase2 = ViEBase::GetInterface(ptrViE);
    numberOfErrors += ViETest::TestError(ptrViEBase != NULL,
                                         "ERROR: %s at line %d", __FUNCTION__,
                                         __LINE__);

    int remainingInterfaces = ptrViEBase->Release();
    numberOfErrors += ViETest::TestError(remainingInterfaces == 1,
                                         "ERROR: %s at line %d", __FUNCTION__,
                                         __LINE__);

    bool vieDeleted = VideoEngine::Delete(ptrViE);
    numberOfErrors += ViETest::TestError(vieDeleted == false,
                                         "ERROR: %s at line %d", __FUNCTION__,
                                         __LINE__);

    remainingInterfaces = ptrViEBase->Release();
    numberOfErrors += ViETest::TestError(remainingInterfaces == 0,
                                         "ERROR: %s at line %d", __FUNCTION__,
                                         __LINE__);

    vieDeleted = VideoEngine::Delete(ptrViE);
    numberOfErrors += ViETest::TestError(vieDeleted == true,
                                         "ERROR: %s at line %d", __FUNCTION__,
                                         __LINE__);

    if (numberOfErrors > 0)
    {
        ViETest::Log(" ");
        ViETest::Log(" ERROR ViEBase API Test FAILED!   ");
        ViETest::Log(" Number of errors: %d", numberOfErrors);
        ViETest::Log("========================================");
        ViETest::Log(" ");

        return numberOfErrors;
    }

    ViETest::Log(" ");
    ViETest::Log(" ViEBase API Test PASSED!");
    ViETest::Log("========================================");
    ViETest::Log(" ");

    return 0;
}
