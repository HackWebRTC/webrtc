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
 *  vie_autotest_custom_call.cc
 *
 */

#include "vie_autotest_defines.h"
#include "vie_autotest.h"

#include <iostream>

int ViEAutoTest::ViECustomCall()
{

    ViETest::Log(" ");
    ViETest::Log("========================================");
    ViETest::Log(" Enter values to use custom settings\n");

    int error = 0;
    bool succeeded = true;
    int numberOfErrors = 0;
    std::string str;

    // VoE
    VoiceEngine* ptrVE = VoiceEngine::Create();
    numberOfErrors += ViETest::TestError(ptrVE != NULL, "ERROR: %s at line %d",
                                         __FUNCTION__, __LINE__);

    VoEBase* ptrVEBase = VoEBase::GetInterface(ptrVE);
    numberOfErrors += ViETest::TestError(ptrVEBase != NULL, 
                                         "ERROR: %s at line %d", __FUNCTION__,
                                         __LINE__);

    error = ptrVEBase->Init();
    numberOfErrors += ViETest::TestError(error == 0, "ERROR: %s at line %d",
                                         __FUNCTION__, __LINE__);

    VoECodec* ptrVECodec = VoECodec::GetInterface(ptrVE);
    numberOfErrors += ViETest::TestError(ptrVECodec != NULL,
                                         "ERROR: %s at line %d", __FUNCTION__,
                                         __LINE__);

    VoEHardware* ptrVEHardware = VoEHardware::GetInterface(ptrVE);
    numberOfErrors += ViETest::TestError(ptrVEHardware != NULL,
                                         "ERROR: %s at line %d", __FUNCTION__,
                                         __LINE__);

    VoEAudioProcessing* ptrVEAPM = VoEAudioProcessing::GetInterface(ptrVE);
    numberOfErrors += ViETest::TestError(ptrVEAPM != NULL,
                                         "ERROR: %s at line %d", __FUNCTION__,
                                         __LINE__);

    // ViE
    VideoEngine* ptrViE = NULL;
    ptrViE = VideoEngine::Create();
    numberOfErrors += ViETest::TestError(ptrViE != NULL,
                                         "ERROR: %s at line %d", __FUNCTION__,
                                         __LINE__);

    ViEBase* ptrViEBase = ViEBase::GetInterface(ptrViE);
    numberOfErrors += ViETest::TestError(ptrViEBase != NULL,
                                         "ERROR: %s at line %d", __FUNCTION__,
                                         __LINE__);

    error = ptrViEBase->Init();
    numberOfErrors += ViETest::TestError(error == 0, "ERROR: %s at line %d",
                                         __FUNCTION__, __LINE__);

    ViECapture* ptrViECapture = ViECapture::GetInterface(ptrViE);
    numberOfErrors += ViETest::TestError(ptrViECapture != NULL,
                                         "ERROR: %s at line %d", __FUNCTION__,
                                         __LINE__);

    ViERender* ptrViERender = ViERender::GetInterface(ptrViE);
    numberOfErrors += ViETest::TestError(ptrViERender != NULL,
                                         "ERROR: %s at line %d", __FUNCTION__,
                                         __LINE__);

    ViECodec* ptrViECodec = ViECodec::GetInterface(ptrViE);
    numberOfErrors += ViETest::TestError(ptrViECodec != NULL,
                                         "ERROR: %s at line %d", __FUNCTION__,
                                         __LINE__);

    ViENetwork* ptrViENetwork = ViENetwork::GetInterface(ptrViE);
    numberOfErrors += ViETest::TestError(ptrViENetwork != NULL,
                                         "ERROR: %s at line %d", __FUNCTION__,
                                         __LINE__);

    bool startCall = false;
    const unsigned int kMaxIPLength = 16;
    char ipAddress[kMaxIPLength] = "";
    const unsigned int KMaxUniqueIdLength = 256;
    char uniqueId[KMaxUniqueIdLength] = "";
    char deviceName[KMaxUniqueIdLength] = "";
    int videoTxPort = 0;
    int videoRxPort = 0;
    int videoChannel = -1;
    int codecIdx = 0;
    webrtc::VideoCodec videoCodec;
    char audioCaptureDeviceName[KMaxUniqueIdLength] = "";
    char audioPlaybackDeviceName[KMaxUniqueIdLength] = "";
    int audioCaptureDeviceIndex = -1;
    int audioPlaybackDeviceIndex = -1;
    int audioTxPort = 0;
    int audioRxPort = 0;
    webrtc::CodecInst audioCodec;
    int audioChannel = -1;

    while(1)
    {
        // IP
        memset(ipAddress, 0, kMaxIPLength);
        GetIPAddress(ipAddress);

        // video devices
        memset(deviceName, 0, KMaxUniqueIdLength);
        memset(uniqueId, 0, KMaxUniqueIdLength);
        GetVideoDevice(ptrViEBase, ptrViECapture, deviceName, uniqueId);

        // video ports
        videoTxPort = 0;
        videoRxPort = 0;
        GetVideoPorts(&videoTxPort, &videoRxPort);


        // video codecs
        memset((void*)&videoCodec, 0, sizeof(videoCodec));
        GetVideoCodec(ptrViECodec, videoCodec);

        // audio devices
        memset(audioCaptureDeviceName, 0, KMaxUniqueIdLength);
        memset(audioPlaybackDeviceName, 0, KMaxUniqueIdLength);
        GetAudioDevices(ptrVEBase, ptrVEHardware, audioCaptureDeviceName,
                        audioCaptureDeviceIndex, audioPlaybackDeviceName,
                        audioPlaybackDeviceIndex);

        // audio port
        audioTxPort = 0;
        audioRxPort = 0;
        GetAudioPorts(&audioTxPort, &audioRxPort);

        // audio codec
        memset((void*)&audioCodec, 0, sizeof(audioCodec));
        GetAudioCodec(ptrVECodec, audioCodec);

        // start the call now
        PrintCallInformation(ipAddress, deviceName, uniqueId, videoCodec,
                             videoTxPort, videoRxPort, audioCaptureDeviceName,
                             audioPlaybackDeviceName, audioCodec, audioTxPort,
                             audioRxPort);

        std::cout << std::endl;
        std::cout << "1. Start the call" << std::endl;
        std::cout << "2. Reconfigure call settings" << std::endl;
        std::cout << "3. Go back to main menu" << std::endl;
        std::cout << "What do you want to do? Press enter for default "
                  "(Start the call): ";

        std::getline(std::cin, str);
        int selection = 0;
        selection = atoi(str.c_str());

        if(selection == 0 || selection == 1)
        {
            startCall = true;
            break;
        }
        else if(selection == 2)
        {
            continue;
        }
        else if(selection == 3)
        {
            startCall = false;
            break;
        }
        else
        {
            // invalid selection
            std::cout << "ERROR: Code=" << error <<
                      " Invalid selection" << std::endl;
            continue;
        }
    }

    //***************************************************************
    // Begin create/initialize WebRTC Video Engine for testing
    //***************************************************************
    if(startCall == true)
    {
        // Configure Audio first
        audioChannel = ptrVEBase->CreateChannel();
        error = ptrVEBase->SetSendDestination(audioChannel, audioTxPort,
                                              ipAddress);
        numberOfErrors += ViETest::TestError(error == 0,
                                             "ERROR: %s at line %d",
                                             __FUNCTION__, __LINE__);

        error = ptrVEBase->SetLocalReceiver(audioChannel, audioRxPort);
        numberOfErrors += ViETest::TestError(error == 0,
                                             "ERROR: %s at line %d",
                                             __FUNCTION__, __LINE__);

        error = ptrVEHardware->SetRecordingDevice(audioCaptureDeviceIndex);
        numberOfErrors += ViETest::TestError(error == 0,
                                             "ERROR: %s at line %d",
                                             __FUNCTION__, __LINE__);

        error = ptrVEHardware->SetPlayoutDevice(audioPlaybackDeviceIndex);
        numberOfErrors += ViETest::TestError(error == 0,
                                             "ERROR: %s at line %d",
                                             __FUNCTION__, __LINE__);

        error = ptrVECodec->SetSendCodec(audioChannel, audioCodec);
        numberOfErrors += ViETest::TestError(error == 0,
                                             "ERROR: %s at line %d",
                                             __FUNCTION__, __LINE__);

        error = ptrVECodec->SetVADStatus(audioChannel, true,
                                         webrtc::kVadAggressiveHigh);
        numberOfErrors += ViETest::TestError(error == 0,
                                             "ERROR: %s at line %d",
                                             __FUNCTION__, __LINE__);

        error = ptrVEAPM->SetAgcStatus(true, kAgcDefault);
        numberOfErrors += ViETest::TestError(error == 0,
                                             "ERROR: %s at line %d",
                                             __FUNCTION__, __LINE__);

        error = ptrVEAPM->SetNsStatus(true, kNsHighSuppression);
        numberOfErrors += ViETest::TestError(error == 0,
                                             "ERROR: %s at line %d",
                                             __FUNCTION__, __LINE__);

        // Configure Video now
        error = ptrViE->SetTraceFile("ViECustomCall.txt");
        numberOfErrors += ViETest::TestError(error == 0,
                                             "ERROR: %s at line %d",
                                             __FUNCTION__, __LINE__);

        error = ptrViEBase->SetVoiceEngine(ptrVE);
        numberOfErrors += ViETest::TestError(error == 0,
                                             "ERROR: %s at line %d",
                                             __FUNCTION__, __LINE__);

        error = ptrViEBase->CreateChannel(videoChannel);
        numberOfErrors += ViETest::TestError(error == 0,
                                             "ERROR: %s at line %d",
                                             __FUNCTION__, __LINE__);

        error = ptrViEBase->ConnectAudioChannel(videoChannel, audioChannel);
        numberOfErrors += ViETest::TestError(error == 0,
                                             "ERROR: %s at line %d",
                                             __FUNCTION__, __LINE__);

        int captureId = 0;
        error = ptrViECapture->AllocateCaptureDevice(uniqueId,
                                                     KMaxUniqueIdLength,
                                                     captureId);
        numberOfErrors += ViETest::TestError(error == 0,
                                             "ERROR: %s at line %d",
                                             __FUNCTION__, __LINE__);

        error = ptrViECapture->ConnectCaptureDevice(captureId, videoChannel);
        numberOfErrors += ViETest::TestError(error == 0,
                                             "ERROR: %s at line %d",
                                             __FUNCTION__, __LINE__);

        error = ptrViECapture->StartCapture(captureId);
        numberOfErrors += ViETest::TestError(error == 0,
                                             "ERROR: %s at line %d",
                                             __FUNCTION__, __LINE__);

        ViERTP_RTCP* ptrViERtpRtcp = ViERTP_RTCP::GetInterface(ptrViE);
        numberOfErrors += ViETest::TestError(ptrViE != NULL,
                                             "ERROR: %s at line %d",
                                             __FUNCTION__, __LINE__);

        error = ptrViERtpRtcp->SetRTCPStatus(videoChannel,
                                             kRtcpCompound_RFC4585);
        numberOfErrors += ViETest::TestError(error == 0,
                                             "ERROR: %s at line %d",
                                             __FUNCTION__, __LINE__);

        error = ptrViERtpRtcp->SetKeyFrameRequestMethod(
            videoChannel, kViEKeyFrameRequestPliRtcp);
        numberOfErrors += ViETest::TestError(error == 0,
                                             "ERROR: %s at line %d",
                                             __FUNCTION__, __LINE__);

        error = ptrViERtpRtcp->SetTMMBRStatus(videoChannel, true);
        numberOfErrors += ViETest::TestError(error == 0,
                                             "ERROR: %s at line %d",
                                             __FUNCTION__, __LINE__);

        error = ptrViERender->AddRenderer(captureId,  _window1, 0, 0.0, 0.0,
                                          1.0, 1.0);
        numberOfErrors += ViETest::TestError(error == 0,
                                             "ERROR: %s at line %d",
                                             __FUNCTION__, __LINE__);

        error = ptrViERender->AddRenderer(videoChannel,  _window2, 1, 0.0, 0.0,
                                          1.0, 1.0);
        numberOfErrors += ViETest::TestError(error == 0,
                                             "ERROR: %s at line %d",
                                             __FUNCTION__, __LINE__);

        error = ptrViENetwork->SetSendDestination(videoChannel, ipAddress,
                                                  videoTxPort);
        numberOfErrors += ViETest::TestError(error == 0,
                                             "ERROR: %s at line %d",
                                             __FUNCTION__, __LINE__);

        error = ptrViENetwork->SetLocalReceiver(videoChannel, videoRxPort);
        numberOfErrors += ViETest::TestError(error == 0,
                                             "ERROR: %s at line %d",
                                             __FUNCTION__, __LINE__);

        error = ptrViECodec->SetSendCodec(videoChannel, videoCodec);
        numberOfErrors += ViETest::TestError(error == 0,
                                             "ERROR: %s at line %d",
                                             __FUNCTION__, __LINE__);

        error = ptrViECodec->SetReceiveCodec(videoChannel, videoCodec);
        numberOfErrors += ViETest::TestError(error == 0,
                                             "ERROR: %s at line %d",
                                             __FUNCTION__, __LINE__);

        // **** start the engines
        // VE first
        error = ptrVEBase->StartReceive(audioChannel);
        numberOfErrors += ViETest::TestError(error == 0,
                                             "ERROR: %s at line %d",
                                             __FUNCTION__, __LINE__);

        error = ptrVEBase->StartPlayout(audioChannel);
        numberOfErrors += ViETest::TestError(error == 0,
                                             "ERROR: %s at line %d",
                                             __FUNCTION__, __LINE__);

        error = ptrVEBase->StartSend(audioChannel);
        numberOfErrors += ViETest::TestError(error == 0,
                                             "ERROR: %s at line %d",
                                             __FUNCTION__, __LINE__);


        // ViE next
        error = ptrViEBase->StartSend(videoChannel);
        numberOfErrors += ViETest::TestError(error == 0,
                                             "ERROR: %s at line %d",
                                             __FUNCTION__, __LINE__);

        error = ptrViEBase->StartReceive(videoChannel);
        numberOfErrors += ViETest::TestError(error == 0,
                                             "ERROR: %s at line %d",
                                             __FUNCTION__, __LINE__);

        error = ptrViERender->MirrorRenderStream(captureId, true, false, true);
        numberOfErrors += ViETest::TestError(error == 0,
                                             "ERROR: %s at line %d",
                                             __FUNCTION__, __LINE__);

        error = ptrViERender->StartRender(captureId);
        numberOfErrors += ViETest::TestError(error == 0,
                                             "ERROR: %s at line %d",
                                             __FUNCTION__, __LINE__);

        error = ptrViERender->StartRender(videoChannel);
        numberOfErrors += ViETest::TestError(error == 0,
                                             "ERROR: %s at line %d",
                                             __FUNCTION__, __LINE__);

        //***************************************************************
        //  Engine ready. Wait for input
        //***************************************************************


        // Call started
        std::cout << std::endl;
        std::cout << "Loopback call started" << std::endl;
        std::cout << std::endl << std::endl;
        std::cout << "Press enter to stop...";
        std::getline(std::cin, str);

        //***************************************************************
        //  Testing finished. Tear down Video Engine
        //***************************************************************


        // audio engine first
        error = ptrVEBase->StopReceive(audioChannel);
        numberOfErrors += ViETest::TestError(error == 0,
                                             "ERROR: %s at line %d",
                                             __FUNCTION__, __LINE__);

        error = ptrVEBase->StopPlayout(audioChannel);
        numberOfErrors += ViETest::TestError(error == 0,
                                             "ERROR: %s at line %d",
                                             __FUNCTION__, __LINE__);

        error = ptrVEBase->DeleteChannel(audioChannel);
        numberOfErrors += ViETest::TestError(error == 0,
                                             "ERROR: %s at line %d",
                                             __FUNCTION__, __LINE__);

        // now do video
        error = ptrViEBase->DisconnectAudioChannel(videoChannel);

        error = ptrViEBase->StopReceive(videoChannel);
        numberOfErrors += ViETest::TestError(error == 0,
                                             "ERROR: %s at line %d",
                                             __FUNCTION__, __LINE__);

        error = ptrViEBase->StopSend(videoChannel);
        numberOfErrors += ViETest::TestError(error == 0,
                                             "ERROR: %s at line %d",
                                             __FUNCTION__, __LINE__);

        error = ptrViERender->StopRender(captureId);
        numberOfErrors += ViETest::TestError(error == 0,
                                             "ERROR: %s at line %d",
                                             __FUNCTION__, __LINE__);

        error = ptrViERender->StopRender(videoChannel);
        numberOfErrors += ViETest::TestError(error == 0,
                                             "ERROR: %s at line %d",
                                             __FUNCTION__, __LINE__);

        error = ptrViERender->RemoveRenderer(captureId);
        numberOfErrors += ViETest::TestError(error == 0,
                                             "ERROR: %s at line %d",
                                             __FUNCTION__, __LINE__);

        error = ptrViERender->RemoveRenderer(videoChannel);
        numberOfErrors += ViETest::TestError(error == 0,
                                             "ERROR: %s at line %d",
                                             __FUNCTION__, __LINE__);

        error = ptrViECapture->StopCapture(captureId);
        numberOfErrors += ViETest::TestError(error == 0,
                                             "ERROR: %s at line %d",
                                             __FUNCTION__, __LINE__);

        error = ptrViECapture->DisconnectCaptureDevice(videoChannel);
        numberOfErrors += ViETest::TestError(error == 0,
                                             "ERROR: %s at line %d",
                                             __FUNCTION__, __LINE__);

        error = ptrViECapture->ReleaseCaptureDevice(captureId);
        numberOfErrors += ViETest::TestError(error == 0,
                                             "ERROR: %s at line %d",
                                             __FUNCTION__, __LINE__);

        error = ptrViEBase->DeleteChannel(videoChannel);
        numberOfErrors += ViETest::TestError(error == 0,
                                             "ERROR: %s at line %d",
                                             __FUNCTION__, __LINE__);

        int remainingInterfaces = 0;
        remainingInterfaces = ptrViECodec->Release();
        numberOfErrors += ViETest::TestError(remainingInterfaces == 0,
                                             "ERROR: %s at line %d",
                                             __FUNCTION__, __LINE__);

        remainingInterfaces = ptrViECapture->Release();
        numberOfErrors += ViETest::TestError(remainingInterfaces == 0,
                                             "ERROR: %s at line %d",
                                             __FUNCTION__, __LINE__);

        remainingInterfaces = ptrViERtpRtcp->Release();
        numberOfErrors += ViETest::TestError(remainingInterfaces == 0,
                                             "ERROR: %s at line %d",
                                             __FUNCTION__, __LINE__);

        remainingInterfaces = ptrViERender->Release();
        numberOfErrors += ViETest::TestError(remainingInterfaces == 0,
                                             "ERROR: %s at line %d",
                                             __FUNCTION__, __LINE__);

        remainingInterfaces = ptrViENetwork->Release();
        numberOfErrors += ViETest::TestError(remainingInterfaces == 0,
                                             "ERROR: %s at line %d",
                                             __FUNCTION__, __LINE__);

        remainingInterfaces = ptrViEBase->Release();
        numberOfErrors += ViETest::TestError(remainingInterfaces == 0,
                                             "ERROR: %s at line %d",
                                             __FUNCTION__, __LINE__);

        bool deleted = VideoEngine::Delete(ptrViE);
        numberOfErrors += ViETest::TestError(deleted == true,
                                             "ERROR: %s at line %d",
                                             __FUNCTION__, __LINE__);

        ViETest::Log(" ");
        ViETest::Log(" ViE Autotest Loopback Call Done");
        ViETest::Log("========================================");
        ViETest::Log(" ");
    }
    return numberOfErrors;
}

