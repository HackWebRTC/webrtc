/*
 * libjingle
 * Copyright 2004 Google Inc.
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

#include "talk/session/media/bundlefilter.h"
#include "webrtc/base/gunit.h"

using cricket::StreamParams;

static const int kPayloadType1 = 0x11;
static const int kPayloadType2 = 0x22;
static const int kPayloadType3 = 0x33;

// SSRC = 0x1111, Payload type = 0x11
static const unsigned char kRtpPacketPt1Ssrc1[] = {
    0x80, kPayloadType1, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x11,
    0x11,
};

// SSRC = 0x2222, Payload type = 0x22
static const unsigned char kRtpPacketPt2Ssrc2[] = {
    0x80, 0x80 + kPayloadType2, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x22, 0x22,
};

// SSRC = 0x2222, Payload type = 0x33
static const unsigned char kRtpPacketPt3Ssrc2[] = {
    0x80, kPayloadType3, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x22,
    0x22,
};

// An SCTP packet.
static const unsigned char kSctpPacket[] = {
    0x00, 0x01, 0x00, 0x01,
    0xff, 0xff, 0xff, 0xff,
    0x00, 0x00, 0x00, 0x00,
    0x03, 0x00, 0x00, 0x04,
    0x00, 0x00, 0x00, 0x00,
};

TEST(BundleFilterTest, RtpPacketTest) {
  cricket::BundleFilter bundle_filter;
  bundle_filter.AddPayloadType(kPayloadType1);
  EXPECT_TRUE(bundle_filter.DemuxPacket(kRtpPacketPt1Ssrc1,
                                        sizeof(kRtpPacketPt1Ssrc1)));
  bundle_filter.AddPayloadType(kPayloadType2);
  EXPECT_TRUE(bundle_filter.DemuxPacket(kRtpPacketPt2Ssrc2,
                                        sizeof(kRtpPacketPt2Ssrc2)));

  // Payload type 0x33 is not added.
  EXPECT_FALSE(bundle_filter.DemuxPacket(kRtpPacketPt3Ssrc2,
                                         sizeof(kRtpPacketPt3Ssrc2)));
  // Size is too small.
  EXPECT_FALSE(bundle_filter.DemuxPacket(kRtpPacketPt1Ssrc1, 11));

  bundle_filter.ClearAllPayloadTypes();
  EXPECT_FALSE(bundle_filter.DemuxPacket(kRtpPacketPt1Ssrc1,
                                         sizeof(kRtpPacketPt1Ssrc1)));
  EXPECT_FALSE(bundle_filter.DemuxPacket(kRtpPacketPt2Ssrc2,
                                         sizeof(kRtpPacketPt2Ssrc2)));
}

TEST(BundleFilterTest, InvalidRtpPacket) {
  cricket::BundleFilter bundle_filter;
  EXPECT_FALSE(bundle_filter.DemuxPacket(kSctpPacket, sizeof(kSctpPacket)));
}
