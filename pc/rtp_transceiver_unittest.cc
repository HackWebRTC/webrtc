/*
 *  Copyright 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

// This file contains tests for |RtpTransceiver|.

#include "pc/rtp_transceiver.h"

#include <memory>

#include "media/base/fake_media_engine.h"
#include "pc/test/mock_channel_interface.h"
#include "pc/test/mock_rtp_receiver_internal.h"
#include "pc/test/mock_rtp_sender_internal.h"
#include "test/gmock.h"
#include "test/gtest.h"

using ::testing::ElementsAre;
using ::testing::Eq;
using ::testing::Field;
using ::testing::Not;
using ::testing::Return;
using ::testing::ReturnRef;

namespace webrtc {

// Checks that a channel cannot be set on a stopped |RtpTransceiver|.
TEST(RtpTransceiverTest, CannotSetChannelOnStoppedTransceiver) {
  RtpTransceiver transceiver(cricket::MediaType::MEDIA_TYPE_AUDIO);
  cricket::MockChannelInterface channel1;
  sigslot::signal1<cricket::ChannelInterface*> signal;
  EXPECT_CALL(channel1, media_type())
      .WillRepeatedly(Return(cricket::MediaType::MEDIA_TYPE_AUDIO));
  EXPECT_CALL(channel1, SignalFirstPacketReceived())
      .WillRepeatedly(ReturnRef(signal));

  transceiver.SetChannel(&channel1);
  EXPECT_EQ(&channel1, transceiver.channel());

  // Stop the transceiver.
  transceiver.Stop();
  EXPECT_EQ(&channel1, transceiver.channel());

  cricket::MockChannelInterface channel2;
  EXPECT_CALL(channel2, media_type())
      .WillRepeatedly(Return(cricket::MediaType::MEDIA_TYPE_AUDIO));

  // Channel can no longer be set, so this call should be a no-op.
  transceiver.SetChannel(&channel2);
  EXPECT_EQ(&channel1, transceiver.channel());
}

// Checks that a channel can be unset on a stopped |RtpTransceiver|
TEST(RtpTransceiverTest, CanUnsetChannelOnStoppedTransceiver) {
  RtpTransceiver transceiver(cricket::MediaType::MEDIA_TYPE_VIDEO);
  cricket::MockChannelInterface channel;
  sigslot::signal1<cricket::ChannelInterface*> signal;
  EXPECT_CALL(channel, media_type())
      .WillRepeatedly(Return(cricket::MediaType::MEDIA_TYPE_VIDEO));
  EXPECT_CALL(channel, SignalFirstPacketReceived())
      .WillRepeatedly(ReturnRef(signal));

  transceiver.SetChannel(&channel);
  EXPECT_EQ(&channel, transceiver.channel());

  // Stop the transceiver.
  transceiver.Stop();
  EXPECT_EQ(&channel, transceiver.channel());

  // Set the channel to |nullptr|.
  transceiver.SetChannel(nullptr);
  EXPECT_EQ(nullptr, transceiver.channel());
}

TEST(RtpTransceiverTest,
     InitsWithChannelManagerRtpHeaderExtensionCapabilities) {
  cricket::ChannelManager channel_manager(
      std::make_unique<cricket::FakeMediaEngine>(),
      std::make_unique<cricket::FakeDataEngine>(), rtc::Thread::Current(),
      rtc::Thread::Current());
  std::vector<RtpHeaderExtensionCapability> extensions({
      RtpHeaderExtensionCapability("uri1", 1,
                                   RtpTransceiverDirection::kSendRecv),
      RtpHeaderExtensionCapability("uri2", 2,
                                   RtpTransceiverDirection::kRecvOnly),
  });
  RtpTransceiver transceiver(
      RtpSenderProxyWithInternal<RtpSenderInternal>::Create(
          rtc::Thread::Current(),
          new rtc::RefCountedObject<MockRtpSenderInternal>()),
      RtpReceiverProxyWithInternal<RtpReceiverInternal>::Create(
          rtc::Thread::Current(),
          new rtc::RefCountedObject<MockRtpReceiverInternal>()),
      &channel_manager, extensions);
  EXPECT_EQ(transceiver.HeaderExtensionsToOffer(), extensions);
}

}  // namespace webrtc