bool ViEAutoTest::GetVideoDevice(ViEBase* ptrViEBase,
                                 ViECapture* ptrViECapture,
                                 char* captureDeviceName,
                                 char* captureDeviceUniqueId)
{
    int error = 0;
    int numberOfErrors = 0;
    int captureDeviceIndex = 0;
    std::string str;

    const unsigned int KMaxDeviceNameLength = 128;
    const unsigned int KMaxUniqueIdLength = 256;
    char deviceName[KMaxDeviceNameLength];
    char uniqueId[KMaxUniqueIdLength];

    while(1)
    {
        memset(deviceName, 0, KMaxDeviceNameLength);
        memset(uniqueId, 0, KMaxUniqueIdLength);

        std::cout << std::endl;
        std::cout << "Available capture devices:" << std::endl;
        int captureIdx = 0;
        for (captureIdx = 0;
             captureIdx < ptrViECapture->NumberOfCaptureDevices(); captureIdx++)
        {
            memset(deviceName, 0, KMaxDeviceNameLength);
            memset(uniqueId, 0, KMaxUniqueIdLength);

            error = ptrViECapture->GetCaptureDevice(captureIdx, deviceName,
                                                    KMaxDeviceNameLength,
                                                    uniqueId,
                                                    KMaxUniqueIdLength);
            numberOfErrors += ViETest::TestError(error == 0,
                                                 "ERROR: %s at line %d",
                                                 __FUNCTION__, __LINE__);
            std::cout << "   " << captureIdx+1 << ". " << deviceName
                      << std::endl;
        }
        std::cout << "Choose a capture device. Press enter for default ("
                  << deviceName << "/" << uniqueId << "): ";
        std::getline(std::cin, str);
        captureDeviceIndex = atoi(str.c_str());

        if(captureDeviceIndex == 0)
        {
            // use default (or first) camera
            error = ptrViECapture->GetCaptureDevice(0, deviceName,
                                                    KMaxDeviceNameLength,
                                                    uniqueId,
                                                    KMaxUniqueIdLength);
            numberOfErrors += ViETest::TestError(error == 0,
                                                 "ERROR: %s at line %d",
                                                 __FUNCTION__, __LINE__);
            strcpy(captureDeviceUniqueId, uniqueId);
            strcpy(captureDeviceName, deviceName);
            return true;
        }
        else if(captureDeviceIndex < 0
                || (captureDeviceIndex >
                    (int)ptrViECapture->NumberOfCaptureDevices()))
        {
            // invalid selection
            continue;
        }
        else
        {
            error = ptrViECapture->GetCaptureDevice(captureDeviceIndex - 1,
                                                    deviceName,
                                                    KMaxDeviceNameLength,
                                                    uniqueId,
                                                    KMaxUniqueIdLength);
            numberOfErrors += ViETest::TestError(error == 0,
                                                 "ERROR: %s at line %d",
                                                 __FUNCTION__, __LINE__);
            strcpy(captureDeviceName, uniqueId);
            strcpy(captureDeviceName, deviceName);
            return true;
        }
    }
}

