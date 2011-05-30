/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MODULES_VIDEO_CODING_SESSION_INFO_H_
#define WEBRTC_MODULES_VIDEO_CODING_SESSION_INFO_H_

#include "typedefs.h"
#include "module_common_types.h"
#include "packet.h"

namespace webrtc
{

class VCMSessionInfo
{
public:
    VCMSessionInfo();
    virtual ~VCMSessionInfo();

    VCMSessionInfo(const VCMSessionInfo& rhs);

    WebRtc_Word32 ZeroOutSeqNum(WebRtc_Word32* list, WebRtc_Word32 num);
    virtual void Reset();

    WebRtc_Word64 InsertPacket(const VCMPacket& packet, WebRtc_UWord8* ptrStartOfLayer);

    virtual bool IsSessionComplete();
    WebRtc_UWord32 MakeSessionDecodable(WebRtc_UWord8* ptrStartOfLayer);

    WebRtc_UWord32 GetSessionLength();
    bool HaveLastPacket();
    void ForceSetHaveLastPacket();
    bool IsRetransmitted();
    webrtc::FrameType FrameType() const { return _frameType; }

    virtual WebRtc_Word32 GetHighestPacketIndex();
    virtual WebRtc_UWord32 GetPacketSize(WebRtc_Word32 packetIndex);
    virtual void ClearPacketSize(WebRtc_Word32 packetIndex);
    virtual void UpdatePacketSize(WebRtc_Word32 packetIndex, WebRtc_UWord32 length);
    virtual void PrependPacketIndices(WebRtc_Word32 numberOfPacketIndexes);

    void SetStartSeqNumber(WebRtc_UWord16 seqNumber);

    bool HaveStartSeqNumber();

    WebRtc_Word32 GetLowSeqNum() const;
    WebRtc_Word32 GetHighSeqNum() const;

    WebRtc_UWord32 PrepareForDecode(WebRtc_UWord8* ptrStartOfLayer, VideoCodecType codec);

    void SetPreviousFrameLoss() { _previousFrameLoss = true; }
    bool PreviousFrameLoss() const { return _previousFrameLoss; }

protected:
    WebRtc_UWord32 InsertBuffer(WebRtc_UWord8* ptrStartOfLayer,
                                WebRtc_Word32 packetIndex,
                                const VCMPacket& packet);
    void FindNaluBorder(WebRtc_Word32 packetIndex,
                        WebRtc_Word32& startIndex,
                        WebRtc_Word32& endIndex);
    WebRtc_UWord32 DeletePackets(WebRtc_UWord8* ptrStartOfLayer,
                                 WebRtc_Word32 startIndex,
                                 WebRtc_Word32 endIndex);
    void UpdateCompleteSession();

    bool _haveFirstPacket;      // If we have inserted the first packet into this frame
    bool _markerBit;            // If we have inserted a packet with markerbit into this frame
    bool _sessionNACK;          // If this session has been NACKed by JB
    bool _completeSession;
    webrtc::FrameType  _frameType;
    bool           _previousFrameLoss;

    WebRtc_Word32  _lowSeqNum;          // Lowest packet sequence number in a session
    WebRtc_Word32  _highSeqNum;         // Highest packet sequence number in a session

    // Highest packet index in this frame
    WebRtc_UWord16 _highestPacketIndex;
    // Length of packet (used for reordering)
    WebRtc_UWord32 _packetSizeBytes[kMaxPacketsInJitterBuffer];
    // Completness of packets. Used for deciding if the frame is decodable.
    WebRtc_UWord8  _naluCompleteness[kMaxPacketsInJitterBuffer];
    bool           _ORwithPrevByte[kMaxPacketsInJitterBuffer];
};

} // namespace webrtc

#endif // WEBRTC_MODULES_VIDEO_CODING_SESSION_INFO_H_
