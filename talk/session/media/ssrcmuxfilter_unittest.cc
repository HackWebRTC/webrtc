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


#include "talk/base/gunit.h"
#include "talk/session/media/ssrcmuxfilter.h"

static const int kSsrc1 = 0x1111;
static const int kSsrc2 = 0x2222;
static const int kSsrc3 = 0x3333;

using cricket::StreamParams;

// SSRC = 0x1111
static const unsigned char kRtpPacketSsrc1[] = {
    0x80, 0x80, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x11, 0x11,
};

// SSRC = 0x2222
static const unsigned char kRtpPacketSsrc2[] = {
    0x80, 0x80, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x22, 0x22,
};

// SSRC = 0
static const unsigned char kRtpPacketInvalidSsrc[] = {
    0x80, 0x80, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
};

// invalid size
static const unsigned char kRtpPacketTooSmall[] = {
    0x80, 0x80, 0x00, 0x00,
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

TEST(SsrcMuxFilterTest, AddRemoveStreamTest) {
  cricket::SsrcMuxFilter ssrc_filter;
  EXPECT_FALSE(ssrc_filter.IsActive());
  EXPECT_TRUE(ssrc_filter.AddStream(StreamParams::CreateLegacy(kSsrc1)));
  StreamParams stream2;
  stream2.ssrcs.push_back(kSsrc2);
  stream2.ssrcs.push_back(kSsrc3);
  EXPECT_TRUE(ssrc_filter.AddStream(stream2));

  EXPECT_TRUE(ssrc_filter.IsActive());
  EXPECT_TRUE(ssrc_filter.FindStream(kSsrc1));
  EXPECT_TRUE(ssrc_filter.FindStream(kSsrc2));
  EXPECT_TRUE(ssrc_filter.FindStream(kSsrc3));
  EXPECT_TRUE(ssrc_filter.RemoveStream(kSsrc1));
  EXPECT_FALSE(ssrc_filter.FindStream(kSsrc1));
  EXPECT_TRUE(ssrc_filter.RemoveStream(kSsrc3));
  EXPECT_FALSE(ssrc_filter.RemoveStream(kSsrc2));  // Already removed.
  EXPECT_FALSE(ssrc_filter.IsActive());
}

TEST(SsrcMuxFilterTest, RtpPacketTest) {
  cricket::SsrcMuxFilter ssrc_filter;
  EXPECT_TRUE(ssrc_filter.AddStream(StreamParams::CreateLegacy(kSsrc1)));
  EXPECT_TRUE(ssrc_filter.DemuxPacket(
      reinterpret_cast<const char*>(kRtpPacketSsrc1),
      sizeof(kRtpPacketSsrc1), false));
  EXPECT_TRUE(ssrc_filter.AddStream(StreamParams::CreateLegacy(kSsrc2)));
  EXPECT_TRUE(ssrc_filter.DemuxPacket(
      reinterpret_cast<const char*>(kRtpPacketSsrc2),
      sizeof(kRtpPacketSsrc2), false));
  EXPECT_TRUE(ssrc_filter.RemoveStream(kSsrc2));
  EXPECT_FALSE(ssrc_filter.DemuxPacket(
      reinterpret_cast<const char*>(kRtpPacketSsrc2),
      sizeof(kRtpPacketSsrc2), false));
  EXPECT_FALSE(ssrc_filter.DemuxPacket(
      reinterpret_cast<const char*>(kRtpPacketInvalidSsrc),
      sizeof(kRtpPacketInvalidSsrc), false));
  EXPECT_FALSE(ssrc_filter.DemuxPacket(
      reinterpret_cast<const char*>(kRtpPacketTooSmall),
      sizeof(kRtpPacketTooSmall), false));
}

TEST(SsrcMuxFilterTest, RtcpPacketTest) {
  cricket::SsrcMuxFilter ssrc_filter;
  EXPECT_TRUE(ssrc_filter.AddStream(StreamParams::CreateLegacy(kSsrc1)));
  EXPECT_TRUE(ssrc_filter.DemuxPacket(
      reinterpret_cast<const char*>(kRtcpPacketCompoundSrSdesSsrc1),
      sizeof(kRtcpPacketCompoundSrSdesSsrc1), true));
  EXPECT_TRUE(ssrc_filter.AddStream(StreamParams::CreateLegacy(kSsrc2)));
  EXPECT_TRUE(ssrc_filter.DemuxPacket(
      reinterpret_cast<const char*>(kRtcpPacketSrSsrc2),
      sizeof(kRtcpPacketSrSsrc2), true));
  EXPECT_TRUE(ssrc_filter.DemuxPacket(
      reinterpret_cast<const char*>(kRtcpPacketSdesSsrc2),
      sizeof(kRtcpPacketSdesSsrc2), true));
  EXPECT_TRUE(ssrc_filter.RemoveStream(kSsrc2));
  // RTCP Packets other than SR and RR are demuxed regardless of SSRC.
  EXPECT_TRUE(ssrc_filter.DemuxPacket(
      reinterpret_cast<const char*>(kRtcpPacketSdesSsrc2),
      sizeof(kRtcpPacketSdesSsrc2), true));
  // RTCP Packets with 'special' SSRC 0x01 are demuxed also
  EXPECT_TRUE(ssrc_filter.DemuxPacket(
      reinterpret_cast<const char*>(kRtcpPacketSrSsrc01),
      sizeof(kRtcpPacketSrSsrc01), true));
  EXPECT_FALSE(ssrc_filter.DemuxPacket(
      reinterpret_cast<const char*>(kRtcpPacketSrSsrc2),
      sizeof(kRtcpPacketSrSsrc2), true));
  EXPECT_FALSE(ssrc_filter.DemuxPacket(
      reinterpret_cast<const char*>(kRtcpPacketFixedHeaderOnly),
      sizeof(kRtcpPacketFixedHeaderOnly), true));
  EXPECT_FALSE(ssrc_filter.DemuxPacket(
      reinterpret_cast<const char*>(kRtcpPacketTooSmall),
      sizeof(kRtcpPacketTooSmall), true));
  EXPECT_TRUE(ssrc_filter.DemuxPacket(
      reinterpret_cast<const char*>(kRtcpPacketNonCompoundRtcpPliFeedback),
      sizeof(kRtcpPacketNonCompoundRtcpPliFeedback), true));
}
