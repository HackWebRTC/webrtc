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
#include <set>
#include <string>

#include "webrtc/call/ssrc_binding_observer.h"
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

class MockSsrcBindingObserver : public SsrcBindingObserver {
 public:
  MOCK_METHOD2(OnSsrcBoundToRsid, void(const std::string& rsid, uint32_t ssrc));
};

class RtpDemuxerTest : public testing::Test {
 protected:
  ~RtpDemuxerTest() {
    for (auto* sink : sinks_to_tear_down_) {
      demuxer_.RemoveSink(sink);
    }
    for (auto* observer : observers_to_tear_down_) {
      demuxer_.DeregisterSsrcBindingObserver(observer);
    }
  }

  bool AddSinkOnlySsrc(uint32_t ssrc, RtpPacketSinkInterface* sink) {
    bool added = demuxer_.AddSink(ssrc, sink);
    if (added) {
      sinks_to_tear_down_.insert(sink);
    }
    return added;
  }

  void AddSinkOnlyRsid(const std::string& rsid, RtpPacketSinkInterface* sink) {
    demuxer_.AddSink(rsid, sink);
    sinks_to_tear_down_.insert(sink);
  }

  bool RemoveSink(RtpPacketSinkInterface* sink) {
    sinks_to_tear_down_.erase(sink);
    return demuxer_.RemoveSink(sink);
  }

  void RegisterSsrcBindingObserver(SsrcBindingObserver* observer) {
    demuxer_.RegisterSsrcBindingObserver(observer);
    observers_to_tear_down_.insert(observer);
  }

  void DeregisterSsrcBindingObserver(SsrcBindingObserver* observer) {
    demuxer_.DeregisterSsrcBindingObserver(observer);
    observers_to_tear_down_.erase(observer);
  }

  // The CreatePacket* methods are helpers for creating new RTP packets with
  // various attributes set. Tests should use the helper that provides the
  // minimum information needed to exercise the behavior under test. Tests also
  // should not rely on any behavior which is not clearly described in the
  // helper name/arguments. Any additional settings that are not covered by the
  // helper should be set manually on the packet once it has been returned.
  // For example, most tests in this file do not care about the RTP sequence
  // number, but to ensure that the returned packets are valid the helpers will
  // auto-increment the sequence number starting with 1. Tests that rely on
  // specific sequence number behavior should call SetSequenceNumber manually on
  // the returned packet.

  // Intended for use only by other CreatePacket* helpers.
  std::unique_ptr<RtpPacketReceived> CreatePacket(
      uint32_t ssrc,
      RtpPacketReceived::ExtensionManager* extension_manager) {
    auto packet = rtc::MakeUnique<RtpPacketReceived>(extension_manager);
    packet->SetSsrc(ssrc);
    packet->SetSequenceNumber(next_sequence_number_++);
    return packet;
  }

  std::unique_ptr<RtpPacketReceived> CreatePacketWithSsrc(uint32_t ssrc) {
    return CreatePacket(ssrc, nullptr);
  }

  std::unique_ptr<RtpPacketReceived> CreatePacketWithSsrcRsid(
      uint32_t ssrc,
      const std::string& rsid) {
    RtpPacketReceived::ExtensionManager extension_manager;
    extension_manager.Register<RtpStreamId>(6);

    auto packet = CreatePacket(ssrc, &extension_manager);
    packet->SetExtension<RtpStreamId>(rsid);
    return packet;
  }

  RtpDemuxer demuxer_;
  std::set<RtpPacketSinkInterface*> sinks_to_tear_down_;
  std::set<SsrcBindingObserver*> observers_to_tear_down_;
  uint16_t next_sequence_number_ = 1;
};

MATCHER_P(SamePacketAs, other, "") {
  return arg.Ssrc() == other.Ssrc() &&
         arg.SequenceNumber() == other.SequenceNumber();
}

TEST_F(RtpDemuxerTest, CanAddSinkBySsrc) {
  MockRtpPacketSink sink;
  constexpr uint32_t ssrc = 1;

  EXPECT_TRUE(AddSinkOnlySsrc(ssrc, &sink));
}

