/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "packet.h"
#include "module_common_types.h"

#include <assert.h>

namespace webrtc {

VCMPacket::VCMPacket(const WebRtc_UWord8* ptr,
                               const WebRtc_UWord32 size,
                               const WebRtcRTPHeader& rtpHeader) :
    payloadType(rtpHeader.header.payloadType),
    timestamp(rtpHeader.header.timestamp),
    seqNum(rtpHeader.header.sequenceNumber),
    dataPtr(ptr),
    sizeBytes(size),
    markerBit(rtpHeader.header.markerBit),

    frameType(rtpHeader.frameType),
    codec(kVideoCodecUnknown),
    isFirstPacket(rtpHeader.type.Video.isFirstPacket),
    completeNALU(kNaluComplete),
    insertStartCode(false),
    bits(false)
{
    CopyCodecSpecifics(rtpHeader.type.Video);
}

VCMPacket::VCMPacket(const WebRtc_UWord8* ptr, WebRtc_UWord32 size, WebRtc_UWord16 seq, WebRtc_UWord32 ts, bool mBit) :
    payloadType(0),
    timestamp(ts),
    seqNum(seq),
    dataPtr(ptr),
    sizeBytes(size),
    markerBit(mBit),

    frameType(kVideoFrameDelta),
    codec(kVideoCodecUnknown),
    isFirstPacket(false),
    completeNALU(kNaluComplete),
    insertStartCode(false),
    bits(false)
{}

void VCMPacket::CopyCodecSpecifics(const RTPVideoHeader& videoHeader)
{
    RTPVideoTypeHeader codecHeader = videoHeader.codecHeader;
    switch(videoHeader.codec)
    {
        case kRTPVideoVP8:
            {
                codec = kVideoCodecVP8;
                break;
            }
        case kRTPVideoI420:
            {
                codec = kVideoCodecI420;
                break;
            }
        default:
            {
                codec = kVideoCodecUnknown;
                break;
            }
    }
}

}
