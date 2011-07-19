/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MODULES_VIDEO_CODING_FRAME_BUFFER_H_
#define WEBRTC_MODULES_VIDEO_CODING_FRAME_BUFFER_H_

#include "typedefs.h"
#include "module_common_types.h"

#include "encoded_frame.h"
#include "frame_list.h"
#include "jitter_buffer_common.h"
#include "session_info.h"

namespace webrtc
{

class VCMFrameBuffer : public VCMEncodedFrame
{
public:
    VCMFrameBuffer();
    virtual ~VCMFrameBuffer();

    VCMFrameBuffer(VCMFrameBuffer& rhs);

    virtual void Reset();

    VCMFrameBufferEnum InsertPacket(const VCMPacket& packet,
                                    WebRtc_Word64 timeInMs);

    // State
    // Get current state of frame
    VCMFrameBufferStateEnum GetState() const;
    // Get current state and timestamp of frame
    VCMFrameBufferStateEnum GetState(WebRtc_UWord32& timeStamp) const;
    void SetState(VCMFrameBufferStateEnum state); // Set state of frame

    bool IsRetransmitted();
    bool IsSessionComplete();
    bool HaveLastPacket();
    bool ForceSetHaveLastPacket();
    // Makes sure the session contain a decodable stream.
    void MakeSessionDecodable();

    // Sequence numbers
    // Get lowest packet sequence number in frame
    WebRtc_Word32 GetLowSeqNum();
    // Get highest packet sequence number in frame
    WebRtc_Word32 GetHighSeqNum();

    // Set counted status (as counted by JB or not)
    void SetCountedFrame(bool frameCounted);
    bool GetCountedFrame();

    // NACK
    // Zero out all entries in list up to and including _lowSeqNum
    WebRtc_Word32 ZeroOutSeqNum(WebRtc_Word32* list, WebRtc_Word32 num);
    // Hybrid extension: only NACK important packets, discard FEC packets
    WebRtc_Word32 ZeroOutSeqNumHybrid(WebRtc_Word32* list,
                                      WebRtc_Word32 num,
                                      float rttScore);
    void IncrementNackCount();
    WebRtc_Word16 GetNackCount() const;

    WebRtc_Word64 LatestPacketTimeMs();

    webrtc::FrameType FrameType() const;
    void SetPreviousFrameLoss();

    WebRtc_Word32 ExtractFromStorage(const EncodedVideoData& frameFromStorage);

protected:
    void RestructureFrameInformation();
    void PrepareForDecode();

private:
    VCMFrameBufferStateEnum    _state;         // Current state of the frame
    bool                       _frameCounted;  // Was this frame counted by JB?
    VCMSessionInfo             _sessionInfo;
    WebRtc_UWord16             _nackCount;
    WebRtc_Word64              _latestPacketTimeMs;
};

} // namespace webrtc

#endif // WEBRTC_MODULES_VIDEO_CODING_FRAME_BUFFER_H_