TEST_F(RtpDemuxerTest, OnRtpPacketCalledOnCorrectSinkBySsrc) {
  constexpr uint32_t ssrcs[] = {101, 202, 303};
  MockRtpPacketSink sinks[arraysize(ssrcs)];
  for (size_t i = 0; i < arraysize(ssrcs); i++) {
    AddSinkOnlySsrc(ssrcs[i], &sinks[i]);
  }

  for (size_t i = 0; i < arraysize(ssrcs); i++) {
    auto packet = CreatePacketWithSsrc(ssrcs[i]);
    EXPECT_CALL(sinks[i], OnRtpPacket(SamePacketAs(*packet))).Times(1);
    EXPECT_TRUE(demuxer_.OnRtpPacket(*packet));
  }
}

TEST_F(RtpDemuxerTest, OnRtpPacketCalledOnCorrectSinkByRsid) {
  const std::string rsids[] = {"a", "b", "c"};
  MockRtpPacketSink sinks[arraysize(rsids)];
  for (size_t i = 0; i < arraysize(rsids); i++) {
    AddSinkOnlyRsid(rsids[i], &sinks[i]);
  }

  for (size_t i = 0; i < arraysize(rsids); i++) {
    auto packet = CreatePacketWithSsrcRsid(i, rsids[i]);
    EXPECT_CALL(sinks[i], OnRtpPacket(SamePacketAs(*packet))).Times(1);
    EXPECT_TRUE(demuxer_.OnRtpPacket(*packet));
  }
}

TEST_F(RtpDemuxerTest, PacketsDeliveredInRightOrder) {
  constexpr uint32_t ssrc = 101;
  MockRtpPacketSink sink;
  AddSinkOnlySsrc(ssrc, &sink);

  std::unique_ptr<RtpPacketReceived> packets[5];
  for (size_t i = 0; i < arraysize(packets); i++) {
    packets[i] = CreatePacketWithSsrc(ssrc);
    packets[i]->SetSequenceNumber(i);
  }

  InSequence sequence;
  for (const auto& packet : packets) {
    EXPECT_CALL(sink, OnRtpPacket(SamePacketAs(*packet))).Times(1);
  }

  for (const auto& packet : packets) {
    EXPECT_TRUE(demuxer_.OnRtpPacket(*packet));
  }
}

TEST_F(RtpDemuxerTest, SinkMappedToMultipleSsrcs) {
  constexpr uint32_t ssrcs[] = {404, 505, 606};
  MockRtpPacketSink sink;
  for (uint32_t ssrc : ssrcs) {
    AddSinkOnlySsrc(ssrc, &sink);
  }

  // The sink which is associated with multiple SSRCs gets the callback
  // triggered for each of those SSRCs.
  for (uint32_t ssrc : ssrcs) {
    auto packet = CreatePacketWithSsrc(ssrc);
    EXPECT_CALL(sink, OnRtpPacket(SamePacketAs(*packet)));
    EXPECT_TRUE(demuxer_.OnRtpPacket(*packet));
  }
}

TEST_F(RtpDemuxerTest, NoCallbackOnSsrcSinkRemovedBeforeFirstPacket) {
  constexpr uint32_t ssrc = 404;
  MockRtpPacketSink sink;
  AddSinkOnlySsrc(ssrc, &sink);

  ASSERT_TRUE(RemoveSink(&sink));

  // The removed sink does not get callbacks.
  auto packet = CreatePacketWithSsrc(ssrc);
  EXPECT_CALL(sink, OnRtpPacket(_)).Times(0);  // Not called.
  EXPECT_FALSE(demuxer_.OnRtpPacket(*packet));
}

TEST_F(RtpDemuxerTest, NoCallbackOnSsrcSinkRemovedAfterFirstPacket) {
  constexpr uint32_t ssrc = 404;
  NiceMock<MockRtpPacketSink> sink;
  AddSinkOnlySsrc(ssrc, &sink);

  InSequence sequence;
  for (size_t i = 0; i < 10; i++) {
    ASSERT_TRUE(demuxer_.OnRtpPacket(*CreatePacketWithSsrc(ssrc)));
  }

  ASSERT_TRUE(RemoveSink(&sink));

  // The removed sink does not get callbacks.
  auto packet = CreatePacketWithSsrc(ssrc);
  EXPECT_CALL(sink, OnRtpPacket(_)).Times(0);  // Not called.
  EXPECT_FALSE(demuxer_.OnRtpPacket(*packet));
}

