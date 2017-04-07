/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <memory>

#include "webrtc/common_types.h"
#include "webrtc/modules/rtp_rtcp/include/rtp_header_parser.h"
#include "webrtc/modules/rtp_rtcp/include/rtp_payload_registry.h"
#include "webrtc/modules/rtp_rtcp/include/rtp_receiver.h"
#include "webrtc/modules/rtp_rtcp/include/rtp_rtcp_defines.h"
#include "webrtc/modules/rtp_rtcp/source/rtp_receiver_impl.h"
#include "webrtc/test/gtest.h"

namespace webrtc {

const uint32_t kTestRate = 64000u;
const uint8_t kTestPayload[] = {'t', 'e', 's', 't'};
const uint8_t kPcmuPayloadType = 96;
const int64_t kGetSourcesTimeoutMs = 10000;
const int kSourceListsSize = 20;

class RtpReceiverTest : public ::testing::Test {
 protected:
  RtpReceiverTest()
      : fake_clock_(123456),
        rtp_receiver_(
            RtpReceiver::CreateAudioReceiver(&fake_clock_,
                                             nullptr,
                                             nullptr,
                                             &rtp_payload_registry_)) {
    CodecInst voice_codec = {};
    voice_codec.pltype = kPcmuPayloadType;
    voice_codec.plfreq = 8000;
    voice_codec.rate = kTestRate;
    memcpy(voice_codec.plname, "PCMU", 5);
    rtp_receiver_->RegisterReceivePayload(voice_codec);
  }
  ~RtpReceiverTest() {}

  bool FindSourceByIdAndType(const std::vector<RtpSource>& sources,
                             uint32_t source_id,
                             RtpSourceType type,
                             RtpSource* source) {
    for (size_t i = 0; i < sources.size(); ++i) {
      if (sources[i].source_id() == source_id &&
          sources[i].source_type() == type) {
        (*source) = sources[i];
        return true;
      }
    }
    return false;
  }

