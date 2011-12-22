/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

// This sub-API supports the following functionalities:
//  - Callbacks for RTP and RTCP events such as modified SSRC or CSRC.
//  - SSRC handling.
//  - Transmission of RTCP reports.
//  - Obtaining RTCP data from incoming RTCP sender reports.
//  - RTP and RTCP statistics (jitter, packet loss, RTT etc.).
//  - Forward Error Correction (FEC).
//  - RTP Keep‐alive for maintaining the NAT mappings associated to RTP flows.
//  - Writing RTP and RTCP packets to binary files for off‐line analysis of the
//    call quality.
//  - Inserting extra RTP packets into active audio stream.


#ifndef WEBRTC_VIDEO_ENGINE_MAIN_INTERFACE_VIE_RTP_RTCP_H_
#define WEBRTC_VIDEO_ENGINE_MAIN_INTERFACE_VIE_RTP_RTCP_H_

#include "common_types.h"

namespace webrtc
{
class VideoEngine;

// This enumerator sets the RTCP mode.
enum ViERTCPMode
{
    kRtcpNone = 0,
    kRtcpCompound_RFC4585 = 1,
    kRtcpNonCompound_RFC5506 = 2
};

// This enumerator describes the key frame request mode.
enum ViEKeyFrameRequestMethod
{
    kViEKeyFrameRequestNone = 0,
    kViEKeyFrameRequestPliRtcp = 1,
    kViEKeyFrameRequestFirRtp = 2,
    kViEKeyFrameRequestFirRtcp = 3
};

enum StreamType
{
    kViEStreamTypeNormal = 0,  // Normal media stream
    kViEStreamTypeRtx = 1      // Retransmission media stream
};

// ----------------------------------------------------------------------------
// ViERTPObserver
// ----------------------------------------------------------------------------

// This class declares an abstract interface for a user defined observer. It is
// up to the VideoEngine user to implement a derived class which implements the
// observer class. The observer is registered using RegisterRTPObserver() and
// deregistered using DeregisterRTPObserver().
class WEBRTC_DLLEXPORT ViERTPObserver
{
public:
    // This method is called if SSRC of the incoming stream is changed.
    virtual void IncomingSSRCChanged(const int videoChannel,
                                     const unsigned int SSRC) = 0;

    // This method is called if a field in CSRC changes or if the number of
    // CSRCs changes.
    virtual void IncomingCSRCChanged(const int videoChannel,
                                     const unsigned int CSRC,
                                     const bool added) = 0;
protected:
    virtual ~ViERTPObserver() {}
};

// ----------------------------------------------------------------------------
// ViERTCPObserver
// ----------------------------------------------------------------------------

// This class declares an abstract interface for a user defined observer. It is
// up to the VideoEngine user to implement a derived class which implements the
// observer class. The observer is registered using RegisterRTCPObserver() and
// deregistered using DeregisterRTCPObserver().

class WEBRTC_DLLEXPORT ViERTCPObserver
{
public:
    // This method is called if a application-defined RTCP packet has been
    // received.
    virtual void OnApplicationDataReceived(
        const int videoChannel, const unsigned char subType,
        const unsigned int name, const char* data,
        const unsigned short dataLengthInBytes) = 0;
protected:
    virtual ~ViERTCPObserver() {}
};

//
class WEBRTC_DLLEXPORT ViERTP_RTCP
{
public:
    // Default values
    enum
    {
        KDefaultDeltaTransmitTimeSeconds = 15
    };
    enum
    {
        KMaxRTCPCNameLength = 256
    };

    // Factory for the ViERTP_RTCP sub‐API and increases an internal reference
    // counter if successful. Returns NULL if the API is not supported or if
    // construction fails.
    static ViERTP_RTCP* GetInterface(VideoEngine* videoEngine);

    // Releases the ViERTP_RTCP sub-API and decreases an internal reference
    // counter. Returns the new reference count. This value should be zero
    // for all sub-API:s before the VideoEngine object can be safely deleted.
    virtual int Release() = 0;