bool ViEAutoTest::GetAudioDevices(VoEBase* ptrVEBase,
                                  VoEHardware* ptrVEHardware,
                                  char* recordingDeviceName,
                                  int& recordingDeviceIndex,
                                  char* playbackDeviceName,
                                  int& playbackDeviceIndex)
{
    int error = 0;
    int numberOfErrors = 0;
    int captureDeviceIndex = 0;
    std::string str;

    const unsigned int KMaxDeviceNameLength = 128;
    const unsigned int KMaxUniqueIdLength = 128;
    char recordingDeviceUniqueName[KMaxDeviceNameLength];
    char playbackDeviceUniqueName[KMaxUniqueIdLength];

    int numberOfRecordingDevices = -1;
    error = ptrVEHardware->GetNumOfRecordingDevices(numberOfRecordingDevices);
    numberOfErrors += ViETest::TestError(error == 0, "ERROR: %s at line %d",
                                         __FUNCTION__, __LINE__);

    while(1)
    {
        recordingDeviceIndex = -1;
        std::cout << std::endl;
        std::cout << "Available audio capture devices:" << std::endl;
        int captureIdx = 0;

        for (captureIdx = 0; captureIdx < numberOfRecordingDevices;
             captureIdx++)
        {
            memset(recordingDeviceName, 0, KMaxDeviceNameLength);
            memset(recordingDeviceUniqueName, 0, KMaxDeviceNameLength);
            error = ptrVEHardware->GetRecordingDeviceName(
                captureIdx, recordingDeviceName, recordingDeviceUniqueName);
            numberOfErrors += ViETest::TestError(error == 0,
                                                 "ERROR: %s at line %d",
                                                 __FUNCTION__, __LINE__);
            std::cout << "   " << captureIdx+1 << ". " << recordingDeviceName
                      << std::endl;
        }

        std::cout << "Choose an audio capture device. Press enter for default("
                  << recordingDeviceName << "): ";
        std::getline(std::cin, str);
        int captureDeviceIndex = atoi(str.c_str());

        if(captureDeviceIndex == 0)
        {
            // use default (or first) camera
            recordingDeviceIndex = 0;
            error = ptrVEHardware->GetRecordingDeviceName(
                recordingDeviceIndex, recordingDeviceName,
                recordingDeviceUniqueName);
            numberOfErrors += ViETest::TestError(error == 0,
                                                 "ERROR: %s at line %d",
                                                 __FUNCTION__, __LINE__);
            break;
        }
        else if(captureDeviceIndex < 0
                || captureDeviceIndex > numberOfRecordingDevices)
        {
            // invalid selection
            continue;
        }
        else
        {
            recordingDeviceIndex = captureDeviceIndex - 1;
            error = ptrVEHardware->GetRecordingDeviceName(
                recordingDeviceIndex, recordingDeviceName,
                recordingDeviceUniqueName);
            numberOfErrors += ViETest::TestError(error == 0,
                                                 "ERROR: %s at line %d",
                                                 __FUNCTION__, __LINE__);
            break;
        }
    }

    int numberOfPlaybackDevices = -1;
    error = ptrVEHardware->GetNumOfPlayoutDevices(numberOfPlaybackDevices);
    numberOfErrors += ViETest::TestError(error == 0, "ERROR: %s at line %d",
                                         __FUNCTION__, __LINE__);

    while(1)
    {
        playbackDeviceIndex = -1;
        std::cout << std::endl;
        std::cout << "Available audio playout devices:" << std::endl;
        int captureIdx = 0;

        for (captureIdx = 0; captureIdx < numberOfPlaybackDevices;
             captureIdx++)
        {
            memset(playbackDeviceName, 0, KMaxDeviceNameLength);
            memset(playbackDeviceUniqueName, 0, KMaxDeviceNameLength);
            error = ptrVEHardware->GetPlayoutDeviceName(
                captureIdx, playbackDeviceName, playbackDeviceUniqueName);
            numberOfErrors += ViETest::TestError(error == 0,
                                                 "ERROR: %s at line %d",
                                                 __FUNCTION__, __LINE__);
            std::cout << "   " << captureIdx+1 << ". " << playbackDeviceName
                      << std::endl;
        }

        std::cout <<
                  "Choose an audio playback device. Press enter for default ("
                  << playbackDeviceName << "): ";
        std::getline(std::cin, str);
        int captureDeviceIndex = atoi(str.c_str());

        if(captureDeviceIndex == 0)
        {
            // use default (or first) camera
            playbackDeviceIndex = 0;
            error = ptrVEHardware->GetPlayoutDeviceName(
                playbackDeviceIndex, playbackDeviceName,
                playbackDeviceUniqueName);
            numberOfErrors += ViETest::TestError(error == 0,
                                                 "ERROR: %s at line %d",
                                                 __FUNCTION__, __LINE__);
            return true;
        }
        else if(captureDeviceIndex < 0
                || captureDeviceIndex > numberOfPlaybackDevices)
        {
            // invalid selection
            continue;
        }
        else
        {
            playbackDeviceIndex = captureDeviceIndex - 1;
            error = ptrVEHardware->GetPlayoutDeviceName(
                playbackDeviceIndex, playbackDeviceName,
                playbackDeviceUniqueName);
            numberOfErrors += ViETest::TestError(error == 0,
                                                 "ERROR: %s at line %d",
                                                 __FUNCTION__, __LINE__);
            return true;
        }
    }
}

