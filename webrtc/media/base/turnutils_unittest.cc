/*
 * libjingle
 * Copyright 2016 Google Inc.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 *  1. Redistributions of source code must retain the above copyright notice,
 *     this list of conditions and the following disclaimer.
 *  2. Redistributions in binary form must reproduce the above copyright notice,
 *     this list of conditions and the following disclaimer in the documentation
 *     and/or other materials provided with the distribution.
 *  3. The name of the author may not be used to endorse or promote products
 *     derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO
 * EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "webrtc/media/base/turnutils.h"

#include <stddef.h>

#include "webrtc/base/gunit.h"

namespace cricket {

// Invalid TURN send indication messages. Messages are proper STUN
// messages with incorrect values in attributes.
TEST(TurnUtilsTest, InvalidTurnSendIndicationMessages) {
  size_t content_pos = SIZE_MAX;
  size_t content_size = SIZE_MAX;

  // Stun Indication message with Zero length
  uint8_t kTurnSendIndicationMsgWithNoAttributes[] = {
      0x00, 0x16, 0x00, 0x00,  // Zero length
      0x21, 0x12, 0xA4, 0x42,  // magic cookie
      '0',  '1',  '2',  '3',   // transaction id
      '4',  '5',  '6',  '7',  '8', '9', 'a', 'b',
  };
  EXPECT_FALSE(UnwrapTurnPacket(kTurnSendIndicationMsgWithNoAttributes,
                                sizeof(kTurnSendIndicationMsgWithNoAttributes),
                                &content_pos, &content_size));
  EXPECT_EQ(SIZE_MAX, content_pos);
  EXPECT_EQ(SIZE_MAX, content_size);

  // Stun Send Indication message with invalid length in stun header.
  const uint8_t kTurnSendIndicationMsgWithInvalidLength[] = {
      0x00, 0x16, 0xFF, 0x00,  // length of 0xFF00
      0x21, 0x12, 0xA4, 0x42,  // magic cookie
      '0',  '1',  '2',  '3',   // transaction id
      '4',  '5',  '6',  '7',  '8', '9', 'a', 'b',
  };
  EXPECT_FALSE(UnwrapTurnPacket(kTurnSendIndicationMsgWithInvalidLength,
                                sizeof(kTurnSendIndicationMsgWithInvalidLength),
                                &content_pos, &content_size));
  EXPECT_EQ(SIZE_MAX, content_pos);
  EXPECT_EQ(SIZE_MAX, content_size);

  // Stun Send Indication message with no DATA attribute in message.
  const uint8_t kTurnSendIndicatinMsgWithNoDataAttribute[] = {
      0x00, 0x16, 0x00, 0x08,  // length of
      0x21, 0x12, 0xA4, 0x42,  // magic cookie
      '0',  '1',  '2',  '3',   // transaction id
      '4',  '5',  '6',  '7',  '8',  '9', 'a',  'b',
      0x00, 0x20, 0x00, 0x04,  // Mapped address.
      0x00, 0x00, 0x00, 0x00,
  };
  EXPECT_FALSE(
      UnwrapTurnPacket(kTurnSendIndicatinMsgWithNoDataAttribute,
                       sizeof(kTurnSendIndicatinMsgWithNoDataAttribute),
                       &content_pos, &content_size));
  EXPECT_EQ(SIZE_MAX, content_pos);
  EXPECT_EQ(SIZE_MAX, content_size);
}

// Valid TURN Send Indication messages.
TEST(TurnUtilsTest, ValidTurnSendIndicationMessage) {
  size_t content_pos = SIZE_MAX;
  size_t content_size = SIZE_MAX;
  // A valid STUN indication message with a valid RTP header in data attribute
  // payload field and no extension bit set.
  const uint8_t kTurnSendIndicationMsgWithoutRtpExtension[] = {
      0x00, 0x16, 0x00, 0x18,  // length of
      0x21, 0x12, 0xA4, 0x42,  // magic cookie
      '0',  '1',  '2',  '3',   // transaction id
      '4',  '5',  '6',  '7',  '8',  '9',  'a',  'b',
      0x00, 0x20, 0x00, 0x04,  // Mapped address.
      0x00, 0x00, 0x00, 0x00,
      0x00, 0x13, 0x00, 0x0C,  // Data attribute.
      0x80, 0x00, 0x00, 0x00,  // RTP packet.
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  };
  EXPECT_TRUE(UnwrapTurnPacket(
      kTurnSendIndicationMsgWithoutRtpExtension,
      sizeof(kTurnSendIndicationMsgWithoutRtpExtension), &content_pos,
      &content_size));
  EXPECT_EQ(12U, content_size);
  EXPECT_EQ(32U, content_pos);
}

// Verify that parsing of valid TURN Channel Messages.
TEST(TurnUtilsTest, ValidTurnChannelMessages) {
  const uint8_t kTurnChannelMsgWithRtpPacket[] = {
      0x40, 0x00, 0x00, 0x0C,
      0x80, 0x00, 0x00, 0x00,  // RTP packet.
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  };

  size_t content_pos = 0, content_size = 0;
  EXPECT_TRUE(UnwrapTurnPacket(
      kTurnChannelMsgWithRtpPacket,
      sizeof(kTurnChannelMsgWithRtpPacket), &content_pos, &content_size));
  EXPECT_EQ(12U, content_size);
  EXPECT_EQ(4U, content_pos);
}

TEST(TurnUtilsTest, ChannelMessageZeroLength) {
  const uint8_t kTurnChannelMsgWithZeroLength[] = {0x40, 0x00, 0x00, 0x00};
  size_t content_pos = SIZE_MAX;
  size_t content_size = SIZE_MAX;
  EXPECT_TRUE(UnwrapTurnPacket(kTurnChannelMsgWithZeroLength,
                               sizeof(kTurnChannelMsgWithZeroLength),
                               &content_pos, &content_size));
  EXPECT_EQ(4, content_pos);
  EXPECT_EQ(0, content_size);
}

}  // namespace cricket
