/*
 *  Copyright (c) 2020 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "call/adaptation/video_source_restrictions.h"

#include "test/gtest.h"

namespace webrtc {

namespace {

VideoSourceRestrictions RestrictionsFromMaxPixelsPerFrame(
    size_t max_pixels_per_frame) {
  return VideoSourceRestrictions(max_pixels_per_frame, absl::nullopt,
                                 absl::nullopt);
}

VideoSourceRestrictions RestrictionsFromMaxFrameRate(double max_frame_rate) {
  return VideoSourceRestrictions(absl::nullopt, absl::nullopt, max_frame_rate);
}

}  // namespace

TEST(VideoSourceRestrictionsTest, DidIncreaseResolution) {
  // smaller restrictions -> larger restrictions
  EXPECT_TRUE(DidIncreaseResolution(RestrictionsFromMaxPixelsPerFrame(10),
                                    RestrictionsFromMaxPixelsPerFrame(11)));
  // unrestricted -> restricted
  EXPECT_FALSE(DidIncreaseResolution(VideoSourceRestrictions(),
                                     RestrictionsFromMaxPixelsPerFrame(10)));
  // restricted -> unrestricted
  EXPECT_TRUE(DidIncreaseResolution(RestrictionsFromMaxPixelsPerFrame(10),
                                    VideoSourceRestrictions()));
  // restricted -> equally restricted
  EXPECT_FALSE(DidIncreaseResolution(RestrictionsFromMaxPixelsPerFrame(10),
                                     RestrictionsFromMaxPixelsPerFrame(10)));
  // unrestricted -> unrestricted
  EXPECT_FALSE(DidIncreaseResolution(VideoSourceRestrictions(),
                                     VideoSourceRestrictions()));
  // larger restrictions -> smaller restrictions
  EXPECT_FALSE(DidIncreaseResolution(RestrictionsFromMaxPixelsPerFrame(10),
                                     RestrictionsFromMaxPixelsPerFrame(9)));
}

TEST(VideoSourceRestrictionsTest, DidDecreaseFrameRate) {
  // samller restrictions -> larger restrictions
  EXPECT_FALSE(DidDecreaseFrameRate(RestrictionsFromMaxFrameRate(10),
                                    RestrictionsFromMaxFrameRate(11)));
  // unrestricted -> restricted
  EXPECT_TRUE(DidDecreaseFrameRate(VideoSourceRestrictions(),
                                   RestrictionsFromMaxFrameRate(10)));
  // restricted -> unrestricted
  EXPECT_FALSE(DidDecreaseFrameRate(RestrictionsFromMaxFrameRate(10),
                                    VideoSourceRestrictions()));
  // restricted -> equally restricted
  EXPECT_FALSE(DidDecreaseFrameRate(RestrictionsFromMaxFrameRate(10),
                                    RestrictionsFromMaxFrameRate(10)));
  // unrestricted -> unrestricted
  EXPECT_FALSE(DidDecreaseFrameRate(VideoSourceRestrictions(),
                                    VideoSourceRestrictions()));
  // larger restrictions -> samller restrictions
  EXPECT_TRUE(DidDecreaseFrameRate(RestrictionsFromMaxFrameRate(10),
                                   RestrictionsFromMaxFrameRate(9)));
}

}  // namespace webrtc
