/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MODULES_RTP_RTCP_SOURCE_RECEIVER_FEC_H_
#define WEBRTC_MODULES_RTP_RTCP_SOURCE_RECEIVER_FEC_H_

#include "rtp_rtcp_defines.h"

#include "typedefs.h"
#include "list_wrapper.h"

namespace webrtc {
class ForwardErrorCorrection;
class RTPReceiverVideo;

class ReceiverFEC
{
public:
    ReceiverFEC(const WebRtc_Word32 id, RTPReceiverVideo* owner);
    virtual ~ReceiverFEC();

    WebRtc_Word32 AddReceivedFECPacket(const WebRtcRTPHeader* rtpHeader,
                                     const WebRtc_UWord8* incomingRtpPacket,
                                     const WebRtc_UWord16 payloadDataLength,
                                     bool& FECpacket);

    void AddReceivedFECInfo(const WebRtcRTPHeader* rtpHeader,
                            const WebRtc_UWord8* incomingRtpPacket,
                            bool& FECpacket);

    WebRtc_Word32 ProcessReceivedFEC(const bool forceFrameDecode);

    void SetPayloadTypeFEC(const WebRtc_Word8 payloadType);

private:
    RTPReceiverVideo*        _owner;
    ForwardErrorCorrection* _fec;
    ListWrapper                _receivedPacketList;
    ListWrapper                _recoveredPacketList;
    WebRtc_Word8              _payloadTypeFEC;
    WebRtc_UWord16            _lastFECSeqNum;
    bool                    _frameComplete;
};
} // namespace webrtc

#endif // WEBRTC_MODULES_RTP_RTCP_SOURCE_RECEIVER_FEC_H_
