/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

/*
 * This file includes unit tests for NetEQ.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>  // memset

#include <vector>

#include "gtest/gtest.h"

#include "modules/audio_coding/neteq/test/NETEQTEST_CodecClass.h"
#include "modules/audio_coding/neteq/test/NETEQTEST_NetEQClass.h"
#include "modules/audio_coding/neteq/test/NETEQTEST_RTPpacket.h"
#include "typedefs.h"  // NOLINT(build/include)
#include "modules/audio_coding/neteq/interface/webrtc_neteq.h"
#include "modules/audio_coding/neteq/interface/webrtc_neteq_help_macros.h"

namespace {

class NetEqDecodingTest : public ::testing::Test {
 protected:
  NetEqDecodingTest();
  virtual void SetUp();
  virtual void TearDown();
  void SelectDecoders(WebRtcNetEQDecoder* used_codec);
  void LoadDecoders();
  void DecodeAndCompare(const char* rtp_file, const char* ref_file);

  NETEQTEST_NetEQClass* neteq_inst_;
  std::vector<NETEQTEST_Decoder*> dec_;
};

NetEqDecodingTest::NetEqDecodingTest() : neteq_inst_(NULL) {}

void NetEqDecodingTest::SetUp() {
  WebRtcNetEQDecoder usedCodec[kDecoderReservedEnd - 1];

  SelectDecoders(usedCodec);
  neteq_inst_ = new NETEQTEST_NetEQClass(usedCodec, dec_.size(), 8000,
                                         kTCPLargeJitter);
  ASSERT_TRUE(neteq_inst_);
  LoadDecoders();
}

void NetEqDecodingTest::TearDown() {
  if (neteq_inst_)
    delete neteq_inst_;
  for (size_t i = 0; i < dec_.size(); ++i) {
    if (dec_[i])
      delete dec_[i];
  }
}

void NetEqDecodingTest::SelectDecoders(WebRtcNetEQDecoder* used_codec) {
  *used_codec++ = kDecoderPCMu;
  dec_.push_back(new decoder_PCMU(0));
  *used_codec++ = kDecoderPCMa;
  dec_.push_back(new decoder_PCMA(8));
  *used_codec++ = kDecoderILBC;
  dec_.push_back(new decoder_ILBC(102));
  *used_codec++ = kDecoderISAC;
  dec_.push_back(new decoder_iSAC(103));
  *used_codec++ = kDecoderISACswb;
  dec_.push_back(new decoder_iSACSWB(104));
  *used_codec++ = kDecoderPCM16B;
  dec_.push_back(new decoder_PCM16B_NB(93));
  *used_codec++ = kDecoderPCM16Bwb;
  dec_.push_back(new decoder_PCM16B_WB(94));
  *used_codec++ = kDecoderPCM16Bswb32kHz;
  dec_.push_back(new decoder_PCM16B_SWB32(95));
  *used_codec++ = kDecoderCNG;
  dec_.push_back(new decoder_CNG(13));
}

void NetEqDecodingTest::LoadDecoders() {
  for (size_t i = 0; i < dec_.size(); ++i) {
    ASSERT_EQ(0, dec_[i]->loadToNetEQ(*neteq_inst_));
  }
}

void NetEqDecodingTest::DecodeAndCompare(const char* rtp_file,
                                         const char* ref_file) {
  NETEQTEST_RTPpacket rtp;
  FILE* rtp_fp = fopen(rtp_file, "rb");
  ASSERT_TRUE(rtp_fp != NULL);
  ASSERT_EQ(0, NETEQTEST_RTPpacket::skipFileHeader(rtp_fp));
  ASSERT_GT(rtp.readFromFile(rtp_fp), 0);

  FILE* ref_fp = fopen(ref_file, "rb");
  ASSERT_TRUE(ref_fp != NULL);

  unsigned int sim_clock = 0;
  const int kTimeStep = 10;
  while (rtp.dataLen() >= 0) {
    // Check if time to receive.
    while ((sim_clock >= rtp.time()) &&
           (rtp.dataLen() >= 0)) {
      if (rtp.dataLen() > 0) {
        ASSERT_EQ(0, neteq_inst_->recIn(rtp));
      }
      // Get next packet.
      ASSERT_NE(-1, rtp.readFromFile(rtp_fp));
    }

    // RecOut
    WebRtc_Word16 out_data[10 * 32];  // 10 ms at 32 kHz
    WebRtc_Word16 out_len = neteq_inst_->recOut(out_data);
    ASSERT_TRUE((out_len == 80) || (out_len == 160) || (out_len == 320));

    // Read from ref file
    WebRtc_Word16 ref_data[10 * 32];  // 10 ms at 32 kHz
    if (static_cast<size_t>(out_len) !=
        fread(ref_data, sizeof(WebRtc_Word16), out_len, ref_fp)) {
      break;
    }

    // Compare
    EXPECT_EQ(0, memcmp(out_data, ref_data, sizeof(WebRtc_Word16) * out_len));

    // Increase time
    sim_clock += kTimeStep;
  }
  ASSERT_NE(0, feof(ref_fp));  // Make sure that we reached the end.
  fclose(rtp_fp);
  fclose(ref_fp);
}

TEST_F(NetEqDecodingTest, TestBitExactness) {
  DecodeAndCompare("test/data/audio_coding/universal.rtp",
                   "test/data/audio_coding/universal_ref.pcm");
}

}  // namespace

int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);

  return RUN_ALL_TESTS();
}

