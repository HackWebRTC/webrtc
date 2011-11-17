/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "codec_primitives.h"

#include "general_primitives.h"
#include "vie_autotest.h"
#include "vie_autotest_defines.h"
#include "vie_to_file_renderer.h"
#include "video_capture_factory.h"
#include "tb_interfaces.h"

// Helper functions

void SetSuitableResolution(webrtc::VideoCodec* video_codec,
                           int forced_codec_width,
                           int forced_codec_height) {
  if (forced_codec_width != kDoNotForceResolution &&
      forced_codec_height != kDoNotForceResolution) {
    video_codec->width = forced_codec_width;
    video_codec->height = forced_codec_height;
  } else if (video_codec->codecType == webrtc::kVideoCodecI420) {
    // I420 is very bandwidth heavy, so limit it here.
    video_codec->width = 176;
    video_codec->height = 144;
  } else if (video_codec->codecType == webrtc::kVideoCodecH263) {
    video_codec->width = 352;
    video_codec->height = 288;
  } else {
    // Otherwise go with 640x480.
    video_codec->width = 640;
    video_codec->height = 480;
  }
}

void TestCodecImageProcess(webrtc::VideoCodec video_codec,
                           webrtc::ViECodec* codec_interface,
                           int video_channel,
                           int* number_of_errors,
                           webrtc::ViEImageProcess* image_process) {

  int error = codec_interface->SetSendCodec(video_channel, video_codec);
  *number_of_errors += ViETest::TestError(error == 0,
                                          "ERROR: %s at line %d",
                                          __FUNCTION__, __LINE__);
  ViEAutoTestEffectFilter frame_counter;
  error = image_process->RegisterRenderEffectFilter(video_channel,
                                                    frame_counter);
  *number_of_errors += ViETest::TestError(error == 0,
                                          "ERROR: %s at line %d",
                                          __FUNCTION__, __LINE__);
  AutoTestSleep (KAutoTestSleepTimeMs);

  int max_number_of_rendered_frames = video_codec.maxFramerate *
      KAutoTestSleepTimeMs / 1000;

  if (video_codec.codecType == webrtc::kVideoCodecI420) {
    // Due to that I420 needs a huge bandwidth, rate control can set
    // frame rate very low. This happen since we use the same channel
    // as we just tested with vp8.
    *number_of_errors += ViETest::TestError(frame_counter.numFrames > 0,
                                            "ERROR: %s at line %d",
                                            __FUNCTION__, __LINE__);
  } else {
#ifdef WEBRTC_ANDROID
    // Special case to get the autotest to pass on some slow devices
    *number_of_errors += ViETest::TestError(frameCounter.numFrames
                                            > max_number_of_rendered_frames / 6,
                                            "ERROR: %s at line %d",
                                            __FUNCTION__, __LINE__);
#else
    *number_of_errors += ViETest::TestError(frame_counter.numFrames
                                            > max_number_of_rendered_frames / 4,
                                            "ERROR: %s at line %d",
                                            __FUNCTION__, __LINE__);
#endif
  }
  error = image_process->DeregisterRenderEffectFilter(video_channel);
  *number_of_errors += ViETest::TestError(error == 0,
                                          "ERROR: %s at line %d",
                                          __FUNCTION__, __LINE__);
}