TEST_F(RtpDemuxerTest, AddSinkFailsIfCalledForTwoSinks) {
  MockRtpPacketSink sink_a;
  MockRtpPacketSink sink_b;
  constexpr uint32_t ssrc = 1;
  ASSERT_TRUE(AddSinkOnlySsrc(ssrc, &sink_a));

  EXPECT_FALSE(AddSinkOnlySsrc(ssrc, &sink_b));
}

// An SSRC may only be mapped to a single sink. However, since configuration
// of this associations might come from the network, we need to fail gracefully.
TEST_F(RtpDemuxerTest, OnlyOneSinkPerSsrcGetsOnRtpPacketTriggered) {
  MockRtpPacketSink sinks[3];
  constexpr uint32_t ssrc = 404;
  ASSERT_TRUE(AddSinkOnlySsrc(ssrc, &sinks[0]));
  ASSERT_FALSE(AddSinkOnlySsrc(ssrc, &sinks[1]));
  ASSERT_FALSE(AddSinkOnlySsrc(ssrc, &sinks[2]));

  // The first sink associated with the SSRC remains active; other sinks
  // were not really added, and so do not get OnRtpPacket() called.
  auto packet = CreatePacketWithSsrc(ssrc);
  EXPECT_CALL(sinks[0], OnRtpPacket(SamePacketAs(*packet))).Times(1);
  EXPECT_CALL(sinks[1], OnRtpPacket(_)).Times(0);
  EXPECT_CALL(sinks[2], OnRtpPacket(_)).Times(0);
  ASSERT_TRUE(demuxer_.OnRtpPacket(*packet));
}

TEST_F(RtpDemuxerTest, AddSinkFailsIfCalledTwiceEvenIfSameSink) {
  MockRtpPacketSink sink;
  constexpr uint32_t ssrc = 1;
  ASSERT_TRUE(AddSinkOnlySsrc(ssrc, &sink));

  EXPECT_FALSE(AddSinkOnlySsrc(ssrc, &sink));
}

TEST_F(RtpDemuxerTest, NoRepeatedCallbackOnRepeatedAddSinkForSameSink) {
  constexpr uint32_t ssrc = 111;
  MockRtpPacketSink sink;

  ASSERT_TRUE(AddSinkOnlySsrc(ssrc, &sink));
  ASSERT_FALSE(AddSinkOnlySsrc(ssrc, &sink));

  auto packet = CreatePacketWithSsrc(ssrc);
  EXPECT_CALL(sink, OnRtpPacket(SamePacketAs(*packet))).Times(1);
  EXPECT_TRUE(demuxer_.OnRtpPacket(*packet));
}

TEST_F(RtpDemuxerTest, RemoveSinkReturnsFalseForNeverAddedSink) {
  MockRtpPacketSink sink;
  EXPECT_FALSE(RemoveSink(&sink));
}

TEST_F(RtpDemuxerTest, RemoveSinkReturnsTrueForPreviouslyAddedSsrcSink) {
  constexpr uint32_t ssrc = 101;
  MockRtpPacketSink sink;
  AddSinkOnlySsrc(ssrc, &sink);

  EXPECT_TRUE(RemoveSink(&sink));
}

TEST_F(RtpDemuxerTest,
       RemoveSinkReturnsTrueForUnresolvedPreviouslyAddedRsidSink) {
  const std::string rsid = "a";
  MockRtpPacketSink sink;
  AddSinkOnlyRsid(rsid, &sink);

  EXPECT_TRUE(RemoveSink(&sink));
}

TEST_F(RtpDemuxerTest,
       RemoveSinkReturnsTrueForResolvedPreviouslyAddedRsidSink) {
  const std::string rsid = "a";
  constexpr uint32_t ssrc = 101;
  NiceMock<MockRtpPacketSink> sink;
  AddSinkOnlyRsid(rsid, &sink);
  ASSERT_TRUE(demuxer_.OnRtpPacket(*CreatePacketWithSsrcRsid(ssrc, rsid)));

  EXPECT_TRUE(RemoveSink(&sink));
}