    // This function enables you to specify the RTP synchronization source
    // identifier (SSRC) explicitly.
    virtual int SetLocalSSRC(const int videoChannel,
                             const unsigned int SSRC,
                             const StreamType usage = kViEStreamTypeNormal,
                             const unsigned char simulcastIdx = 0) = 0;

    // This function gets the SSRC for the outgoing RTP stream for the specified
    // channel.
    virtual int GetLocalSSRC(const int videoChannel,
                             unsigned int& SSRC) const = 0;

    // This function map a incoming SSRC to a StreamType so that the engine
    // can know which is the normal stream and which is the RTX
    virtual int SetRemoteSSRCType(const int videoChannel,
                                  const StreamType usage,
                                  const unsigned int SSRC) const = 0;

    // This function gets the SSRC for the incoming RTP stream for the specified
    // channel.
    virtual int GetRemoteSSRC(const int videoChannel,
                              unsigned int& SSRC) const = 0;

    // This function returns the CSRCs of the incoming RTP packets.
    virtual int GetRemoteCSRCs(const int videoChannel,
                               unsigned int CSRCs[kRtpCsrcSize]) const = 0;

    // This function enables manual initialization of the sequence number. The
    // start sequence number is normally a random number.
    virtual int SetStartSequenceNumber(const int videoChannel,
                                       unsigned short sequenceNumber) = 0;

    // This function sets the RTCP status for the specified channel.
    // Default mode is kRtcpCompound_RFC4585.
    virtual int SetRTCPStatus(const int videoChannel,
                              const ViERTCPMode rtcpMode) = 0;

    // This function gets the RTCP status for the specified channel.
    virtual int GetRTCPStatus(const int videoChannel,
                              ViERTCPMode& rtcpMode) const = 0;

    // This function sets the RTCP canonical name (CNAME) for the RTCP reports
    // on a specific channel.
    virtual int SetRTCPCName(const int videoChannel,
                             const char rtcpCName[KMaxRTCPCNameLength]) = 0;

    // This function gets the RTCP canonical name (CNAME) for the RTCP reports
    // sent the specified channel.
    virtual int GetRTCPCName(const int videoChannel,
                             char rtcpCName[KMaxRTCPCNameLength]) const = 0;

    // This function gets the RTCP canonical name (CNAME) for the RTCP reports
    // received on the specified channel.
    virtual int GetRemoteRTCPCName(
        const int videoChannel, char rtcpCName[KMaxRTCPCNameLength]) const = 0;

    // This function sends an RTCP APP packet on a specific channel.
    virtual int SendApplicationDefinedRTCPPacket(
        const int videoChannel, const unsigned char subType,
        unsigned int name, const char* data,
        unsigned short dataLengthInBytes) = 0;

    // This function enables Negative Acknowledgment (NACK) using RTCP,
    // implemented based on RFC 4585. NACK retransmits RTP packets if lost on
    // the network. This creates a lossless transport at the expense of delay.
    // If using NACK, NACK should be enabled on both endpoints in a call.
    virtual int SetNACKStatus(const int videoChannel, const bool enable) = 0;

    // This function enables Forward Error Correction (FEC) using RTCP,
    // implemented based on RFC 5109, to improve packet loss robustness. Extra
    // FEC packets are sent together with the usual media packets, hence
    // part of the bitrate will be used for FEC packets.
    virtual int SetFECStatus(const int videoChannel, const bool enable,
                             const unsigned char payloadTypeRED,
                             const unsigned char payloadTypeFEC) = 0;

    // This function enables hybrid Negative Acknowledgment using RTCP
    // and Forward Error Correction (FEC) implemented based on RFC 5109,
    // to improve packet loss robustness. Extra
    // FEC packets are sent together with the usual media packets, hence will
    // part of the bitrate be used for FEC packets.
    // The hybrid mode will choose between nack only, fec only and both based on
    // network conditions. When both are applied, only packets that were not
    // recovered by the FEC will be nacked.
    virtual int SetHybridNACKFECStatus(const int videoChannel,
                                       const bool enable,
                                       const unsigned char payloadTypeRED,
                                       const unsigned char payloadTypeFEC) = 0;

