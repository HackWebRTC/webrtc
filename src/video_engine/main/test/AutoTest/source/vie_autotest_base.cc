/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "engine_configurations.h"
#include "gtest/gtest.h"
#include "video_capture_factory.h"
#include "vie_autotest_defines.h"
#include "vie_autotest.h"
#include "vie_fake_camera.h"
#include "vie_to_file_renderer.h"

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

void ViEAutoTest::RenderInWindow(webrtc::ViERender* video_render_interface,
                                 int* numberOfErrors,
                                 int frame_provider_id,
                                 void* os_window,
                                 float z_index) {
  int error = video_render_interface->AddRenderer(frame_provider_id, os_window,
                                                  z_index, 0.0, 0.0, 1.0, 1.0);
  *numberOfErrors += ViETest::TestError(error == 0, "ERROR: %s at line %d",
                                        __FUNCTION__, __LINE__);

  error = video_render_interface->StartRender(frame_provider_id);
  numberOfErrors += ViETest::TestError(error == 0, "ERROR: %s at line %d",
                                       __FUNCTION__, __LINE__);
}

// Tests a I420-to-I420 call. This test exercises the most basic WebRTC ViE
// functionality by setting up the codec interface to recognize the most common
// codecs, and the initiating a I420 call.
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

  AutoTestSleep(KAutoTestSleepTimeMs);

  // Done
  error = ptrViEBase->StopSend(videoChannel);
  numberOfErrors += ViETest::TestError(error == 0, "ERROR: %s at line %d",
                                       __FUNCTION__, __LINE__);

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

  webrtc::ViERender *ptrViERender = webrtc::ViERender::GetInterface(ptrViE);
  ViETest::TestError(ptrViERender != NULL,
                     "ERROR: %s at line %d", __FUNCTION__,
                     __LINE__);

  error = ptrViERender->RegisterVideoRenderModule(*_vrm1);
  numberOfErrors += ViETest::TestError(error == 0, "ERROR: %s at line %d",
                                       __FUNCTION__, __LINE__);
  error = ptrViERender->RegisterVideoRenderModule(*_vrm2);
  numberOfErrors += ViETest::TestError(error == 0, "ERROR: %s at line %d",
                                       __FUNCTION__, __LINE__);

  RenderInWindow(ptrViERender, &numberOfErrors, captureId, _window1, 0);
  RenderInWindow(ptrViERender, &numberOfErrors, videoChannel, _window2, 1);

  webrtc::ViECodec *ptrViECodec = webrtc::ViECodec::GetInterface(ptrViE);
  numberOfErrors += ViETest::TestError(ptrViECodec != NULL,
                                       "ERROR: %s at line %d", __FUNCTION__,
                                       __LINE__);

  // ***************************************************************
  // Run the actual test:
  // ***************************************************************

  webrtc::ViENetwork *ptrViENetwork =
        TestCallSetup(ptrViECodec, numberOfErrors, videoChannel,
                      ptrViE, ptrViEBase, deviceName);

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

void ViEAutoTest::RenderToFile(webrtc::ViERender* renderer_interface,
                               int render_id,
                               ViEToFileRenderer *to_file_renderer)
{
    int result = renderer_interface->AddRenderer(render_id,
                                                 webrtc::kVideoI420,
                                                 to_file_renderer);
    ViETest::TestError(result == 0, "ERROR: %s at line %d",
                     __FUNCTION__, __LINE__);
    result = renderer_interface->StartRender(render_id);
    ViETest::TestError(result == 0, "ERROR: %s at line %d",
                     __FUNCTION__, __LINE__);
}

void ViEAutoTest::ViEAutomatedBaseStandardTest(
    const std::string& i420_test_video_path,
    int width,
    int height,
    ViEToFileRenderer* local_file_renderer,
    ViEToFileRenderer* remote_file_renderer) {
  int ignored;

  // Initialize the test:
  webrtc::VideoEngine* video_engine =
      InitializeVideoEngine(ignored);
  webrtc::ViEBase *base_interface =
      InitializeViEBase(video_engine, ignored);

  int video_channel = -1;
  webrtc::ViECapture *capture_interface =
      InitializeChannel(base_interface, video_channel, ignored, video_engine);

  ViEFakeCamera fake_camera(capture_interface);
  if (!fake_camera.StartCameraInNewThread(i420_test_video_path,
                                          width,
                                          height)) {
    // No point in continuing if we have no proper video source
    ViETest::TestError(false, "ERROR: %s at line %d: "
                       "Could not open input video %s: aborting test...",
                       __FUNCTION__, __LINE__, i420_test_video_path.c_str());
    return;
  }
  int capture_id = fake_camera.capture_id();

  // Apparently, we need to connect external capture devices, but we should
  // not start them since the external device is not a proper device.
  ConnectCaptureDevice(capture_interface, capture_id, video_channel,
                       ignored);

  webrtc::ViERTP_RTCP *rtcp_interface =
      ConfigureRtpRtcp(video_engine, ignored, video_channel);

  webrtc::ViERender *render_interface =
      webrtc::ViERender::GetInterface(video_engine);
  ViETest::TestError(render_interface != NULL,
                     "ERROR: %s at line %d", __FUNCTION__,
                     __LINE__);

  render_interface->RegisterVideoRenderModule(*_vrm1);
  render_interface->RegisterVideoRenderModule(*_vrm2);
  RenderToFile(render_interface, capture_id, local_file_renderer);
  RenderToFile(render_interface, video_channel, remote_file_renderer);

  webrtc::ViECodec *codec_interface =
      webrtc::ViECodec::GetInterface(video_engine);
  ViETest::TestError(codec_interface != NULL,
                     "ERROR: %s at line %d", __FUNCTION__,
                     __LINE__);

  // Run the test itself:
  const WebRtc_UWord8* device_name =
      reinterpret_cast<const WebRtc_UWord8*>("Fake Capture Device");
  webrtc::ViENetwork *network_interface =
      TestCallSetup(codec_interface, ignored, video_channel,
                    video_engine, base_interface, device_name);

  AutoTestSleep(KAutoTestSleepTimeMs);

  StopEverything(base_interface, video_channel, ignored, render_interface,
                 capture_id, capture_interface, _vrm1, _vrm2);

  // Stop sending data, clean up the camera thread and release the capture
  // device. Note that this all happens after StopEverything, so this
  // tests that the system doesn't mind that the external capture device sends
  // data after rendering has been stopped.
  fake_camera.StopCamera();

  ReleaseEverything(capture_interface, ignored, base_interface,
                    video_channel, codec_interface, rtcp_interface,
                    render_interface, network_interface, video_engine);
}
