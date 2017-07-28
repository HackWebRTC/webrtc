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
#include <string>

#include "webrtc/call/rsid_resolution_observer.h"
#include "webrtc/call/test/mock_rtp_packet_sink_interface.h"
#include "webrtc/common_types.h"
#include "webrtc/modules/rtp_rtcp/include/rtp_header_extension_map.h"
#include "webrtc/modules/rtp_rtcp/source/rtp_header_extensions.h"
#include "webrtc/modules/rtp_rtcp/source/rtp_packet_received.h"
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
using ::testing::InSequence;
using ::testing::NiceMock;

class MockRsidResolutionObserver : public RsidResolutionObserver {
 public:
  MOCK_METHOD2(OnRsidResolved, void(const std::string& rsid, uint32_t ssrc));
};

MATCHER_P(SamePacketAs, other, "") {
  return arg.Ssrc() == other.Ssrc() &&
         arg.SequenceNumber() == other.SequenceNumber();
}

std::unique_ptr<RtpPacketReceived> CreateRtpPacketReceived(
    uint32_t ssrc,
    size_t sequence_number = 0) {
  // |sequence_number| is declared |size_t| to prevent ugly casts when calling
  // the function, but should in reality always be a |uint16_t|.
  EXPECT_LT(sequence_number, 1u << 16);

  auto packet = rtc::MakeUnique<RtpPacketReceived>();
  packet->SetSsrc(ssrc);
  packet->SetSequenceNumber(static_cast<uint16_t>(sequence_number));
  return packet;
}

std::unique_ptr<RtpPacketReceived> CreateRtpPacketReceivedWithRsid(
    const std::string& rsid,
    uint32_t ssrc,
    size_t sequence_number = 0) {
  // |sequence_number| is declared |size_t| to prevent ugly casts when calling
  // the function, but should in reality always be a |uint16_t|.
  EXPECT_LT(sequence_number, 1u << 16);

  const int rsid_extension_id = 6;
  RtpPacketReceived::ExtensionManager extension_manager;
  extension_manager.Register<RtpStreamId>(rsid_extension_id);
  auto packet = rtc::MakeUnique<RtpPacketReceived>(&extension_manager);
  packet->SetExtension<RtpStreamId>(rsid);
  packet->SetSsrc(ssrc);
  packet->SetSequenceNumber(static_cast<uint16_t>(sequence_number));
  return packet;
}

TEST(RtpDemuxerTest, OnRtpPacketCalledOnCorrectSinkBySsrc) {
  RtpDemuxer demuxer;

  constexpr uint32_t ssrcs[] = {101, 202, 303};
  MockRtpPacketSink sinks[arraysize(ssrcs)];
  for (size_t i = 0; i < arraysize(ssrcs); i++) {
    demuxer.AddSink(ssrcs[i], &sinks[i]);
  }

  for (size_t i = 0; i < arraysize(ssrcs); i++) {
    auto packet = CreateRtpPacketReceived(ssrcs[i]);
    EXPECT_CALL(sinks[i], OnRtpPacket(SamePacketAs(*packet))).Times(1);
    EXPECT_TRUE(demuxer.OnRtpPacket(*packet));
  }

  // Test tear-down
  for (const auto& sink : sinks) {
    demuxer.RemoveSink(&sink);
  }
}

TEST(RtpDemuxerTest, OnRtpPacketCalledOnCorrectSinkByRsid) {
  RtpDemuxer demuxer;

  const std::string rsids[] = {"a", "b", "c"};
  MockRtpPacketSink sinks[arraysize(rsids)];
  for (size_t i = 0; i < arraysize(rsids); i++) {
    demuxer.AddSink(rsids[i], &sinks[i]);
  }

  for (size_t i = 0; i < arraysize(rsids); i++) {
    auto packet =
        CreateRtpPacketReceivedWithRsid(rsids[i], static_cast<uint32_t>(i), i);
    EXPECT_CALL(sinks[i], OnRtpPacket(SamePacketAs(*packet))).Times(1);
    EXPECT_TRUE(demuxer.OnRtpPacket(*packet));
  }

  // Test tear-down
  for (const auto& sink : sinks) {
    demuxer.RemoveSink(&sink);
  }
}

