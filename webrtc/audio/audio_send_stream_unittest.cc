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

#include "webrtc/audio/audio_send_stream.h"

namespace webrtc {

TEST(AudioSendStreamTest, ConfigToString) {
  const int kAbsSendTimeId = 3;
  AudioSendStream::Config config(nullptr);
  config.rtp.ssrc = 1234;
  config.rtp.extensions.push_back(
      RtpExtension(RtpExtension::kAbsSendTime, kAbsSendTimeId));
  config.voe_channel_id = 1;
  config.cng_payload_type = 42;
  config.red_payload_type = 17;
  EXPECT_GT(config.ToString().size(), 0u);
}

TEST(AudioSendStreamTest, ConstructDestruct) {
  AudioSendStream::Config config(nullptr);
  config.voe_channel_id = 1;
  internal::AudioSendStream send_stream(config);
}
}  // namespace webrtc
