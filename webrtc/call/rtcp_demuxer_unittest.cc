/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/call/rtcp_demuxer.h"

#include <memory>

#include "webrtc/call/rtcp_packet_sink_interface.h"
#include "webrtc/common_types.h"
#include "webrtc/modules/rtp_rtcp/source/rtcp_packet/bye.h"
#include "webrtc/rtc_base/arraysize.h"
#include "webrtc/rtc_base/basictypes.h"
#include "webrtc/rtc_base/checks.h"
#include "webrtc/rtc_base/ptr_util.h"
#include "webrtc/test/gmock.h"
#include "webrtc/test/gtest.h"

namespace webrtc {

namespace {

using ::testing::_;
using ::testing::AtLeast;
using ::testing::ElementsAreArray;
using ::testing::InSequence;
using ::testing::NiceMock;

class MockRtcpPacketSink : public RtcpPacketSinkInterface {
 public:
  MOCK_METHOD1(OnRtcpPacket, void(rtc::ArrayView<const uint8_t>));
};

// Produces a packet buffer representing an RTCP packet with a given SSRC,
// as it would look when sent over the wire.
// |distinguishing_string| allows different RTCP packets with the same SSRC
// to be distinguished. How this is set into the actual packet is
// unimportant, and depends on which RTCP message we choose to use.
rtc::Buffer CreateRtcpPacket(uint32_t ssrc,
                             const std::string& distinguishing_string = "") {
  rtcp::Bye packet;
  packet.SetSenderSsrc(ssrc);
  if (distinguishing_string != "") {
    // Actual way we use |distinguishing_string| is unimportant, so long
    // as it ends up in the packet.
    packet.SetReason(distinguishing_string);
  }
  return packet.Build();
}

}  // namespace

TEST(RtcpDemuxerTest, OnRtcpPacketCalledOnCorrectSinkBySsrc) {
  RtcpDemuxer demuxer;

  constexpr uint32_t ssrcs[] = {101, 202, 303};
  MockRtcpPacketSink sinks[arraysize(ssrcs)];
  for (size_t i = 0; i < arraysize(ssrcs); i++) {
    demuxer.AddSink(ssrcs[i], &sinks[i]);
  }

  for (size_t i = 0; i < arraysize(ssrcs); i++) {
    auto packet = CreateRtcpPacket(ssrcs[i]);
    EXPECT_CALL(sinks[i],
                OnRtcpPacket(ElementsAreArray(packet.cbegin(), packet.cend())))
        .Times(1);
    demuxer.OnRtcpPacket(packet);
  }

  // Test tear-down
  for (const auto& sink : sinks) {
    demuxer.RemoveSink(&sink);
  }
}

TEST(RtcpDemuxerTest, OnRtcpPacketCalledOnResolvedRsidSink) {
  RtcpDemuxer demuxer;

  // Set up some RSID sinks.
  const std::string rsids[] = {"a", "b", "c"};
  MockRtcpPacketSink sinks[arraysize(rsids)];
  for (size_t i = 0; i < arraysize(rsids); i++) {
    demuxer.AddSink(rsids[i], &sinks[i]);
  }

  // Only resolve one of the sinks.
  constexpr size_t resolved_sink_index = 0;
  constexpr uint32_t ssrc = 345;
  demuxer.OnSsrcBoundToRsid(rsids[resolved_sink_index], ssrc);

  // The resolved sink gets notifications of RTCP messages with its SSRC.
  auto packet = CreateRtcpPacket(ssrc);
  EXPECT_CALL(sinks[resolved_sink_index],
              OnRtcpPacket(ElementsAreArray(packet.cbegin(), packet.cend())))
      .Times(1);

  // RTCP received; expected calls triggered.
  demuxer.OnRtcpPacket(packet);

  // Test tear-down
  for (const auto& sink : sinks) {
    demuxer.RemoveSink(&sink);
  }
}

TEST(RtcpDemuxerTest,
     SingleCallbackAfterResolutionOfAnRsidToAlreadyRegisteredSsrc) {
  RtcpDemuxer demuxer;

  // Associate a sink with an SSRC.
  MockRtcpPacketSink sink;
  constexpr uint32_t ssrc = 999;
  demuxer.AddSink(ssrc, &sink);

  // Associate the same sink with an RSID.
  const std::string rsid = "r";
  demuxer.AddSink(rsid, &sink);

  // Resolve the RSID to the aforementioned SSRC.
  demuxer.OnSsrcBoundToRsid(rsid, ssrc);

  // OnRtcpPacket still called only a single time for messages with this SSRC.
  auto packet = CreateRtcpPacket(ssrc);
  EXPECT_CALL(sink,
              OnRtcpPacket(ElementsAreArray(packet.cbegin(), packet.cend())))
      .Times(1);
  demuxer.OnRtcpPacket(packet);

  // Test tear-down
  demuxer.RemoveSink(&sink);
}

TEST(RtcpDemuxerTest, OnRtcpPacketCalledOnAllBroadcastSinksForAllRtcpPackets) {
  RtcpDemuxer demuxer;

  MockRtcpPacketSink sinks[3];
  for (MockRtcpPacketSink& sink : sinks) {
    demuxer.AddBroadcastSink(&sink);
  }

  constexpr uint32_t ssrc = 747;
  auto packet = CreateRtcpPacket(ssrc);

  for (MockRtcpPacketSink& sink : sinks) {
    EXPECT_CALL(sink,
                OnRtcpPacket(ElementsAreArray(packet.cbegin(), packet.cend())))
        .Times(1);
  }

  // RTCP received; expected calls triggered.
  demuxer.OnRtcpPacket(packet);

  // Test tear-down
  for (const auto& sink : sinks) {
    demuxer.RemoveBroadcastSink(&sink);
  }
}

TEST(RtcpDemuxerTest, PacketsDeliveredInRightOrderToNonBroadcastSink) {
  RtcpDemuxer demuxer;

  constexpr uint32_t ssrc = 101;
  MockRtcpPacketSink sink;
  demuxer.AddSink(ssrc, &sink);

  std::vector<rtc::Buffer> packets;
  for (size_t i = 0; i < 5; i++) {
    packets.push_back(CreateRtcpPacket(ssrc, std::to_string(i)));
  }

  InSequence sequence;
  for (const auto& packet : packets) {
    EXPECT_CALL(sink,
                OnRtcpPacket(ElementsAreArray(packet.cbegin(), packet.cend())))
        .Times(1);
  }

  for (const auto& packet : packets) {
    demuxer.OnRtcpPacket(packet);
  }

  // Test tear-down
  demuxer.RemoveSink(&sink);
}

TEST(RtcpDemuxerTest, PacketsDeliveredInRightOrderToBroadcastSink) {
  RtcpDemuxer demuxer;

  MockRtcpPacketSink sink;
  demuxer.AddBroadcastSink(&sink);

  std::vector<rtc::Buffer> packets;
  for (size_t i = 0; i < 5; i++) {
    constexpr uint32_t ssrc = 101;
    packets.push_back(CreateRtcpPacket(ssrc, std::to_string(i)));
  }

  InSequence sequence;
  for (const auto& packet : packets) {
    EXPECT_CALL(sink,
                OnRtcpPacket(ElementsAreArray(packet.cbegin(), packet.cend())))
        .Times(1);
  }

  for (const auto& packet : packets) {
    demuxer.OnRtcpPacket(packet);
  }

  // Test tear-down
  demuxer.RemoveBroadcastSink(&sink);
}

TEST(RtcpDemuxerTest, MultipleSinksMappedToSameSsrc) {
  RtcpDemuxer demuxer;

  MockRtcpPacketSink sinks[3];
  constexpr uint32_t ssrc = 404;
  for (auto& sink : sinks) {
    demuxer.AddSink(ssrc, &sink);
  }

  // Reception of an RTCP packet associated with the shared SSRC triggers the
  // callback on all of the sinks associated with it.
  auto packet = CreateRtcpPacket(ssrc);
  for (auto& sink : sinks) {
    EXPECT_CALL(sink,
                OnRtcpPacket(ElementsAreArray(packet.cbegin(), packet.cend())));
  }
  demuxer.OnRtcpPacket(packet);

  // Test tear-down
  for (const auto& sink : sinks) {
    demuxer.RemoveSink(&sink);
  }
}

TEST(RtcpDemuxerTest, SinkMappedToMultipleSsrcs) {
  RtcpDemuxer demuxer;

  constexpr uint32_t ssrcs[] = {404, 505, 606};
  MockRtcpPacketSink sink;
  for (uint32_t ssrc : ssrcs) {
    demuxer.AddSink(ssrc, &sink);
  }

  // The sink which is associated with multiple SSRCs gets the callback
  // triggered for each of those SSRCs.
  for (uint32_t ssrc : ssrcs) {
    auto packet = CreateRtcpPacket(ssrc);
    EXPECT_CALL(sink,
                OnRtcpPacket(ElementsAreArray(packet.cbegin(), packet.cend())));
    demuxer.OnRtcpPacket(packet);
  }

  // Test tear-down
  demuxer.RemoveSink(&sink);
}

TEST(RtcpDemuxerTest, MultipleRsidsOnSameSink) {
  RtcpDemuxer demuxer;

  // Sink associated with multiple sinks.
  MockRtcpPacketSink sink;
  const std::string rsids[] = {"a", "b", "c"};
  for (const auto& rsid : rsids) {
    demuxer.AddSink(rsid, &sink);
  }

  // RSIDs resolved to SSRCs.
  uint32_t ssrcs[arraysize(rsids)];
  for (size_t i = 0; i < arraysize(rsids); i++) {
    ssrcs[i] = 1000 + static_cast<uint32_t>(i);
    demuxer.OnSsrcBoundToRsid(rsids[i], ssrcs[i]);
  }

  // Set up packets to match those RSIDs/SSRCs.
  std::vector<rtc::Buffer> packets;
  for (size_t i = 0; i < arraysize(rsids); i++) {
    packets.push_back(CreateRtcpPacket(ssrcs[i]));
  }

  // The sink expects to receive all of the packets.
  for (const auto& packet : packets) {
    EXPECT_CALL(sink,
                OnRtcpPacket(ElementsAreArray(packet.cbegin(), packet.cend())))
        .Times(1);
  }

  // Packet demuxed correctly; OnRtcpPacket() triggered on sink.
  for (const auto& packet : packets) {
    demuxer.OnRtcpPacket(packet);
  }

  // Test tear-down
  demuxer.RemoveSink(&sink);
}

TEST(RtcpDemuxerTest, RsidUsedByMultipleSinks) {
  RtcpDemuxer demuxer;

  MockRtcpPacketSink sinks[3];
  const std::string shared_rsid = "a";

  for (MockRtcpPacketSink& sink : sinks) {
    demuxer.AddSink(shared_rsid, &sink);
  }

  constexpr uint32_t shared_ssrc = 888;
  demuxer.OnSsrcBoundToRsid(shared_rsid, shared_ssrc);

  auto packet = CreateRtcpPacket(shared_ssrc);

  for (MockRtcpPacketSink& sink : sinks) {
    EXPECT_CALL(sink,
                OnRtcpPacket(ElementsAreArray(packet.cbegin(), packet.cend())))
        .Times(1);
  }

  demuxer.OnRtcpPacket(packet);

  // Test tear-down
  for (MockRtcpPacketSink& sink : sinks) {
    demuxer.RemoveSink(&sink);
  }
}

TEST(RtcpDemuxerTest, NoCallbackOnSsrcSinkRemovedBeforeFirstPacket) {
  RtcpDemuxer demuxer;

  constexpr uint32_t ssrc = 404;
  MockRtcpPacketSink sink;
  demuxer.AddSink(ssrc, &sink);

  demuxer.RemoveSink(&sink);

  // The removed sink does not get callbacks.
  auto packet = CreateRtcpPacket(ssrc);
  EXPECT_CALL(sink, OnRtcpPacket(_)).Times(0);  // Not called.
  demuxer.OnRtcpPacket(packet);
}

TEST(RtcpDemuxerTest, NoCallbackOnSsrcSinkRemovedAfterFirstPacket) {
  RtcpDemuxer demuxer;

  constexpr uint32_t ssrc = 404;
  NiceMock<MockRtcpPacketSink> sink;
  demuxer.AddSink(ssrc, &sink);

  auto before_packet = CreateRtcpPacket(ssrc);
  demuxer.OnRtcpPacket(before_packet);

  demuxer.RemoveSink(&sink);

  // The removed sink does not get callbacks.
  auto after_packet = CreateRtcpPacket(ssrc);
  EXPECT_CALL(sink, OnRtcpPacket(_)).Times(0);  // Not called.
  demuxer.OnRtcpPacket(after_packet);
}

TEST(RtcpDemuxerTest, NoCallbackOnRsidSinkRemovedBeforeRsidResolution) {
  RtcpDemuxer demuxer;

  const std::string rsid = "a";
  constexpr uint32_t ssrc = 404;
  MockRtcpPacketSink sink;
  demuxer.AddSink(rsid, &sink);

  // Removal before resolution.
  demuxer.RemoveSink(&sink);
  demuxer.OnSsrcBoundToRsid(rsid, ssrc);

  // The removed sink does not get callbacks.
  auto packet = CreateRtcpPacket(ssrc);
  EXPECT_CALL(sink, OnRtcpPacket(_)).Times(0);  // Not called.
  demuxer.OnRtcpPacket(packet);
}

TEST(RtcpDemuxerTest, NoCallbackOnRsidSinkRemovedAfterRsidResolution) {
  RtcpDemuxer demuxer;

  const std::string rsid = "a";
  constexpr uint32_t ssrc = 404;
  MockRtcpPacketSink sink;
  demuxer.AddSink(rsid, &sink);

  // Removal after resolution.
  demuxer.OnSsrcBoundToRsid(rsid, ssrc);
  demuxer.RemoveSink(&sink);

  // The removed sink does not get callbacks.
  auto packet = CreateRtcpPacket(ssrc);
  EXPECT_CALL(sink, OnRtcpPacket(_)).Times(0);  // Not called.
  demuxer.OnRtcpPacket(packet);
}

TEST(RtcpDemuxerTest, NoCallbackOnBroadcastSinkRemovedBeforeFirstPacket) {
  RtcpDemuxer demuxer;

  MockRtcpPacketSink sink;
  demuxer.AddBroadcastSink(&sink);

  demuxer.RemoveBroadcastSink(&sink);

  // The removed sink does not get callbacks.
  constexpr uint32_t ssrc = 404;
  auto packet = CreateRtcpPacket(ssrc);
  EXPECT_CALL(sink, OnRtcpPacket(_)).Times(0);  // Not called.
  demuxer.OnRtcpPacket(packet);
}

TEST(RtcpDemuxerTest, NoCallbackOnBroadcastSinkRemovedAfterFirstPacket) {
  RtcpDemuxer demuxer;

  NiceMock<MockRtcpPacketSink> sink;
  demuxer.AddBroadcastSink(&sink);

  constexpr uint32_t ssrc = 404;
  auto before_packet = CreateRtcpPacket(ssrc);
  demuxer.OnRtcpPacket(before_packet);

  demuxer.RemoveBroadcastSink(&sink);

  // The removed sink does not get callbacks.
  auto after_packet = CreateRtcpPacket(ssrc);
  EXPECT_CALL(sink, OnRtcpPacket(_)).Times(0);  // Not called.
  demuxer.OnRtcpPacket(after_packet);
}

// The RSID to SSRC mapping should be one-to-one. If we end up receiving
// two (or more) packets with the same SSRC, but different RSIDs, we guarantee
// remembering the first one; no guarantees are made about further associations.
TEST(RtcpDemuxerTest, FirstRsolutionOfRsidNotForgotten) {
  RtcpDemuxer demuxer;
  MockRtcpPacketSink sink;

  const std::string rsid = "a";
  demuxer.AddSink(rsid, &sink);

  constexpr uint32_t ssrc_a = 111;  // First resolution - guaranteed effective.
  demuxer.OnSsrcBoundToRsid(rsid, ssrc_a);

  constexpr uint32_t ssrc_b = 222;  // Second resolution - no guarantees.
  demuxer.OnSsrcBoundToRsid(rsid, ssrc_b);

  auto packet_a = CreateRtcpPacket(ssrc_a);
  EXPECT_CALL(
      sink, OnRtcpPacket(ElementsAreArray(packet_a.cbegin(), packet_a.cend())))
      .Times(1);
  demuxer.OnRtcpPacket(packet_a);

  auto packet_b = CreateRtcpPacket(ssrc_b);
  EXPECT_CALL(
      sink, OnRtcpPacket(ElementsAreArray(packet_b.cbegin(), packet_b.cend())))
      .Times(AtLeast(0));
  demuxer.OnRtcpPacket(packet_b);

  // Test tear-down
  demuxer.RemoveSink(&sink);
}

#if RTC_DCHECK_IS_ON && GTEST_HAS_DEATH_TEST && !defined(WEBRTC_ANDROID)
TEST(RtcpDemuxerTest, RepeatedSsrcToSinkAssociationsDisallowed) {
  RtcpDemuxer demuxer;
  MockRtcpPacketSink sink;

  constexpr uint32_t ssrc = 101;
  demuxer.AddSink(ssrc, &sink);
  EXPECT_DEATH(demuxer.AddSink(ssrc, &sink), "");

  // Test tear-down
  demuxer.RemoveSink(&sink);
}

TEST(RtcpDemuxerTest, RepeatedRsidToSinkAssociationsDisallowed) {
  RtcpDemuxer demuxer;
  MockRtcpPacketSink sink;

  const std::string rsid = "z";
  demuxer.AddSink(rsid, &sink);
  EXPECT_DEATH(demuxer.AddSink(rsid, &sink), "");

  // Test tear-down
  demuxer.RemoveSink(&sink);
}

TEST(RtcpDemuxerTest, RepeatedBroadcastSinkRegistrationDisallowed) {
  RtcpDemuxer demuxer;
  MockRtcpPacketSink sink;

  demuxer.AddBroadcastSink(&sink);
  EXPECT_DEATH(demuxer.AddBroadcastSink(&sink), "");

  // Test tear-down
  demuxer.RemoveBroadcastSink(&sink);
}

TEST(RtcpDemuxerTest, SsrcSinkCannotAlsoBeRegisteredAsBroadcast) {
  RtcpDemuxer demuxer;
  MockRtcpPacketSink sink;

  constexpr uint32_t ssrc = 101;
  demuxer.AddSink(ssrc, &sink);
  EXPECT_DEATH(demuxer.AddBroadcastSink(&sink), "");

  // Test tear-down
  demuxer.RemoveSink(&sink);
}

TEST(RtcpDemuxerTest, RsidSinkCannotAlsoBeRegisteredAsBroadcast) {
  RtcpDemuxer demuxer;
  MockRtcpPacketSink sink;

  const std::string rsid = "z";
  demuxer.AddSink(rsid, &sink);
  EXPECT_DEATH(demuxer.AddBroadcastSink(&sink), "");

  // Test tear-down
  demuxer.RemoveSink(&sink);
}

TEST(RtcpDemuxerTest, BroadcastSinkCannotAlsoBeRegisteredAsSsrcSink) {
  RtcpDemuxer demuxer;
  MockRtcpPacketSink sink;

  demuxer.AddBroadcastSink(&sink);
  constexpr uint32_t ssrc = 101;
  EXPECT_DEATH(demuxer.AddSink(ssrc, &sink), "");

  // Test tear-down
  demuxer.RemoveBroadcastSink(&sink);
}

TEST(RtcpDemuxerTest, BroadcastSinkCannotAlsoBeRegisteredAsRsidSink) {
  RtcpDemuxer demuxer;
  MockRtcpPacketSink sink;

  demuxer.AddBroadcastSink(&sink);
  const std::string rsid = "j";
  EXPECT_DEATH(demuxer.AddSink(rsid, &sink), "");

  // Test tear-down
  demuxer.RemoveBroadcastSink(&sink);
}

TEST(RtcpDemuxerTest, MayNotCallRemoveSinkOnNeverAddedSink) {
  RtcpDemuxer demuxer;
  MockRtcpPacketSink sink;

  EXPECT_DEATH(demuxer.RemoveSink(&sink), "");
}

TEST(RtcpDemuxerTest, MayNotCallRemoveBroadcastSinkOnNeverAddedSink) {
  RtcpDemuxer demuxer;
  MockRtcpPacketSink sink;

  EXPECT_DEATH(demuxer.RemoveBroadcastSink(&sink), "");
}

TEST(RtcpDemuxerTest, RsidMustBeNonEmpty) {
  RtcpDemuxer demuxer;
  MockRtcpPacketSink sink;
  EXPECT_DEATH(demuxer.AddSink("", &sink), "");
}

TEST(RtcpDemuxerTest, RsidMustBeAlphaNumeric) {
  RtcpDemuxer demuxer;
  MockRtcpPacketSink sink;
  EXPECT_DEATH(demuxer.AddSink("a_3", &sink), "");
}

TEST(RtcpDemuxerTest, RsidMustNotExceedMaximumLength) {
  RtcpDemuxer demuxer;
  MockRtcpPacketSink sink;
  std::string rsid(StreamId::kMaxSize + 1, 'a');
  EXPECT_DEATH(demuxer.AddSink(rsid, &sink), "");
}
#endif
}  // namespace webrtc
