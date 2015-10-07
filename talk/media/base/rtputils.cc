/*
 * libjingle
 * Copyright 2011 Google Inc.
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

#include "talk/media/base/rtputils.h"

namespace cricket {

static const uint8_t kRtpVersion = 2;
static const size_t kRtpFlagsOffset = 0;
static const size_t kRtpPayloadTypeOffset = 1;
static const size_t kRtpSeqNumOffset = 2;
static const size_t kRtpTimestampOffset = 4;
static const size_t kRtpSsrcOffset = 8;
static const size_t kRtcpPayloadTypeOffset = 1;

bool GetUint8(const void* data, size_t offset, int* value) {
  if (!data || !value) {
    return false;
  }
  *value = *(static_cast<const uint8_t*>(data) + offset);
  return true;
}

bool GetUint16(const void* data, size_t offset, int* value) {
  if (!data || !value) {
    return false;
  }
  *value = static_cast<int>(
      rtc::GetBE16(static_cast<const uint8_t*>(data) + offset));
  return true;
}

bool GetUint32(const void* data, size_t offset, uint32_t* value) {
  if (!data || !value) {
    return false;
  }
  *value = rtc::GetBE32(static_cast<const uint8_t*>(data) + offset);
  return true;
}

bool SetUint8(void* data, size_t offset, uint8_t value) {
  if (!data) {
    return false;
  }
  rtc::Set8(data, offset, value);
  return true;
}

bool SetUint16(void* data, size_t offset, uint16_t value) {
  if (!data) {
    return false;
  }
  rtc::SetBE16(static_cast<uint8_t*>(data) + offset, value);
  return true;
}

bool SetUint32(void* data, size_t offset, uint32_t value) {
  if (!data) {
    return false;
  }
  rtc::SetBE32(static_cast<uint8_t*>(data) + offset, value);
  return true;
}

bool GetRtpFlags(const void* data, size_t len, int* value) {
  if (len < kMinRtpPacketLen) {
    return false;
  }
  return GetUint8(data, kRtpFlagsOffset, value);
}

bool GetRtpPayloadType(const void* data, size_t len, int* value) {
  if (len < kMinRtpPacketLen) {
    return false;
  }
  if (!GetUint8(data, kRtpPayloadTypeOffset, value)) {
    return false;
  }
  *value &= 0x7F;
  return true;
}

bool GetRtpSeqNum(const void* data, size_t len, int* value) {
  if (len < kMinRtpPacketLen) {
    return false;
  }
  return GetUint16(data, kRtpSeqNumOffset, value);
}

bool GetRtpTimestamp(const void* data, size_t len, uint32_t* value) {
  if (len < kMinRtpPacketLen) {
    return false;
  }
  return GetUint32(data, kRtpTimestampOffset, value);
}

bool GetRtpSsrc(const void* data, size_t len, uint32_t* value) {
  if (len < kMinRtpPacketLen) {
    return false;
  }
  return GetUint32(data, kRtpSsrcOffset, value);
}

bool GetRtpHeaderLen(const void* data, size_t len, size_t* value) {
  if (!data || len < kMinRtpPacketLen || !value) return false;
  const uint8_t* header = static_cast<const uint8_t*>(data);
  // Get base header size + length of CSRCs (not counting extension yet).
  size_t header_size = kMinRtpPacketLen + (header[0] & 0xF) * sizeof(uint32_t);
  if (len < header_size) return false;
  // If there's an extension, read and add in the extension size.
  if (header[0] & 0x10) {
    if (len < header_size + sizeof(uint32_t))
      return false;
    header_size +=
        ((rtc::GetBE16(header + header_size + 2) + 1) * sizeof(uint32_t));
    if (len < header_size) return false;
  }
  *value = header_size;
  return true;
}

bool GetRtpHeader(const void* data, size_t len, RtpHeader* header) {
  return (GetRtpPayloadType(data, len, &(header->payload_type)) &&
          GetRtpSeqNum(data, len, &(header->seq_num)) &&
          GetRtpTimestamp(data, len, &(header->timestamp)) &&
          GetRtpSsrc(data, len, &(header->ssrc)));
}

bool GetRtcpType(const void* data, size_t len, int* value) {
  if (len < kMinRtcpPacketLen) {
    return false;
  }
  return GetUint8(data, kRtcpPayloadTypeOffset, value);
}

// This method returns SSRC first of RTCP packet, except if packet is SDES.
// TODO(mallinath) - Fully implement RFC 5506. This standard doesn't restrict
// to send non-compound packets only to feedback messages.
bool GetRtcpSsrc(const void* data, size_t len, uint32_t* value) {
  // Packet should be at least of 8 bytes, to get SSRC from a RTCP packet.
  if (!data || len < kMinRtcpPacketLen + 4 || !value) return false;
  int pl_type;
  if (!GetRtcpType(data, len, &pl_type)) return false;
  // SDES packet parsing is not supported.
  if (pl_type == kRtcpTypeSDES) return false;
  *value = rtc::GetBE32(static_cast<const uint8_t*>(data) + 4);
  return true;
}

bool SetRtpSsrc(void* data, size_t len, uint32_t value) {
  return SetUint32(data, kRtpSsrcOffset, value);
}

// Assumes version 2, no padding, no extensions, no csrcs.
bool SetRtpHeader(void* data, size_t len, const RtpHeader& header) {
  if (!IsValidRtpPayloadType(header.payload_type) ||
      header.seq_num < 0 || header.seq_num > UINT16_MAX) {
    return false;
  }
  return (SetUint8(data, kRtpFlagsOffset, kRtpVersion << 6) &&
          SetUint8(data, kRtpPayloadTypeOffset, header.payload_type & 0x7F) &&
          SetUint16(data, kRtpSeqNumOffset,
                    static_cast<uint16_t>(header.seq_num)) &&
          SetUint32(data, kRtpTimestampOffset, header.timestamp) &&
          SetRtpSsrc(data, len, header.ssrc));
}

bool IsRtpPacket(const void* data, size_t len) {
  if (len < kMinRtpPacketLen)
    return false;

  return (static_cast<const uint8_t*>(data)[0] >> 6) == kRtpVersion;
}

bool IsValidRtpPayloadType(int payload_type) {
  return payload_type >= 0 && payload_type <= 127;
}

}  // namespace cricket
