/*
 *  Copyright (c) 2014 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#include "webrtc/test/call_test.h"

#include "webrtc/test/encoder_settings.h"

namespace webrtc {
namespace test {

CallTest::CallTest()
    : send_stream_(NULL),
      receive_stream_(NULL),
      fake_encoder_(Clock::GetRealTimeClock()) {
}
CallTest::~CallTest() {
}

void CallTest::RunBaseTest(BaseTest* test) {
  CreateSenderCall(test->GetSenderCallConfig());
  if (test->ShouldCreateReceivers())
    CreateReceiverCall(test->GetReceiverCallConfig());
  test->OnCallsCreated(sender_call_.get(), receiver_call_.get());

  if (test->ShouldCreateReceivers()) {
    test->SetReceivers(receiver_call_->Receiver(), sender_call_->Receiver());
  } else {
    // Sender-only call delivers to itself.
    test->SetReceivers(sender_call_->Receiver(), NULL);
  }

  CreateSendConfig(test->GetNumStreams());
  if (test->ShouldCreateReceivers()) {
    CreateMatchingReceiveConfigs();
  }
  test->ModifyConfigs(&send_config_, &receive_config_, &video_streams_);
  CreateStreams();
  test->OnStreamsCreated(send_stream_, receive_stream_);

  CreateFrameGeneratorCapturer();
  test->OnFrameGeneratorCapturerCreated(frame_generator_capturer_.get());

  Start();
  test->PerformTest();
  test->StopSending();
  Stop();

  DestroyStreams();
}

void CallTest::Start() {
  send_stream_->Start();
  if (receive_stream_ != NULL)
    receive_stream_->Start();
  frame_generator_capturer_->Start();
}

void CallTest::Stop() {
  frame_generator_capturer_->Stop();
  if (receive_stream_ != NULL)
    receive_stream_->Stop();
  send_stream_->Stop();
}

void CallTest::CreateCalls(const Call::Config& sender_config,
                           const Call::Config& receiver_config) {
  CreateSenderCall(sender_config);
  CreateReceiverCall(receiver_config);
}

void CallTest::CreateSenderCall(const Call::Config& config) {
  sender_call_.reset(Call::Create(config));
}

void CallTest::CreateReceiverCall(const Call::Config& config) {
  receiver_call_.reset(Call::Create(config));
}

void CallTest::CreateSendConfig(size_t num_streams) {
  assert(num_streams <= kNumSsrcs);
  send_config_ = sender_call_->GetDefaultSendConfig();
  send_config_.encoder_settings.encoder = &fake_encoder_;
  send_config_.encoder_settings.payload_name = "FAKE";
  send_config_.encoder_settings.payload_type = kFakeSendPayloadType;
  video_streams_ = test::CreateVideoStreams(num_streams);
  for (size_t i = 0; i < num_streams; ++i)
    send_config_.rtp.ssrcs.push_back(kSendSsrcs[i]);
}

// TODO(pbos): Make receive configs into a vector.
void CallTest::CreateMatchingReceiveConfigs() {
  assert(send_config_.rtp.ssrcs.size() == 1);
  receive_config_ = receiver_call_->GetDefaultReceiveConfig();
  VideoCodec codec =
      test::CreateDecoderVideoCodec(send_config_.encoder_settings);
  receive_config_.codecs.push_back(codec);
  if (send_config_.encoder_settings.encoder == &fake_encoder_) {
    ExternalVideoDecoder decoder;
    decoder.decoder = &fake_decoder_;
    decoder.payload_type = send_config_.encoder_settings.payload_type;
    receive_config_.external_decoders.push_back(decoder);
  }
  receive_config_.rtp.remote_ssrc = send_config_.rtp.ssrcs[0];
  receive_config_.rtp.local_ssrc = kReceiverLocalSsrc;
}

void CallTest::CreateFrameGeneratorCapturer() {
  VideoStream stream = video_streams_.back();
  frame_generator_capturer_.reset(
      test::FrameGeneratorCapturer::Create(send_stream_->Input(),
                                           stream.width,
                                           stream.height,
                                           stream.max_framerate,
                                           Clock::GetRealTimeClock()));
}
void CallTest::CreateStreams() {
  assert(send_stream_ == NULL);
  assert(receive_stream_ == NULL);

  send_stream_ =
      sender_call_->CreateVideoSendStream(send_config_, video_streams_, NULL);

  if (receiver_call_.get() != NULL)
    receive_stream_ = receiver_call_->CreateVideoReceiveStream(receive_config_);
}

void CallTest::DestroyStreams() {
  if (send_stream_ != NULL)
    sender_call_->DestroyVideoSendStream(send_stream_);
  if (receive_stream_ != NULL)
    receiver_call_->DestroyVideoReceiveStream(receive_stream_);
  send_stream_ = NULL;
  receive_stream_ = NULL;
}

const unsigned int CallTest::kDefaultTimeoutMs = 30 * 1000;
const unsigned int CallTest::kLongTimeoutMs = 120 * 1000;
const uint8_t CallTest::kSendPayloadType = 100;
const uint8_t CallTest::kFakeSendPayloadType = 125;
const uint8_t CallTest::kSendRtxPayloadType = 98;
const uint32_t CallTest::kSendRtxSsrc = 0xBADCAFE;
const uint32_t CallTest::kSendSsrcs[kNumSsrcs] = {0xC0FFED, 0xC0FFEE, 0xC0FFEF};
const uint32_t CallTest::kReceiverLocalSsrc = 0x123456;
const int CallTest::kNackRtpHistoryMs = 1000;

BaseTest::BaseTest(unsigned int timeout_ms) : RtpRtcpObserver(timeout_ms) {
}

BaseTest::BaseTest(unsigned int timeout_ms,
                   const FakeNetworkPipe::Config& config)
    : RtpRtcpObserver(timeout_ms, config) {
}

BaseTest::~BaseTest() {
}

Call::Config BaseTest::GetSenderCallConfig() {
  return Call::Config(SendTransport());
}

Call::Config BaseTest::GetReceiverCallConfig() {
  return Call::Config(ReceiveTransport());
}

void BaseTest::OnCallsCreated(Call* sender_call, Call* receiver_call) {
}

size_t BaseTest::GetNumStreams() const {
  return 1;
}

void BaseTest::ModifyConfigs(VideoSendStream::Config* send_config,
                             VideoReceiveStream::Config* receive_config,
                             std::vector<VideoStream>* video_streams) {
}

void BaseTest::OnStreamsCreated(VideoSendStream* send_stream,
                                VideoReceiveStream* receive_stream) {
}

void BaseTest::OnFrameGeneratorCapturerCreated(
    FrameGeneratorCapturer* frame_generator_capturer) {
}

SendTest::SendTest(unsigned int timeout_ms) : BaseTest(timeout_ms) {
}

SendTest::SendTest(unsigned int timeout_ms,
                   const FakeNetworkPipe::Config& config)
    : BaseTest(timeout_ms, config) {
}

bool SendTest::ShouldCreateReceivers() const {
  return false;
}

EndToEndTest::EndToEndTest(unsigned int timeout_ms) : BaseTest(timeout_ms) {
}

EndToEndTest::EndToEndTest(unsigned int timeout_ms,
                           const FakeNetworkPipe::Config& config)
    : BaseTest(timeout_ms, config) {
}

bool EndToEndTest::ShouldCreateReceivers() const {
  return true;
}

}  // namespace test
}  // namespace webrtc
