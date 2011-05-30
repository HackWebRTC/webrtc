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
 * vie_receiver.h
 */

#ifndef WEBRTC_VIDEO_ENGINE_MAIN_SOURCE_VIE_RECEIVER_H_
#define WEBRTC_VIDEO_ENGINE_MAIN_SOURCE_VIE_RECEIVER_H_

// Defines
#include "engine_configurations.h"
#include "vie_defines.h"
#include "typedefs.h"
#include "udp_transport.h"
#include "rtp_rtcp_defines.h"

#ifdef WEBRTC_SRTP
class SrtpModule;
#endif

namespace webrtc
{
class CriticalSectionWrapper;
// Forward declarations
class RtpDump;
class RtpRtcp;
class VideoCodingModule;
class Encryption;

class ViEReceiver: public UdpTransportData,
    public RtpData,
    public RtpVideoFeedback
{
public:
    ViEReceiver(int engineId, int channelId, RtpRtcp& moduleRtpRtcp,
                webrtc::VideoCodingModule& moduleVcm);
    ~ViEReceiver();

    int RegisterExternalDecryption(Encryption* decryption);
    int DeregisterExternalDecryption();

#ifdef WEBRTC_SRTP
    int RegisterSRTPModule(SrtpModule* srtpModule);
    int DeregisterSRTPModule();

    int RegisterSRTCPModule(SrtpModule* srtpModule);
    int DeregisterSRTCPModule();
#endif

    void StartReceive();
    void StopReceive();
    int StartRTPDump(const char fileNameUTF8[1024]);
    int StopRTPDump();

    // From SocketTransportData, receiving packets from the socket
    virtual void IncomingRTPPacket(const WebRtc_Word8* incomingRtpPacket,
                                   const WebRtc_Word32 incomingRtpPacketLength,
                                   const WebRtc_Word8* fromIP,
                                   const WebRtc_UWord16 fromPort);
    virtual void IncomingRTCPPacket(const WebRtc_Word8* incomingRtcpPacket,
                                    const WebRtc_Word32 incomingRtcpPacketLength,
                                    const WebRtc_Word8* fromIP,
                                    const WebRtc_UWord16 fromPort);

    // Receives packets from external transport
    int ReceivedRTPPacket(const void* rtpPacket, int rtpPacketLength);

    int ReceivedRTCPPacket(const void* rtcpPacket, int rtcpPacketLength);

    // From RtpData, callback for data from RTP module
    virtual WebRtc_Word32
    OnReceivedPayloadData(const WebRtc_UWord8* payloadData,
                              const WebRtc_UWord16 payloadSize,
                              const WebRtcRTPHeader* rtpHeader);

    // Implements RtpVideoFeedback
    virtual void OnReceivedIntraFrameRequest(const WebRtc_Word32 id,
                                             const WebRtc_UWord8 message = 0);
    virtual void OnNetworkChanged(const WebRtc_Word32 id,
                                  const WebRtc_UWord32 minBitrateBps,
                                  const WebRtc_UWord32 maxBitrateBps,
                                  const WebRtc_UWord8 fractionLost,
                                  const WebRtc_UWord16 roundTripTimeMs,
                                  const WebRtc_UWord16 bwEstimateKbitMin,
                                  const WebRtc_UWord16 bwEstimateKbitMax);
private:
    int InsertRTPPacket(const WebRtc_Word8* rtpPacket, int rtpPacketLength);
    int InsertRTCPPacket(const WebRtc_Word8* rtcpPacket, int rtcpPacketLength);
    // Registered members
    CriticalSectionWrapper& _receiveCritsect;
    int _engineId;
    int _channelId;
    RtpRtcp& _rtpRtcp;
    VideoCodingModule& _vcm;

#ifdef WEBRTC_SRTP
    SrtpModule* _ptrSrtp;
    SrtpModule* _ptrSrtcp;
    WebRtc_UWord8* _ptrSrtpBuffer;
    WebRtc_UWord8* _ptrSrtcpBuffer;
#endif
    Encryption* _ptrExternalDecryption;
    WebRtc_UWord8* _ptrDecryptionBuffer;
    RtpDump* _rtpDump;
    bool _receiving; // Only needed to protect external transport
};
} // namespace webrt
#endif  //  WEBRTC_VIDEO_ENGINE_MAIN_SOURCE_VIE_RECEIVER_H_