TEST_F(RtpDemuxerTest, OnRtpPacketCalledForRsidSink) {
  MockRtpPacketSink sink;
  const std::string rsid = "a";
  AddSinkOnlyRsid(rsid, &sink);

  // Create a sequence of RTP packets, where only the first one actually
  // mentions the RSID.
  std::unique_ptr<RtpPacketReceived> packets[5];
  constexpr uint32_t rsid_ssrc = 111;
  packets[0] = CreatePacketWithSsrcRsid(rsid_ssrc, rsid);
  for (size_t i = 1; i < arraysize(packets); i++) {
    packets[i] = CreatePacketWithSsrc(rsid_ssrc);
  }

  // The first packet associates the RSID with the SSRC, thereby allowing the
  // demuxer to correctly demux all of the packets.
  InSequence sequence;
  for (const auto& packet : packets) {
    EXPECT_CALL(sink, OnRtpPacket(SamePacketAs(*packet))).Times(1);
  }
  for (const auto& packet : packets) {
    EXPECT_TRUE(demuxer_.OnRtpPacket(*packet));
  }
}

TEST_F(RtpDemuxerTest, NoCallbackOnRsidSinkRemovedBeforeFirstPacket) {
  MockRtpPacketSink sink;
  const std::string rsid = "a";
  AddSinkOnlyRsid(rsid, &sink);

  // Sink removed - it won't get triggers even if packets with its RSID arrive.
  ASSERT_TRUE(RemoveSink(&sink));

  constexpr uint32_t ssrc = 111;
  auto packet = CreatePacketWithSsrcRsid(ssrc, rsid);
  EXPECT_CALL(sink, OnRtpPacket(_)).Times(0);  // Not called.
  EXPECT_FALSE(demuxer_.OnRtpPacket(*packet));
}

TEST_F(RtpDemuxerTest, NoCallbackOnRsidSinkRemovedAfterFirstPacket) {
  NiceMock<MockRtpPacketSink> sink;
  const std::string rsid = "a";
  AddSinkOnlyRsid(rsid, &sink);

  InSequence sequence;
  constexpr uint32_t ssrc = 111;
  for (size_t i = 0; i < 10; i++) {
    auto packet = CreatePacketWithSsrcRsid(ssrc, rsid);
    ASSERT_TRUE(demuxer_.OnRtpPacket(*packet));
  }

  // Sink removed - it won't get triggers even if packets with its RSID arrive.
  ASSERT_TRUE(RemoveSink(&sink));

  auto packet = CreatePacketWithSsrcRsid(ssrc, rsid);
  EXPECT_CALL(sink, OnRtpPacket(_)).Times(0);  // Not called.
  EXPECT_FALSE(demuxer_.OnRtpPacket(*packet));
}

