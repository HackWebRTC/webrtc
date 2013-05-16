/*
 *  Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <gtest/gtest.h>

#include <iostream>
#include <map>

#include "webrtc/video_engine/new_include/video_engine.h"
#include "webrtc/video_engine/test/common/direct_transport.h"
#include "webrtc/video_engine/test/common/generate_ssrcs.h"
#include "webrtc/video_engine/test/common/video_capturer.h"
#include "webrtc/video_engine/test/common/video_renderer.h"
#include "webrtc/typedefs.h"

namespace webrtc {

class LoopbackTest : public ::testing::Test {
 protected:
  std::map<uint32_t, bool> reserved_ssrcs;
};

TEST_F(LoopbackTest, Test) {
  test::VideoRenderer* local_preview =
      test::VideoRenderer::Create("Local Preview");
  test::VideoRenderer* loopback_video =
      test::VideoRenderer::Create("Loopback Video");

  newapi::VideoEngine* video_engine =
      newapi::VideoEngine::Create(webrtc::newapi::VideoEngineConfig());

  test::DirectTransport transport(NULL);
  newapi::VideoCall* call = video_engine->CreateCall(&transport);

  // Loopback, call sends to itself.
  transport.SetReceiver(call->Receiver());

  newapi::VideoSendStreamConfig send_config;
  call->GetDefaultSendConfig(&send_config);
  test::GenerateRandomSsrcs(&send_config, &reserved_ssrcs);

  send_config.local_renderer = local_preview;

  // TODO(pbos): Should be specified by command-line parameters. And not even
  //             visible in the test. Break it out to some get-test-defaults
  //             class
  send_config.codec.width = 640;
  send_config.codec.height = 480;
  send_config.codec.minBitrate = 1000;
  send_config.codec.startBitrate = 1500;
  send_config.codec.maxBitrate = 2000;

  newapi::VideoSendStream* send_stream = call->CreateSendStream(send_config);

  test::VideoCapturer* camera =
      test::VideoCapturer::Create(send_stream->Input());

  newapi::VideoReceiveStreamConfig receive_config;
  call->GetDefaultReceiveConfig(&receive_config);
  receive_config.rtp.ssrc = send_config.rtp.ssrcs[0];
  receive_config.renderer = loopback_video;

  newapi::VideoReceiveStream* receive_stream =
      call->CreateReceiveStream(receive_config);

  receive_stream->StartReceive();
  send_stream->StartSend();

  camera->Start();

  // TODO(pbos): Run this time limited (optionally), so it can run automated.
  std::cout << ">> Press ENTER to continue..." << std::endl;
  std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');

  receive_stream->StopReceive();
  send_stream->StopSend();

  // Stop sending
  delete camera;

  call->DestroyReceiveStream(receive_stream);
  call->DestroySendStream(send_stream);

  delete call;
  delete video_engine;

  delete loopback_video;
  delete local_preview;
}
}  // webrtc

int main(int argc, char* argv[]) {
  ::testing::InitGoogleTest(&argc, argv);

  return RUN_ALL_TESTS();
}