  SimulatedClock fake_clock_;
  RTPPayloadRegistry rtp_payload_registry_;
  std::unique_ptr<RtpReceiver> rtp_receiver_;
};

TEST_F(RtpReceiverTest, GetSources) {
  RTPHeader header;
  header.payloadType = kPcmuPayloadType;
  header.ssrc = 1;
  header.timestamp = fake_clock_.TimeInMilliseconds();
  header.numCSRCs = 2;
  header.arrOfCSRCs[0] = 111;
  header.arrOfCSRCs[1] = 222;
  PayloadUnion payload_specific = {AudioPayload()};
  bool in_order = false;
  RtpSource source(0, 0, RtpSourceType::SSRC);

  EXPECT_TRUE(rtp_receiver_->IncomingRtpPacket(header, kTestPayload, 4,
                                               payload_specific, in_order));
  auto sources = rtp_receiver_->GetSources();
  // One SSRC source and two CSRC sources.
  ASSERT_EQ(3u, sources.size());
  ASSERT_TRUE(FindSourceByIdAndType(sources, 1u, RtpSourceType::SSRC, &source));
  EXPECT_EQ(fake_clock_.TimeInMilliseconds(), source.timestamp_ms());
  ASSERT_TRUE(
      FindSourceByIdAndType(sources, 222u, RtpSourceType::CSRC, &source));
  EXPECT_EQ(fake_clock_.TimeInMilliseconds(), source.timestamp_ms());
  ASSERT_TRUE(
      FindSourceByIdAndType(sources, 111u, RtpSourceType::CSRC, &source));
  EXPECT_EQ(fake_clock_.TimeInMilliseconds(), source.timestamp_ms());

  // Advance the fake clock and the method is expected to return the
  // contributing source object with same source id and updated timestamp.
  fake_clock_.AdvanceTimeMilliseconds(1);
  EXPECT_TRUE(rtp_receiver_->IncomingRtpPacket(header, kTestPayload, 4,
                                               payload_specific, in_order));
  sources = rtp_receiver_->GetSources();
  ASSERT_EQ(3u, sources.size());
  ASSERT_TRUE(FindSourceByIdAndType(sources, 1u, RtpSourceType::SSRC, &source));
  EXPECT_EQ(fake_clock_.TimeInMilliseconds(), source.timestamp_ms());
  ASSERT_TRUE(
      FindSourceByIdAndType(sources, 222u, RtpSourceType::CSRC, &source));
  EXPECT_EQ(fake_clock_.TimeInMilliseconds(), source.timestamp_ms());
  ASSERT_TRUE(
      FindSourceByIdAndType(sources, 111u, RtpSourceType::CSRC, &source));
  EXPECT_EQ(fake_clock_.TimeInMilliseconds(), source.timestamp_ms());

  // Test the edge case that the sources are still there just before the
  // timeout.
  int64_t prev_timestamp = fake_clock_.TimeInMilliseconds();
  fake_clock_.AdvanceTimeMilliseconds(kGetSourcesTimeoutMs);
  sources = rtp_receiver_->GetSources();
  ASSERT_EQ(3u, sources.size());
  ASSERT_TRUE(FindSourceByIdAndType(sources, 1u, RtpSourceType::SSRC, &source));
  EXPECT_EQ(prev_timestamp, source.timestamp_ms());
  ASSERT_TRUE(
      FindSourceByIdAndType(sources, 222u, RtpSourceType::CSRC, &source));
  EXPECT_EQ(prev_timestamp, source.timestamp_ms());
  ASSERT_TRUE(
      FindSourceByIdAndType(sources, 111u, RtpSourceType::CSRC, &source));
  EXPECT_EQ(prev_timestamp, source.timestamp_ms());

  // Time out.
  fake_clock_.AdvanceTimeMilliseconds(1);
  sources = rtp_receiver_->GetSources();
  // All the sources should be out of date.
  ASSERT_EQ(0u, sources.size());
}

// Test the case that the SSRC is changed.
TEST_F(RtpReceiverTest, GetSourcesChangeSSRC) {
  int64_t prev_time = -1;
  int64_t cur_time = fake_clock_.TimeInMilliseconds();
  RTPHeader header;
  header.payloadType = kPcmuPayloadType;
  header.ssrc = 1;
  header.timestamp = cur_time;
  PayloadUnion payload_specific = {AudioPayload()};
  bool in_order = false;
  RtpSource source(0, 0, RtpSourceType::SSRC);

  EXPECT_TRUE(rtp_receiver_->IncomingRtpPacket(header, kTestPayload, 4,
                                               payload_specific, in_order));
  auto sources = rtp_receiver_->GetSources();
  ASSERT_EQ(1u, sources.size());
  EXPECT_EQ(1u, sources[0].source_id());
  EXPECT_EQ(cur_time, sources[0].timestamp_ms());

  // The SSRC is changed and the old SSRC is expected to be returned.
  fake_clock_.AdvanceTimeMilliseconds(100);
  prev_time = cur_time;
  cur_time = fake_clock_.TimeInMilliseconds();
  header.ssrc = 2;
  header.timestamp = cur_time;
  EXPECT_TRUE(rtp_receiver_->IncomingRtpPacket(header, kTestPayload, 4,
                                               payload_specific, in_order));
  sources = rtp_receiver_->GetSources();
  ASSERT_EQ(2u, sources.size());
  ASSERT_TRUE(FindSourceByIdAndType(sources, 2u, RtpSourceType::SSRC, &source));
  EXPECT_EQ(cur_time, source.timestamp_ms());
  ASSERT_TRUE(FindSourceByIdAndType(sources, 1u, RtpSourceType::SSRC, &source));
  EXPECT_EQ(prev_time, source.timestamp_ms());

  // The SSRC is changed again and happen to be changed back to 1. No
  // duplication is expected.
  fake_clock_.AdvanceTimeMilliseconds(100);
  header.ssrc = 1;
  header.timestamp = cur_time;
  prev_time = cur_time;
  cur_time = fake_clock_.TimeInMilliseconds();
  EXPECT_TRUE(rtp_receiver_->IncomingRtpPacket(header, kTestPayload, 4,
                                               payload_specific, in_order));
  sources = rtp_receiver_->GetSources();
  ASSERT_EQ(2u, sources.size());
  ASSERT_TRUE(FindSourceByIdAndType(sources, 1u, RtpSourceType::SSRC, &source));
  EXPECT_EQ(cur_time, source.timestamp_ms());
  ASSERT_TRUE(FindSourceByIdAndType(sources, 2u, RtpSourceType::SSRC, &source));
  EXPECT_EQ(prev_time, source.timestamp_ms());

  // Old SSRC source timeout.
  fake_clock_.AdvanceTimeMilliseconds(kGetSourcesTimeoutMs);
  cur_time = fake_clock_.TimeInMilliseconds();
  EXPECT_TRUE(rtp_receiver_->IncomingRtpPacket(header, kTestPayload, 4,
                                               payload_specific, in_order));
  sources = rtp_receiver_->GetSources();
  ASSERT_EQ(1u, sources.size());
  EXPECT_EQ(1u, sources[0].source_id());
  EXPECT_EQ(cur_time, sources[0].timestamp_ms());
  EXPECT_EQ(RtpSourceType::SSRC, sources[0].source_type());
}

TEST_F(RtpReceiverTest, GetSourcesRemoveOutdatedSource) {
  int64_t timestamp = fake_clock_.TimeInMilliseconds();
  bool in_order = false;
  RTPHeader header;
  header.payloadType = kPcmuPayloadType;
  header.timestamp = timestamp;
  PayloadUnion payload_specific = {AudioPayload()};
  header.numCSRCs = 1;
  RtpSource source(0, 0, RtpSourceType::SSRC);

  for (size_t i = 0; i < kSourceListsSize; ++i) {
    header.ssrc = i;
    header.arrOfCSRCs[0] = (i + 1);
    EXPECT_TRUE(rtp_receiver_->IncomingRtpPacket(header, kTestPayload, 4,
                                                 payload_specific, in_order));
  }

  auto sources = rtp_receiver_->GetSources();
  // Expect |kSourceListsSize| SSRC sources and |kSourceListsSize| CSRC sources.
  ASSERT_TRUE(sources.size() == 2 * kSourceListsSize);
  for (size_t i = 0; i < kSourceListsSize; ++i) {
    // The SSRC source IDs are expected to be 19, 18, 17 ... 0
    ASSERT_TRUE(
        FindSourceByIdAndType(sources, i, RtpSourceType::SSRC, &source));
    EXPECT_EQ(timestamp, source.timestamp_ms());

    // The CSRC source IDs are expected to be 20, 19, 18 ... 1
    ASSERT_TRUE(
        FindSourceByIdAndType(sources, (i + 1), RtpSourceType::CSRC, &source));
    EXPECT_EQ(timestamp, source.timestamp_ms());
  }

  fake_clock_.AdvanceTimeMilliseconds(kGetSourcesTimeoutMs);
  for (size_t i = 0; i < kSourceListsSize; ++i) {
    // The SSRC source IDs are expected to be 19, 18, 17 ... 0
    ASSERT_TRUE(
        FindSourceByIdAndType(sources, i, RtpSourceType::SSRC, &source));
    EXPECT_EQ(timestamp, source.timestamp_ms());

    // The CSRC source IDs are expected to be 20, 19, 18 ... 1
    ASSERT_TRUE(
        FindSourceByIdAndType(sources, (i + 1), RtpSourceType::CSRC, &source));
    EXPECT_EQ(timestamp, source.timestamp_ms());
  }

  // Timeout. All the existing objects are out of date and are expected to be
  // removed.
  fake_clock_.AdvanceTimeMilliseconds(1);
  header.ssrc = 111;
  header.arrOfCSRCs[0] = 222;
  EXPECT_TRUE(rtp_receiver_->IncomingRtpPacket(header, kTestPayload, 4,
                                               payload_specific, in_order));
  auto rtp_receiver_impl = static_cast<RtpReceiverImpl*>(rtp_receiver_.get());
  auto ssrc_sources = rtp_receiver_impl->ssrc_sources_for_testing();
  ASSERT_EQ(1u, ssrc_sources.size());
  EXPECT_EQ(111u, ssrc_sources.begin()->source_id());
  EXPECT_EQ(RtpSourceType::SSRC, ssrc_sources.begin()->source_type());
  EXPECT_EQ(fake_clock_.TimeInMilliseconds(),
            ssrc_sources.begin()->timestamp_ms());

  auto csrc_sources = rtp_receiver_impl->csrc_sources_for_testing();
  ASSERT_EQ(1u, csrc_sources.size());
  EXPECT_EQ(222u, csrc_sources.begin()->source_id());
  EXPECT_EQ(RtpSourceType::CSRC, csrc_sources.begin()->source_type());
  EXPECT_EQ(fake_clock_.TimeInMilliseconds(),
            csrc_sources.begin()->timestamp_ms());
}

}  // namespace webrtc
