/*
 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/common_video/h264/profile_level_id.h"

#include "webrtc/test/gtest.h"

namespace webrtc {
namespace H264 {

TEST(H264ProfileLevelIdParsing, TestInvalid) {
  // Malformed strings.
  EXPECT_FALSE(ParseProfileLevelId(""));
  EXPECT_FALSE(ParseProfileLevelId(" 42e01f"));
  EXPECT_FALSE(ParseProfileLevelId("4242e01f"));
  EXPECT_FALSE(ParseProfileLevelId("e01f"));
  EXPECT_FALSE(ParseProfileLevelId("gggggg"));

  // Invalid level.
  EXPECT_FALSE(ParseProfileLevelId("42e000"));
  EXPECT_FALSE(ParseProfileLevelId("42e00f"));
  EXPECT_FALSE(ParseProfileLevelId("42e0ff"));

  // Invalid profile.
  EXPECT_FALSE(ParseProfileLevelId("42e11f"));
  EXPECT_FALSE(ParseProfileLevelId("58601f"));
  EXPECT_FALSE(ParseProfileLevelId("64e01f"));
}

TEST(H264ProfileLevelIdParsing, TestLevel) {
  EXPECT_EQ(kLevel3_1, ParseProfileLevelId("42e01f")->level);
  EXPECT_EQ(kLevel1_1, ParseProfileLevelId("42e00b")->level);
  EXPECT_EQ(kLevel1_b, ParseProfileLevelId("42f00b")->level);
  EXPECT_EQ(kLevel4_2, ParseProfileLevelId("42C02A")->level);
  EXPECT_EQ(kLevel5_2, ParseProfileLevelId("640c34")->level);
}

TEST(H264ProfileLevelIdParsing, TestConstrainedBaseline) {
  EXPECT_EQ(kProfileConstrainedBaseline,
            ParseProfileLevelId("42e01f")->profile);
  EXPECT_EQ(kProfileConstrainedBaseline,
            ParseProfileLevelId("42C02A")->profile);
  EXPECT_EQ(kProfileConstrainedBaseline,
            ParseProfileLevelId("4de01f")->profile);
  EXPECT_EQ(kProfileConstrainedBaseline,
            ParseProfileLevelId("58f01f")->profile);
}

TEST(H264ProfileLevelIdParsing, TestBaseline) {
  EXPECT_EQ(kProfileBaseline, ParseProfileLevelId("42a01f")->profile);
  EXPECT_EQ(kProfileBaseline, ParseProfileLevelId("58A01F")->profile);
}

TEST(H264ProfileLevelIdParsing, TestMain) {
  EXPECT_EQ(kProfileMain, ParseProfileLevelId("4D401f")->profile);
}

TEST(H264ProfileLevelIdParsing, TestHigh) {
  EXPECT_EQ(kProfileHigh, ParseProfileLevelId("64001f")->profile);
}

TEST(H264ProfileLevelIdParsing, TestConstrainedHigh) {
  EXPECT_EQ(kProfileConstrainedHigh, ParseProfileLevelId("640c1f")->profile);
}

}  // namespace H264
}  // namespace webrtc
