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

#include "gtest/gtest.h"
#include "video_capture_factory.h"
#include "vie_autotest_defines.h"
#include "vie_autotest.h"
#include "vie_fake_camera.h"
#include "vie_file_capture_device.h"
#include "thread_wrapper.h"

class BaseObserver : public webrtc::ViEBaseObserver {
 public:
  BaseObserver()
      : cpu_load_(0) {}

  virtual void PerformanceAlarm(const unsigned int cpu_load) {
    cpu_load_ = cpu_load;
  }
  unsigned int cpu_load_;
};

webrtc::VideoEngine *InitializeVideoEngine(int & numberOfErrors) {
  ViETest::Log("Starting a loopback call...");
  webrtc::VideoEngine *ptrViE = webrtc::VideoEngine::Create();
  numberOfErrors += ViETest::TestError(ptrViE != NULL,
                                       "ERROR: %s at line %d", __FUNCTION__,
                                       __LINE__);

#ifdef WEBRTC_ANDROID
  int error = ptrViE->SetTraceFile("/sdcard/ViEBaseStandardTest_trace.txt");
  numberOfErrors += ViETest::TestError(error == 0,
                                       "ERROR: %s at line %d", __FUNCTION__,
                                       __LINE__);
#else
  int error = ptrViE->SetTraceFile("ViEBaseStandardTest_trace.txt");
  numberOfErrors += ViETest::TestError(error == 0, "ERROR: %s at line %d",
                                       __FUNCTION__, __LINE__);
#endif
  return ptrViE;
}

webrtc::ViEBase *InitializeViEBase(webrtc::VideoEngine * ptrViE,
                                   int & numberOfErrors) {
  webrtc::ViEBase *ptrViEBase = webrtc::ViEBase::GetInterface(ptrViE);
  numberOfErrors += ViETest::TestError(ptrViEBase != NULL,
                                       "ERROR: %s at line %d", __FUNCTION__,
                                       __LINE__);
  int error = ptrViEBase->Init();
  numberOfErrors += ViETest::TestError(error == 0, "ERROR: %s at line %d",
                                       __FUNCTION__, __LINE__);
  return ptrViEBase;
}

webrtc::ViECapture *InitializeChannel(webrtc::ViEBase * ptrViEBase,
                                      int & videoChannel,
                                      int & numberOfErrors,
                                      webrtc::VideoEngine * ptrViE) {
  int error = ptrViEBase->CreateChannel(videoChannel);
  numberOfErrors += ViETest::TestError(error == 0, "ERROR: %s at line %d",
                                       __FUNCTION__, __LINE__);

  webrtc::ViECapture *ptrViECapture = webrtc::ViECapture::GetInterface(ptrViE);
  numberOfErrors += ViETest::TestError(ptrViECapture != NULL,
                                       "ERROR: %s at line %d", __FUNCTION__,
                                       __LINE__);
  return ptrViECapture;
}

void ConnectCaptureDevice(webrtc::ViECapture * ptrViECapture,
                          int captureId,
                          int videoChannel,
                          int & numberOfErrors) {
  int error = ptrViECapture->ConnectCaptureDevice(captureId, videoChannel);
  numberOfErrors += ViETest::TestError(error == 0, "ERROR: %s at line %d",
                                       __FUNCTION__, __LINE__);
}

webrtc::ViERTP_RTCP *ConfigureRtpRtcp(webrtc::VideoEngine * ptrViE,
                                      int & numberOfErrors,
                                      int videoChannel) {
  webrtc::ViERTP_RTCP *ptrViERtpRtcp =
      webrtc::ViERTP_RTCP::GetInterface(ptrViE);
  numberOfErrors += ViETest::TestError(ptrViE != NULL,
                                       "ERROR: %s at line %d", __FUNCTION__,
                                       __LINE__);
  int error = ptrViERtpRtcp->
      SetRTCPStatus(videoChannel, webrtc::kRtcpCompound_RFC4585);
  numberOfErrors += ViETest::TestError(error == 0, "ERROR: %s at line %d",
                                       __FUNCTION__, __LINE__);
  error = ptrViERtpRtcp->
      SetKeyFrameRequestMethod(videoChannel,
                               webrtc::kViEKeyFrameRequestPliRtcp);
  numberOfErrors += ViETest::TestError(error == 0, "ERROR: %s at line %d",
                                       __FUNCTION__, __LINE__);
  error = ptrViERtpRtcp->SetTMMBRStatus(videoChannel, true);
  numberOfErrors += ViETest::TestError(error == 0, "ERROR: %s at line %d",
                                       __FUNCTION__, __LINE__);
  return ptrViERtpRtcp;
}

