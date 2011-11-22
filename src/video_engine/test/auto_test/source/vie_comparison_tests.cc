/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "vie_comparison_tests.h"

#include "base_primitives.h"
#include "codec_primitives.h"
#include "general_primitives.h"
#include "tb_interfaces.h"
#include "vie_autotest_defines.h"
#include "vie_fake_camera.h"
#include "vie_to_file_renderer.h"

ViEComparisonTests::ViEComparisonTests() {
  ViETest::Init(ViETest::kUseGTestExpectsForTestErrors);
}

ViEComparisonTests::~ViEComparisonTests() {
  ViETest::Terminate();
}

void ViEComparisonTests::TestCallSetup(
    const std::string& i420_test_video_path,
    int width,
    int height,
    ViEToFileRenderer* local_file_renderer,
    ViEToFileRenderer* remote_file_renderer) {
  int ignored;

  TbInterfaces interfaces("TestCallSetup", ignored);

  int video_channel = -1;
  int error = interfaces.base->CreateChannel(video_channel);
  ViETest::TestError(error == 0, "ERROR: %s at line %d",
                     __FUNCTION__, __LINE__);

  ViEFakeCamera fake_camera(interfaces.capture);
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
  error = interfaces.capture->ConnectCaptureDevice(capture_id,
                                                   video_channel);
  ViETest::TestError(error == 0, "ERROR: %s at line %d",
                     __FUNCTION__, __LINE__);

  ConfigureRtpRtcp(interfaces.rtp_rtcp, &ignored, video_channel);

  webrtc::ViERender *render_interface = interfaces.render;

  RenderToFile(render_interface, capture_id, local_file_renderer);
  RenderToFile(render_interface, video_channel, remote_file_renderer);

  // Run the test itself:
  const WebRtc_UWord8* device_name =
      reinterpret_cast<const WebRtc_UWord8*>("Fake Capture Device");

  ::TestI420CallSetup(interfaces.codec, interfaces.video_engine,
                      interfaces.base, interfaces.network,
                      &ignored, video_channel, device_name);

  AutoTestSleep(KAutoTestSleepTimeMs);

  error = interfaces.base->StopReceive(video_channel);
  ViETest::TestError(error == 0, "ERROR: %s at line %d",
                     __FUNCTION__, __LINE__);

  StopAndRemoveRenderers(interfaces.base, render_interface, &ignored,
                video_channel, capture_id);

  interfaces.capture->DisconnectCaptureDevice(video_channel);

  // Stop sending data, clean up the camera thread and release the capture
  // device. Note that this all happens after StopEverything, so this
  // tests that the system doesn't mind that the external capture device sends
  // data after rendering has been stopped.
  fake_camera.StopCamera();

  error = interfaces.base->DeleteChannel(video_channel);
  ViETest::TestError(error == 0, "ERROR: %s at line %d",
                     __FUNCTION__, __LINE__);
}

void ViEComparisonTests::TestCodecs(
    const std::string& i420_video_file,
    int width,
    int height,
    ViEToFileRenderer* local_file_renderer,
    ViEToFileRenderer* remote_file_renderer) {
  int ignored = 0;

  TbInterfaces interfaces = TbInterfaces("TestCodecs", ignored);

  ViEFakeCamera fake_camera(interfaces.capture);
  if (!fake_camera.StartCameraInNewThread(i420_video_file, width, height)) {
    // No point in continuing if we have no proper video source
    ViETest::TestError(false, "ERROR: %s at line %d: "
                       "Could not open input video %s: aborting test...",
                       __FUNCTION__, __LINE__, i420_video_file.c_str());
    return;
  }

  int video_channel = -1;
  int capture_id = fake_camera.capture_id();

  int error = interfaces.base->CreateChannel(video_channel);
  ViETest::TestError(error == 0, "ERROR: %s at line %d",
                     __FUNCTION__, __LINE__);
  error = interfaces.capture->ConnectCaptureDevice(capture_id, video_channel);
  ViETest::TestError(error == 0, "ERROR: %s at line %d",
                     __FUNCTION__, __LINE__);

  ConfigureRtpRtcp(interfaces.rtp_rtcp, &ignored, video_channel);

  RenderToFile(interfaces.render, capture_id, local_file_renderer);
  RenderToFile(interfaces.render, video_channel, remote_file_renderer);

  // Force the codec resolution to what our input video is so we can make
  // comparisons later. Our comparison algorithms wouldn't like scaling.
  ::TestCodecs(interfaces, ignored, fake_camera.capture_id(), video_channel,
               width, height);

  fake_camera.StopCamera();
}
