/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/call/rtp_demuxer.h"

#include <memory>

#include "webrtc/base/arraysize.h"
#include "webrtc/base/checks.h"
#include "webrtc/base/ptr_util.h"
#include "webrtc/modules/rtp_rtcp/source/rtp_packet_received.h"
#include "webrtc/test/gmock.h"
#include "webrtc/test/gtest.h"

namespace webrtc {

namespace {

using ::testing::_;

class MockRtpPacketSink : public RtpPacketSinkInterface {
 public:
  MOCK_METHOD1(OnRtpPacket, void(const RtpPacketReceived&));
};

constexpr uint32_t kSsrcs[] = {101, 202, 303};

MATCHER_P(SsrcSameAsIn, other, "") {
  return arg.Ssrc() == other.Ssrc();
}

std::unique_ptr<RtpPacketReceived> CreateRtpPacketReceived(uint32_t ssrc) {
  auto packet = rtc::MakeUnique<RtpPacketReceived>();
  packet->SetSsrc(ssrc);
  return packet;
}

class RtpDemuxerTest : public ::testing::Test {
 protected:
  RtpDemuxerTest() {
    for (size_t i = 0; i < arraysize(sinks); i++) {
      demuxer.AddSink(kSsrcs[i], &sinks[i]);
    }
  }

  ~RtpDemuxerTest() override {
    for (auto& sink : sinks) {
      EXPECT_EQ(demuxer.RemoveSink(&sink), 1u);
    }
  }

  RtpDemuxer demuxer;
  MockRtpPacketSink sinks[arraysize(kSsrcs)];
};

TEST_F(RtpDemuxerTest, OnRtpPacketCalledOnCorrectSink) {
  for (size_t i = 0; i < arraysize(sinks); i++) {
    auto packet = CreateRtpPacketReceived(kSsrcs[i]);
    EXPECT_CALL(sinks[i], OnRtpPacket(SsrcSameAsIn(*packet)));
    demuxer.OnRtpPacket(*packet);
  }
}

TEST_F(RtpDemuxerTest, MultipleSinksMappedToSameSsrc) {
  // |sinks| associated with different SSRCs each. Add a few additional sinks
  // that are all associated with one new, distinct SSRC.
  MockRtpPacketSink same_ssrc_sinks[arraysize(sinks)];
  constexpr uint32_t kSharedSsrc = 404;
  for (auto& sink : same_ssrc_sinks) {
    demuxer.AddSink(kSharedSsrc, &sink);
  }

  // Reception of an RTP packet associated with the shared SSRC triggers the
  // callback on all of the interfaces associated with it.
  auto packet = CreateRtpPacketReceived(kSharedSsrc);
  for (auto& sink : same_ssrc_sinks) {
    EXPECT_CALL(sink, OnRtpPacket(SsrcSameAsIn(*packet)));
  }
  demuxer.OnRtpPacket(*packet);

  // Test-specific tear-down
  for (auto& sink : same_ssrc_sinks) {
    EXPECT_EQ(demuxer.RemoveSink(&sink), 1u);
  }
}

TEST_F(RtpDemuxerTest, SinkMappedToMultipleSsrcs) {
  // |sinks| associated with different SSRCs each. We set one of them to also
  // be mapped to additional SSRCs.
  constexpr uint32_t kSsrcsOfMultiSsrcSink[] = {404, 505, 606};
  MockRtpPacketSink multi_ssrc_sink;
  for (uint32_t ssrc : kSsrcsOfMultiSsrcSink) {
    demuxer.AddSink(ssrc, &multi_ssrc_sink);
  }

  // The sink which is associated with multiple SSRCs gets the callback
  // triggered for each of those SSRCs.
  for (uint32_t ssrc : kSsrcsOfMultiSsrcSink) {
    auto packet = CreateRtpPacketReceived(ssrc);
    EXPECT_CALL(multi_ssrc_sink, OnRtpPacket(SsrcSameAsIn(*packet)));
    demuxer.OnRtpPacket(*packet);
  }

  // Test-specific tear-down
  EXPECT_EQ(demuxer.RemoveSink(&multi_ssrc_sink),
            arraysize(kSsrcsOfMultiSsrcSink));
}

TEST_F(RtpDemuxerTest, OnRtpPacketNotCalledOnRemovedSinks) {
  // |sinks| associated with different SSRCs each. We set one of them to also
  // be mapped to additional SSRCs.
  constexpr uint32_t kSsrcsOfMultiSsrcSink[] = {404, 505, 606};
  MockRtpPacketSink multi_ssrc_sink;
  for (uint32_t ssrc : kSsrcsOfMultiSsrcSink) {
    demuxer.AddSink(ssrc, &multi_ssrc_sink);
  }

  // Remove the sink.
  EXPECT_EQ(demuxer.RemoveSink(&multi_ssrc_sink),
            arraysize(kSsrcsOfMultiSsrcSink));

  // The removed sink does not get callbacks triggered for any of the SSRCs
  // with which it was previously associated.
  EXPECT_CALL(multi_ssrc_sink, OnRtpPacket(_)).Times(0);
  for (uint32_t ssrc : kSsrcsOfMultiSsrcSink) {
    auto packet = CreateRtpPacketReceived(ssrc);
    demuxer.OnRtpPacket(*packet);
  }
}

#if RTC_DCHECK_IS_ON && GTEST_HAS_DEATH_TEST && !defined(WEBRTC_ANDROID)
TEST_F(RtpDemuxerTest, RepeatedAssociationsForbidden) {
  // Set-up already associated sinks[0] with kSsrcs[0]. Repeating the
  // association is an error.
  EXPECT_DEATH(demuxer.AddSink(kSsrcs[0], &sinks[0]), "");
}
#endif

}  // namespace
}  // namespace webrtc