webrtc::ViERender *ViEAutoTest::RenderInBothWindows(
    webrtc::VideoEngine * ptrViE, int & numberOfErrors,
    int captureId, int videoChannel) {
  webrtc::ViERender *ptrViERender = webrtc::ViERender::GetInterface(ptrViE);
  numberOfErrors += ViETest::TestError(ptrViERender != NULL,
                                       "ERROR: %s at line %d", __FUNCTION__,
                                       __LINE__);
  int error = ptrViERender->RegisterVideoRenderModule(*_vrm1);
  numberOfErrors += ViETest::TestError(error == 0, "ERROR: %s at line %d",
                                       __FUNCTION__, __LINE__);
  error = ptrViERender->AddRenderer(captureId, _window1, 0, 0.0,
                                    0.0, 1.0, 1.0);
  numberOfErrors += ViETest::TestError(error == 0, "ERROR: %s at line %d",
                                       __FUNCTION__, __LINE__);
  error = ptrViERender->StartRender(captureId);
  numberOfErrors += ViETest::TestError(error == 0, "ERROR: %s at line %d",
                                       __FUNCTION__, __LINE__);
  error = ptrViERender->RegisterVideoRenderModule(*_vrm2);
  numberOfErrors += ViETest::TestError(error == 0, "ERROR: %s at line %d",
                                       __FUNCTION__, __LINE__);
  error = ptrViERender->AddRenderer(videoChannel, _window2, 1, 0.0,
                                    0.0, 1.0, 1.0);
  numberOfErrors += ViETest::TestError(error == 0, "ERROR: %s at line %d",
                                       __FUNCTION__, __LINE__);
  error = ptrViERender->StartRender(videoChannel);
  numberOfErrors += ViETest::TestError(error == 0, "ERROR: %s at line %d",
                                       __FUNCTION__, __LINE__);
  return ptrViERender;
}

