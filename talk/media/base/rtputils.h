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

#ifndef TALK_MEDIA_BASE_RTPUTILS_H_
#define TALK_MEDIA_BASE_RTPUTILS_H_

#include "talk/base/byteorder.h"

namespace cricket {

const size_t kMinRtpPacketLen = 12;
const size_t kMaxRtpPacketLen = 2048;
const size_t kMinRtcpPacketLen = 4;

struct RtpHeader {
  int payload_type;
  int seq_num;
  uint32 timestamp;
  uint32 ssrc;
};

enum RtcpTypes {
  kRtcpTypeSR = 200,      // Sender report payload type.
  kRtcpTypeRR = 201,      // Receiver report payload type.
  kRtcpTypeSDES = 202,    // SDES payload type.
  kRtcpTypeBye = 203,     // BYE payload type.
  kRtcpTypeApp = 204,     // APP payload type.
  kRtcpTypeRTPFB = 205,   // Transport layer Feedback message payload type.
  kRtcpTypePSFB = 206,    // Payload-specific Feedback message payload type.
};

bool GetRtpVersion(const void* data, size_t len, int* version);
bool GetRtpPayloadType(const void* data, size_t len, int* value);
bool GetRtpSeqNum(const void* data, size_t len, int* value);
bool GetRtpTimestamp(const void* data, size_t len, uint32* value);
bool GetRtpSsrc(const void* data, size_t len, uint32* value);
bool GetRtpHeaderLen(const void* data, size_t len, size_t* value);
bool GetRtcpType(const void* data, size_t len, int* value);
bool GetRtcpSsrc(const void* data, size_t len, uint32* value);
bool GetRtpHeader(const void* data, size_t len, RtpHeader* header);

// Assumes marker bit is 0.
bool SetRtpHeaderFlags(
    void* data, size_t len,
    bool padding, bool extension, int csrc_count);
bool SetRtpPayloadType(void* data, size_t len, int value);
bool SetRtpSeqNum(void* data, size_t len, int value);
bool SetRtpTimestamp(void* data, size_t len, uint32 value);
bool SetRtpSsrc(void* data, size_t len, uint32 value);
// Assumes version 2, no padding, no extensions, no csrcs.
bool SetRtpHeader(void* data, size_t len, const RtpHeader& header);

}  // namespace cricket

#endif  // TALK_MEDIA_BASE_RTPUTILS_H_