// The RSID to SSRC mapping should be one-to-one. If we end up receiving
// two (or more) packets with the same SSRC, but different RSIDs, we guarantee
// remembering the first one; no guarantees are made about further associations.
TEST_F(RtpDemuxerTest, FirstSsrcAssociatedWithAnRsidIsNotForgotten) {
  // Each sink has a distinct RSID.
  MockRtpPacketSink sink_a;
  const std::string rsid_a = "a";
  AddSinkOnlyRsid(rsid_a, &sink_a);

  MockRtpPacketSink sink_b;
  const std::string rsid_b = "b";
  AddSinkOnlyRsid(rsid_b, &sink_b);

  InSequence sequence;  // Verify that the order of delivery is unchanged.

  constexpr uint32_t shared_ssrc = 100;

  // First a packet with |rsid_a| is received, and |sink_a| is associated with
  // its SSRC.
  auto packet_a = CreatePacketWithSsrcRsid(shared_ssrc, rsid_a);
  EXPECT_CALL(sink_a, OnRtpPacket(SamePacketAs(*packet_a))).Times(1);
  EXPECT_TRUE(demuxer_.OnRtpPacket(*packet_a));

  // Second, a packet with |rsid_b| is received. We guarantee that |sink_a|
  // would receive it, and make no guarantees about |sink_b|.
  auto packet_b = CreatePacketWithSsrcRsid(shared_ssrc, rsid_b);
  EXPECT_CALL(sink_a, OnRtpPacket(SamePacketAs(*packet_b))).Times(1);
  EXPECT_CALL(sink_b, OnRtpPacket(SamePacketAs(*packet_b))).Times(AtLeast(0));
  EXPECT_TRUE(demuxer_.OnRtpPacket(*packet_b));

  // Known edge-case; adding a new RSID association makes us re-examine all
  // SSRCs. |sink_b| may or may not be associated with the SSRC now; we make
  // no promises on that. We do however still guarantee that |sink_a| still
  // receives the new packets.
  MockRtpPacketSink sink_c;
  const std::string rsid_c = "c";
  constexpr uint32_t some_other_ssrc = shared_ssrc + 1;
  AddSinkOnlySsrc(some_other_ssrc, &sink_c);
  auto packet_c = CreatePacketWithSsrcRsid(shared_ssrc, rsid_c);
  EXPECT_CALL(sink_a, OnRtpPacket(SamePacketAs(*packet_c))).Times(1);
  EXPECT_CALL(sink_b, OnRtpPacket(SamePacketAs(*packet_c))).Times(AtLeast(0));
  EXPECT_TRUE(demuxer_.OnRtpPacket(*packet_c));
}

TEST_F(RtpDemuxerTest, MultipleRsidsOnSameSink) {
  MockRtpPacketSink sink;
  const std::string rsids[] = {"a", "b", "c"};

  for (const std::string& rsid : rsids) {
    AddSinkOnlyRsid(rsid, &sink);
  }

  InSequence sequence;
  for (size_t i = 0; i < arraysize(rsids); i++) {
    // Assign different SSRCs and sequence numbers to all packets.
    const uint32_t ssrc = 1000 + static_cast<uint32_t>(i);
    auto packet = CreatePacketWithSsrcRsid(ssrc, rsids[i]);
    EXPECT_CALL(sink, OnRtpPacket(SamePacketAs(*packet))).Times(1);
    EXPECT_TRUE(demuxer_.OnRtpPacket(*packet));
  }
}

TEST_F(RtpDemuxerTest, SinkWithBothRsidAndSsrcAssociations) {
  constexpr uint32_t standalone_ssrc = 10101;
  constexpr uint32_t rsid_ssrc = 20202;
  const std::string rsid = "a";

  MockRtpPacketSink sink;
  AddSinkOnlySsrc(standalone_ssrc, &sink);
  AddSinkOnlyRsid(rsid, &sink);

  InSequence sequence;

  auto ssrc_packet = CreatePacketWithSsrc(standalone_ssrc);
  EXPECT_CALL(sink, OnRtpPacket(SamePacketAs(*ssrc_packet))).Times(1);
  EXPECT_TRUE(demuxer_.OnRtpPacket(*ssrc_packet));

  auto rsid_packet = CreatePacketWithSsrcRsid(rsid_ssrc, rsid);
  EXPECT_CALL(sink, OnRtpPacket(SamePacketAs(*rsid_packet))).Times(1);
  EXPECT_TRUE(demuxer_.OnRtpPacket(*rsid_packet));
}

TEST_F(RtpDemuxerTest, AssociatingByRsidAndBySsrcCannotTriggerDoubleCall) {
  constexpr uint32_t ssrc = 10101;
  const std::string rsid = "a";

  MockRtpPacketSink sink;
  AddSinkOnlySsrc(ssrc, &sink);
  AddSinkOnlyRsid(rsid, &sink);

  auto packet = CreatePacketWithSsrcRsid(ssrc, rsid);
  EXPECT_CALL(sink, OnRtpPacket(SamePacketAs(*packet))).Times(1);
  EXPECT_TRUE(demuxer_.OnRtpPacket(*packet));
}

