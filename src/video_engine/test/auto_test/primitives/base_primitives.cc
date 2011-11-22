/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "base_primitives.h"

#include "vie_autotest.h"
#include "vie_autotest_defines.h"
#include "video_capture_factory.h"

void TestI420CallSetup(webrtc::ViECodec* codec_interface,
                       webrtc::VideoEngine* video_engine,
                       webrtc::ViEBase* base_interface,
                       webrtc::ViENetwork* network_interface,
                       int* number_of_errors,
                       int video_channel,
                       const unsigned char *device_name) {
  int error;
  webrtc::VideoCodec video_codec;
  memset(&video_codec, 0, sizeof(webrtc::VideoCodec));

  // Set up the codec interface with all known receive codecs and with
  // I420 as the send codec.
  for (int i = 0; i < codec_interface->NumberOfCodecs(); i++) {
    error = codec_interface->GetCodec(i, video_codec);
    *number_of_errors += ViETest::TestError(error == 0,
                                            "ERROR: %s at line %d",
                                            __FUNCTION__, __LINE__);

    // Try to keep the test frame size small when I420.
    if (video_codec.codecType == webrtc::kVideoCodecI420) {
      video_codec.width = 176;
      video_codec.height = 144;
      error = codec_interface->SetSendCodec(video_channel, video_codec);
      *number_of_errors += ViETest::TestError(error == 0,
                                              "ERROR: %s at line %d",
                                              __FUNCTION__, __LINE__);
    }

    error = codec_interface->SetReceiveCodec(video_channel, video_codec);
    *number_of_errors += ViETest::TestError(error == 0,
                                            "ERROR: %s at line %d",
                                            __FUNCTION__, __LINE__);
  }

  // Verify that we really found the I420 codec.
  error = codec_interface->GetSendCodec(video_channel, video_codec);
  *number_of_errors += ViETest::TestError(error == 0,
                                          "ERROR: %s at line %d",
                                          __FUNCTION__, __LINE__);
  *number_of_errors += ViETest::TestError(
      video_codec.codecType == webrtc::kVideoCodecI420,
      "ERROR: %s at line %d", __FUNCTION__, __LINE__);

  // Set up senders and receivers.
  char version[1024] = "";
  error = base_interface->GetVersion(version);
  ViETest::Log("\nUsing WebRTC Video Engine version: %s", version);
  *number_of_errors += ViETest::TestError(error == 0, "ERROR: %s at line %d",
                                          __FUNCTION__, __LINE__);
  const char *ipAddress = "127.0.0.1";
  WebRtc_UWord16 rtpPortListen = 6100;
  WebRtc_UWord16 rtpPortSend = 6100;
  error = network_interface->SetLocalReceiver(video_channel, rtpPortListen);
  *number_of_errors += ViETest::TestError(error == 0, "ERROR: %s at line %d",
                                          __FUNCTION__, __LINE__);
  error = base_interface->StartReceive(video_channel);
  *number_of_errors += ViETest::TestError(error == 0, "ERROR: %s at line %d",
                                          __FUNCTION__, __LINE__);
  error = network_interface->SetSendDestination(video_channel, ipAddress,
                                                rtpPortSend);
  *number_of_errors += ViETest::TestError(error == 0, "ERROR: %s at line %d",
                                          __FUNCTION__, __LINE__);
  error = base_interface->StartSend(video_channel);
  *number_of_errors += ViETest::TestError(error == 0, "ERROR: %s at line %d",
                                          __FUNCTION__, __LINE__);

  // Call started.
  ViETest::Log("Call started");
  ViETest::Log("You should see a local preview from camera %s"
               " in window 1 and the remote video in window 2.", device_name);

  AutoTestSleep(KAutoTestSleepTimeMs);

  // Done.
  error = base_interface->StopSend(video_channel);
  *number_of_errors += ViETest::TestError(error == 0, "ERROR: %s at line %d",
                                          __FUNCTION__, __LINE__);
}
