/*
 *  Copyright (c) 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "testing/gtest/include/gtest/gtest.h"
#include "webrtc/modules/audio_coding/main/acm2/rent_a_codec.h"

namespace webrtc {
namespace acm2 {

TEST(RentACodecTest, RentEncoderError) {
  const CodecInst codec_inst = {
      0, "Robert'); DROP TABLE Students;", 8000, 160, 1, 64000};
  RentACodec rent_a_codec;
  EXPECT_FALSE(rent_a_codec.RentEncoder(codec_inst));
}

}  // namespace acm2
}  // namespace webrtc
