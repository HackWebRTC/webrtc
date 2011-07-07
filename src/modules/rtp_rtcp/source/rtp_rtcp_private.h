/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MODULES_RTP_RTCP_SOURCE_RTP_RTCP_PRIVATE_H_
#define WEBRTC_MODULES_RTP_RTCP_SOURCE_RTP_RTCP_PRIVATE_H_

#include "bwe_defines.h"
#include "rtp_rtcp.h"
#include "tmmbr_help.h"
#include "rtp_utility.h"

namespace webrtc {
class ModuleRtpRtcpPrivate : public RtpRtcp
{
public:
    virtual void RegisterChildModule(RtpRtcp* module) = 0;
    virtual void DeRegisterChildModule(RtpRtcp* module) = 0;

    virtual WebRtc_Word32 RegisterVideoModule(RtpRtcp* videoModule) = 0;
    virtual void DeRegisterVideoModule() = 0;

    virtual void SetRemoteSSRC(const WebRtc_UWord32 SSRC) = 0;

    virtual WebRtc_Word8 SendPayloadType() const = 0;

    virtual RtpVideoCodecTypes ReceivedVideoCodec() const = 0;

    virtual RtpVideoCodecTypes SendVideoCodec() const = 0;

    // lipsync
    virtual void OnReceivedNTP() = 0;

    // bw estimation
    virtual void OnPacketLossStatisticsUpdate(const WebRtc_UWord8 fractionLost,
                                              const WebRtc_UWord16 roundTripTime,
                                              const WebRtc_UWord32 lastReceivedExtendedHighSeqNum,
                                              const WebRtc_UWord32 jitter) = 0;

    // bw estimation
    virtual void OnReceivedTMMBR() = 0;

    // bw estimation
    virtual void OnReceivedBandwidthEstimateUpdate( const WebRtc_UWord16 bwEstimateMinKbit,
                                                    const WebRtc_UWord16 bwEstimateMaxKbit ) = 0;

    //
    virtual RateControlRegion OnOverUseStateUpdate(const RateControlInput& rateControlInput) = 0;

    // received a request for a new key frame
    virtual void OnReceivedIntraFrameRequest(const WebRtc_UWord8 message) = 0;

    // received a request for a new SLI
    virtual void OnReceivedSliceLossIndication(const WebRtc_UWord8 pictureID) = 0;

    // received a new refereence frame
    virtual void OnReceivedReferencePictureSelectionIndication(const WebRtc_UWord64 pitureID) = 0;

    // request for a RTCP send report
    virtual void OnRequestSendReport() = 0;

    // Get remote SequenceNumber
    virtual WebRtc_UWord16 RemoteSequenceNumber() const = 0;

    virtual WebRtc_UWord32 PacketCountSent() const = 0;

    virtual int CurrentSendFrequencyHz() const = 0;

    virtual WebRtc_UWord32 ByteCountSent() const = 0;

    virtual WebRtc_UWord32 BitrateReceivedNow() const = 0;

    virtual WebRtc_UWord32 SendTimeOfSendReport(const WebRtc_UWord32 sendReport) = 0;

    virtual WebRtc_Word32 LastReceivedNTP(WebRtc_UWord32& NTPsecs, // when we received the last report
                                        WebRtc_UWord32& NTPfrac,
                                        WebRtc_UWord32& remoteSR) = 0; // NTP inside the last received (mid 16 bits from sec and frac)

    virtual WebRtc_Word32 ReportBlockStatistics(WebRtc_UWord8 *fraction_lost,
                                              WebRtc_UWord32 *cum_lost,
                                              WebRtc_UWord32 *ext_max,
                                              WebRtc_UWord32 *jitter) = 0;

    // bad state of RTP receiver request a keyframe
    virtual void OnRequestIntraFrame( const FrameType frameType) = 0;

    /*
    *   NACK
    */
    virtual void OnReceivedNACK(const WebRtc_UWord16 nackSequenceNumbersLength,
                                const WebRtc_UWord16* nackSequenceNumbers) = 0;

    /*
    *   TMMBR
    */
    virtual WebRtc_Word32 UpdateTMMBR() = 0;

    virtual WebRtc_Word32 SetTMMBN(const TMMBRSet* boundingSet,
                                 const WebRtc_UWord32 maxBitrateKbit) = 0;

    virtual WebRtc_Word32 BoundingSet(bool &tmmbrOwner,
                                    TMMBRSet*& boundingSetRec)= 0;

    virtual WebRtc_Word32 TMMBRReceived(const WebRtc_UWord32 size,
                                      const WebRtc_UWord32 accNumCandidates,
                                      TMMBRSet* candidateSet) const = 0;
};
} // namespace webrtc
#endif // WEBRTC_MODULES_RTP_RTCP_SOURCE_RTP_RTCP_PRIVATE_H_
