/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */


/*
 * This file includes unit tests for the ModuleRTPUtility.
 */

#include <gtest/gtest.h>

#include "typedefs.h"
#include "rtp_utility.h"
#include "rtp_format_vp8.h"

namespace {

using webrtc::ModuleRTPUtility::RTPPayloadParser;
using webrtc::ModuleRTPUtility::RTPPayload;
using webrtc::ModuleRTPUtility::RTPPayloadVP8;
using webrtc::RtpVideoCodecTypes;

// Payload descriptor
//     0 1 2 3 4 5 6 7
//    +-+-+-+-+-+-+-+-+
//    |X|R|N|S|PartID | (REQUIRED)
//    +-+-+-+-+-+-+-+-+
// X: |I|L|T|  RSV-A  | (OPTIONAL)
//    +-+-+-+-+-+-+-+-+
// I: |   PictureID   | (OPTIONAL)
//    +-+-+-+-+-+-+-+-+
// L: |   TL0PICIDX   | (OPTIONAL)
//    +-+-+-+-+-+-+-+-+
// T: | TID |  RSV-B  | (OPTIONAL)
//    +-+-+-+-+-+-+-+-+
//
// Payload header
//     0 1 2 3 4 5 6 7
//    +-+-+-+-+-+-+-+-+
//    |Size0|H| VER |P|
//    +-+-+-+-+-+-+-+-+
//    |     Size1     |
//    +-+-+-+-+-+-+-+-+
//    |     Size2     |
//    +-+-+-+-+-+-+-+-+
//    | Bytes 4..N of |
//    | VP8 payload   |
//    :               :
//    +-+-+-+-+-+-+-+-+
//    | OPTIONAL RTP  |
//    | padding       |
//    :               :
//    +-+-+-+-+-+-+-+-+

void VerifyBasicHeader(const RTPPayloadVP8 &header, bool N, bool S, int PartID)
{
    EXPECT_EQ(N, header.nonReferenceFrame);
    EXPECT_EQ(S, header.beginningOfPartition);
    EXPECT_EQ(PartID, header.partitionID);
}

void VerifyExtensions(const RTPPayloadVP8 &header, bool I, bool L, bool T)
{
    EXPECT_EQ(I, header.hasPictureID);
    EXPECT_EQ(L, header.hasTl0PicIdx);
    EXPECT_EQ(T, header.hasTID);
}

TEST(ParseVP8Test, BasicHeader) {
    WebRtc_UWord8 payload[4] = {0};
    payload[0] = 0x14; // binary 0001 0100; S = 1, PartID = 4
    payload[1] = 0x01; // P frame

    RTPPayloadParser rtpPayloadParser(webrtc::kRtpVp8Video, payload, 4, 0);

    RTPPayload parsedPacket;
    ASSERT_TRUE(rtpPayloadParser.Parse(parsedPacket));

    EXPECT_EQ(webrtc::ModuleRTPUtility::kPFrame, parsedPacket.frameType);
    EXPECT_EQ(webrtc::kRtpVp8Video, parsedPacket.type);

    VerifyBasicHeader(parsedPacket.info.VP8, 0 /*N*/, 1 /*S*/, 4 /*PartID*/);
    VerifyExtensions(parsedPacket.info.VP8, 0 /*I*/, 0 /*L*/, 0 /*T*/);

    EXPECT_EQ(payload + 1, parsedPacket.info.VP8.data);
    EXPECT_EQ(4 - 1, parsedPacket.info.VP8.dataLength);
}

TEST(ParseVP8Test, PictureID) {
    WebRtc_UWord8 payload[10] = {0};
    payload[0] = 0xA0;
    payload[1] = 0x80;
    payload[2] = 17;

    RTPPayloadParser rtpPayloadParser(webrtc::kRtpVp8Video, payload, 10, 0);

    RTPPayload parsedPacket;
    ASSERT_TRUE(rtpPayloadParser.Parse(parsedPacket));

    EXPECT_EQ(webrtc::ModuleRTPUtility::kPFrame, parsedPacket.frameType);
    EXPECT_EQ(webrtc::kRtpVp8Video, parsedPacket.type);

    VerifyBasicHeader(parsedPacket.info.VP8, 1 /*N*/, 0 /*S*/, 0 /*PartID*/);
    VerifyExtensions(parsedPacket.info.VP8, 1 /*I*/, 0 /*L*/, 0 /*T*/);

    EXPECT_EQ(17, parsedPacket.info.VP8.pictureID);

    EXPECT_EQ(payload + 3, parsedPacket.info.VP8.data);
    EXPECT_EQ(10 - 3, parsedPacket.info.VP8.dataLength);


    // Re-use payload, but change to long PictureID
    payload[2] = 0x80 | 17;
    payload[3] = 17;
    RTPPayloadParser rtpPayloadParser2(webrtc::kRtpVp8Video, payload, 10, 0);

    ASSERT_TRUE(rtpPayloadParser2.Parse(parsedPacket));

    VerifyBasicHeader(parsedPacket.info.VP8, 1 /*N*/, 0 /*S*/, 0 /*PartID*/);
    VerifyExtensions(parsedPacket.info.VP8, 1 /*I*/, 0 /*L*/, 0 /*T*/);

    EXPECT_EQ((17<<8) + 17, parsedPacket.info.VP8.pictureID);

    EXPECT_EQ(payload + 4, parsedPacket.info.VP8.data);
    EXPECT_EQ(10 - 4, parsedPacket.info.VP8.dataLength);
}

TEST(ParseVP8Test, Tl0PicIdx) {
    WebRtc_UWord8 payload[13] = {0};
    payload[0] = 0x90;
    payload[1] = 0x40;
    payload[2] = 17;

    RTPPayloadParser rtpPayloadParser(webrtc::kRtpVp8Video, payload, 13, 0);

    RTPPayload parsedPacket;
    ASSERT_TRUE(rtpPayloadParser.Parse(parsedPacket));

    EXPECT_EQ(webrtc::ModuleRTPUtility::kIFrame, parsedPacket.frameType);
    EXPECT_EQ(webrtc::kRtpVp8Video, parsedPacket.type);

    VerifyBasicHeader(parsedPacket.info.VP8, 0 /*N*/, 1 /*S*/, 0 /*PartID*/);
    VerifyExtensions(parsedPacket.info.VP8, 0 /*I*/, 1 /*L*/, 0 /*T*/);

    EXPECT_EQ(17, parsedPacket.info.VP8.tl0PicIdx);

    EXPECT_EQ(payload + 3, parsedPacket.info.VP8.data);
    EXPECT_EQ(13 - 3, parsedPacket.info.VP8.dataLength);
}

TEST(ParseVP8Test, TID) {
    WebRtc_UWord8 payload[10] = {0};
    payload[0] = 0x88;
    payload[1] = 0x20;
    payload[2] = 0x40;

    RTPPayloadParser rtpPayloadParser(webrtc::kRtpVp8Video, payload, 10, 0);

    RTPPayload parsedPacket;
    ASSERT_TRUE(rtpPayloadParser.Parse(parsedPacket));

    EXPECT_EQ(webrtc::ModuleRTPUtility::kPFrame, parsedPacket.frameType);
    EXPECT_EQ(webrtc::kRtpVp8Video, parsedPacket.type);

    VerifyBasicHeader(parsedPacket.info.VP8, 0 /*N*/, 0 /*S*/, 8 /*PartID*/);
    VerifyExtensions(parsedPacket.info.VP8, 0 /*I*/, 0 /*L*/, 1 /*T*/);

    EXPECT_EQ(2, parsedPacket.info.VP8.tID);

    EXPECT_EQ(payload + 3, parsedPacket.info.VP8.data);
    EXPECT_EQ(10 - 3, parsedPacket.info.VP8.dataLength);
}

TEST(ParseVP8Test, MultipleExtensions) {
    WebRtc_UWord8 payload[10] = {0};
    payload[0] = 0x88;
    payload[1] = 0x80 | 0x40 | 0x20;
    payload[2] = 0x80 | 17; // PictureID, high 7 bits
    payload[3] = 17; // PictureID, low 8 bits
    payload[4] = 42; // Tl0PicIdx
    payload[5] = 0x40; // TID

    RTPPayloadParser rtpPayloadParser(webrtc::kRtpVp8Video, payload, 10, 0);

    RTPPayload parsedPacket;
    ASSERT_TRUE(rtpPayloadParser.Parse(parsedPacket));

    EXPECT_EQ(webrtc::ModuleRTPUtility::kPFrame, parsedPacket.frameType);
    EXPECT_EQ(webrtc::kRtpVp8Video, parsedPacket.type);

    VerifyBasicHeader(parsedPacket.info.VP8, 0 /*N*/, 0 /*S*/, 8 /*PartID*/);
    VerifyExtensions(parsedPacket.info.VP8, 1 /*I*/, 1 /*L*/, 1 /*T*/);

    EXPECT_EQ((17<<8) + 17, parsedPacket.info.VP8.pictureID);
    EXPECT_EQ(42, parsedPacket.info.VP8.tl0PicIdx);
    EXPECT_EQ(2, parsedPacket.info.VP8.tID);

    EXPECT_EQ(payload + 6, parsedPacket.info.VP8.data);
    EXPECT_EQ(10 - 6, parsedPacket.info.VP8.dataLength);
}

TEST(ParseVP8Test, TooShortHeader) {
    WebRtc_UWord8 payload[4] = {0};
    payload[0] = 0x88;
    payload[1] = 0x80 | 0x40 | 0x20; // All extensions are enabled
    payload[2] = 0x80 | 17; //... but only 2 bytes PictureID is provided
    payload[3] = 17; // PictureID, low 8 bits

    RTPPayloadParser rtpPayloadParser(webrtc::kRtpVp8Video, payload, 4, 0);

    RTPPayload parsedPacket;
    EXPECT_FALSE(rtpPayloadParser.Parse(parsedPacket));
}

using webrtc::RtpFormatVp8;
using webrtc::RTPVideoHeaderVP8;

TEST(ParseVP8Test, TestWithPacketizer) {
    WebRtc_UWord8 payload[10] = {0};
    WebRtc_UWord8 packet[20] = {0};
    RTPVideoHeaderVP8 inputHeader;
    inputHeader.nonReference = true;
    inputHeader.pictureId = 300;
    inputHeader.temporalIdx = 1;
    inputHeader.tl0PicIdx = -1; // disable
    RtpFormatVp8 packetizer = RtpFormatVp8(payload, 10, inputHeader);
    bool last;
    int send_bytes;
    ASSERT_EQ(0, packetizer.NextPacket(20, packet, &send_bytes, &last));
    ASSERT_TRUE(last);

    RTPPayloadParser rtpPayloadParser(webrtc::kRtpVp8Video, packet, send_bytes,
                                      0);

    RTPPayload parsedPacket;
    ASSERT_TRUE(rtpPayloadParser.Parse(parsedPacket));

    EXPECT_EQ(webrtc::ModuleRTPUtility::kIFrame, parsedPacket.frameType);
    EXPECT_EQ(webrtc::kRtpVp8Video, parsedPacket.type);

    VerifyBasicHeader(parsedPacket.info.VP8,
                      inputHeader.nonReference /*N*/,
                      1 /*S*/,
                      0 /*PartID*/);
    VerifyExtensions(parsedPacket.info.VP8,
                     1 /*I*/,
                     0 /*L*/,
                     1 /*T*/);

    EXPECT_EQ(inputHeader.pictureId, parsedPacket.info.VP8.pictureID);
    EXPECT_EQ(inputHeader.temporalIdx, parsedPacket.info.VP8.tID);

    EXPECT_EQ(packet + 5, parsedPacket.info.VP8.data);
    EXPECT_EQ(send_bytes - 5, parsedPacket.info.VP8.dataLength);
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);

    return RUN_ALL_TESTS();
}

} // namespace
