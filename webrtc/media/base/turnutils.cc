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

#include "webrtc/base/byteorder.h"
#include "webrtc/base/checks.h"
#include "webrtc/p2p/base/stun.h"

namespace cricket {

namespace {

const size_t kTurnChannelHeaderLength = 4;

bool IsTurnChannelData(const uint8_t* data, size_t length) {
  return length >= kTurnChannelHeaderLength && ((*data & 0xC0) == 0x40);
}

bool IsTurnSendIndicationPacket(const uint8_t* data, size_t length) {
  if (length < kStunHeaderSize) {
    return false;
  }

  uint16_t type = rtc::GetBE16(data);
  return (type == TURN_SEND_INDICATION);
}

}  // namespace

bool UnwrapTurnPacket(const uint8_t* packet,
                      size_t packet_size,
                      size_t* content_position,
                      size_t* content_size) {
  if (IsTurnChannelData(packet, packet_size)) {
    // Turn Channel Message header format.
    //   0                   1                   2                   3
    //  0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
    // +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
    // |         Channel Number        |            Length             |
    // +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
    // |                                                               |
    // /                       Application Data                        /
    // /                                                               /
    // +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
    size_t length = rtc::GetBE16(&packet[2]);
    if (length + kTurnChannelHeaderLength > packet_size) {
      return false;
    }

    *content_position = kTurnChannelHeaderLength;
    *content_size = length;
    return true;
  }

  if (IsTurnSendIndicationPacket(packet, packet_size)) {
    // Validate STUN message length.
    const size_t stun_message_length = rtc::GetBE16(&packet[2]);
    if (stun_message_length + kStunHeaderSize != packet_size) {
      return false;
    }

    // First skip mandatory stun header which is of 20 bytes.
    size_t pos = kStunHeaderSize;
    // Loop through STUN attributes until we find STUN DATA attribute.
    while (pos < packet_size) {
      // Keep reading STUN attributes until we hit DATA attribute.
      // Attribute will be a TLV structure.
      // +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
      // |         Type                  |            Length             |
      // +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
      // |                         Value (variable)                ....
      // +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
      // The value in the length field MUST contain the length of the Value
      // part of the attribute, prior to padding, measured in bytes.  Since
      // STUN aligns attributes on 32-bit boundaries, attributes whose content
      // is not a multiple of 4 bytes are padded with 1, 2, or 3 bytes of
      // padding so that its value contains a multiple of 4 bytes.  The
      // padding bits are ignored, and may be any value.
      uint16_t attr_type, attr_length;
      const int kAttrHeaderLength = sizeof(attr_type) + sizeof(attr_length);

      if (packet_size < pos + kAttrHeaderLength) {
        return false;
      }

      // Getting attribute type and length.
      attr_type = rtc::GetBE16(&packet[pos]);
      attr_length = rtc::GetBE16(&packet[pos + sizeof(attr_type)]);

      pos += kAttrHeaderLength;  // Skip STUN_DATA_ATTR header.

      // Checking for bogus attribute length.
      if (pos + attr_length > packet_size) {
        return false;
      }

      if (attr_type == STUN_ATTR_DATA) {
        *content_position = pos;
        *content_size = attr_length;
        return true;
      }

      pos += attr_length;
      if ((attr_length % 4) != 0) {
        pos += (4 - (attr_length % 4));
      }
    }

    // There is no data attribute present in the message.
    return false;
  }

  // This is not a TURN packet.
  *content_position = 0;
  *content_size = packet_size;
  return true;
}

}  // namespace cricket
