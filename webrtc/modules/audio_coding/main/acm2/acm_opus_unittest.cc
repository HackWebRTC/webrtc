/*
 *  Copyright (c) 2014 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/modules/audio_coding/main/acm2/acm_opus.h"

#include "gtest/gtest.h"
#include "webrtc/modules/audio_coding/main/acm2/acm_codec_database.h"

namespace webrtc {

namespace acm2 {

namespace {
  const CodecInst kOpusCodecInst = {105, "opus", 48000, 960, 1, 32000};
  // These constants correspond to those used in ACMOpus::SetPacketLossRate().
  const int kPacketLossRateHigh = 20;
  const int kPacketLossRateMedium = 10;
  const int kPacketLossRateLow = 1;
  const int kLossRateHighMargin = 2;
  const int kLossRateMediumMargin = 1;
}  // namespace

class AcmOpusTest : public ACMOpus {
 public:
  explicit AcmOpusTest(int16_t codec_id)
      : ACMOpus(codec_id) {}
  ~AcmOpusTest() {}
  int packet_loss_rate() { return packet_loss_rate_; }

  void TestSetPacketLossRate(int from, int to, int expected_return);
};

#ifdef WEBRTC_CODEC_OPUS
void AcmOpusTest::TestSetPacketLossRate(int from, int to, int expected_return) {
  for (int loss = from; loss <= to; (to >= from) ? ++loss : --loss) {
    EXPECT_EQ(0, SetPacketLossRate(loss));
    EXPECT_EQ(expected_return, packet_loss_rate());
  }
}

TEST(AcmOpusTest, PacketLossRateOptimized) {
  AcmOpusTest opus(ACMCodecDB::kOpus);
  WebRtcACMCodecParams params;
  memcpy(&(params.codec_inst), &kOpusCodecInst, sizeof(CodecInst));
  EXPECT_EQ(0, opus.InitEncoder(&params, true));
  EXPECT_EQ(0, opus.SetFEC(true));

  // Note that the order of the following calls is critical.
  opus.TestSetPacketLossRate(0, 0, 0);
  opus.TestSetPacketLossRate(kPacketLossRateLow,
                             kPacketLossRateMedium + kLossRateMediumMargin - 1,
                             kPacketLossRateLow);
  opus.TestSetPacketLossRate(kPacketLossRateMedium + kLossRateMediumMargin,
                             kPacketLossRateHigh + kLossRateHighMargin - 1,
                             kPacketLossRateMedium);
  opus.TestSetPacketLossRate(kPacketLossRateHigh + kLossRateHighMargin,
                             100,
                             kPacketLossRateHigh);
  opus.TestSetPacketLossRate(kPacketLossRateHigh + kLossRateHighMargin,
                             kPacketLossRateHigh - kLossRateHighMargin,
                             kPacketLossRateHigh);
  opus.TestSetPacketLossRate(kPacketLossRateHigh - kLossRateHighMargin - 1,
                             kPacketLossRateMedium - kLossRateMediumMargin,
                             kPacketLossRateMedium);
  opus.TestSetPacketLossRate(kPacketLossRateMedium - kLossRateMediumMargin - 1,
                             kPacketLossRateLow,
                             kPacketLossRateLow);
  opus.TestSetPacketLossRate(0, 0, 0);
}
#else
void AcmOpusTest:TestSetPacketLossRate(int /* from */, int /* to */,
                                       int /* expected_return */) {
  return;
}
#endif  // WEBRTC_CODEC_OPUS

}  // namespace acm2

}  // namespace webrtc