TEST_F(RtpDemuxerTest, RsidObserversInformedOfResolutionsOfTrackedRsids) {
  constexpr uint32_t ssrc = 111;
  const std::string rsid = "a";

  // Only RSIDs which the demuxer knows may be resolved.
  NiceMock<MockRtpPacketSink> sink;
  AddSinkOnlyRsid(rsid, &sink);

  MockSsrcBindingObserver rsid_resolution_observers[3];
  for (auto& observer : rsid_resolution_observers) {
    RegisterSsrcBindingObserver(&observer);
    EXPECT_CALL(observer, OnSsrcBoundToRsid(rsid, ssrc)).Times(1);
  }

  // The expected calls to OnSsrcBoundToRsid() will be triggered by this.
  demuxer_.OnRtpPacket(*CreatePacketWithSsrcRsid(ssrc, rsid));
}

TEST_F(RtpDemuxerTest, RsidObserversNotInformedOfResolutionsOfUntrackedRsids) {
  constexpr uint32_t ssrc = 111;
  const std::string rsid = "a";

  MockSsrcBindingObserver rsid_resolution_observers[3];
  for (auto& observer : rsid_resolution_observers) {
    RegisterSsrcBindingObserver(&observer);
    EXPECT_CALL(observer, OnSsrcBoundToRsid(rsid, ssrc)).Times(0);
  }

  // The expected calls to OnRsidResolved() will be triggered by this.
  demuxer_.OnRtpPacket(*CreatePacketWithSsrcRsid(ssrc, rsid));
}

// If one sink is associated with SSRC x, and another sink with RSID y, we
// should never observe RSID x being resolved to SSRC x, or else we'd end
// up with one SSRC mapped to two sinks. However, if such faulty input
// ever reaches us, we should handle it gracefully - not crash, and keep the
// packets routed only to the SSRC sink.
TEST_F(RtpDemuxerTest,
       PacketFittingBothRsidSinkAndSsrcSinkGivenOnlyToSsrcSink) {
  constexpr uint32_t ssrc = 111;
  MockRtpPacketSink ssrc_sink;
  AddSinkOnlySsrc(ssrc, &ssrc_sink);

  const std::string rsid = "a";
  MockRtpPacketSink rsid_sink;
  AddSinkOnlyRsid(rsid, &rsid_sink);

  auto packet = CreatePacketWithSsrcRsid(ssrc, rsid);
  EXPECT_CALL(ssrc_sink, OnRtpPacket(SamePacketAs(*packet))).Times(1);
  EXPECT_CALL(rsid_sink, OnRtpPacket(SamePacketAs(*packet))).Times(0);
  demuxer_.OnRtpPacket(*packet);
}

TEST_F(RtpDemuxerTest,
       PacketFittingBothRsidSinkAndSsrcSinkDoesNotTriggerResolutionCallbacks) {
  constexpr uint32_t ssrc = 111;
  NiceMock<MockRtpPacketSink> ssrc_sink;
  AddSinkOnlySsrc(ssrc, &ssrc_sink);

  const std::string rsid = "a";
  NiceMock<MockRtpPacketSink> rsid_sink;
  AddSinkOnlyRsid(rsid, &rsid_sink);

  MockSsrcBindingObserver observer;
  RegisterSsrcBindingObserver(&observer);

  auto packet = CreatePacketWithSsrcRsid(ssrc, rsid);
  EXPECT_CALL(observer, OnSsrcBoundToRsid(_, _)).Times(0);
  demuxer_.OnRtpPacket(*packet);
}

// We're not expecting RSIDs to be resolved to SSRCs which were previously
// mapped to sinks, and make no guarantees except for graceful handling.
TEST_F(RtpDemuxerTest,
       GracefullyHandleRsidBeingMappedToPrevouslyAssociatedSsrc) {
  constexpr uint32_t ssrc = 111;
  NiceMock<MockRtpPacketSink> ssrc_sink;
  AddSinkOnlySsrc(ssrc, &ssrc_sink);

  const std::string rsid = "a";
  MockRtpPacketSink rsid_sink;
  AddSinkOnlyRsid(rsid, &rsid_sink);

  MockSsrcBindingObserver observer;
  RegisterSsrcBindingObserver(&observer);

  // The SSRC was mapped to an SSRC sink, but was even active (packets flowed
  // over it).
  auto packet = CreatePacketWithSsrcRsid(ssrc, rsid);
  demuxer_.OnRtpPacket(*packet);

  // If the SSRC sink is ever removed, the RSID sink *might* receive indications
  // of packets, and observers *might* be informed. Only graceful handling
  // is guaranteed.
  RemoveSink(&ssrc_sink);
  EXPECT_CALL(rsid_sink, OnRtpPacket(SamePacketAs(*packet))).Times(AtLeast(0));
  EXPECT_CALL(observer, OnSsrcBoundToRsid(rsid, ssrc)).Times(AtLeast(0));
  demuxer_.OnRtpPacket(*packet);
}

