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

static const int kRtpVersion = 2;
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
  *value = *(static_cast<const uint8*>(data) + offset);
  return true;
}

bool GetUint16(const void* data, size_t offset, int* value) {
  if (!data || !value) {
    return false;
  }
  *value = static_cast<int>(
      talk_base::GetBE16(static_cast<const uint8*>(data) + offset));
  return true;
}

bool GetUint32(const void* data, size_t offset, uint32* value) {
  if (!data || !value) {
    return false;
  }
  *value = talk_base::GetBE32(static_cast<const uint8*>(data) + offset);
  return true;
}

bool SetUint8(void* data, size_t offset, int value) {
  if (!data) {
    return false;
  }
  talk_base::Set8(data, offset, value);
  return true;
}

bool SetUint16(void* data, size_t offset, int value) {
  if (!data) {
    return false;
  }
  talk_base::SetBE16(static_cast<uint8*>(data) + offset, value);
  return true;
}

bool SetUint32(void* data, size_t offset, uint32 value) {
  if (!data) {
    return false;
  }
  talk_base::SetBE32(static_cast<uint8*>(data) + offset, value);
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

bool GetRtpTimestamp(const void* data, size_t len, uint32* value) {
  if (len < kMinRtpPacketLen) {
    return false;
  }
  return GetUint32(data, kRtpTimestampOffset, value);
}

bool GetRtpSsrc(const void* data, size_t len, uint32* value) {
  if (len < kMinRtpPacketLen) {
    return false;
  }
  return GetUint32(data, kRtpSsrcOffset, value);
}

bool GetRtpHeaderLen(const void* data, size_t len, size_t* value) {
  if (!data || len < kMinRtpPacketLen || !value) return false;
  const uint8* header = static_cast<const uint8*>(data);
  // Get base header size + length of CSRCs (not counting extension yet).
  size_t header_size = kMinRtpPacketLen + (header[0] & 0xF) * sizeof(uint32);
  if (len < header_size) return false;
  // If there's an extension, read and add in the extension size.
  if (header[0] & 0x10) {
    if (len < header_size + sizeof(uint32)) return false;
    header_size += ((talk_base::GetBE16(header + header_size + 2) + 1) *
                    sizeof(uint32));
    if (len < header_size) return false;
  }
  *value = header_size;
  return true;
}

bool GetRtpVersion(const void* data, size_t len, int* version) {
  if (len == 0) {
    return false;
  }

  const uint8 first = static_cast<const uint8*>(data)[0];
  *version = static_cast<int>((first >> 6) & 0x3);
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
bool GetRtcpSsrc(const void* data, size_t len, uint32* value) {
  // Packet should be at least of 8 bytes, to get SSRC from a RTCP packet.
  if (!data || len < kMinRtcpPacketLen + 4 || !value) return false;
  int pl_type;
  if (!GetRtcpType(data, len, &pl_type)) return false;
  // SDES packet parsing is not supported.
  if (pl_type == kRtcpTypeSDES) return false;
  *value = talk_base::GetBE32(static_cast<const uint8*>(data) + 4);
  return true;
}

bool SetRtpHeaderFlags(
    void* data, size_t len,
    bool padding, bool extension, int csrc_count) {
  if (csrc_count > 0x0F) {
    return false;
  }
  int flags = 0;
  flags |= (kRtpVersion << 6);
  flags |= ((padding ? 1 : 0) << 5);
  flags |= ((extension ? 1 : 0) << 4);
  flags |= csrc_count;
  return SetUint8(data, kRtpFlagsOffset, flags);
}

// Assumes marker bit is 0.
bool SetRtpPayloadType(void* data, size_t len, int value) {
  if (value >= 0x7F) {
    return false;
  }
  return SetUint8(data, kRtpPayloadTypeOffset, value & 0x7F);
}

bool SetRtpSeqNum(void* data, size_t len, int value) {
  return SetUint16(data, kRtpSeqNumOffset, value);
}

bool SetRtpTimestamp(void* data, size_t len, uint32 value) {
  return SetUint32(data, kRtpTimestampOffset, value);
}

bool SetRtpSsrc(void* data, size_t len, uint32 value) {
  return SetUint32(data, kRtpSsrcOffset, value);
}

// Assumes version 2, no padding, no extensions, no csrcs.
bool SetRtpHeader(void* data, size_t len, const RtpHeader& header) {
  return (SetRtpHeaderFlags(data, len, false, false, 0) &&
          SetRtpPayloadType(data, len, header.payload_type) &&
          SetRtpSeqNum(data, len, header.seq_num) &&
          SetRtpTimestamp(data, len, header.timestamp) &&
          SetRtpSsrc(data, len, header.ssrc));
}

}  // namespace cricket