// general settings functions
bool ViEAutoTest::GetIPAddress(char* iIP)
{
    int error = 0;
    char oIP[16] = DEFAULT_SEND_IP;
    std::string str;

    while(1)
    {
        std::cout << std::endl;
        std::cout << "Enter destination IP. Press enter for default ("
                  << oIP << "): ";
        std::getline(std::cin, str);

        if(str.compare("") == 0)
        {
            // use default value;
            strcpy(iIP, oIP);
            return true;
        }

        if(ValidateIP(str) == false)
        {
            std::cout << "Invalid entry. Try again." << std::endl;
            continue;
        }

        // done. Copy std::string to c_string and return
        strcpy(iIP, str.c_str());
        return true;
    }
    assert(false);
    return false;
}

bool ViEAutoTest::ValidateIP(std::string iStr)
{
    if(0 == iStr.compare(""))
    {
        return false;
    }
    return true;
}

// video settings functions
bool ViEAutoTest::GetVideoPorts(int* txPort, int* rxPort)
{
    int error = 0;
    std::string str;
    int port = 0;

    // set to default values
    *txPort = DEFAULT_VIDEO_PORT;
    *rxPort = DEFAULT_VIDEO_PORT;

    while(1)
    {
        std::cout << "Enter video send port. Press enter for default ("
                  << *txPort << ")";
        std::getline(std::cin, str);
        port = atoi(str.c_str());

        if(port == 0)
        {
            // default value
            break;
        }
        else
        {
            // user selection
            if(port <= 0 || port > 63556)
            {
                // invalid selection
                continue;
            }
            else
            {
                *txPort = port;
                break; // move on to rxport
            }
        }
    }

    while(1)
    {
        std::cout << "Enter video receive port. Press enter for default ("
                  << *rxPort << ")";
        std::getline(std::cin, str);
        port = atoi(str.c_str());

        if(port == 0)
        {
            // default value
            return true;
        }
        else
        {
            // user selection
            if(port <= 0 || port > 63556)
            {
                // invalid selection
                continue;
            }
            else
            {
              *rxPort = port;
              return true;
            }
      }
  }
  assert(false);
  return false;
}
bool ViEAutoTest::GetVideoCodec(ViECodec* ptrViECodec,
                                webrtc::VideoCodec& videoCodec)
{
    int error = 0;
    int numberOfErrors = 0;
    int codecSelection = 0;
    std::string str;
    memset(&videoCodec, 0, sizeof(webrtc::VideoCodec));

    bool exitLoop=false;
    while(!exitLoop)
    {
        std::cout << std::endl;
        std::cout << "Available video codecs:" << std::endl;
        int codecIdx = 0;
        int defaultCodecIdx = 0;
        for (codecIdx = 0; codecIdx < ptrViECodec->NumberOfCodecs(); codecIdx++)
        {
            error = ptrViECodec->GetCodec(codecIdx, videoCodec);
            numberOfErrors += ViETest::TestError(error == 0,
                                                 "ERROR: %s at line %d",
                                                 __FUNCTION__, __LINE__);

            // test for default codec index
            if(strcmp(videoCodec.plName, DEFAULT_VIDEO_CODEC) == 0)
            {
                defaultCodecIdx = codecIdx;
            }

            std::cout << "   " << codecIdx+1 << ". " << videoCodec.plName
                      << std::endl;
        }
        std::cout << std::endl;
        std::cout << "Choose video codec. Press enter for default ("
                  << DEFAULT_VIDEO_CODEC << ")";
        std::getline(std::cin, str);
        codecSelection = atoi(str.c_str());

        if(codecSelection == 0)
        {
            // use default
            error = ptrViECodec->GetCodec(defaultCodecIdx, videoCodec);
            numberOfErrors += ViETest::TestError(error == 0,
                                                 "ERROR: %s at line %d",
                                                 __FUNCTION__, __LINE__);
            exitLoop=true;
        }
        else
        {
            // user selection
            codecSelection = atoi(str.c_str())-1;
            error = ptrViECodec->GetCodec(codecSelection, videoCodec);
            numberOfErrors += ViETest::TestError(error == 0,
                                                 "ERROR: %s at line %d",
                                                 __FUNCTION__, __LINE__);
            if(error != 0)
            {
                std::cout << "ERROR: Code=" << error << " Invalid selection"
                          << std::endl;
                continue;
            }
            exitLoop=true;
      }
    }

    std::cout << "Choose video width. Press enter for default ("
              << DEFAULT_VIDEO_CODEC_WIDTH << ")";
    std::getline(std::cin, str);
    int sizeSelection = atoi(str.c_str());
    if(sizeSelection!=0)
    {
        videoCodec.width=sizeSelection;
    }

    std::cout << "Choose video height. Press enter for default ("
              << DEFAULT_VIDEO_CODEC_HEIGHT << ")";
    std::getline(std::cin, str);
    sizeSelection = atoi(str.c_str());
    if(sizeSelection!=0)
    {
        videoCodec.height=sizeSelection;
    }
    return true;
}

