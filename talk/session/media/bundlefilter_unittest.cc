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

static const int kSsrc1 = 0x1111;
static const int kSsrc2 = 0x2222;
static const int kSsrc3 = 0x3333;
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

// PT = 200 = SR, len = 28, SSRC of sender = 0x0001
// NTP TS = 0, RTP TS = 0, packet count = 0
static const unsigned char kRtcpPacketSrSsrc01[] = {
    0x80, 0xC8, 0x00, 0x1B, 0x00, 0x00, 0x00, 0x01,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00,
};

// PT = 200 = SR, len = 28, SSRC of sender = 0x2222
// NTP TS = 0, RTP TS = 0, packet count = 0
static const unsigned char kRtcpPacketSrSsrc2[] = {
    0x80, 0xC8, 0x00, 0x1B, 0x00, 0x00, 0x22, 0x22,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00,
};

// First packet - SR = PT = 200, len = 0, SSRC of sender = 0x1111
// NTP TS = 0, RTP TS = 0, packet count = 0
// second packet - SDES = PT =  202, count = 0, SSRC = 0x1111, cname len = 0
static const unsigned char kRtcpPacketCompoundSrSdesSsrc1[] = {
    0x80, 0xC8, 0x00, 0x01, 0x00, 0x00, 0x11, 0x11,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00,
    0x81, 0xCA, 0x00, 0x00, 0x00, 0x00, 0x11, 0x11, 0x01, 0x00,
};

// SDES = PT =  202, count = 0, SSRC = 0x2222, cname len = 0
static const unsigned char kRtcpPacketSdesSsrc2[] = {
    0x81, 0xCA, 0x00, 0x00, 0x00, 0x00, 0x22, 0x22, 0x01, 0x00,
};

// Packet has only mandatory fixed RTCP header
static const unsigned char kRtcpPacketFixedHeaderOnly[] = {
    0x80, 0xC8, 0x00, 0x00,
};

// Small packet for SSRC demux.
static const unsigned char kRtcpPacketTooSmall[] = {
    0x80, 0xC8, 0x00, 0x00, 0x00, 0x00,
};

// PT = 206, FMT = 1, Sender SSRC  = 0x1111, Media SSRC = 0x1111
// No FCI information is needed for PLI.
static const unsigned char kRtcpPacketNonCompoundRtcpPliFeedback[] = {
    0x81, 0xCE, 0x00, 0x0C, 0x00, 0x00, 0x11, 0x11, 0x00, 0x00, 0x11, 0x11,
};

// An SCTP packet.
static const unsigned char kSctpPacket[] = {
    0x00, 0x01, 0x00, 0x01,
    0xff, 0xff, 0xff, 0xff,
    0x00, 0x00, 0x00, 0x00,
    0x03, 0x00, 0x00, 0x04,
    0x00, 0x00, 0x00, 0x00,
};

TEST(BundleFilterTest, AddRemoveStreamTest) {
  cricket::BundleFilter bundle_filter;
  EXPECT_FALSE(bundle_filter.HasStreams());
  EXPECT_TRUE(bundle_filter.AddStream(StreamParams::CreateLegacy(kSsrc1)));
  StreamParams stream2;
  stream2.ssrcs.push_back(kSsrc2);
  stream2.ssrcs.push_back(kSsrc3);
  EXPECT_TRUE(bundle_filter.AddStream(stream2));

  EXPECT_TRUE(bundle_filter.HasStreams());
  EXPECT_TRUE(bundle_filter.FindStream(kSsrc1));
  EXPECT_TRUE(bundle_filter.FindStream(kSsrc2));
  EXPECT_TRUE(bundle_filter.FindStream(kSsrc3));
  EXPECT_TRUE(bundle_filter.RemoveStream(kSsrc1));
  EXPECT_FALSE(bundle_filter.FindStream(kSsrc1));
  EXPECT_TRUE(bundle_filter.RemoveStream(kSsrc3));
  EXPECT_FALSE(bundle_filter.RemoveStream(kSsrc2));  // Already removed.
  EXPECT_FALSE(bundle_filter.HasStreams());
}