TEST(RtpDemuxerTest, PacketsDeliveredInRightOrder) {
  RtpDemuxer demuxer;

  constexpr uint32_t ssrc = 101;
  MockRtpPacketSink sink;
  demuxer.AddSink(ssrc, &sink);

  std::unique_ptr<RtpPacketReceived> packets[5];
  for (size_t i = 0; i < arraysize(packets); i++) {
    packets[i] = CreateRtpPacketReceived(ssrc, i);
  }

  InSequence sequence;
  for (const auto& packet : packets) {
    EXPECT_CALL(sink, OnRtpPacket(SamePacketAs(*packet))).Times(1);
  }

  for (const auto& packet : packets) {
    EXPECT_TRUE(demuxer.OnRtpPacket(*packet));
  }

  // Test tear-down
  demuxer.RemoveSink(&sink);
}

TEST(RtpDemuxerTest, MultipleSinksMappedToSameSsrc) {
  RtpDemuxer demuxer;

  MockRtpPacketSink sinks[3];
  constexpr uint32_t ssrc = 404;
  for (auto& sink : sinks) {
    demuxer.AddSink(ssrc, &sink);
  }

  // Reception of an RTP packet associated with the shared SSRC triggers the
  // callback on all of the sinks associated with it.
  auto packet = CreateRtpPacketReceived(ssrc);
  for (auto& sink : sinks) {
    EXPECT_CALL(sink, OnRtpPacket(SamePacketAs(*packet)));
  }
  EXPECT_TRUE(demuxer.OnRtpPacket(*packet));

  // Test tear-down
  for (const auto& sink : sinks) {
    demuxer.RemoveSink(&sink);
  }
}

TEST(RtpDemuxerTest, SinkMappedToMultipleSsrcs) {
  RtpDemuxer demuxer;

  constexpr uint32_t ssrcs[] = {404, 505, 606};
  MockRtpPacketSink sink;
  for (uint32_t ssrc : ssrcs) {
    demuxer.AddSink(ssrc, &sink);
  }

  // The sink which is associated with multiple SSRCs gets the callback
  // triggered for each of those SSRCs.
  for (uint32_t ssrc : ssrcs) {
    auto packet = CreateRtpPacketReceived(ssrc);
    EXPECT_CALL(sink, OnRtpPacket(SamePacketAs(*packet)));
    EXPECT_TRUE(demuxer.OnRtpPacket(*packet));
  }

  // Test tear-down
  demuxer.RemoveSink(&sink);
}

TEST(RtpDemuxerTest, NoCallbackOnSsrcSinkRemovedBeforeFirstPacket) {
  RtpDemuxer demuxer;

  constexpr uint32_t ssrc = 404;
  MockRtpPacketSink sink;
  demuxer.AddSink(ssrc, &sink);

  ASSERT_TRUE(demuxer.RemoveSink(&sink));

  // The removed sink does not get callbacks.
  auto packet = CreateRtpPacketReceived(ssrc);
  EXPECT_CALL(sink, OnRtpPacket(_)).Times(0);  // Not called.
  EXPECT_FALSE(demuxer.OnRtpPacket(*packet));
}

TEST(RtpDemuxerTest, NoCallbackOnSsrcSinkRemovedAfterFirstPacket) {
  RtpDemuxer demuxer;

  constexpr uint32_t ssrc = 404;
  NiceMock<MockRtpPacketSink> sink;
  demuxer.AddSink(ssrc, &sink);

  InSequence sequence;
  uint16_t seq_num;
  for (seq_num = 0; seq_num < 10; seq_num++) {
    ASSERT_TRUE(demuxer.OnRtpPacket(*CreateRtpPacketReceived(ssrc, seq_num)));
  }

  ASSERT_TRUE(demuxer.RemoveSink(&sink));

  // The removed sink does not get callbacks.
  auto packet = CreateRtpPacketReceived(ssrc, seq_num);
  EXPECT_CALL(sink, OnRtpPacket(_)).Times(0);  // Not called.
  EXPECT_FALSE(demuxer.OnRtpPacket(*packet));
}