// Test switching from i420 to VP8 as send codec and make sure that
// the codec observer gets called after the switch.
void TestCodecCallbacks(webrtc::ViEBase *& base_interface,
                        webrtc::ViECodec *codec_interface,
                        int video_channel,
                        int* number_of_errors,
                        int forced_codec_width,
                        int forced_codec_height) {

  // Set I420 as send codec so we don't make any assumptions about what
  // we currently have as send codec:
  SetSendCodec(webrtc::kVideoCodecI420, codec_interface, video_channel,
               number_of_errors, forced_codec_width, forced_codec_height);

  // Register the observer:
  ViEAutotestCodecObserver codec_observer;
  int error = codec_interface->RegisterEncoderObserver(video_channel,
                                                       codec_observer);
  *number_of_errors += ViETest::TestError(error == 0, "ERROR: %s at line %d",
                                          __FUNCTION__, __LINE__);
  error = codec_interface->RegisterDecoderObserver(video_channel,
                                                   codec_observer);
  *number_of_errors += ViETest::TestError(error == 0, "ERROR: %s at line %d",
                                          __FUNCTION__, __LINE__);

  // Make the switch
  ViETest::Log("Testing codec callbacks...");

  SetSendCodec(webrtc::kVideoCodecVP8, codec_interface, video_channel,
               number_of_errors, forced_codec_width, forced_codec_height);

  AutoTestSleep (KAutoTestSleepTimeMs);

  // Verify that we got the right codec
  *number_of_errors += ViETest::TestError(
      codec_observer.incomingCodec.codecType == webrtc::kVideoCodecVP8,
      "ERROR: %s at line %d", __FUNCTION__, __LINE__);

  // Clean up
  error = codec_interface->DeregisterEncoderObserver(video_channel);
  *number_of_errors += ViETest::TestError(error == 0, "ERROR: %s at line %d",
                                          __FUNCTION__, __LINE__);
  error = codec_interface->DeregisterDecoderObserver(video_channel);
  *number_of_errors += ViETest::TestError(error == 0, "ERROR: %s at line %d",
                                          __FUNCTION__, __LINE__);
  *number_of_errors += ViETest::TestError(
      codec_observer.incomingCodecCalled > 0,
      "ERROR: %s at line %d", __FUNCTION__, __LINE__);
  *number_of_errors += ViETest::TestError(
      codec_observer.incomingRatecalled > 0,
      "ERROR: %s at line %d", __FUNCTION__, __LINE__);
  *number_of_errors += ViETest::TestError(
      codec_observer.outgoingRatecalled > 0,
      "ERROR: %s at line %d", __FUNCTION__, __LINE__);
}

