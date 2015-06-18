/*
 *  Copyright (c) 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifdef RTC_AUDIOCODING_DEBUG_DUMP

#include <stdio.h>
#include <string>
#include <vector>

#include "testing/gtest/include/gtest/gtest.h"
#include "webrtc/base/scoped_ptr.h"
#include "webrtc/modules/audio_coding/main/acm2/acm_dump.h"
#include "webrtc/system_wrappers/interface/clock.h"
#include "webrtc/test/test_suite.h"
#include "webrtc/test/testsupport/fileutils.h"
#include "webrtc/test/testsupport/gtest_disable.h"

// Files generated at build-time by the protobuf compiler.
#ifdef WEBRTC_ANDROID_PLATFORM_BUILD
#include "external/webrtc/webrtc/modules/audio_coding/dump.pb.h"
#else
#include "webrtc/audio_coding/dump.pb.h"
#endif

namespace webrtc {

// Test for the acm dump class. Dumps some RTP packets to disk, then reads them
// back to see if they match.
class AcmDumpTest : public ::testing::Test {
 public:
  AcmDumpTest() : log_dumper_(AcmDump::Create()) {}
  void VerifyResults(const ACMDumpEventStream& parsed_stream,
                     size_t packet_size) {
    // Verify the result.
    EXPECT_EQ(3, parsed_stream.stream_size());
    const ACMDumpEvent& start_event = parsed_stream.stream(0);
    ASSERT_TRUE(start_event.has_type());
    EXPECT_EQ(ACMDumpEvent::DEBUG_EVENT, start_event.type());
    EXPECT_TRUE(start_event.has_timestamp_us());
    EXPECT_FALSE(start_event.has_packet());
    ASSERT_TRUE(start_event.has_debug_event());
    auto start_debug_event = start_event.debug_event();
    ASSERT_TRUE(start_debug_event.has_type());
    EXPECT_EQ(ACMDumpDebugEvent::LOG_START, start_debug_event.type());
    ASSERT_TRUE(start_debug_event.has_message());

    for (int i = 1; i < parsed_stream.stream_size(); i++) {
      const ACMDumpEvent& test_event = parsed_stream.stream(i);
      ASSERT_TRUE(test_event.has_type());
      EXPECT_EQ(ACMDumpEvent::RTP_EVENT, test_event.type());
      EXPECT_TRUE(test_event.has_timestamp_us());
      EXPECT_FALSE(test_event.has_debug_event());
      ASSERT_TRUE(test_event.has_packet());
      const ACMDumpRTPPacket& test_packet = test_event.packet();
      ASSERT_TRUE(test_packet.has_direction());
      if (i == 1) {
        EXPECT_EQ(ACMDumpRTPPacket::INCOMING, test_packet.direction());
      } else if (i == 2) {
        EXPECT_EQ(ACMDumpRTPPacket::OUTGOING, test_packet.direction());
      }
      ASSERT_TRUE(test_packet.has_rtp_data());
      ASSERT_EQ(packet_size, test_packet.rtp_data().size());
      for (size_t i = 0; i < packet_size; i++) {
        EXPECT_EQ(rtp_packet_[i],
                  static_cast<uint8_t>(test_packet.rtp_data()[i]));
      }
    }
  }

  void Run(int packet_size, int random_seed) {
    rtp_packet_.clear();
    rtp_packet_.reserve(packet_size);
    srand(random_seed);
    // Fill the packet vector with random data.
    for (int i = 0; i < packet_size; i++) {
      rtp_packet_.push_back(rand());
    }
    // Find the name of the current test, in order to use it as a temporary
    // filename.
    auto test_info = ::testing::UnitTest::GetInstance()->current_test_info();
    const std::string temp_filename =
        test::OutputPath() + test_info->test_case_name() + test_info->name();

    log_dumper_->StartLogging(temp_filename, 10000000);
    log_dumper_->LogRtpPacket(true, rtp_packet_.data(), rtp_packet_.size());
    log_dumper_->LogRtpPacket(false, rtp_packet_.data(), rtp_packet_.size());

    // Read the generated file from disk.
    ACMDumpEventStream parsed_stream;

    ASSERT_EQ(true, AcmDump::ParseAcmDump(temp_filename, &parsed_stream));

    VerifyResults(parsed_stream, packet_size);

    // Clean up temporary file - can be pretty slow.
    remove(temp_filename.c_str());
  }

  std::vector<uint8_t> rtp_packet_;
  rtc::scoped_ptr<AcmDump> log_dumper_;
};

TEST_F(AcmDumpTest, DumpAndRead) {
  Run(256, 321);
  Run(256, 123);
}

}  // namespace webrtc

#endif  // RTC_AUDIOCODING_DEBUG_DUMP