TEST(BundleFilterTest, RtpPacketTest) {
  cricket::BundleFilter bundle_filter;
  bundle_filter.AddPayloadType(kPayloadType1);
  EXPECT_TRUE(bundle_filter.DemuxPacket(
      reinterpret_cast<const char*>(kRtpPacketPt1Ssrc1),
      sizeof(kRtpPacketPt1Ssrc1), false));
  bundle_filter.AddPayloadType(kPayloadType2);
  EXPECT_TRUE(bundle_filter.DemuxPacket(
      reinterpret_cast<const char*>(kRtpPacketPt2Ssrc2),
      sizeof(kRtpPacketPt2Ssrc2), false));

  // Payload type 0x33 is not added.
  EXPECT_FALSE(bundle_filter.DemuxPacket(
      reinterpret_cast<const char*>(kRtpPacketPt3Ssrc2),
      sizeof(kRtpPacketPt3Ssrc2), false));
  // Size is too small.
  EXPECT_FALSE(bundle_filter.DemuxPacket(
      reinterpret_cast<const char*>(kRtpPacketPt1Ssrc1), 11, false));

  bundle_filter.ClearAllPayloadTypes();
  EXPECT_FALSE(bundle_filter.DemuxPacket(
      reinterpret_cast<const char*>(kRtpPacketPt1Ssrc1),
      sizeof(kRtpPacketPt1Ssrc1), false));
  EXPECT_FALSE(bundle_filter.DemuxPacket(
      reinterpret_cast<const char*>(kRtpPacketPt2Ssrc2),
      sizeof(kRtpPacketPt2Ssrc2), false));
}

TEST(BundleFilterTest, RtcpPacketTest) {
  cricket::BundleFilter bundle_filter;
  EXPECT_TRUE(bundle_filter.AddStream(StreamParams::CreateLegacy(kSsrc1)));
  EXPECT_TRUE(bundle_filter.DemuxPacket(
      reinterpret_cast<const char*>(kRtcpPacketCompoundSrSdesSsrc1),
      sizeof(kRtcpPacketCompoundSrSdesSsrc1), true));
  EXPECT_TRUE(bundle_filter.AddStream(StreamParams::CreateLegacy(kSsrc2)));
  EXPECT_TRUE(bundle_filter.DemuxPacket(
      reinterpret_cast<const char*>(kRtcpPacketSrSsrc2),
      sizeof(kRtcpPacketSrSsrc2), true));
  EXPECT_TRUE(bundle_filter.DemuxPacket(
      reinterpret_cast<const char*>(kRtcpPacketSdesSsrc2),
      sizeof(kRtcpPacketSdesSsrc2), true));
  EXPECT_TRUE(bundle_filter.RemoveStream(kSsrc2));
  // RTCP Packets other than SR and RR are demuxed regardless of SSRC.
  EXPECT_TRUE(bundle_filter.DemuxPacket(
      reinterpret_cast<const char*>(kRtcpPacketSdesSsrc2),
      sizeof(kRtcpPacketSdesSsrc2), true));
  // RTCP Packets with 'special' SSRC 0x01 are demuxed also
  EXPECT_TRUE(bundle_filter.DemuxPacket(
      reinterpret_cast<const char*>(kRtcpPacketSrSsrc01),
      sizeof(kRtcpPacketSrSsrc01), true));
  EXPECT_FALSE(bundle_filter.DemuxPacket(
      reinterpret_cast<const char*>(kRtcpPacketSrSsrc2),
      sizeof(kRtcpPacketSrSsrc2), true));
  EXPECT_FALSE(bundle_filter.DemuxPacket(
      reinterpret_cast<const char*>(kRtcpPacketFixedHeaderOnly),
      sizeof(kRtcpPacketFixedHeaderOnly), true));
  EXPECT_FALSE(bundle_filter.DemuxPacket(
      reinterpret_cast<const char*>(kRtcpPacketTooSmall),
      sizeof(kRtcpPacketTooSmall), true));
  EXPECT_TRUE(bundle_filter.DemuxPacket(
      reinterpret_cast<const char*>(kRtcpPacketNonCompoundRtcpPliFeedback),
      sizeof(kRtcpPacketNonCompoundRtcpPliFeedback), true));
  // If the streams_ is empty, rtcp packet passes through
  EXPECT_TRUE(bundle_filter.RemoveStream(kSsrc1));
  EXPECT_FALSE(bundle_filter.HasStreams());
  EXPECT_TRUE(bundle_filter.DemuxPacket(
      reinterpret_cast<const char*>(kRtcpPacketSrSsrc2),
      sizeof(kRtcpPacketSrSsrc2), true));
}

TEST(BundleFilterTest, InvalidRtpPacket) {
  cricket::BundleFilter bundle_filter;
  EXPECT_TRUE(bundle_filter.AddStream(StreamParams::CreateLegacy(kSsrc1)));
  EXPECT_FALSE(bundle_filter.DemuxPacket(
      reinterpret_cast<const char*>(kSctpPacket),
      sizeof(kSctpPacket), false));
}