void TestCodecs(const TbInterfaces& interfaces,
                int & number_of_errors,
                int capture_id,
                int video_channel,
                int forced_codec_width,
                int forced_codec_height) {
  int error;
  webrtc::VideoEngine *video_engine_interface = interfaces.video_engine;
  webrtc::ViEBase *base_interface = interfaces.base;
  webrtc::ViECapture *capture_interface = interfaces.capture;
  webrtc::ViERender *render_interface = interfaces.render;
  webrtc::ViECodec *codec_interface = interfaces.codec;
  webrtc::ViENetwork *network_interface = interfaces.network;

  // ***************************************************************
  // Engine ready. Begin testing class
  // ***************************************************************
  webrtc::VideoCodec video_codec;
  memset(&video_codec, 0, sizeof (webrtc::VideoCodec));

  // Set up all receive codecs. This basically trains the codec interface
  // to be able to recognize all receive codecs based on payload type.
  for (int idx = 0; idx < codec_interface->NumberOfCodecs(); idx++) {
    error = codec_interface->GetCodec(idx, video_codec);
    number_of_errors += ViETest::TestError(error == 0, "ERROR: %s at line %d",
                                           __FUNCTION__, __LINE__);
    SetSuitableResolution(&video_codec,
                          forced_codec_width,
                          forced_codec_height);

    error = codec_interface->SetReceiveCodec(video_channel, video_codec);
    number_of_errors += ViETest::TestError(error == 0, "ERROR: %s at line %d",
                                           __FUNCTION__, __LINE__);
  }
  const char *ip_address = "127.0.0.1";
  const unsigned short rtp_port = 6000;
  error = network_interface->SetLocalReceiver(video_channel, rtp_port);
  number_of_errors += ViETest::TestError(error == 0, "ERROR: %s at line %d",
                                         __FUNCTION__, __LINE__);
  error = base_interface->StartReceive(video_channel);
  number_of_errors += ViETest::TestError(error == 0, "ERROR: %s at line %d",
                                         __FUNCTION__, __LINE__);
  error = network_interface->SetSendDestination(video_channel, ip_address,
                                                rtp_port);
  number_of_errors += ViETest::TestError(error == 0, "ERROR: %s at line %d",
                                         __FUNCTION__, __LINE__);
  error = base_interface->StartSend(video_channel);
  number_of_errors += ViETest::TestError(error == 0, "ERROR: %s at line %d",
                                         __FUNCTION__, __LINE__);
  // Run all found codecs
  webrtc::ViEImageProcess *image_process =
      webrtc::ViEImageProcess::GetInterface(video_engine_interface);
  number_of_errors += ViETest::TestError(error == 0,
                                         "ERROR: %s at line %d",
                                         __FUNCTION__, __LINE__);

  ViETest::Log("Loop through all codecs for %d seconds",
               KAutoTestSleepTimeMs / 1000);
  for (int i = 0; i < codec_interface->NumberOfCodecs(); i++) {
    error = codec_interface->GetCodec(i, video_codec);
    number_of_errors += ViETest::TestError(error == 0,
                                           "ERROR: %s at line %d",
                                           __FUNCTION__, __LINE__);

    if (video_codec.codecType == webrtc::kVideoCodecMPEG4 ||
        video_codec.codecType == webrtc::kVideoCodecRED ||
        video_codec.codecType == webrtc::kVideoCodecULPFEC) {
      ViETest::Log("\t %d. %s not tested", i, video_codec.plName);
    } else {
      ViETest::Log("\t %d. %s", i, video_codec.plName);
      SetSuitableResolution(&video_codec,
                            forced_codec_width, forced_codec_height);
      TestCodecImageProcess(video_codec, codec_interface, video_channel,
                            &number_of_errors, image_process);
    }
  }
  image_process->Release();

  TestCodecCallbacks(base_interface, codec_interface, video_channel,
                     &number_of_errors, forced_codec_width,
                     forced_codec_height);

  ViETest::Log("Done!");

  // ***************************************************************
  // Testing finished. Tear down Video Engine
  // ***************************************************************
  error = base_interface->StopSend(video_channel);
  number_of_errors += ViETest::TestError(error == 0, "ERROR: %s at line %d",
                                         __FUNCTION__, __LINE__);
  error = base_interface->StopReceive(video_channel);
  number_of_errors += ViETest::TestError(error == 0, "ERROR: %s at line %d",
                                         __FUNCTION__, __LINE__);
  error = render_interface->StopRender(capture_id);
  number_of_errors += ViETest::TestError(error == 0, "ERROR: %s at line %d",
                                         __FUNCTION__, __LINE__);
  error = render_interface->StopRender(video_channel);
  number_of_errors += ViETest::TestError(error == 0, "ERROR: %s at line %d",
                                         __FUNCTION__, __LINE__);
  error = render_interface->RemoveRenderer(capture_id);
  number_of_errors += ViETest::TestError(error == 0, "ERROR: %s at line %d",
                                         __FUNCTION__, __LINE__);
  error = render_interface->RemoveRenderer(video_channel);
  number_of_errors += ViETest::TestError(error == 0, "ERROR: %s at line %d",
                                         __FUNCTION__, __LINE__);
  error = capture_interface->DisconnectCaptureDevice(video_channel);
  number_of_errors += ViETest::TestError(error == 0, "ERROR: %s at line %d",
                                         __FUNCTION__, __LINE__);
  error = base_interface->DeleteChannel(video_channel);
  number_of_errors += ViETest::TestError(error == 0, "ERROR: %s at line %d",
                                         __FUNCTION__, __LINE__);
}

void SetSendCodec(webrtc::VideoCodecType of_type,
                  webrtc::ViECodec* codec_interface,
                  int video_channel,
                  int* number_of_errors,
                  int forced_codec_width,
                  int forced_codec_height) {
  webrtc::VideoCodec codec;
  bool ok = FindSpecificCodec(of_type, codec_interface, &codec);
  *number_of_errors += ViETest::TestError(ok == true, "ERROR: %s at line %d",
                                          __FUNCTION__, __LINE__);
  if (!ok) {
    return;
  }

  SetSuitableResolution(&codec, forced_codec_width, forced_codec_height);

  int error = codec_interface->SetSendCodec(video_channel, codec);
  *number_of_errors += ViETest::TestError(error == 0,
                                          "ERROR: %s at line %d",
                                          __FUNCTION__, __LINE__);
}