// audio settings functions
bool ViEAutoTest::GetAudioPorts(int* txPort, int* rxPort)
{
    int error = 0;
    int port = 0;
    std::string str;

    // set to default values
    *txPort = DEFAULT_AUDIO_PORT;
    *rxPort = DEFAULT_AUDIO_PORT;

    while(1)
    {
        std::cout << "Enter audio send port. Press enter for default ("
                  << *txPort << ")";
        std::getline(std::cin, str);
        port = atoi(str.c_str());

        if(port == 0)
        {
            // default value
            break;
        }
        else
        {
            // user selection
            if(port <= 0 || port > 63556)
            {
                // invalid selection
                continue;
            }
            else
            {
                *txPort = port;
                break; // move on to rxport
            }
        }
    }

    while(1)
    {
        std::cout << "Enter audio receive port. Press enter for default ("
                  << *rxPort << ")";
        std::getline(std::cin, str);
        port = atoi(str.c_str());

        if(port == 0)
        {
            // default value
            return true;
        }
        else
        {
            // user selection
            if(port <= 0 || port > 63556)
            {
                // invalid selection
                continue;
            }
            else
            {
                *rxPort = port;
                return true;
            }
        }
    }
    assert(false);
    return false;
}

bool ViEAutoTest::GetAudioCodec(webrtc::VoECodec* ptrVeCodec, webrtc::CodecInst& audioCodec)
{
    int error = 0;
    int numberOfErrors = 0;
    int codecSelection = 0;
    std::string str;
    memset(&audioCodec, 0, sizeof(webrtc::CodecInst));

    while(1)
    {
        std::cout << std::endl;
        std::cout << "Available audio codecs:" << std::endl;
        int codecIdx = 0;
        int defaultCodecIdx = 0;
        for (codecIdx = 0; codecIdx < ptrVeCodec->NumOfCodecs(); codecIdx++)
        {
            error = ptrVeCodec->GetCodec(codecIdx, audioCodec);
            numberOfErrors += ViETest::TestError(error == 0,
                                                 "ERROR: %s at line %d",
                                                 __FUNCTION__, __LINE__);

            // test for default codec index
            if(strcmp(audioCodec.plname, DEFAULT_AUDIO_CODEC) == 0)
            {
              defaultCodecIdx = codecIdx;
            }
            std::cout << "   " << codecIdx+1 << ". " << audioCodec.plname
                      << std::endl;
        }
        std::cout << std::endl;
        std::cout << "Choose audio codec. Press enter for default ("
                  << DEFAULT_AUDIO_CODEC << ")";
        std::getline(std::cin, str);
        codecSelection = atoi(str.c_str());

        if(codecSelection == 0)
        {
            // use default
            error = ptrVeCodec->GetCodec(defaultCodecIdx, audioCodec);
            numberOfErrors += ViETest::TestError(error == 0,
                                                 "ERROR: %s at line %d",
                                                 __FUNCTION__, __LINE__);
            return true;
        }
        else
        {
            // user selection
            codecSelection = atoi(str.c_str())-1;
            error = ptrVeCodec->GetCodec(codecSelection, audioCodec);
            numberOfErrors += ViETest::TestError(error == 0,
                                                 "ERROR: %s at line %d",
                                                 __FUNCTION__, __LINE__);
            if(error != 0)
            {
                std::cout << "ERROR: Code = " << error << " Invalid selection"
                          << std::endl;
                continue;
            }
            return true;
        }
    }
    assert(false);
    return false;
}

