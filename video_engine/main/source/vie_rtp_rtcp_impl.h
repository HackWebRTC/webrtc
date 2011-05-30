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
 * vie_rtp_rtcp_impl.h
 */

#ifndef WEBRTC_VIDEO_ENGINE_MAIN_SOURCE_VIE_RTP_RTCP_IMPL_H_
#define WEBRTC_VIDEO_ENGINE_MAIN_SOURCE_VIE_RTP_RTCP_IMPL_H_

#include "vie_defines.h"

#include "rtp_rtcp_defines.h"
#include "typedefs.h"
#include "vie_ref_count.h"
#include "vie_rtp_rtcp.h"
#include "vie_shared_data.h"

namespace webrtc
{

// ----------------------------------------------------------------------------
//	ViERTP_RTCPImpl
// ----------------------------------------------------------------------------

class ViERTP_RTCPImpl : public virtual ViESharedData,
                        public ViERTP_RTCP,
                        public ViERefCount
{
public:
    virtual int Release();

    // SSRC/CSRC
    virtual int SetLocalSSRC(const int videoChannel, const unsigned int SSRC);

    virtual int GetLocalSSRC(const int videoChannel, unsigned int& SSRC) const;

    virtual int GetRemoteSSRC(const int videoChannel, unsigned int& SSRC) const;

    virtual int GetRemoteCSRCs(const int videoChannel,
                               unsigned int CSRCs[kRtpCsrcSize]) const;

    virtual int SetStartSequenceNumber(const int videoChannel,
                                       unsigned short sequenceNumber);

    // RTCP
    virtual int SetRTCPStatus(const int videoChannel,
                              const ViERTCPMode rtcpMode);

    virtual int GetRTCPStatus(const int videoChannel, ViERTCPMode& rtcpMode);

    virtual int SetRTCPCName(const int videoChannel,
                             const char rtcpCName[KMaxRTCPCNameLength]);

    virtual int GetRTCPCName(const int videoChannel,
                             char rtcpCName[KMaxRTCPCNameLength]);

    virtual int GetRemoteRTCPCName(const int videoChannel,
                                   char rtcpCName[KMaxRTCPCNameLength]) const;

    virtual int
        SendApplicationDefinedRTCPPacket(const int videoChannel,
                                         const unsigned char subType,
                                         unsigned int name, const char* data,
                                         unsigned short dataLengthInBytes);

    virtual int SetNACKStatus(const int videoChannel, const bool enable);

    virtual int SetFECStatus(const int videoChannel, const bool enable,
                             const unsigned char payloadTypeRED,
                             const unsigned char payloadTypeFEC);

    virtual int SetKeyFrameRequestMethod(const int videoChannel,
                                         const ViEKeyFrameRequestMethod method);

    virtual int SetTMMBRStatus(const int videoChannel, const bool enable);

    // Statistics
    virtual int GetReceivedRTCPStatistics(
        const int videoChannel, unsigned short& fractionLost,
        unsigned int& cumulativeLost, unsigned int& extendedMax,
        unsigned int& jitter, int& rttMs) const;

    virtual int GetSentRTCPStatistics(const int videoChannel,
                                      unsigned short& fractionLost,
                                      unsigned int& cumulativeLost,
                                      unsigned int& extendedMax,
                                      unsigned int& jitter, int& rttMs) const;

    virtual int GetRTPStatistics(const int videoChannel,
                                 unsigned int& bytesSent,
                                 unsigned int& packetsSent,
                                 unsigned int& bytesReceived,
                                 unsigned int& packetsReceived) const;

    // Keep alive
    virtual int SetRTPKeepAliveStatus(
        const int videoChannel, bool enable, const char unknownPayloadType,
        const unsigned int deltaTransmitTimeSeconds);

    virtual int GetRTPKeepAliveStatus(const int videoChannel, bool& enabled,
                                      char& unkownPayloadType,
                                      unsigned int& deltaTransmitTimeSeconds);

    // Dump RTP stream, for debug purpose
    virtual int StartRTPDump(const int videoChannel,
                             const char fileNameUTF8[1024],
                             RTPDirections direction);

    virtual int StopRTPDump(const int videoChannel, RTPDirections direction);

    // Callbacks
    virtual int RegisterRTPObserver(const int videoChannel,
                                    ViERTPObserver& observer);

    virtual int DeregisterRTPObserver(const int videoChannel);

    virtual int RegisterRTCPObserver(const int videoChannel,
                                     ViERTCPObserver& observer);

    virtual int DeregisterRTCPObserver(const int videoChannel);

protected:
    ViERTP_RTCPImpl();
    virtual ~ViERTP_RTCPImpl();

private:
    RTCPMethod ViERTCPModeToRTCPMethod(ViERTCPMode apiMode);
    ViERTCPMode RTCPMethodToViERTCPMode(RTCPMethod moduleMethod);
    KeyFrameRequestMethod
    APIRequestToModuleRequest(const ViEKeyFrameRequestMethod apiMethod);
};
} // namespace webrtc
#endif  // WEBRTC_VIDEO_ENGINE_MAIN_SOURCE_VIE_RTP_RTCP_IMPL_H_