    // This function enables RTCP key frame requests.
    virtual int SetKeyFrameRequestMethod(
        const int videoChannel, const ViEKeyFrameRequestMethod method) = 0;

    // This function enables signaling of temporary bitrate constraints using
    // RTCP, implemented based on RFC4585.
    virtual int SetTMMBRStatus(const int videoChannel, const bool enable) = 0;

    // Enables and disables REMB packets for this channel. |sender| indicates
    // this channel is encoding, |receiver| tells the bitrate estimate for
    // this channel should be included in the REMB packet.
    virtual bool SetRembStatus(int video_channel, bool sender,
                               bool receiver) = 0;

    // The function gets statistics from the received RTCP report.
    virtual int GetReceivedRTCPStatistics(
        const int videoChannel, unsigned short& fractionLost,
        unsigned int& cumulativeLost, unsigned int& extendedMax,
        unsigned int& jitter, int& rttMs) const = 0;

    // The function gets statistics from the RTCP report sent to the receiver.
    virtual int GetSentRTCPStatistics(const int videoChannel,
                                      unsigned short& fractionLost,
                                      unsigned int& cumulativeLost,
                                      unsigned int& extendedMax,
                                      unsigned int& jitter,
                                      int& rttMs) const = 0;

    // The function gets statistics from the sent and received RTP streams.
    virtual int GetRTPStatistics(const int videoChannel,
                                 unsigned int& bytesSent,
                                 unsigned int& packetsSent,
                                 unsigned int& bytesReceived,
                                 unsigned int& packetsReceived) const = 0;

    // The function gets bandwidth usage statistics from the sent RTP streams in
    // bits/s.
    virtual int GetBandwidthUsage(const int videoChannel,
                                  unsigned int& totalBitrateSent,
                                  unsigned int& videoBitrateSent,
                                  unsigned int& fecBitrateSent,
                                  unsigned int& nackBitrateSent) const = 0;

    // This function enables or disables an RTP keep-alive mechanism which can
    // be used to maintain an existing Network Address Translator (NAT) mapping
    // while regular RTP is no longer transmitted.
    virtual int SetRTPKeepAliveStatus(
        const int videoChannel, bool enable, const char unknownPayloadType,
        const unsigned int deltaTransmitTimeSeconds =
            KDefaultDeltaTransmitTimeSeconds) = 0;

    // This function gets the RTP keep-alive status.
    virtual int GetRTPKeepAliveStatus(
        const int videoChannel, bool& enabled, char& unkownPayloadType,
        unsigned int& deltaTransmitTimeSeconds) const = 0;

    // This function enables capturing of RTP packets to a binary file on a
    // specific channel and for a given direction. The file can later be
    // replayed using e.g. RTP Tools rtpplay since the binary file format is
    // compatible with the rtpdump format.
    virtual int StartRTPDump(const int videoChannel,
                             const char fileNameUTF8[1024],
                             RTPDirections direction) = 0;

    // This function disables capturing of RTP packets to a binary file on a
    // specific channel and for a given direction.
    virtual int StopRTPDump(const int videoChannel,
                            RTPDirections direction) = 0;

    // Registers an instance of a user implementation of the ViERTPObserver.
    virtual int RegisterRTPObserver(const int videoChannel,
                                    ViERTPObserver& observer) = 0;

    // Removes a registered instance of ViERTPObserver.
    virtual int DeregisterRTPObserver(const int videoChannel) = 0;

    // Registers an instance of a user implementation of the ViERTCPObserver.
    virtual int RegisterRTCPObserver(const int videoChannel,
                                     ViERTCPObserver& observer) = 0;

    // Removes a registered instance of ViERTCPObserver.
    virtual int DeregisterRTCPObserver(const int videoChannel) = 0;

protected:
    ViERTP_RTCP() {};
    virtual ~ViERTP_RTCP() {};
};
} // namespace webrtc
#endif  // WEBRTC_VIDEO_ENGINE_MAIN_INTERFACE_VIE_RTP_RTCP_H_
