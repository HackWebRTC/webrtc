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

#include "gflags/gflags.h"
#include "testing/gtest/include/gtest/gtest.h"

#include "webrtc/call.h"
#include "webrtc/modules/video_coding/codecs/vp8/include/vp8.h"
#include "webrtc/system_wrappers/interface/clock.h"
#include "webrtc/system_wrappers/interface/scoped_ptr.h"
#include "webrtc/test/direct_transport.h"
#include "webrtc/test/encoder_settings.h"
#include "webrtc/test/fake_encoder.h"
#include "webrtc/test/run_loop.h"
#include "webrtc/test/run_tests.h"
#include "webrtc/test/video_capturer.h"
#include "webrtc/test/video_renderer.h"
#include "webrtc/typedefs.h"

namespace webrtc {
namespace flags {

DEFINE_int32(width, 640, "Video width.");
size_t Width() { return static_cast<size_t>(FLAGS_width); }

DEFINE_int32(height, 480, "Video height.");
size_t Height() { return static_cast<size_t>(FLAGS_height); }

DEFINE_int32(fps, 30, "Frames per second.");
int Fps() { return static_cast<int>(FLAGS_fps); }

DEFINE_int32(min_bitrate, 50, "Minimum video bitrate.");
size_t MinBitrate() { return static_cast<size_t>(FLAGS_min_bitrate); }

DEFINE_int32(start_bitrate, 300, "Video starting bitrate.");
size_t StartBitrate() { return static_cast<size_t>(FLAGS_start_bitrate); }

DEFINE_int32(max_bitrate, 800, "Maximum video bitrate.");
size_t MaxBitrate() { return static_cast<size_t>(FLAGS_max_bitrate); }
}  // namespace flags

static const uint32_t kSendSsrc = 0x654321;
static const uint32_t kReceiverLocalSsrc = 0x123456;

void Loopback() {
  scoped_ptr<test::VideoRenderer> local_preview(test::VideoRenderer::Create(
      "Local Preview", flags::Width(), flags::Height()));
  scoped_ptr<test::VideoRenderer> loopback_video(test::VideoRenderer::Create(
      "Loopback Video", flags::Width(), flags::Height()));

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
  stream->width = flags::Width();
  stream->height = flags::Height();
  stream->min_bitrate_bps = static_cast<int>(flags::MinBitrate()) * 1000;
  stream->target_bitrate_bps =
      static_cast<int>(flags::MaxBitrate()) * 1000;
  stream->max_bitrate_bps = static_cast<int>(flags::MaxBitrate()) * 1000;
  stream->max_framerate = 30;
  stream->max_qp = 56;

  VideoSendStream* send_stream = call->CreateVideoSendStream(send_config);

  Clock* test_clock = Clock::GetRealTimeClock();

  scoped_ptr<test::VideoCapturer> camera(
      test::VideoCapturer::Create(send_stream->Input(),
                                  flags::Width(),
                                  flags::Height(),
                                  flags::Fps(),
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

int main(int argc, char* argv[]) {
  ::testing::InitGoogleTest(&argc, argv);
  google::ParseCommandLineFlags(&argc, &argv, true);

  webrtc::Loopback();
  return 0;
}
