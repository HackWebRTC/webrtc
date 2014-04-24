/*
 *  Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <stdio.h>

#include <map>

#include "testing/gtest/include/gtest/gtest.h"

#include "webrtc/call.h"
#include "webrtc/modules/video_coding/codecs/vp8/include/vp8.h"
#include "webrtc/system_wrappers/interface/clock.h"
#include "webrtc/system_wrappers/interface/scoped_ptr.h"
#include "webrtc/test/direct_transport.h"
#include "webrtc/test/encoder_settings.h"
#include "webrtc/test/fake_encoder.h"
#include "webrtc/test/flags.h"
#include "webrtc/test/run_loop.h"
#include "webrtc/test/run_tests.h"
#include "webrtc/test/video_capturer.h"
#include "webrtc/test/video_renderer.h"
#include "webrtc/typedefs.h"

namespace webrtc {

class LoopbackTest : public ::testing::Test {
 protected:
  std::map<uint32_t, bool> reserved_ssrcs_;
};

static const uint32_t kSendSsrc = 0x654321;
static const uint32_t kReceiverLocalSsrc = 0x123456;

TEST_F(LoopbackTest, Test) {
  scoped_ptr<test::VideoRenderer> local_preview(test::VideoRenderer::Create(
      "Local Preview", test::flags::Width(), test::flags::Height()));
  scoped_ptr<test::VideoRenderer> loopback_video(test::VideoRenderer::Create(
      "Loopback Video", test::flags::Width(), test::flags::Height()));

  test::DirectTransport transport;
  Call::Config call_config(&transport);
  scoped_ptr<Call> call(Call::Create(call_config));

  // Loopback, call sends to itself.
  transport.SetReceiver(call->Receiver());

  VideoSendStream::Config send_config = call->GetDefaultSendConfig();
  send_config.rtp.ssrcs.push_back(kSendSsrc);

  send_config.local_renderer = local_preview.get();

  scoped_ptr<VP8Encoder> encoder(VP8Encoder::Create());
  send_config.encoder_settings =
      test::CreateEncoderSettings(encoder.get(), "VP8", 124, 1);
  VideoStream* stream = &send_config.encoder_settings.streams[0];
  stream->width = test::flags::Width();
  stream->height = test::flags::Height();
  stream->min_bitrate_bps = static_cast<int>(test::flags::MinBitrate()) * 1000;
  stream->target_bitrate_bps =
      static_cast<int>(test::flags::MaxBitrate()) * 1000;
  stream->max_bitrate_bps = static_cast<int>(test::flags::MaxBitrate()) * 1000;
  stream->max_framerate = 30;
  stream->max_qp = 56;

  VideoSendStream* send_stream = call->CreateVideoSendStream(send_config);

  Clock* test_clock = Clock::GetRealTimeClock();

  scoped_ptr<test::VideoCapturer> camera(
      test::VideoCapturer::Create(send_stream->Input(),
                                  test::flags::Width(),
                                  test::flags::Height(),
                                  test::flags::Fps(),
                                  test_clock));

  VideoReceiveStream::Config receive_config = call->GetDefaultReceiveConfig();
  receive_config.rtp.remote_ssrc = send_config.rtp.ssrcs[0];
  receive_config.rtp.local_ssrc = kReceiverLocalSsrc;
  receive_config.renderer = loopback_video.get();
  VideoCodec codec =
      test::CreateDecoderVideoCodec(send_config.encoder_settings);
  receive_config.codecs.push_back(codec);

  VideoReceiveStream* receive_stream =
      call->CreateVideoReceiveStream(receive_config);

  receive_stream->Start();
  send_stream->Start();
  camera->Start();

  test::PressEnterToContinue();

  camera->Stop();
  send_stream->Stop();
  receive_stream->Stop();

  call->DestroyVideoReceiveStream(receive_stream);
  call->DestroyVideoSendStream(send_stream);

  transport.StopSending();
}
}  // namespace webrtc
