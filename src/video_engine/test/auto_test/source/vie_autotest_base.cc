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

#include "base_primitives.h"
#include "general_primitives.h"
#include "tb_interfaces.h"
#include "vie_autotest_defines.h"
#include "video_capture_factory.h"

class BaseObserver : public webrtc::ViEBaseObserver {
 public:
  BaseObserver()
      : cpu_load_(0) {}

  virtual void PerformanceAlarm(const unsigned int cpu_load) {
    cpu_load_ = cpu_load;
  }
  unsigned int cpu_load_;
};

int ViEAutoTest::ViEBaseStandardTest() {
  ViETest::Log(" ");
  ViETest::Log("========================================");
  ViETest::Log(" ViEBase Standard Test");

  // ***************************************************************
  // Begin create/initialize WebRTC Video Engine for testing
  // ***************************************************************

  int number_of_errors = 0;

  TbInterfaces interfaces("ViEBaseStandardTest", number_of_errors);

  // ***************************************************************
  // Engine ready. Set up the test case:
  // ***************************************************************
  int video_channel = -1;
  int error = interfaces.base->CreateChannel(video_channel);
    number_of_errors += ViETest::TestError(error == 0, "ERROR: %s at line %d",
                                           __FUNCTION__, __LINE__);

  webrtc::VideoCaptureModule* video_capture_module(NULL);
  const unsigned int kMaxDeviceNameLength = 128;
  WebRtc_UWord8 device_name[kMaxDeviceNameLength];
  memset(device_name, 0, kMaxDeviceNameLength);
  int capture_id;

  webrtc::ViEBase *base_interface = interfaces.base;
  webrtc::ViERender *render_interface = interfaces.render;
  webrtc::ViECapture *capture_interface = interfaces.capture;

  FindCaptureDeviceOnSystem(capture_interface,
                            device_name,
                            kMaxDeviceNameLength,
                            &capture_id,
                            &number_of_errors,
                            &video_capture_module);

  error = capture_interface->ConnectCaptureDevice(capture_id,
                                                  video_channel);
  number_of_errors += ViETest::TestError(error == 0, "ERROR: %s at line %d",
                                         __FUNCTION__, __LINE__);

  error = capture_interface->StartCapture(capture_id);
  number_of_errors += ViETest::TestError(error == 0, "ERROR: %s at line %d",
                                         __FUNCTION__, __LINE__);

  ConfigureRtpRtcp(interfaces.rtp_rtcp, &number_of_errors, video_channel);

  error = render_interface->RegisterVideoRenderModule(*_vrm1);
  number_of_errors += ViETest::TestError(error == 0, "ERROR: %s at line %d",
                                         __FUNCTION__, __LINE__);
  error = render_interface->RegisterVideoRenderModule(*_vrm2);
  number_of_errors += ViETest::TestError(error == 0, "ERROR: %s at line %d",
                                         __FUNCTION__, __LINE__);

  RenderInWindow(render_interface, &number_of_errors, capture_id,
                 _window1, 0);
  RenderInWindow(render_interface, &number_of_errors, video_channel,
                 _window2, 1);

  // ***************************************************************
  // Run the actual test:
  // ***************************************************************
  TestI420CallSetup(interfaces.codec, interfaces.video_engine,
                    base_interface, interfaces.network, &number_of_errors,
                    video_channel, device_name);

  // ***************************************************************
  // Testing finished. Tear down Video Engine
  // ***************************************************************
  error = capture_interface->StopCapture(capture_id);
  number_of_errors += ViETest::TestError(error == 0, "ERROR: %s at line %d",
                                         __FUNCTION__, __LINE__);
  error = base_interface->StopReceive(video_channel);
  number_of_errors += ViETest::TestError(error == 0, "ERROR: %s at line %d",
                                         __FUNCTION__, __LINE__);

  StopAndRemoveRenderers(base_interface, render_interface, &number_of_errors,
                video_channel, capture_id);

  error = render_interface->DeRegisterVideoRenderModule(*_vrm1);
  number_of_errors += ViETest::TestError(error == 0, "ERROR: %s at line %d",
                                       __FUNCTION__, __LINE__);
  error = render_interface->DeRegisterVideoRenderModule(*_vrm2);
  number_of_errors += ViETest::TestError(error == 0, "ERROR: %s at line %d",
                                       __FUNCTION__, __LINE__);

  error = capture_interface->ReleaseCaptureDevice(capture_id);
  number_of_errors += ViETest::TestError(error == 0, "ERROR: %s at line %d",
                                         __FUNCTION__, __LINE__);

  video_capture_module->Release();
  video_capture_module = NULL;

  error = base_interface->DeleteChannel(video_channel);
  number_of_errors += ViETest::TestError(error == 0, "ERROR: %s at line %d",
                                       __FUNCTION__, __LINE__);

  if (number_of_errors > 0) {
    // Test failed
    ViETest::Log(" ");
    ViETest::Log(" ERROR ViEBase Standard Test FAILED!");
    ViETest::Log(" Number of errors: %d", number_of_errors);
    ViETest::Log("========================================");
    ViETest::Log(" ");
    return number_of_errors;
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
  error = video_engine->SetTraceFile("/sdcard/WebRTC/ViEBaseAPI_trace.txt");
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