TEST(RtpDemuxerTest, RepeatedSsrcAssociationsDoNotTriggerRepeatedCallbacks) {
  RtpDemuxer demuxer;

  constexpr uint32_t ssrc = 111;
  MockRtpPacketSink sink;

  demuxer.AddSink(ssrc, &sink);
  demuxer.AddSink(ssrc, &sink);

  auto packet = CreateRtpPacketReceived(ssrc);
  EXPECT_CALL(sink, OnRtpPacket(SamePacketAs(*packet))).Times(1);
  EXPECT_TRUE(demuxer.OnRtpPacket(*packet));

  // Test tear-down
  demuxer.RemoveSink(&sink);
}

TEST(RtpDemuxerTest, RemoveSinkReturnsFalseForNeverAddedSink) {
  RtpDemuxer demuxer;
  MockRtpPacketSink sink;

  EXPECT_FALSE(demuxer.RemoveSink(&sink));
}

TEST(RtpDemuxerTest, RemoveSinkReturnsTrueForPreviouslyAddedSsrcSink) {
  RtpDemuxer demuxer;

  constexpr uint32_t ssrc = 101;
  MockRtpPacketSink sink;
  demuxer.AddSink(ssrc, &sink);

  EXPECT_TRUE(demuxer.RemoveSink(&sink));
}

TEST(RtpDemuxerTest,
     RemoveSinkReturnsTrueForUnresolvedPreviouslyAddedRsidSink) {
  RtpDemuxer demuxer;

  const std::string rsid = "a";
  MockRtpPacketSink sink;
  demuxer.AddSink(rsid, &sink);

  EXPECT_TRUE(demuxer.RemoveSink(&sink));
}

TEST(RtpDemuxerTest, RemoveSinkReturnsTrueForResolvedPreviouslyAddedRsidSink) {
  RtpDemuxer demuxer;

  const std::string rsid = "a";
  constexpr uint32_t ssrc = 101;
  NiceMock<MockRtpPacketSink> sink;
  demuxer.AddSink(rsid, &sink);
  ASSERT_TRUE(
      demuxer.OnRtpPacket(*CreateRtpPacketReceivedWithRsid(rsid, ssrc)));

  EXPECT_TRUE(demuxer.RemoveSink(&sink));
}

TEST(RtpDemuxerTest, OnRtpPacketCalledForRsidSink) {
  RtpDemuxer demuxer;

  MockRtpPacketSink sink;
  const std::string rsid = "a";
  demuxer.AddSink(rsid, &sink);

  // Create a sequence of RTP packets, where only the first one actually
  // mentions the RSID.
  std::unique_ptr<RtpPacketReceived> packets[5];
  constexpr uint32_t rsid_ssrc = 111;
  packets[0] = CreateRtpPacketReceivedWithRsid(rsid, rsid_ssrc);
  for (size_t i = 1; i < arraysize(packets); i++) {
    packets[i] = CreateRtpPacketReceived(rsid_ssrc, i);
  }

  // The first packet associates the RSID with the SSRC, thereby allowing the
  // demuxer to correctly demux all of the packets.
  InSequence sequence;
  for (const auto& packet : packets) {
    EXPECT_CALL(sink, OnRtpPacket(SamePacketAs(*packet))).Times(1);
  }
  for (const auto& packet : packets) {
    EXPECT_TRUE(demuxer.OnRtpPacket(*packet));
  }

  // Test tear-down
  demuxer.RemoveSink(&sink);
}

TEST(RtpDemuxerTest, NoCallbackOnRsidSinkRemovedBeforeFirstPacket) {
  RtpDemuxer demuxer;

  MockRtpPacketSink sink;
  const std::string rsid = "a";
  demuxer.AddSink(rsid, &sink);

  // Sink removed - it won't get triggers even if packets with its RSID arrive.
  ASSERT_TRUE(demuxer.RemoveSink(&sink));

  constexpr uint32_t ssrc = 111;
  auto packet = CreateRtpPacketReceivedWithRsid(rsid, ssrc);
  EXPECT_CALL(sink, OnRtpPacket(_)).Times(0);  // Not called.
  EXPECT_FALSE(demuxer.OnRtpPacket(*packet));
}