void ViEAutoTest::PrintCallInformation(char* IP, char* videoCaptureDeviceName,
                                       char* videoCaptureUniqueId,
                                       webrtc::VideoCodec videoCodec,
                                       int videoTxPort, int videoRxPort,
                                       char* audioCaptureDeviceName,
                                       char* audioPlaybackDeviceName,
                                       webrtc::CodecInst audioCodec,
                                       int audioTxPort, int audioRxPort)
{
    std::string str;

    std::cout << "************************************************"
              << std::endl;
    std::cout << "The call will use the following settings: " << std::endl;
    std::cout << "\tIP: " << IP << std::endl;
    std::cout << "\tVideo Capture Device: " << videoCaptureDeviceName
              << std::endl;
    std::cout << "\t\tName: " << videoCaptureDeviceName << std::endl;
    std::cout << "\t\tUniqueId: " << videoCaptureUniqueId << std::endl;
    std::cout << "\tVideo Codec: " << std::endl;
    std::cout << "\t\tplName: " << videoCodec.plName << std::endl;
    std::cout << "\t\tplType: " << (int)videoCodec.plType << std::endl;
    std::cout << "\t\twidth: " << videoCodec.width << std::endl;
    std::cout << "\t\theight: " << videoCodec.height << std::endl;
    std::cout << "\t Video Tx Port: " << videoTxPort << std::endl;
    std::cout << "\t Video Rx Port: " << videoRxPort << std::endl;
    std::cout << "\tAudio Capture Device: " << audioCaptureDeviceName
              << std::endl;
    std::cout << "\tAudio Playback Device: " << audioPlaybackDeviceName
              << std::endl;
    std::cout << "\tAudio Codec: " << std::endl;
    std::cout << "\t\tplname: " << audioCodec.plname << std::endl;
    std::cout << "\t\tpltype: " << (int)audioCodec.pltype << std::endl;
    std::cout << "\t Audio Tx Port: " << audioTxPort << std::endl;
    std::cout << "\t Audio Rx Port: " << audioRxPort << std::endl;
    std::cout << "************************************************"
              << std::endl;
}
