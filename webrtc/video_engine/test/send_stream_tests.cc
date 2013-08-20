/*
 *  Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#include "testing/gtest/include/gtest/gtest.h"
#include "webrtc/modules/rtp_rtcp/interface/rtp_header_parser.h"
#include "webrtc/system_wrappers/interface/event_wrapper.h"
#include "webrtc/system_wrappers/interface/scoped_ptr.h"
#include "webrtc/video_engine/test/common/frame_generator.h"
#include "webrtc/video_engine/test/common/frame_generator_capturer.h"
#include "webrtc/video_engine/test/common/null_transport.h"
#include "webrtc/video_engine/new_include/video_call.h"
#include "webrtc/video_engine/new_include/video_send_stream.h"

namespace webrtc {

class VideoSendStreamTest : public ::testing::Test {};
class SendTransportObserver : public test::NullTransport {
 public:
  explicit SendTransportObserver(unsigned long timeout_ms)
      : rtp_header_parser_(RtpHeaderParser::Create()),
        send_test_complete_(EventWrapper::Create()),
        timeout_ms_(timeout_ms) {}

  EventTypeWrapper Wait() {
    return send_test_complete_->Wait(timeout_ms_);
  }

 protected:
  scoped_ptr<RtpHeaderParser> rtp_header_parser_;
  scoped_ptr<EventWrapper> send_test_complete_;

 private:
  unsigned long timeout_ms_;
};

TEST_F(VideoSendStreamTest, SendsSetSsrc) {
  static const uint32_t kSendSsrc = 0xC0FFEE;
  class SendSsrcObserver : public SendTransportObserver {
   public:
    SendSsrcObserver() : SendTransportObserver(30 * 1000) {}

    virtual bool SendRTP(const uint8_t* packet, size_t length) OVERRIDE {
      RTPHeader header;
      EXPECT_TRUE(
          rtp_header_parser_->Parse(packet, static_cast<int>(length), &header));

      if (header.ssrc == kSendSsrc)
        send_test_complete_->Set();

      return true;
    }
  } observer;

  newapi::VideoCall::Config call_config(&observer);
  scoped_ptr<newapi::VideoCall> call(newapi::VideoCall::Create(call_config));

  newapi::VideoSendStream::Config send_config = call->GetDefaultSendConfig();
  send_config.rtp.ssrcs.push_back(kSendSsrc);
  newapi::VideoSendStream* send_stream = call->CreateSendStream(send_config);
  scoped_ptr<test::FrameGeneratorCapturer> frame_generator_capturer(
      test::FrameGeneratorCapturer::Create(
          send_stream->Input(),
          test::FrameGenerator::Create(320, 240, Clock::GetRealTimeClock()),
          30));
  send_stream->StartSend();
  frame_generator_capturer->Start();

  EXPECT_EQ(kEventSignaled, observer.Wait());

  frame_generator_capturer->Stop();
  send_stream->StopSend();
  call->DestroySendStream(send_stream);
}

}  // namespace webrtc