TEST(RtpDemuxerTest, NoCallbackOnRsidSinkRemovedAfterFirstPacket) {
  RtpDemuxer demuxer;

  NiceMock<MockRtpPacketSink> sink;
  const std::string rsid = "a";
  demuxer.AddSink(rsid, &sink);

  InSequence sequence;
  constexpr uint32_t ssrc = 111;
  uint16_t seq_num;
  for (seq_num = 0; seq_num < 10; seq_num++) {
    auto packet = CreateRtpPacketReceivedWithRsid(rsid, ssrc, seq_num);
    ASSERT_TRUE(demuxer.OnRtpPacket(*packet));
  }

  // Sink removed - it won't get triggers even if packets with its RSID arrive.
  ASSERT_TRUE(demuxer.RemoveSink(&sink));

  auto packet = CreateRtpPacketReceivedWithRsid(rsid, ssrc, seq_num);
  EXPECT_CALL(sink, OnRtpPacket(_)).Times(0);  // Not called.
  EXPECT_FALSE(demuxer.OnRtpPacket(*packet));
}

// The RSID to SSRC mapping should be one-to-one. If we end up receiving
// two (or more) packets with the same SSRC, but different RSIDs, we guarantee
// remembering the first one; no guarantees are made about further associations.
TEST(RtpDemuxerTest, FirstSsrcAssociatedWithAnRsidIsNotForgotten) {
  RtpDemuxer demuxer;

  // Each sink has a distinct RSID.
  MockRtpPacketSink sink_a;
  const std::string rsid_a = "a";
  demuxer.AddSink(rsid_a, &sink_a);

  MockRtpPacketSink sink_b;
  const std::string rsid_b = "b";
  demuxer.AddSink(rsid_b, &sink_b);

  InSequence sequence;  // Verify that the order of delivery is unchanged.

  constexpr uint32_t shared_ssrc = 100;

  // First a packet with |rsid_a| is received, and |sink_a| is associated with
  // its SSRC.
  auto packet_a = CreateRtpPacketReceivedWithRsid(rsid_a, shared_ssrc, 10);
  EXPECT_CALL(sink_a, OnRtpPacket(SamePacketAs(*packet_a))).Times(1);
  EXPECT_TRUE(demuxer.OnRtpPacket(*packet_a));

  // Second, a packet with |rsid_b| is received. We guarantee that |sink_a|
  // would receive it, and make no guarantees about |sink_b|.
  auto packet_b = CreateRtpPacketReceivedWithRsid(rsid_b, shared_ssrc, 20);
  EXPECT_CALL(sink_a, OnRtpPacket(SamePacketAs(*packet_b))).Times(1);
  EXPECT_CALL(sink_b, OnRtpPacket(SamePacketAs(*packet_b))).Times(AtLeast(0));
  EXPECT_TRUE(demuxer.OnRtpPacket(*packet_b));

  // Known edge-case; adding a new RSID association makes us re-examine all
  // SSRCs. |sink_b| may or may not be associated with the SSRC now; we make
  // no promises on that. We do however still guarantee that |sink_a| still
  // receives the new packets.
  MockRtpPacketSink sink_c;
  const std::string rsid_c = "c";
  constexpr uint32_t some_other_ssrc = shared_ssrc + 1;
  demuxer.AddSink(some_other_ssrc, &sink_c);
  auto packet_c = CreateRtpPacketReceivedWithRsid(rsid_c, shared_ssrc, 30);
  EXPECT_CALL(sink_a, OnRtpPacket(SamePacketAs(*packet_c))).Times(1);
  EXPECT_CALL(sink_b, OnRtpPacket(SamePacketAs(*packet_c))).Times(AtLeast(0));
  EXPECT_TRUE(demuxer.OnRtpPacket(*packet_c));

  // Test tear-down
  demuxer.RemoveSink(&sink_a);
  demuxer.RemoveSink(&sink_b);
  demuxer.RemoveSink(&sink_c);
}

