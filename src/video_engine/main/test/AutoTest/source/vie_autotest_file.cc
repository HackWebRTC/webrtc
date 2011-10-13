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
// vie_autotest_file.cc
//

#include "vie_autotest_defines.h"
#include "vie_autotest.h"
#include "engine_configurations.h"

#include "tb_interfaces.h"
#include "tb_capture_device.h"

#include "voe_codec.h"

class ViEAutotestFileObserver: public webrtc::ViEFileObserver
{
public:
    ViEAutotestFileObserver() {};
    ~ViEAutotestFileObserver() {};

    void PlayFileEnded(const WebRtc_Word32 fileId)
    {
        ViETest::Log("PlayFile ended");
    }
};

int ViEAutoTest::ViEFileStandardTest()
{
    ViETest::Log(" ");
    ViETest::Log("========================================");
    ViETest::Log(" ViEFile Standard Test\n");

#ifdef WEBRTC_VIDEO_ENGINE_FILE_API
    //***************************************************************
    //	Begin create/initialize WebRTC Video Engine for testing
    //***************************************************************


    int error = 0;
    int numberOfErrors = 0;

    {
        ViETest::Log("Starting a loopback call...");

        tbInterfaces interfaces = tbInterfaces("ViEFileStandardTest",
                                               numberOfErrors);

        webrtc::VideoEngine* ptrViE = interfaces.ptrViE;
        webrtc::ViEBase* ptrViEBase = interfaces.ptrViEBase;
        webrtc::ViECapture* ptrViECapture = interfaces.ptrViECapture;
        webrtc::ViERender* ptrViERender = interfaces.ptrViERender;
        webrtc::ViECodec* ptrViECodec = interfaces.ptrViECodec;
        webrtc::ViERTP_RTCP* ptrViERtpRtcp = interfaces.ptrViERtpRtcp;
        webrtc::ViENetwork* ptrViENetwork = interfaces.ptrViENetwork;

        tbCaptureDevice captureDevice = tbCaptureDevice(interfaces,
                                                        numberOfErrors);
        int captureId = captureDevice.captureId;

        int videoChannel = -1;
        error = ptrViEBase->CreateChannel(videoChannel);
        numberOfErrors += ViETest::TestError(error == 0,
                                             "ERROR:%d %s at line %d",
                                             ptrViEBase->LastError(),
                                             __FUNCTION__, __LINE__);

        error = ptrViECapture->ConnectCaptureDevice(captureId, videoChannel);
        numberOfErrors += ViETest::TestError(error == 0,
                                             "ERROR:%d %s at line %d",
                                             ptrViEBase->LastError(),
                                             __FUNCTION__, __LINE__);

        error = ptrViERtpRtcp->SetRTCPStatus(videoChannel,
                                             webrtc::kRtcpCompound_RFC4585);
        numberOfErrors += ViETest::TestError(error == 0,
                                             "ERROR:%d %s at line %d",
                                             ptrViEBase->LastError(),
                                             __FUNCTION__, __LINE__);

        error = ptrViERtpRtcp->SetKeyFrameRequestMethod(
            videoChannel, webrtc::kViEKeyFrameRequestPliRtcp);
        numberOfErrors += ViETest::TestError(error == 0,
                                             "ERROR:%d %s at line %d",
                                             ptrViEBase->LastError(),
                                             __FUNCTION__, __LINE__);

        error = ptrViERtpRtcp->SetTMMBRStatus(videoChannel, true);
        numberOfErrors += ViETest::TestError(error == 0,
                                             "ERROR:%d %s at line %d",
                                             ptrViEBase->LastError(),
                                             __FUNCTION__, __LINE__);

        error = ptrViERender->AddRenderer(captureId, _window1, 0, 0.0, 0.0,
                                          1.0, 1.0);
        numberOfErrors += ViETest::TestError(error == 0,
                                             "ERROR:%d %s at line %d",
                                             ptrViEBase->LastError(),
                                             __FUNCTION__, __LINE__);

        error = ptrViERender->AddRenderer(videoChannel, _window2, 1, 0.0, 0.0,
                                          1.0, 1.0);
        numberOfErrors += ViETest::TestError(error == 0,
                                             "ERROR:%d %s at line %d",
                                             ptrViEBase->LastError(),
                                             __FUNCTION__, __LINE__);

        error = ptrViERender->StartRender(captureId);
        numberOfErrors += ViETest::TestError(error == 0,
                                             "ERROR:%d %s at line %d",
                                             ptrViEBase->LastError(),
                                             __FUNCTION__, __LINE__);

        error = ptrViERender->StartRender(videoChannel);
        numberOfErrors += ViETest::TestError(error == 0,
                                             "ERROR:%d %s at line %d",
                                             ptrViEBase->LastError(),
                                             __FUNCTION__, __LINE__);

        webrtc::VideoCodec videoCodec;
        memset(&videoCodec, 0, sizeof(webrtc::VideoCodec));
        for (int idx = 0; idx < ptrViECodec->NumberOfCodecs(); idx++)
        {
            error = ptrViECodec->GetCodec(idx, videoCodec);
            numberOfErrors += ViETest::TestError(error == 0,
                                                 "ERROR:%d %s at line %d",
                                                 ptrViEBase->LastError(),
                                                 __FUNCTION__, __LINE__);

            error = ptrViECodec->SetReceiveCodec(videoChannel, videoCodec);
            numberOfErrors += ViETest::TestError(error == 0,
                                                 "ERROR:%d %s at line %d",
                                                 ptrViEBase->LastError(),
                                                 __FUNCTION__, __LINE__);
        }

        // Find the codec used for encoding the channel
        for (int idx = 0; idx < ptrViECodec->NumberOfCodecs(); idx++)
        {
            error = ptrViECodec->GetCodec(idx, videoCodec);
            numberOfErrors += ViETest::TestError(error == 0,
                                                 "ERROR:%d %s at line %d",
                                                 ptrViEBase->LastError(),
                                                 __FUNCTION__, __LINE__);
            if (videoCodec.codecType == webrtc::kVideoCodecVP8)
            {
                error = ptrViECodec->SetSendCodec(videoChannel, videoCodec);
                numberOfErrors += ViETest::TestError(error == 0,
                                                     "ERROR:%d %s at line %d",
                                                     ptrViEBase->LastError(),
                                                     __FUNCTION__, __LINE__);
                break;
            }
        }
        // Find the codec used for recording.
        for (int idx = 0; idx < ptrViECodec->NumberOfCodecs(); idx++)
        {
            error = ptrViECodec->GetCodec(idx, videoCodec);
            numberOfErrors += ViETest::TestError(error == 0,
                                                 "ERROR:%d %s at line %d",
                                                 ptrViEBase->LastError(),
                                                 __FUNCTION__, __LINE__);
            if (videoCodec.codecType == webrtc::kVideoCodecI420)
            {
                break;
            }
        }


        const char* ipAddress = "127.0.0.1";
        const unsigned short rtpPort = 6000;
        error = ptrViENetwork->SetLocalReceiver(videoChannel, rtpPort);
        numberOfErrors += ViETest::TestError(error == 0,
                                             "ERROR:%d %s at line %d",
                                             ptrViEBase->LastError(),
                                             __FUNCTION__, __LINE__);

        error = ptrViEBase->StartReceive(videoChannel);
        numberOfErrors += ViETest::TestError(error == 0,
                                             "ERROR:%d %s at line %d",
                                             ptrViEBase->LastError(),
                                             __FUNCTION__, __LINE__);

        error = ptrViENetwork->SetSendDestination(videoChannel, ipAddress,
                                                  rtpPort);
        numberOfErrors += ViETest::TestError(error == 0,
                                             "ERROR:%d %s at line %d",
                                             ptrViEBase->LastError(),
                                             __FUNCTION__, __LINE__);

        error = ptrViEBase->StartSend(videoChannel);
        numberOfErrors += ViETest::TestError(error == 0,
                                             "ERROR:%d %s at line %d",
                                             ptrViEBase->LastError(),
                                             __FUNCTION__, __LINE__);

        webrtc::ViEFile* ptrViEFile = webrtc::ViEFile::GetInterface(ptrViE);
        numberOfErrors += ViETest::TestError(error == 0,
                                             "ERROR:%d %s at line %d",
                                             ptrViEBase->LastError(),
                                             __FUNCTION__, __LINE__);

        webrtc::VoiceEngine* ptrVEEngine = webrtc::VoiceEngine::Create();
        webrtc::VoEBase* ptrVEBase = webrtc::VoEBase::GetInterface(ptrVEEngine);
        ptrVEBase->Init();

        int audioChannel = ptrVEBase->CreateChannel();
        ptrViEBase->SetVoiceEngine(ptrVEEngine);
        ptrViEBase->ConnectAudioChannel(videoChannel, audioChannel);

        webrtc::CodecInst audioCodec;
        webrtc::VoECodec* ptrVECodec =
            webrtc::VoECodec::GetInterface(ptrVEEngine);
        for (int index = 0; index < ptrVECodec->NumOfCodecs(); index++)
        {
            ptrVECodec->GetCodec(index, audioCodec);
            if (0 == strcmp(audioCodec.plname, "PCMU") || 0
                == strcmp(audioCodec.plname, "PCMA"))
            {
                break; // these two types are allowed as avi recording formats
            }
        }

        webrtc::CodecInst audioCodec2;

        //***************************************************************
        //	Engine ready. Begin testing class
        //***************************************************************

        // Call started
        ViETest::Log("Call started\nYou should see local preview from camera\n"
                     "in window 1 and the remote video in window 2.");
        AutoTestSleep(2000);

        const int RENDER_TIMEOUT = 1000;
        const int TEST_SPACING = 1000;
        const int VIDEO_LENGTH = 5000;


        const char renderStartImage[1024] = VIE_TEST_FILES_ROOT "renderStartImage.jpg";
        const char captureDeviceImage[1024] = VIE_TEST_FILES_ROOT "captureDeviceImage.jpg";
        const char renderTimeoutFile[1024] = VIE_TEST_FILES_ROOT "renderTimeoutImage.jpg";
        const char snapshotCaptureDeviceFileName[256] = VIE_TEST_FILES_ROOT
            "snapshotCaptureDevice.jpg";
        const char incomingVideo[1024] = VIE_TEST_FILES_ROOT "incomingVideo.avi";
        const char outgoingVideo[1024] = VIE_TEST_FILES_ROOT "outgoingVideo.avi";
        char snapshotRenderFileName[256] = VIE_TEST_FILES_ROOT "snapshotRenderer.jpg";

        webrtc::ViEPicture capturePicture;
        webrtc::ViEPicture renderPicture;
        webrtc::ViEPicture renderTimeoutPicture; // TODO: init with and image

        ViEAutotestFileObserver fileObserver;
        int fileId;

        AutoTestSleep(TEST_SPACING);

        // testing StartRecordIncomingVideo and StopRecordIncomingVideo
        {
            ViETest::Log("Recording incoming video (currently no audio) for %d "
                         "seconds", VIDEO_LENGTH);

            error = ptrViEFile->StartRecordIncomingVideo(videoChannel,
                                                         incomingVideo,
                                                         webrtc::NO_AUDIO,
                                                         audioCodec2,
                                                         videoCodec);
            numberOfErrors += ViETest::TestError(error == 0,
                                                 "ERROR:%d %s at line %d",
                                                 ptrViEBase->LastError(),
                                                 __FUNCTION__, __LINE__);

            AutoTestSleep(VIDEO_LENGTH);
            ViETest::Log("Stop recording incoming video");

            error = ptrViEFile->StopRecordIncomingVideo(videoChannel);
            numberOfErrors += ViETest::TestError(error == 0,
                                                 "ERROR:%d %s at line %d",
                                                 ptrViEBase->LastError(),
                                                 __FUNCTION__, __LINE__);
            ViETest::Log("Done\n");
        }

        AutoTestSleep(TEST_SPACING);

        // testing GetFileInformation
        {
            webrtc::VideoCodec fileVideoCodec;
            webrtc::CodecInst fileAudioCodec;
            ViETest::Log("Reading video file information");

            error = ptrViEFile->GetFileInformation(incomingVideo,
                                                   fileVideoCodec,
                                                   fileAudioCodec);
            numberOfErrors += ViETest::TestError(error == 0,
                                                 "ERROR:%d %s at line %d",
                                                 ptrViEBase->LastError(),
                                                 __FUNCTION__, __LINE__);
            PrintAudioCodec(fileAudioCodec);
            PrintVideoCodec(fileVideoCodec);
        }

        // testing StartPlayFile and RegisterObserver
        {
            ViETest::Log("Start playing file: %s with observer", incomingVideo);
            error = ptrViEFile->StartPlayFile(incomingVideo, fileId);
            numberOfErrors += ViETest::TestError(error == 0,
                                                 "ERROR:%d %s at line %d",
                                                 ptrViEBase->LastError(),
                                                 __FUNCTION__, __LINE__);

            ViETest::Log("Registering file observer");
            error = ptrViEFile->RegisterObserver(fileId, fileObserver);
            numberOfErrors += ViETest::TestError(error == 0,
                                                 "ERROR:%d %s at line %d",
                                                 ptrViEBase->LastError(),
                                                 __FUNCTION__, __LINE__);
            numberOfErrors += ViETest::TestError(error == 0,
                                                 "ERROR:%d %s at line %d",
                                                 ptrViEBase->LastError(),
                                                 __FUNCTION__, __LINE__);

            ViETest::Log("Done\n");
        }

        // testing SendFileOnChannel and StopSendFileOnChannel
        {
            ViETest::Log("Sending video on channel");
            // should fail since we are sending the capture device.
            error = ptrViEFile->SendFileOnChannel(fileId, videoChannel);
            numberOfErrors += ViETest::TestError(error == -1,
                                                 "ERROR:%d %s at line %d",
                                                 ptrViEBase->LastError(),
                                                 __FUNCTION__, __LINE__);

            // Disconnect the camera
            error = ptrViECapture->DisconnectCaptureDevice(videoChannel);
            numberOfErrors += ViETest::TestError(error == 0,
                                                 "ERROR:%d %s at line %d",
                                                 ptrViEBase->LastError(),
                                                 __FUNCTION__, __LINE__);

            // And try playing the file again.
            error = ptrViEFile->SendFileOnChannel(fileId, videoChannel);
            numberOfErrors += ViETest::TestError(error == 0,
                                                 "ERROR:%d %s at line %d",
                                                 ptrViEBase->LastError(),
                                                 __FUNCTION__, __LINE__);

            AutoTestSleep(VIDEO_LENGTH);
            ViETest::Log("Stopped sending video on channel");
            error = ptrViEFile->StopSendFileOnChannel(videoChannel);
            numberOfErrors += ViETest::TestError(error == 0,
                                                 "ERROR:%d %s at line %d",
                                                 ptrViEBase->LastError(),
                                                 __FUNCTION__, __LINE__);
            ViETest::Log("Done\n");
        }

        AutoTestSleep(TEST_SPACING);

        // stop playing the file
        {
            ViETest::Log("Stop playing the file.");
            error = ptrViEFile->StopPlayFile(fileId);
            numberOfErrors += ViETest::TestError(error == 0,
                                                 "ERROR:%d %s at line %d",
                                                 ptrViEBase->LastError(),
                                                 __FUNCTION__, __LINE__);
            ViETest::Log("Done\n");
        }

        // testing StartRecordOutgoingVideo and StopRecordOutgoingVideo
        {
            // connect the camera to the output.
            error = ptrViECapture->ConnectCaptureDevice(captureId,
                                                        videoChannel);
            numberOfErrors += ViETest::TestError(error == 0,
                                                 "ERROR:%d %s at line %d",
                                                 ptrViEBase->LastError(),
                                                 __FUNCTION__, __LINE__);

            ViETest::Log("Recording outgoing video (currently no audio) for %d "
                         "seconds", VIDEO_LENGTH);
            error = ptrViEFile->StartRecordOutgoingVideo(videoChannel,
                                                         outgoingVideo,
                                                         webrtc::NO_AUDIO,
                                                         audioCodec2,
                                                         videoCodec);
            numberOfErrors += ViETest::TestError(error == 0,
                                                 "ERROR:%d %s at line %d",
                                                 ptrViEBase->LastError(),
                                                 __FUNCTION__, __LINE__);

            AutoTestSleep(VIDEO_LENGTH);
            ViETest::Log("Stop recording outgoing video");
            error = ptrViEFile->StopRecordOutgoingVideo(videoChannel);
            numberOfErrors += ViETest::TestError(error == 0,
                                                 "ERROR:%d %s at line %d",
                                                 ptrViEBase->LastError(),
                                                 __FUNCTION__, __LINE__);
            ViETest::Log("Done\n");
        }

        // again testing GetFileInformation
        {
            error = ptrViEFile->GetFileInformation(incomingVideo, videoCodec,
                                                   audioCodec2);
            numberOfErrors += ViETest::TestError(error == 0,
                                                 "ERROR:%d %s at line %d",
                                                 ptrViEBase->LastError(),
                                                 __FUNCTION__, __LINE__);
            PrintAudioCodec(audioCodec2);
            PrintVideoCodec(videoCodec);
        }

        AutoTestSleep(TEST_SPACING);

        // GetCaptureDeviceSnapshot
        {
            ViETest::Log("Testing GetCaptureDeviceSnapshot(int, ViEPicture)");
            ViETest::Log("Taking a picture to use for displaying ViEPictures "
                         "for the rest of file test");
            ViETest::Log("Hold an object to the camera. Ready?...");
            AutoTestSleep(1000);
            ViETest::Log("3");
            AutoTestSleep(1000);
            ViETest::Log("...2");
            AutoTestSleep(1000);
            ViETest::Log("...1");
            AutoTestSleep(1000);
            ViETest::Log("...Taking picture!");
            error = ptrViEFile->GetCaptureDeviceSnapshot(captureId,
                                                         capturePicture);
            numberOfErrors += ViETest::TestError(error == 0,
                                                 "ERROR:%d %s at line %d",
                                                 ptrViEBase->LastError(),
                                                 __FUNCTION__, __LINE__);
            ViETest::Log("Remove paper. Picture has been taken");
            AutoTestSleep(TEST_SPACING);

            ViETest::Log("Done\n");
        }

        AutoTestSleep(TEST_SPACING);

        // GetRenderSnapshot
        {
            ViETest::Log("Testing GetRenderSnapshot(int, char*)");

            ViETest::Log("Taking snapshot of videoChannel %d", captureId);
            error = ptrViEFile->GetRenderSnapshot(captureId,
                                                  snapshotRenderFileName);
            ViETest::Log("Wrote image to file %s", snapshotRenderFileName);
            numberOfErrors += ViETest::TestError(error == 0,
                                                 "ERROR:%d %s at line %d",
                                                 ptrViEBase->LastError(),
                                                 __FUNCTION__, __LINE__);
            ViETest::Log("Done\n");
            AutoTestSleep(TEST_SPACING);
        }

        // GetRenderSnapshot
        {
            ViETest::Log("Testing GetRenderSnapshot(int, ViEPicture)");
            error = ptrViEFile->GetRenderSnapshot(captureId, renderPicture);
            numberOfErrors += ViETest::TestError(error == 0,
                                                 "ERROR:%d %s at line %d",
                                                 ptrViEBase->LastError(),
                                                 __FUNCTION__, __LINE__);
            ViETest::Log("Done\n");
        }

        AutoTestSleep(TEST_SPACING);

        // GetCaptureDeviceSnapshot
        {
            ViETest::Log("Testing GetCaptureDeviceSnapshot(int, char*)");
            ViETest::Log("Taking snapshot from capture device %d", captureId);
            error = ptrViEFile->GetCaptureDeviceSnapshot(
                captureId, snapshotCaptureDeviceFileName);
            ViETest::Log("Wrote image to file %s",
                         snapshotCaptureDeviceFileName);
            numberOfErrors += ViETest::TestError(error == 0,
                                                 "ERROR:%d %s at line %d",
                                                 ptrViEBase->LastError(),
                                                 __FUNCTION__, __LINE__);
            ViETest::Log("Done\n");
        }

        AutoTestSleep(TEST_SPACING);

        // Testing: SetCaptureDeviceImage
        {
            ViETest::Log("Testing SetCaptureDeviceImage(int, char*)");
            error = ptrViECapture->StopCapture(captureId);
            numberOfErrors += ViETest::TestError(error == 0,
                                                 "ERROR:%d %s at line %d",
                                                 ptrViEBase->LastError(),
                                                 __FUNCTION__, __LINE__);

            error = ptrViEFile->SetCaptureDeviceImage(captureId,
                                                      captureDeviceImage);
            numberOfErrors += ViETest::TestError(error == 0,
                                                 "ERROR:%d %s at line %d",
                                                 ptrViEBase->LastError(),
                                                 __FUNCTION__, __LINE__);

            ViETest::Log("you should see the capture device image now");
            AutoTestSleep(2 * RENDER_TIMEOUT);
            error = ptrViECapture->StartCapture(captureId);
            numberOfErrors += ViETest::TestError(error == 0,
                                                 "ERROR:%d %s at line %d",
                                                 ptrViEBase->LastError(),
                                                 __FUNCTION__, __LINE__);
            ViETest::Log("Done\n");
        }

        AutoTestSleep(TEST_SPACING);

        // Testing: SetCaptureDeviceImage
        {
            ViETest::Log("Testing SetCaptureDeviceImage(int, ViEPicture)");
            error = ptrViECapture->StopCapture(captureId);
            numberOfErrors += ViETest::TestError(error == 0,
                                                 "ERROR:%d %s at line %d",
                                                 ptrViEBase->LastError(),
                                                 __FUNCTION__, __LINE__);

            error
                = ptrViEFile->SetCaptureDeviceImage(captureId, capturePicture);
            numberOfErrors += ViETest::TestError(error == 0,
                                                 "ERROR:%d %s at line %d",
                                                 ptrViEBase->LastError(),
                                                 __FUNCTION__, __LINE__);

            ViETest::Log("you should see the capture device image now");
            AutoTestSleep(2 * RENDER_TIMEOUT);
            error = ptrViECapture->StartCapture(captureId);
            numberOfErrors += ViETest::TestError(error == 0,
                                                 "ERROR:%d %s at line %d",
                                                 ptrViEBase->LastError(),
                                                 __FUNCTION__, __LINE__);
            ViETest::Log("Done\n");
        }

        AutoTestSleep(TEST_SPACING);

        // testing SetRenderStartImage(videoChannel, renderStartImage);
        {
            ViETest::Log("Testing SetRenderStartImage(int, char*)");
            // set render image, then stop capture and stop render to display it
            ViETest::Log("Stoping renderer, setting start image, then "
                         "restarting");
            error = ptrViEFile->SetRenderStartImage(videoChannel,
                                                    renderStartImage);
            numberOfErrors += ViETest::TestError(error == 0,
                                                 "ERROR:%d %s at line %d",
                                                 ptrViEBase->LastError(),
                                                 __FUNCTION__, __LINE__);
            error = ptrViECapture->StopCapture(captureId);
            numberOfErrors += ViETest::TestError(error == 0,
                                                 "ERROR:%d %s at line %d",
                                                 ptrViEBase->LastError(),
                                                 __FUNCTION__, __LINE__);
            error = ptrViERender->StopRender(videoChannel);
            numberOfErrors += ViETest::TestError(error == 0,
                                                 "ERROR:%d %s at line %d",
                                                 ptrViEBase->LastError(),
                                                 __FUNCTION__, __LINE__);

            ViETest::Log("Render start image should be displayed.");
            AutoTestSleep(RENDER_TIMEOUT);

            // restarting capture and render
            error = ptrViECapture->StartCapture(captureId);
            numberOfErrors += ViETest::TestError(error == 0,
                                                 "ERROR:%d %s at line %d",
                                                 ptrViEBase->LastError(),
                                                 __FUNCTION__, __LINE__);

            error = ptrViERender->StartRender(videoChannel);
            numberOfErrors += ViETest::TestError(error == 0,
                                                 "ERROR:%d %s at line %d",
                                                 ptrViEBase->LastError(),
                                                 __FUNCTION__, __LINE__);
            ViETest::Log("Done\n");
        }

        AutoTestSleep(TEST_SPACING);

        // testing SetRenderStartImage(videoChannel, renderStartImage);
        {
            ViETest::Log("Testing SetRenderStartImage(int, ViEPicture)");
            // set render image, then stop capture and stop render to display it
            ViETest::Log("Stoping renderer, setting start image, then "
                         "restarting");
            error = ptrViEFile->SetRenderStartImage(videoChannel,
                                                    capturePicture);
            numberOfErrors += ViETest::TestError(error == 0,
                                                 "ERROR:%d %s at line %d",
                                                 ptrViEBase->LastError(),
                                                 __FUNCTION__, __LINE__);
            error = ptrViECapture->StopCapture(captureId);
            numberOfErrors += ViETest::TestError(error == 0,
                                                 "ERROR:%d %s at line %d",
                                                 ptrViEBase->LastError(),
                                                 __FUNCTION__, __LINE__);
            error = ptrViERender->StopRender(videoChannel);
            numberOfErrors += ViETest::TestError(error == 0,
                                                 "ERROR:%d %s at line %d",
                                                 ptrViEBase->LastError(),
                                                 __FUNCTION__, __LINE__);

            ViETest::Log("Render start image should be displayed.");
            AutoTestSleep(RENDER_TIMEOUT);

            // restarting capture and render
            error = ptrViECapture->StartCapture(captureId);
            numberOfErrors += ViETest::TestError(error == 0,
                                                 "ERROR:%d %s at line %d",
                                                 ptrViEBase->LastError(),
                                                 __FUNCTION__, __LINE__);
            error = ptrViERender->StartRender(videoChannel);
            numberOfErrors += ViETest::TestError(error == 0,
                                                 "ERROR:%d %s at line %d",
                                                 ptrViEBase->LastError(),
                                                 __FUNCTION__, __LINE__);
            ViETest::Log("Done\n");
        }

        AutoTestSleep(TEST_SPACING);

        // testing SetRenderTimeoutImage(videoChannel, renderTimeoutFile,
        // RENDER_TIMEOUT);
        {
            ViETest::Log("Testing SetRenderTimeoutImage(int, char*)");
            ViETest::Log("Stopping capture device to induce timeout of %d ms",
                         RENDER_TIMEOUT);
            error = ptrViEFile->SetRenderTimeoutImage(videoChannel,
                                                      renderTimeoutFile,
                                                      RENDER_TIMEOUT);
            numberOfErrors += ViETest::TestError(error == 0,
                                                 "ERROR:%d %s at line %d",
                                                 ptrViEBase->LastError(),
                                                 __FUNCTION__, __LINE__);

            // now stop sending frames to the remote renderer and wait for
            // timeout
            error = ptrViECapture->StopCapture(captureId);
            numberOfErrors += ViETest::TestError(error == 0,
                                                 "ERROR:%d %s at line %d",
                                                 ptrViEBase->LastError(),
                                                 __FUNCTION__, __LINE__);
            AutoTestSleep(RENDER_TIMEOUT);
            ViETest::Log("Timeout image should be displayed now for %d ms",
                         RENDER_TIMEOUT * 2);
            AutoTestSleep(RENDER_TIMEOUT * 2);

            // restart the capture device to undo the timeout
            error = ptrViECapture->StartCapture(captureId);
            numberOfErrors += ViETest::TestError(error == 0,
                                                 "ERROR:%d %s at line %d",
                                                 ptrViEBase->LastError(),
                                                 __FUNCTION__, __LINE__);
            ViETest::Log("Restarting capture device");
            AutoTestSleep(RENDER_TIMEOUT);
            ViETest::Log("Done\n");
        }

        AutoTestSleep(TEST_SPACING);

        // Need to create a ViEPicture object to pass into this function.
        // SetRenderTimeoutImage(videoChannel, renderTimeoutFile,
        // RENDER_TIMEOUT);
        {
            ViETest::Log("Testing SetRenderTimeoutImage(int, ViEPicture)");
            ViETest::Log("Stopping capture device to induce timeout of %d",
                         RENDER_TIMEOUT);
            error = ptrViEFile->SetRenderTimeoutImage(videoChannel,
                                                      capturePicture,
                                                      RENDER_TIMEOUT);
            numberOfErrors += ViETest::TestError(error == 0,
                                                 "ERROR:%d %s at line %d",
                                                 ptrViEBase->LastError(),
                                                 __FUNCTION__, __LINE__);

            // now stop sending frames to the remote renderer and wait for
            // timeout
            error = ptrViECapture->StopCapture(captureId);
            numberOfErrors += ViETest::TestError(error == 0,
                                                 "ERROR:%d %s at line %d",
                                                 ptrViEBase->LastError(),
                                                 __FUNCTION__, __LINE__);
            AutoTestSleep(RENDER_TIMEOUT);
            ViETest::Log("Timeout image should be displayed now for %d",
                         RENDER_TIMEOUT * 2);
            AutoTestSleep(RENDER_TIMEOUT * 2);

            // restart the capture device to undo the timeout
            error = ptrViECapture->StartCapture(captureId);
            numberOfErrors += ViETest::TestError(error == 0,
                                                 "ERROR:%d %s at line %d",
                                                 ptrViEBase->LastError(),
                                                 __FUNCTION__, __LINE__);
            ViETest::Log("Restarting capture device");
            ViETest::Log("Done\n");
        }

        // testing DeregisterObserver
        {
            ViETest::Log("Deregistering file observer");
            // Should fail since we don't observe this file.
            error = ptrViEFile->DeregisterObserver(fileId, fileObserver);
            numberOfErrors += ViETest::TestError(error == -1,
                                                 "ERROR:%d %s at line %d",
                                                 ptrViEBase->LastError(),
                                                 __FUNCTION__, __LINE__);
        }

        //***************************************************************
        //	Testing finished. Tear down Video Engine
        //***************************************************************


        error = ptrViEBase->StopReceive(videoChannel);
        numberOfErrors += ViETest::TestError(error == 0,
                                             "ERROR:%d %s at line %d",
                                             ptrViEBase->LastError(),
                                             __FUNCTION__, __LINE__);

        error = ptrViEBase->StopSend(videoChannel);
        numberOfErrors += ViETest::TestError(error == 0,
                                             "ERROR:%d %s at line %d",
                                             ptrViEBase->LastError(),
                                             __FUNCTION__, __LINE__);

        error = ptrViERender->StopRender(videoChannel);
        numberOfErrors += ViETest::TestError(error == 0,
                                             "ERROR:%d %s at line %d",
                                             ptrViEBase->LastError(),
                                             __FUNCTION__, __LINE__);

        error = ptrViERender->RemoveRenderer(captureId);
        numberOfErrors += ViETest::TestError(error == 0,
                                             "ERROR:%d %s at line %d",
                                             ptrViEBase->LastError(),
                                             __FUNCTION__, __LINE__);

        error = ptrViERender->RemoveRenderer(videoChannel);
        numberOfErrors += ViETest::TestError(error == 0,
                                             "ERROR:%d %s at line %d",
                                             ptrViEBase->LastError(),
                                             __FUNCTION__, __LINE__);
        
        error = ptrViECapture->DisconnectCaptureDevice(videoChannel);
        numberOfErrors += ViETest::TestError(error == 0,
                                             "ERROR:%d %s at line %d",
                                             ptrViEBase->LastError(),
                                             __FUNCTION__, __LINE__);

        error = ptrViEFile->FreePicture(capturePicture);
        numberOfErrors += ViETest::TestError(error == 0,
                                             "ERROR:%d %s at line %d",
                                             ptrViEBase->LastError(),
                                             __FUNCTION__, __LINE__);

        error = ptrViEFile->FreePicture(renderPicture);
        numberOfErrors += ViETest::TestError(error == 0,
                                             "ERROR:%d %s at line %d",
                                             ptrViEBase->LastError(),
                                             __FUNCTION__, __LINE__);

        error = ptrViEFile->FreePicture(renderTimeoutPicture);
        numberOfErrors += ViETest::TestError(error == 0,
                                             "ERROR:%d %s at line %d",
                                             ptrViEBase->LastError(),
                                             __FUNCTION__, __LINE__);

        error = ptrViEBase->DeleteChannel(videoChannel);
        numberOfErrors += ViETest::TestError(error == 0,
                                             "ERROR:%d %s at line %d",
                                             ptrViEBase->LastError(),
                                             __FUNCTION__, __LINE__);

        int remainingInterfaces = 0;

        remainingInterfaces = ptrViEFile->Release();
        numberOfErrors += ViETest::TestError(remainingInterfaces == 0,
                                             "ERROR:%d %s at line %d",
                                             ptrViEBase->LastError(),
                                             __FUNCTION__, __LINE__);

    }
    if (numberOfErrors > 0)
    {
        // Test failed
        ViETest::Log(" ");
        ViETest::Log(" ERROR ViEFile API Test FAILED!");
        ViETest::Log(" Number of errors: %d", numberOfErrors);
        ViETest::Log("========================================");
        ViETest::Log(" ");
        return numberOfErrors;
    }
#endif

    ViETest::Log(" ");
    ViETest::Log(" ViEFile Standard Test PASSED!");
    ViETest::Log("========================================");
    ViETest::Log(" ");

    return 0;
}

int ViEAutoTest::ViEFileExtendedTest()
{
   
    ViETest::Log(" ");
    ViETest::Log("========================================");
    ViETest::Log(" ViEFile Extended Test\n");

    ViETest::Log(" ");
    ViETest::Log(" ViEFile Extended Test PASSED!");
    ViETest::Log("========================================");
    ViETest::Log(" ");
    return 0;
}

int ViEAutoTest::ViEFileAPITest()
{

    ViETest::Log(" ");
    ViETest::Log("========================================");
    ViETest::Log(" ViEFile API Test- nothing tested. Only tested in Standard test.\n");

    //***************************************************************
    //	Begin create/initialize WebRTC Video Engine for testing
    //***************************************************************




    ViETest::Log(" ");
    ViETest::Log(" ViEFile API Test PASSED!");
    ViETest::Log("========================================");
    ViETest::Log(" ");
    return 0;
}
