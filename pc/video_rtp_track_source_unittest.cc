/*
 *  Copyright (c) 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "pc/video_rtp_track_source.h"

#include "rtc_base/ref_counted_object.h"
#include "test/gmock.h"
#include "test/gtest.h"

namespace webrtc {
namespace {

TEST(VideoRtpTrackSourceTest, CreatesWithRemoteAtttributeSet) {
  rtc::scoped_refptr<VideoRtpTrackSource> source(
      new rtc::RefCountedObject<VideoRtpTrackSource>());
  EXPECT_TRUE(source->remote());
}

}  // namespace
}  // namespace webrtc