TEST(RtpDemuxerTest, MultipleRsidsOnSameSink) {
  RtpDemuxer demuxer;

  MockRtpPacketSink sink;
  const std::string rsids[] = {"a", "b", "c"};

  for (const std::string& rsid : rsids) {
    demuxer.AddSink(rsid, &sink);
  }

  InSequence sequence;
  for (size_t i = 0; i < arraysize(rsids); i++) {
    // Assign different SSRCs and sequence numbers to all packets.
    const uint32_t ssrc = 1000 + static_cast<uint32_t>(i);
    const uint16_t sequence_number = 50 + static_cast<uint16_t>(i);
    auto packet =
        CreateRtpPacketReceivedWithRsid(rsids[i], ssrc, sequence_number);
    EXPECT_CALL(sink, OnRtpPacket(SamePacketAs(*packet))).Times(1);
    EXPECT_TRUE(demuxer.OnRtpPacket(*packet));
  }

  // Test tear-down
  demuxer.RemoveSink(&sink);
}

TEST(RtpDemuxerTest, RsidUsedByMultipleSinks) {
  RtpDemuxer demuxer;

  MockRtpPacketSink sinks[3];
  const std::string shared_rsid = "a";

  for (MockRtpPacketSink& sink : sinks) {
    demuxer.AddSink(shared_rsid, &sink);
  }

  constexpr uint32_t shared_ssrc = 888;
  auto packet = CreateRtpPacketReceivedWithRsid(shared_rsid, shared_ssrc);

  for (auto& sink : sinks) {
    EXPECT_CALL(sink, OnRtpPacket(SamePacketAs(*packet))).Times(1);
  }

  EXPECT_TRUE(demuxer.OnRtpPacket(*packet));

  // Test tear-down
  for (MockRtpPacketSink& sink : sinks) {
    demuxer.RemoveSink(&sink);
  }
}

TEST(RtpDemuxerTest, SinkWithBothRsidAndSsrcAssociations) {
  RtpDemuxer demuxer;

  MockRtpPacketSink sink;
  constexpr uint32_t standalone_ssrc = 10101;
  constexpr uint32_t rsid_ssrc = 20202;
  const std::string rsid = "a";

  demuxer.AddSink(standalone_ssrc, &sink);
  demuxer.AddSink(rsid, &sink);

  InSequence sequence;

  auto ssrc_packet = CreateRtpPacketReceived(standalone_ssrc, 11);
  EXPECT_CALL(sink, OnRtpPacket(SamePacketAs(*ssrc_packet))).Times(1);
  EXPECT_TRUE(demuxer.OnRtpPacket(*ssrc_packet));

  auto rsid_packet = CreateRtpPacketReceivedWithRsid(rsid, rsid_ssrc, 22);
  EXPECT_CALL(sink, OnRtpPacket(SamePacketAs(*rsid_packet))).Times(1);
  EXPECT_TRUE(demuxer.OnRtpPacket(*rsid_packet));

  // Test tear-down
  demuxer.RemoveSink(&sink);
}

TEST(RtpDemuxerTest, AssociatingByRsidAndBySsrcCannotTriggerDoubleCall) {
  RtpDemuxer demuxer;
  MockRtpPacketSink sink;

  constexpr uint32_t ssrc = 10101;
  demuxer.AddSink(ssrc, &sink);

  const std::string rsid = "a";
  demuxer.AddSink(rsid, &sink);

  constexpr uint16_t seq_num = 999;
  auto packet = CreateRtpPacketReceivedWithRsid(rsid, ssrc, seq_num);
  EXPECT_CALL(sink, OnRtpPacket(SamePacketAs(*packet))).Times(1);
  EXPECT_TRUE(demuxer.OnRtpPacket(*packet));

  // Test tear-down
  demuxer.RemoveSink(&sink);
}