webrtc::ViENetwork *TestCallSetup(webrtc::ViECodec * ptrViECodec,
                                  int & numberOfErrors,
                                  int videoChannel,
                                  webrtc::VideoEngine * ptrViE,
                                  webrtc::ViEBase * ptrViEBase,
                                  const WebRtc_UWord8 *deviceName) {
  int error;
  webrtc::VideoCodec videoCodec;
  memset(&videoCodec, 0, sizeof(webrtc::VideoCodec));
  for (int idx = 0; idx < ptrViECodec->NumberOfCodecs(); idx++) {
    error = ptrViECodec->GetCodec(idx, videoCodec);
    numberOfErrors += ViETest::TestError(error == 0,
                                         "ERROR: %s at line %d",
                                         __FUNCTION__, __LINE__);

    // try to keep the test frame size small when I420
    if (videoCodec.codecType == webrtc::kVideoCodecI420) {
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
  webrtc::ViENetwork *ptrViENetwork =
      webrtc::ViENetwork::GetInterface(ptrViE);
  numberOfErrors += ViETest::TestError(ptrViENetwork != NULL,
                                       "ERROR: %s at line %d", __FUNCTION__,
                                       __LINE__);
  char version[1024] = "";
  error = ptrViEBase->GetVersion(version);
  ViETest::Log("\nUsing WebRTC Video Engine version: %s", version);
  numberOfErrors += ViETest::TestError(error == 0, "ERROR: %s at line %d",
                                       __FUNCTION__, __LINE__);
  const char *ipAddress = "127.0.0.1";
  WebRtc_UWord16 rtpPortListen = 6000;
  WebRtc_UWord16 rtpPortSend = 6000;
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

  // Call started
  ViETest::Log("Call started");
  ViETest::Log("You should see a local preview from camera %s"
      " in window 1 and the remote video in window 2.", deviceName);
  return ptrViENetwork;
}

void StopEverything(webrtc::ViEBase * ptrViEBase,
                    int videoChannel,
                    int & numberOfErrors,
                    webrtc::ViERender * ptrViERender,
                    int captureId,
                    webrtc::ViECapture * ptrViECapture,
                    webrtc::VideoRender* vrm1,
                    webrtc::VideoRender* vrm2) {
  int error = ptrViEBase->StopReceive(videoChannel);
  numberOfErrors += ViETest::TestError(error == 0, "ERROR: %s at line %d",
                                       __FUNCTION__, __LINE__);
  error = ptrViERender->StopRender(videoChannel);
  numberOfErrors += ViETest::TestError(error == 0, "ERROR: %s at line %d",
                                       __FUNCTION__, __LINE__);
  error = ptrViERender->RemoveRenderer(videoChannel);
  numberOfErrors += ViETest::TestError(error == 0, "ERROR: %s at line %d",
                                       __FUNCTION__, __LINE__);
  error = ptrViERender->DeRegisterVideoRenderModule(*vrm2);
  numberOfErrors += ViETest::TestError(error == 0, "ERROR: %s at line %d",
                                       __FUNCTION__, __LINE__);
  error = ptrViEBase->StopSend(videoChannel);
  numberOfErrors += ViETest::TestError(error == 0, "ERROR: %s at line %d",
                                       __FUNCTION__, __LINE__);
  error = ptrViERender->RemoveRenderer(captureId);
  numberOfErrors += ViETest::TestError(error == 0, "ERROR: %s at line %d",
                                       __FUNCTION__, __LINE__);
  error = ptrViERender->DeRegisterVideoRenderModule(*vrm1);
  numberOfErrors += ViETest::TestError(error == 0, "ERROR: %s at line %d",
                                       __FUNCTION__, __LINE__);
  error = ptrViECapture->DisconnectCaptureDevice(videoChannel);
  numberOfErrors += ViETest::TestError(error == 0, "ERROR: %s at line %d",
                                       __FUNCTION__, __LINE__);
}

void ReleaseEverything(webrtc::ViECapture *ptrViECapture,
                       int& numberOfErrors,
                       webrtc::ViEBase *ptrViEBase,
                       int videoChannel,
                       webrtc::ViECodec *ptrViECodec,
                       webrtc::ViERTP_RTCP *ptrViERtpRtcp,
                       webrtc::ViERender *ptrViERender,
                       webrtc::ViENetwork *ptrViENetwork,
                       webrtc::VideoEngine *ptrViE) {
  int remainingInterfaces = ptrViECapture->Release();
  numberOfErrors += ViETest::TestError(remainingInterfaces == 0,
                                       "ERROR: %s at line %d", __FUNCTION__,
                                       __LINE__);
  int error2 = ptrViEBase->DeleteChannel(videoChannel);
  numberOfErrors += ViETest::TestError(error2 == 0, "ERROR: %s at line %d",
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
  bool deleted = webrtc::VideoEngine::Delete(ptrViE);
  numberOfErrors += ViETest::TestError(deleted == true,
                                       "ERROR: %s at line %d", __FUNCTION__,
                                       __LINE__);
}

int ViEAutoTest::ViEBaseStandardTest() {
  ViETest::Log(" ");
  ViETest::Log("========================================");
  ViETest::Log(" ViEBase Standard Test");

  // ***************************************************************
  // Begin create/initialize WebRTC Video Engine for testing
  // ***************************************************************

  int numberOfErrors = 0;

  webrtc::VideoEngine* ptrViE = InitializeVideoEngine(numberOfErrors);
  webrtc::ViEBase *ptrViEBase = InitializeViEBase(ptrViE, numberOfErrors);

  // ***************************************************************
  // Engine ready. Set up the test case:
  // ***************************************************************
  int videoChannel = -1;
  webrtc::ViECapture *ptrViECapture =
      InitializeChannel(ptrViEBase, videoChannel, numberOfErrors, ptrViE);

  webrtc::VideoCaptureModule* vcpm(NULL);
  const unsigned int kMaxDeviceNameLength = 128;
  WebRtc_UWord8 deviceName[kMaxDeviceNameLength];
  memset(deviceName, 0, kMaxDeviceNameLength);
  int captureId;

  FindCaptureDeviceOnSystem(ptrViECapture,
                            deviceName,
                            kMaxDeviceNameLength,
                            &captureId,
                            &numberOfErrors,
                            &vcpm);

  ConnectCaptureDevice(ptrViECapture, captureId, videoChannel, numberOfErrors);
  int error = ptrViECapture->StartCapture(captureId);
  numberOfErrors += ViETest::TestError(error == 0, "ERROR: %s at line %d",
                                       __FUNCTION__, __LINE__);

  webrtc::ViERTP_RTCP *ptrViERtpRtcp =
      ConfigureRtpRtcp(ptrViE, numberOfErrors, videoChannel);
  webrtc::ViERender *ptrViERender =
      RenderInBothWindows(ptrViE, numberOfErrors, captureId, videoChannel);

  webrtc::ViECodec *ptrViECodec = webrtc::ViECodec::GetInterface(ptrViE);
  numberOfErrors += ViETest::TestError(ptrViECodec != NULL,
                                       "ERROR: %s at line %d", __FUNCTION__,
                                       __LINE__);

  // ***************************************************************
  // Set up the call and wait.
  // ***************************************************************

  webrtc::ViENetwork *ptrViENetwork =
      TestCallSetup(ptrViECodec, numberOfErrors, videoChannel,
                    ptrViE, ptrViEBase, deviceName);

  AutoTestSleep(KAutoTestSleepTimeMs);

  // ***************************************************************
  // Testing finished. Tear down Video Engine
  // ***************************************************************

  error = ptrViECapture->StopCapture(captureId);
  numberOfErrors += ViETest::TestError(error == 0, "ERROR: %s at line %d",
                                       __FUNCTION__, __LINE__);

  StopEverything(ptrViEBase, videoChannel, numberOfErrors, ptrViERender,
                 captureId, ptrViECapture, _vrm1, _vrm2);

  error = ptrViECapture->ReleaseCaptureDevice(captureId);
  numberOfErrors += ViETest::TestError(error == 0, "ERROR: %s at line %d",
                                         __FUNCTION__, __LINE__);

  vcpm->Release();
  vcpm = NULL;

  ReleaseEverything(ptrViECapture, numberOfErrors, ptrViEBase, videoChannel,
                    ptrViECodec, ptrViERtpRtcp, ptrViERender, ptrViENetwork,
                    ptrViE);

  if (numberOfErrors > 0) {
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

int ViEAutoTest::ViEBaseExtendedTest() {
  // Start with standard test
  ViEBaseAPITest();
  ViEBaseStandardTest();

  ViETest::Log(" ");
  ViETest::Log("========================================");
  ViETest::Log(" ViEBase Extended Test");

    // ***************************************************************
    // Test BaseObserver
    // ***************************************************************
    // TODO(mflodman) Add test for base observer. Cpu load must be over 75%.
//    BaseObserver base_observer;
//    error = ptrViEBase->RegisterObserver(base_observer);
//    numberOfErrors += ViETest::TestError(error == 0, "ERROR: %s at line %d",
//                                         __FUNCTION__, __LINE__);
//
//    AutoTestSleep(KAutoTestSleepTimeMs);
//
//    error = ptrViEBase->DeregisterObserver();
//    numberOfErrors += ViETest::TestError(error == 0, "ERROR: %s at line %d",
//                                         __FUNCTION__, __LINE__);
//    numberOfErrors += ViETest::TestError(base_observer.cpu_load_ > 0,
//                                         "ERROR: %s at line %d",
//                                         __FUNCTION__, __LINE__);

    ViETest::Log(" ");
    ViETest::Log(" ViEBase Extended Test PASSED!");
    ViETest::Log("========================================");
    ViETest::Log(" ");

  return 0;
}

int ViEAutoTest::ViEBaseAPITest() {
  ViETest::Log(" ");
  ViETest::Log("========================================");
  ViETest::Log(" ViEBase API Test");

  // ***************************************************************
  // Begin create/initialize WebRTC Video Engine for testing
  // ***************************************************************
  int error = 0;
  int numberOfErrors = 0;

  webrtc::VideoEngine* ptrViE = NULL;
  webrtc::ViEBase* ptrViEBase = NULL;

  // Get the ViEBase API
  ptrViEBase = webrtc::ViEBase::GetInterface(ptrViE);
  numberOfErrors += ViETest::TestError(ptrViEBase == NULL);

  ptrViE = webrtc::VideoEngine::Create();
  numberOfErrors += ViETest::TestError(ptrViE != NULL, "VideoEngine::Create");

#ifdef WEBRTC_ANDROID
  error = ptrViE->SetTraceFile("/sdcard/WebRTC/ViEBaseAPI_trace.txt");
  numberOfErrors += ViETest::TestError(error == 0, "SetTraceFile error");
#else
  error = ptrViE->SetTraceFile("ViEBaseAPI_trace.txt");
  numberOfErrors += ViETest::TestError(error == 0, "SetTraceFile error");
#endif

  ptrViEBase = webrtc::ViEBase::GetInterface(ptrViE);
  numberOfErrors += ViETest::TestError(ptrViEBase != NULL);

  // ***************************************************************
  // Engine ready. Begin testing class
  // ***************************************************************

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
  webrtc::VoiceEngine* ptrVoE = NULL;
  webrtc::VoEBase* ptrVoEBase = NULL;
  int audioChannel = -1;

  ptrVoE = webrtc::VoiceEngine::Create();
  numberOfErrors += ViETest::TestError(ptrVoE != NULL,
                                       "ERROR: %s at line %d", __FUNCTION__,
                                       __LINE__);

  ptrVoEBase = webrtc::VoEBase::GetInterface(ptrVoE);
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

  // ***************************************************************
  // Testing finished. Tear down Video Engine
  // ***************************************************************
  error = ptrViEBase->DisconnectAudioChannel(videoChannel + 5);
  numberOfErrors += ViETest::TestError(error != 0, "ERROR: %s at line %d",
                                       __FUNCTION__, __LINE__);

  error = ptrViEBase->DisconnectAudioChannel(videoChannel);
  numberOfErrors += ViETest::TestError(error == 0, "ERROR: %s at line %d",
                                       __FUNCTION__, __LINE__);

  error = ptrViEBase->SetVoiceEngine(NULL);
  numberOfErrors += ViETest::TestError(error == 0, "ERROR: %s at line %d",
                                       __FUNCTION__, __LINE__);

  webrtc::ViEBase* ptrViEBase2 = webrtc::ViEBase::GetInterface(ptrViE);
  numberOfErrors += ViETest::TestError(ptrViEBase2 != NULL,
                                       "ERROR: %s at line %d", __FUNCTION__,
                                       __LINE__);

  int remainingInterfaces = ptrViEBase->Release();
  numberOfErrors += ViETest::TestError(remainingInterfaces == 1,
                                       "ERROR: %s at line %d", __FUNCTION__,
                                       __LINE__);

  bool vieDeleted = webrtc::VideoEngine::Delete(ptrViE);
  numberOfErrors += ViETest::TestError(vieDeleted == false,
                                       "ERROR: %s at line %d", __FUNCTION__,
                                       __LINE__);

  remainingInterfaces = ptrViEBase->Release();
  numberOfErrors += ViETest::TestError(remainingInterfaces == 0,
                                       "ERROR: %s at line %d", __FUNCTION__,
                                       __LINE__);

  vieDeleted = webrtc::VideoEngine::Delete(ptrViE);
  numberOfErrors += ViETest::TestError(vieDeleted == true,
                                       "ERROR: %s at line %d", __FUNCTION__,
                                       __LINE__);

  if (numberOfErrors > 0) {
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

void ViEAutoTest::ViEAutomatedBaseStandardTest(
    const std::string& pathToTestI420Video, int width, int height) {
  int ignoredNumberOfErrors;

  // Initialize the test:
  webrtc::VideoEngine* ptrViE =
      InitializeVideoEngine(ignoredNumberOfErrors);
  webrtc::ViEBase *ptrViEBase =
      InitializeViEBase(ptrViE, ignoredNumberOfErrors);

  int videoChannel = -1;
  webrtc::ViECapture *ptrViECapture =
      InitializeChannel(ptrViEBase, videoChannel,
                        ignoredNumberOfErrors, ptrViE);

  ViEFakeCamera fakeCamera(ptrViECapture);
  if (!fakeCamera.StartCameraInNewThread(pathToTestI420Video, width, height)) {
    // No point in continuing if we have no proper video source
    ViETest::TestError(false, "ERROR: %s at line %d: "
                       "Could not open input video %s: aborting test...",
                       __FUNCTION__, __LINE__, pathToTestI420Video.c_str());
    return;
  }
  int captureId = fakeCamera.capture_id();

  // Apparently, we need to connect external capture devices, but we should
  // not start them since the external device is not a proper device.
  ConnectCaptureDevice(ptrViECapture, captureId, videoChannel,
                       ignoredNumberOfErrors);

  webrtc::ViERTP_RTCP *ptrViERtpRtcp =
      ConfigureRtpRtcp(ptrViE, ignoredNumberOfErrors, videoChannel);
  webrtc::ViERender *ptrViERender =
      RenderInBothWindows(ptrViE, ignoredNumberOfErrors,
                          captureId, videoChannel);

  webrtc::ViECodec *ptrViECodec = webrtc::ViECodec::GetInterface(ptrViE);
  ignoredNumberOfErrors +=
      ViETest::TestError(ptrViECodec != NULL,
                         "ERROR: %s at line %d", __FUNCTION__,
                         __LINE__);

  // Run the test itself:
  const WebRtc_UWord8* deviceName =
      reinterpret_cast<const WebRtc_UWord8*>("Fake Capture Device");
  webrtc::ViENetwork *ptrViENetwork =
      TestCallSetup(ptrViECodec, ignoredNumberOfErrors, videoChannel,
                    ptrViE, ptrViEBase, deviceName);

  AutoTestSleep(KAutoTestSleepTimeMs);

  StopEverything(ptrViEBase, videoChannel, ignoredNumberOfErrors, ptrViERender,
                 captureId, ptrViECapture, _vrm1, _vrm2);

  // Stop sending data, clean up the camera thread and release the capture
  // device. Note that this all happens after StopEverything, so this is
  // tests that the system doesn't mind that the external capture device sends
  // data after rendering has been stopped.
  fakeCamera.StopCamera();

  ReleaseEverything(ptrViECapture, ignoredNumberOfErrors, ptrViEBase,
                    videoChannel, ptrViECodec, ptrViERtpRtcp, ptrViERender,
                    ptrViENetwork, ptrViE);
}

void ViEAutoTest::FindCaptureDeviceOnSystem(
    webrtc::ViECapture* capture,
    unsigned char* device_name,
    unsigned int device_name_length,
    int* device_id,
    int* number_of_errors,
    webrtc::VideoCaptureModule** device_video) {

  bool capture_device_set = false;
  webrtc::VideoCaptureModule::DeviceInfo *dev_info =
      webrtc::VideoCaptureFactory::CreateDeviceInfo(0);

  const unsigned int kMaxUniqueIdLength = 256;
  WebRtc_UWord8 unique_id[kMaxUniqueIdLength];
  memset(unique_id, 0, kMaxUniqueIdLength);

  for (unsigned int i = 0; i < dev_info->NumberOfDevices(); i++) {
    int error = dev_info->GetDeviceName(i, device_name, device_name_length,
                                        unique_id, kMaxUniqueIdLength);
    *number_of_errors += ViETest::TestError(
        error == 0, "ERROR: %s at line %d", __FUNCTION__, __LINE__);
    *device_video =
        webrtc::VideoCaptureFactory::Create(4571, unique_id);

    *number_of_errors += ViETest::TestError(
        *device_video != NULL, "ERROR: %s at line %d", __FUNCTION__, __LINE__);
    (*device_video)->AddRef();

    error = capture->AllocateCaptureDevice(**device_video, *device_id);
    if (error == 0) {
      ViETest::Log("Using capture device: %s, captureId: %d.",
                   device_name, *device_id);
      capture_device_set = true;
      break;
    } else {
      (*device_video)->Release();
      (*device_video) = NULL;
    }
  }
  delete dev_info;
  *number_of_errors += ViETest::TestError(
      capture_device_set, "ERROR: %s at line %d - Could not set capture device",
      __FUNCTION__, __LINE__);
}