TEST_F(RtpDemuxerTest, DeregisteredRsidObserversNotInformedOfResolutions) {
  constexpr uint32_t ssrc = 111;
  const std::string rsid = "a";
  NiceMock<MockRtpPacketSink> sink;
  AddSinkOnlyRsid(rsid, &sink);

  // Register several, then deregister only one, to show that not all of the
  // observers had been forgotten when one was removed.
  MockSsrcBindingObserver observer_1;
  MockSsrcBindingObserver observer_2_removed;
  MockSsrcBindingObserver observer_3;

  RegisterSsrcBindingObserver(&observer_1);
  RegisterSsrcBindingObserver(&observer_2_removed);
  RegisterSsrcBindingObserver(&observer_3);

  DeregisterSsrcBindingObserver(&observer_2_removed);

  EXPECT_CALL(observer_1, OnSsrcBoundToRsid(rsid, ssrc)).Times(1);
  EXPECT_CALL(observer_2_removed, OnSsrcBoundToRsid(_, _)).Times(0);
  EXPECT_CALL(observer_3, OnSsrcBoundToRsid(rsid, ssrc)).Times(1);

  // The expected calls to OnSsrcBoundToRsid() will be triggered by this.
  demuxer_.OnRtpPacket(*CreatePacketWithSsrcRsid(ssrc, rsid));
}

#if RTC_DCHECK_IS_ON && GTEST_HAS_DEATH_TEST && !defined(WEBRTC_ANDROID)
TEST_F(RtpDemuxerTest, RsidMustBeNonEmpty) {
  MockRtpPacketSink sink;
  EXPECT_DEATH(AddSinkOnlyRsid("", &sink), "");
}

TEST_F(RtpDemuxerTest, RsidMustBeAlphaNumeric) {
  MockRtpPacketSink sink;
  EXPECT_DEATH(AddSinkOnlyRsid("a_3", &sink), "");
}

TEST_F(RtpDemuxerTest, RsidMustNotExceedMaximumLength) {
  MockRtpPacketSink sink;
  std::string rsid(StreamId::kMaxSize + 1, 'a');
  EXPECT_DEATH(AddSinkOnlyRsid(rsid, &sink), "");
}

TEST_F(RtpDemuxerTest, RepeatedRsidAssociationsDisallowed) {
  MockRtpPacketSink sink_a;
  MockRtpPacketSink sink_b;

  const std::string rsid = "a";
  AddSinkOnlyRsid(rsid, &sink_a);

  EXPECT_DEATH(AddSinkOnlyRsid(rsid, &sink_b), "");
}

TEST_F(RtpDemuxerTest, RepeatedRsidAssociationsDisallowedEvenIfSameSink) {
  const std::string rsid = "a";
  MockRtpPacketSink sink;
  AddSinkOnlyRsid(rsid, &sink);

  EXPECT_DEATH(AddSinkOnlyRsid(rsid, &sink), "");
}

TEST_F(RtpDemuxerTest, DoubleRegisterationOfRsidResolutionObserverDisallowed) {
  MockSsrcBindingObserver observer;
  RegisterSsrcBindingObserver(&observer);
  EXPECT_DEATH(RegisterSsrcBindingObserver(&observer), "");
}

TEST_F(RtpDemuxerTest,
       DregisterationOfNeverRegisteredRsidResolutionObserverDisallowed) {
  MockSsrcBindingObserver observer;
  EXPECT_DEATH(DeregisterSsrcBindingObserver(&observer), "");
}

#endif

}  // namespace
}  // namespace webrtc