TEST(RtpDemuxerTest, RsidObserversInformedOfResolutions) {
  RtpDemuxer demuxer;

  constexpr uint32_t ssrc = 111;
  const std::string rsid = "a";

  MockRsidResolutionObserver rsid_resolution_observers[3];
  for (auto& observer : rsid_resolution_observers) {
    demuxer.RegisterRsidResolutionObserver(&observer);
    EXPECT_CALL(observer, OnRsidResolved(rsid, ssrc)).Times(1);
  }

  // The expected calls to OnRsidResolved() will be triggered by this.
  demuxer.OnRtpPacket(*CreateRtpPacketReceivedWithRsid(rsid, ssrc));

  // Test tear-down
  for (auto& observer : rsid_resolution_observers) {
    demuxer.DeregisterRsidResolutionObserver(&observer);
  }
}

TEST(RtpDemuxerTest, DeregisteredRsidObserversNotInformedOfResolutions) {
  RtpDemuxer demuxer;

  constexpr uint32_t ssrc = 111;
  const std::string rsid = "a";
  NiceMock<MockRtpPacketSink> sink;
  demuxer.AddSink(rsid, &sink);

  // Register several, then deregister only one, to show that not all of the
  // observers had been forgotten when one was removed.
  MockRsidResolutionObserver observer_1;
  MockRsidResolutionObserver observer_2_removed;
  MockRsidResolutionObserver observer_3;

  demuxer.RegisterRsidResolutionObserver(&observer_1);
  demuxer.RegisterRsidResolutionObserver(&observer_2_removed);
  demuxer.RegisterRsidResolutionObserver(&observer_3);

  demuxer.DeregisterRsidResolutionObserver(&observer_2_removed);

  EXPECT_CALL(observer_1, OnRsidResolved(rsid, ssrc)).Times(1);
  EXPECT_CALL(observer_2_removed, OnRsidResolved(_, _)).Times(0);
  EXPECT_CALL(observer_3, OnRsidResolved(rsid, ssrc)).Times(1);

  // The expected calls to OnRsidResolved() will be triggered by this.
  demuxer.OnRtpPacket(*CreateRtpPacketReceivedWithRsid(rsid, ssrc));

  // Test tear-down
  demuxer.RemoveSink(&sink);
  demuxer.DeregisterRsidResolutionObserver(&observer_1);
  demuxer.DeregisterRsidResolutionObserver(&observer_3);
}

#if RTC_DCHECK_IS_ON && GTEST_HAS_DEATH_TEST && !defined(WEBRTC_ANDROID)
TEST(RtpDemuxerTest, RsidMustBeNonEmpty) {
  RtpDemuxer demuxer;
  MockRtpPacketSink sink;
  EXPECT_DEATH(demuxer.AddSink("", &sink), "");
}

TEST(RtpDemuxerTest, RsidMustBeAlphaNumeric) {
  RtpDemuxer demuxer;
  MockRtpPacketSink sink;
  EXPECT_DEATH(demuxer.AddSink("a_3", &sink), "");
}

TEST(RtpDemuxerTest, RsidMustNotExceedMaximumLength) {
  RtpDemuxer demuxer;
  MockRtpPacketSink sink;
  std::string rsid(StreamId::kMaxSize + 1, 'a');
  EXPECT_DEATH(demuxer.AddSink(rsid, &sink), "");
}

TEST(RtpDemuxerTest, RepeatedRsidAssociationsDisallowed) {
  RtpDemuxer demuxer;
  MockRtpPacketSink sink;
  demuxer.AddSink("a", &sink);
  EXPECT_DEATH(demuxer.AddSink("a", &sink), "");
  demuxer.RemoveSink(&sink);
}

TEST(RtpDemuxerTest,
     DoubleRegisterationOfNeverRegisteredRsidResolutionObserverDisallowed) {
  RtpDemuxer demuxer;
  MockRsidResolutionObserver observer;
  demuxer.RegisterRsidResolutionObserver(&observer);
  EXPECT_DEATH(demuxer.RegisterRsidResolutionObserver(&observer), "");
  demuxer.DeregisterRsidResolutionObserver(&observer);
}

TEST(RtpDemuxerTest,
     DregisterationOfNeverRegisteredRsidResolutionObserverDisallowed) {
  RtpDemuxer demuxer;
  MockRsidResolutionObserver observer;
  EXPECT_DEATH(demuxer.DeregisterRsidResolutionObserver(&observer), "");
}

#endif

}  // namespace
}  // namespace webrtc
