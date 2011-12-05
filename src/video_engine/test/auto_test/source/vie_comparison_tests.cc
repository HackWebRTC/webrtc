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

bool ViEComparisonTests::TestCallSetup(
    const std::string& i420_video_file,
    int width,
    int height,
    ViEToFileRenderer* local_file_renderer,
    ViEToFileRenderer* remote_file_renderer) {

  TbInterfaces interfaces("TestCallSetup");

  int video_channel = -1;
  EXPECT_EQ(0, interfaces.base->CreateChannel(video_channel));

  ViEFakeCamera fake_camera(interfaces.capture);
  if (!fake_camera.StartCameraInNewThread(i420_video_file,
                                          width,
                                          height)) {
    // No point in continuing if we have no proper video source
    ADD_FAILURE() << "Could not open input video " << i420_video_file <<
        ": aborting test...";
    return false;
  }
  int capture_id = fake_camera.capture_id();

  // Apparently, we need to connect external capture devices, but we should
  // not start them since the external device is not a proper device.
  EXPECT_EQ(0, interfaces.capture->ConnectCaptureDevice(
      capture_id, video_channel));

  ConfigureRtpRtcp(interfaces.rtp_rtcp, video_channel);

  webrtc::ViERender *render_interface = interfaces.render;

  RenderToFile(render_interface, capture_id, local_file_renderer);
  RenderToFile(render_interface, video_channel, remote_file_renderer);

  // Run the test itself:
  const WebRtc_UWord8* device_name =
      reinterpret_cast<const WebRtc_UWord8*>("Fake Capture Device");

  ::TestI420CallSetup(interfaces.codec, interfaces.video_engine,
                      interfaces.base, interfaces.network, video_channel,
                      device_name);

  AutoTestSleep(KAutoTestSleepTimeMs);

  EXPECT_EQ(0, interfaces.base->StopReceive(video_channel));

  StopAndRemoveRenderers(interfaces.base, render_interface, video_channel,
                         capture_id);

  interfaces.capture->DisconnectCaptureDevice(video_channel);

  // Stop sending data, clean up the camera thread and release the capture
  // device. Note that this all happens after StopEverything, so this
  // tests that the system doesn't mind that the external capture device sends
  // data after rendering has been stopped.
  fake_camera.StopCamera();

  EXPECT_EQ(0, interfaces.base->DeleteChannel(video_channel));
  return true;
}

bool ViEComparisonTests::TestCodecs(
    const std::string& i420_video_file,
    int width,
    int height,
    ViEToFileRenderer* local_file_renderer,
    ViEToFileRenderer* remote_file_renderer) {

  TbInterfaces interfaces = TbInterfaces("TestCodecs");

  ViEFakeCamera fake_camera(interfaces.capture);
  if (!fake_camera.StartCameraInNewThread(i420_video_file, width, height)) {
    // No point in continuing if we have no proper video source
    ADD_FAILURE() << "Could not open input video " << i420_video_file <<
        ": aborting test...";
    return false;
  }

  int video_channel = -1;
  int capture_id = fake_camera.capture_id();

  EXPECT_EQ(0, interfaces.base->CreateChannel(video_channel));
  EXPECT_EQ(0, interfaces.capture->ConnectCaptureDevice(
      capture_id, video_channel));

  ConfigureRtpRtcp(interfaces.rtp_rtcp, video_channel);

  RenderToFile(interfaces.render, capture_id, local_file_renderer);
  RenderToFile(interfaces.render, video_channel, remote_file_renderer);

  // Force the codec resolution to what our input video is so we can make
  // comparisons later. Our comparison algorithms wouldn't like scaling.
  ::TestCodecs(interfaces, fake_camera.capture_id(), video_channel,
               width, height);

  fake_camera.StopCamera();
  return true;
}
