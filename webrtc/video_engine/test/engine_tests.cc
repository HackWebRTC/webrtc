/*
 *  Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#include <map>

#include "testing/gtest/include/gtest/gtest.h"

#include "webrtc/modules/rtp_rtcp/interface/rtp_header_parser.h"
#include "webrtc/modules/rtp_rtcp/source/rtcp_utility.h"
#include "webrtc/system_wrappers/interface/critical_section_wrapper.h"
#include "webrtc/system_wrappers/interface/scoped_ptr.h"
#include "webrtc/system_wrappers/interface/event_wrapper.h"
#include "webrtc/video_engine/new_include/video_engine.h"
#include "webrtc/video_engine/test/common/direct_transport.h"
#include "webrtc/video_engine/test/common/frame_generator.h"
#include "webrtc/video_engine/test/common/frame_generator_capturer.h"
#include "webrtc/video_engine/test/common/generate_ssrcs.h"

namespace webrtc {

class NackObserver {
 public:
  class SenderTransport : public test::DirectTransport {
   public:
    explicit SenderTransport(NackObserver* observer) : observer_(observer) {}

    virtual bool SendRTP(const uint8_t* packet, size_t length) OVERRIDE {
      {
        CriticalSectionScoped lock(observer_->crit_.get());
        if (observer_->DropSendPacket(packet, length))
          return true;
        ++observer_->sent_rtp_packets_;
      }

      return test::DirectTransport::SendRTP(packet, length);
    }

    NackObserver* observer_;
  } sender_transport_;

  class ReceiverTransport : public test::DirectTransport {
   public:
    explicit ReceiverTransport(NackObserver* observer) : observer_(observer) {}

    bool SendRTCP(const uint8_t* packet, size_t length) {
      {
        CriticalSectionScoped lock(observer_->crit_.get());

        RTCPUtility::RTCPParserV2 parser(packet, length, true);
        EXPECT_TRUE(parser.IsValid());

        bool received_nack = false;
        RTCPUtility::RTCPPacketTypes packet_type = parser.Begin();
        while (packet_type != RTCPUtility::kRtcpNotValidCode) {
          if (packet_type == RTCPUtility::kRtcpRtpfbNackCode)
            received_nack = true;

          packet_type = parser.Iterate();
        }

        if (received_nack) {
          observer_->ReceivedNack();
        } else {
          observer_->RtcpWithoutNack();
        }
      }
      return DirectTransport::SendRTCP(packet, length);
    }

    NackObserver* observer_;
  } receiver_transport_;

  NackObserver()
      : sender_transport_(this),
        receiver_transport_(this),
        crit_(CriticalSectionWrapper::CreateCriticalSection()),
        received_all_retransmissions_(EventWrapper::Create()),
        rtp_parser_(RtpHeaderParser::Create()),
        drop_burst_count_(0),
        sent_rtp_packets_(0),
        nacks_left_(4) {}

  EventTypeWrapper Wait() {
    // 2 minutes should be more than enough time for the test to finish.
    return received_all_retransmissions_->Wait(2 * 60 * 1000);
  }

  void StopSending() {
    sender_transport_.StopSending();
    receiver_transport_.StopSending();
  }

 private:
  // Decides whether a current packet should be dropped or not. A retransmitted
  // packet will never be dropped. Packets are dropped in short bursts. When
  // enough NACKs have been received, no further packets are dropped.
  bool DropSendPacket(const uint8_t* packet, size_t length) {
    EXPECT_FALSE(RtpHeaderParser::IsRtcp(packet, static_cast<int>(length)));

    RTPHeader header;
    EXPECT_TRUE(rtp_parser_->Parse(packet, static_cast<int>(length), &header));

    // Never drop retransmitted packets.
    if (dropped_packets_.find(header.sequenceNumber) !=
        dropped_packets_.end()) {
      retransmitted_packets_.insert(header.sequenceNumber);
      return false;
    }

    // Enough NACKs received, stop dropping packets.
    if (nacks_left_ == 0)
      return false;

    // Still dropping packets.
    if (drop_burst_count_ > 0) {
      --drop_burst_count_;
      dropped_packets_.insert(header.sequenceNumber);
      return true;
    }

    if (sent_rtp_packets_ > 0 && rand() % 20 == 0) {
      drop_burst_count_ = rand() % 10;
      dropped_packets_.insert(header.sequenceNumber);
      return true;
    }

    return false;
  }

  void ReceivedNack() {
    if (nacks_left_ > 0)
      --nacks_left_;
    rtcp_without_nack_count_ = 0;
  }

  void RtcpWithoutNack() {
    if (nacks_left_ > 0)
      return;
    ++rtcp_without_nack_count_;

    // All packets retransmitted and no recent NACKs.
    if (dropped_packets_.size() == retransmitted_packets_.size() &&
        rtcp_without_nack_count_ >= kRequiredRtcpsWithoutNack) {
      received_all_retransmissions_->Set();
    }
  }

  scoped_ptr<CriticalSectionWrapper> crit_;
  scoped_ptr<EventWrapper> received_all_retransmissions_;

  scoped_ptr<RtpHeaderParser> rtp_parser_;
  std::set<uint16_t> dropped_packets_;
  std::set<uint16_t> retransmitted_packets_;
  int drop_burst_count_;
  uint64_t sent_rtp_packets_;
  int nacks_left_;
  int rtcp_without_nack_count_;
  static const int kRequiredRtcpsWithoutNack = 2;
};

struct EngineTestParams {
  size_t width, height;
  struct {
    unsigned int min, start, max;
  } bitrate;
};

class EngineTest : public ::testing::TestWithParam<EngineTestParams> {
 public:
  virtual void SetUp() {
    video_engine_.reset(
        newapi::VideoEngine::Create(newapi::VideoEngineConfig()));
    reserved_ssrcs_.clear();
  }

 protected:
  newapi::VideoCall* CreateTestCall(newapi::Transport* transport) {
    newapi::VideoCall::Config call_config;
    call_config.send_transport = transport;
    return video_engine_->CreateCall(call_config);
  }

  newapi::VideoSendStream::Config CreateTestSendConfig(
      newapi::VideoCall* call,
      EngineTestParams params) {
    newapi::VideoSendStream::Config config = call->GetDefaultSendConfig();

    test::GenerateRandomSsrcs(&config, &reserved_ssrcs_);

    config.codec.width = static_cast<uint16_t>(params.width);
    config.codec.height = static_cast<uint16_t>(params.height);
    config.codec.minBitrate = params.bitrate.min;
    config.codec.startBitrate = params.bitrate.start;
    config.codec.maxBitrate = params.bitrate.max;

    return config;
  }

  test::FrameGeneratorCapturer* CreateTestFrameGeneratorCapturer(
      newapi::VideoSendStream* target,
      EngineTestParams params) {
    return test::FrameGeneratorCapturer::Create(
        target->Input(),
        test::FrameGenerator::Create(
            params.width, params.height, Clock::GetRealTimeClock()),
        30);
  }

  scoped_ptr<newapi::VideoEngine> video_engine_;
  std::map<uint32_t, bool> reserved_ssrcs_;
};

// TODO(pbos): What are sane values here for bitrate? Are we missing any
// important resolutions?
EngineTestParams video_1080p = {1920, 1080, {300, 600, 800}};
EngineTestParams video_720p = {1280, 720, {300, 600, 800}};
EngineTestParams video_vga = {640, 480, {300, 600, 800}};
EngineTestParams video_qvga = {320, 240, {300, 600, 800}};
EngineTestParams video_4cif = {704, 576, {300, 600, 800}};
EngineTestParams video_cif = {352, 288, {300, 600, 800}};
EngineTestParams video_qcif = {176, 144, {300, 600, 800}};

TEST_P(EngineTest, ReceivesAndRetransmitsNack) {
  EngineTestParams params = GetParam();

  // Set up a video call per sender and receiver. Both send RTCP, and have a set
  // RTP history > 0 to enable NACK and retransmissions.
  NackObserver observer;

  scoped_ptr<newapi::VideoCall> sender_call(
      CreateTestCall(&observer.sender_transport_));
  scoped_ptr<newapi::VideoCall> receiver_call(
      CreateTestCall(&observer.receiver_transport_));

  observer.receiver_transport_.SetReceiver(sender_call->Receiver());
  observer.sender_transport_.SetReceiver(receiver_call->Receiver());

  newapi::VideoSendStream::Config send_config =
      CreateTestSendConfig(sender_call.get(), params);
  send_config.rtp.nack.rtp_history_ms = 1000;

  newapi::VideoReceiveStream::Config receive_config =
      receiver_call->GetDefaultReceiveConfig();
  receive_config.rtp.ssrc = send_config.rtp.ssrcs[0];
  receive_config.rtp.nack.rtp_history_ms = send_config.rtp.nack.rtp_history_ms;

  newapi::VideoSendStream* send_stream =
      sender_call->CreateSendStream(send_config);
  newapi::VideoReceiveStream* receive_stream =
      receiver_call->CreateReceiveStream(receive_config);

  scoped_ptr<test::FrameGeneratorCapturer> frame_generator_capturer(
      CreateTestFrameGeneratorCapturer(send_stream, params));
  ASSERT_TRUE(frame_generator_capturer.get() != NULL);

  receive_stream->StartReceive();
  send_stream->StartSend();
  frame_generator_capturer->Start();

  EXPECT_EQ(kEventSignaled, observer.Wait());

  frame_generator_capturer->Stop();
  send_stream->StopSend();
  receive_stream->StopReceive();

  receiver_call->DestroyReceiveStream(receive_stream);
  receiver_call->DestroySendStream(send_stream);
  observer.StopSending();
}

INSTANTIATE_TEST_CASE_P(EngineTest, EngineTest, ::testing::Values(video_vga));
}  // namespace webrtc
