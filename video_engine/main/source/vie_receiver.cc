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
 * ViEChannel.cpp
 */

#include "vie_receiver.h"

#include "critical_section_wrapper.h"
#include "rtp_rtcp.h"
#ifdef WEBRTC_SRTP
#include "SrtpModule.h"
#endif
#include "video_coding.h"
#include "rtp_dump.h"
#include "trace.h"

namespace webrtc {

// ----------------------------------------------------------------------------
// Constructor
// ----------------------------------------------------------------------------

ViEReceiver::ViEReceiver(int engineId, int channelId,
                         RtpRtcp& moduleRtpRtcp,
                         VideoCodingModule& moduleVcm)
    :   _receiveCritsect(*CriticalSectionWrapper::CreateCriticalSection()),
        _engineId(engineId), _channelId(channelId), _rtpRtcp(moduleRtpRtcp),
        _vcm(moduleVcm),
#ifdef WEBRTC_SRTP
        _ptrSrtp(NULL),
        _ptrSrtcp(NULL),
        _ptrSrtpBuffer(NULL),
        _ptrSrtcpBuffer(NULL),
#endif
        _ptrExternalDecryption(NULL), _ptrDecryptionBuffer(NULL),
        _rtpDump(NULL), _receiving(false)
{
    _rtpRtcp.RegisterIncomingVideoCallback(this);
}

// ----------------------------------------------------------------------------
// Destructor
// ----------------------------------------------------------------------------

ViEReceiver::~ViEReceiver()
{
    delete &_receiveCritsect;
#ifdef WEBRTC_SRTP
    if (_ptrSrtpBuffer)
    {
        delete [] _ptrSrtpBuffer;
        _ptrSrtpBuffer = NULL;
    }
    if (_ptrSrtcpBuffer)
    {
        delete [] _ptrSrtcpBuffer;
        _ptrSrtcpBuffer = NULL;
    }
#endif
    if (_ptrDecryptionBuffer)
    {
        delete[] _ptrDecryptionBuffer;
        _ptrDecryptionBuffer = NULL;
    }
    if (_rtpDump)
    {
        _rtpDump->Stop();
        RtpDump::DestroyRtpDump(_rtpDump);
        _rtpDump = NULL;
    }
}

// ============================================================================
// Decryption
// ============================================================================

// ----------------------------------------------------------------------------
// RegisterExternalDecryption
// ----------------------------------------------------------------------------

int ViEReceiver::RegisterExternalDecryption(Encryption* decryption)
{
    CriticalSectionScoped cs(_receiveCritsect);
    if (_ptrExternalDecryption)
    {
        return -1;
    }
    _ptrDecryptionBuffer = new WebRtc_UWord8[kViEMaxMtu];
    if (_ptrDecryptionBuffer == NULL)
    {
        return -1;
    }
    _ptrExternalDecryption = decryption;
    return 0;
}

// ----------------------------------------------------------------------------
// DeregisterExternalDecryption
// ----------------------------------------------------------------------------

int ViEReceiver::DeregisterExternalDecryption()
{
    CriticalSectionScoped cs(_receiveCritsect);
    if (_ptrExternalDecryption == NULL)
    {
        return -1;
    }
    _ptrExternalDecryption = NULL;
    return 0;
}

#ifdef WEBRTC_SRTP
// ----------------------------------------------------------------------------
// RegisterSRTPModule
// ----------------------------------------------------------------------------

int ViEReceiver::RegisterSRTPModule(SrtpModule* srtpModule)
{
    CriticalSectionScoped cs(_receiveCritsect);
    if (_ptrSrtp ||
        srtpModule == NULL)
    {
        return -1;
    }
    _ptrSrtpBuffer = new WebRtc_UWord8[kViEMaxMtu];
    if (_ptrSrtpBuffer == NULL)
    {
        return -1;
    }
    _ptrSrtp = srtpModule;

    return 0;
}

// ----------------------------------------------------------------------------
// DeregisterSRTPModule
// ----------------------------------------------------------------------------

int ViEReceiver::DeregisterSRTPModule()
{
    CriticalSectionScoped cs(_receiveCritsect);
    if (_ptrSrtp == NULL)
    {
        return -1;
    }
    if (_ptrSrtpBuffer)
    {
        delete [] _ptrSrtpBuffer;
        _ptrSrtpBuffer = NULL;
    }
    _ptrSrtp = NULL;
    return 0;
}

// ----------------------------------------------------------------------------
// RegisterSRTCPModule
// ----------------------------------------------------------------------------

int ViEReceiver::RegisterSRTCPModule(SrtpModule* srtcpModule)
{
    CriticalSectionScoped cs(_receiveCritsect);
    if (_ptrSrtcp ||
        srtcpModule == NULL)
    {
        return -1;
    }
    _ptrSrtcpBuffer = new WebRtc_UWord8[kViEMaxMtu];
    if (_ptrSrtcpBuffer == NULL)
    {
        return -1;
    }
    _ptrSrtcp = srtcpModule;

    return 0;
}

// ----------------------------------------------------------------------------
// DeregisterSRTPCModule
// ----------------------------------------------------------------------------

int ViEReceiver::DeregisterSRTCPModule()
{
    CriticalSectionScoped cs(_receiveCritsect);
    if (_ptrSrtcp == NULL)
    {
        return -1;
    }
    if (_ptrSrtcpBuffer)
    {
        delete [] _ptrSrtcpBuffer;
        _ptrSrtcpBuffer = NULL;
    }
    _ptrSrtcp = NULL;
    return 0;
}

#endif

// ----------------------------------------------------------------------------
// IncomingRTPPacket
//
// Receives RTP packets from SocketTransport
// ----------------------------------------------------------------------------

void ViEReceiver::IncomingRTPPacket(const WebRtc_Word8* incomingRtpPacket,
                                    const WebRtc_Word32 incomingRtpPacketLength,
                                    const WebRtc_Word8* fromIP,
                                    const WebRtc_UWord16 fromPort)
{
    InsertRTPPacket(incomingRtpPacket, incomingRtpPacketLength);
    return;
}

// ----------------------------------------------------------------------------
// IncomingRTCPPacket
//
// Receives RTCP packets from SocketTransport
// ----------------------------------------------------------------------------

void ViEReceiver::IncomingRTCPPacket(const WebRtc_Word8* incomingRtcpPacket,
                                     const WebRtc_Word32 incomingRtcpPacketLength,
                                     const WebRtc_Word8* fromIP,
                                     const WebRtc_UWord16 fromPort)
{
    InsertRTCPPacket(incomingRtcpPacket, incomingRtcpPacketLength);
    return;
}

// ----------------------------------------------------------------------------
// ReceivedRTPPacket
//
// Receives RTP packets from external transport
// ----------------------------------------------------------------------------

int ViEReceiver::ReceivedRTPPacket(const void* rtpPacket, int rtpPacketLength)
{
    if (!_receiving)
    {
        return -1;
    }
    return InsertRTPPacket((const WebRtc_Word8*) rtpPacket, rtpPacketLength);
}

// ----------------------------------------------------------------------------
// ReceivedRTCPPacket
//
// Receives RTCP packets from external transport
// ----------------------------------------------------------------------------

int ViEReceiver::ReceivedRTCPPacket(const void* rtcpPacket,
                                    int rtcpPacketLength)
{
    if (!_receiving)
    {
        return -1;
    }
    return InsertRTCPPacket((const WebRtc_Word8*) rtcpPacket, rtcpPacketLength);
}

// ----------------------------------------------------------------------------
// OnReceivedPayloadData
//
// From RtpData, callback for data from RTP module
// ----------------------------------------------------------------------------
WebRtc_Word32 ViEReceiver::OnReceivedPayloadData(const WebRtc_UWord8* payloadData,
                                                 const WebRtc_UWord16 payloadSize,
                                                 const WebRtcRTPHeader* rtpHeader)
{
    if (rtpHeader == NULL)
    {
        return 0;
    }
    if (rtpHeader->frameType == webrtc::kFrameEmpty)
    {
        // Don't care about empty rtp packets, we might
        // get this e.g. when using FEC
        return 0;
    }
    if (_vcm.IncomingPacket(payloadData, payloadSize, *rtpHeader) != 0)
    {
        // Check this...
        return -1;
    }
    return 0;
}

// ============================================================================
// Private methods
// ============================================================================

// ----------------------------------------------------------------------------
// InsertRTPPacket
// ----------------------------------------------------------------------------

int ViEReceiver::InsertRTPPacket(const WebRtc_Word8* rtpPacket,
                                 int rtpPacketLength)
{
    WebRtc_UWord8* receivedPacket = (WebRtc_UWord8*) (rtpPacket);
    int receivedPacketLength = rtpPacketLength;

    {
        CriticalSectionScoped cs(_receiveCritsect);

        if (_ptrExternalDecryption)
        {
            int decryptedLength = 0;
            _ptrExternalDecryption->decrypt(_channelId, receivedPacket,
                                            _ptrDecryptionBuffer,
                                            (int) receivedPacketLength,
                                            (int*) &decryptedLength);
            if (decryptedLength <= 0)
            {
                WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideo, ViEId(_engineId,
                                                                 _channelId),
                           "RTP decryption failed");
                return -1;
            } else if (decryptedLength > kViEMaxMtu)
            {
                WEBRTC_TRACE(webrtc::kTraceCritical, webrtc::kTraceVideo,
                           ViEId(_engineId, _channelId),
                           "  %d bytes is allocated as RTP decrytption output => memory is now corrupted",
                           kViEMaxMtu);
                return -1;
            }
            receivedPacket = _ptrDecryptionBuffer;
            receivedPacketLength = decryptedLength;
        }
#ifdef WEBRTC_SRTP
        if (_ptrSrtp)
        {
            int decryptedLength = 0;
            _ptrSrtp->decrypt(_channelId, receivedPacket, _ptrSrtpBuffer, receivedPacketLength, &decryptedLength);
            if (decryptedLength <= 0)
            {
                WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideo, ViEId(_engineId, _channelId), "RTP decryption failed");
                return -1;
            }
            else if (decryptedLength > kViEMaxMtu)
            {
                WEBRTC_TRACE(webrtc::kTraceCritical, webrtc::kTraceVideo,ViEId(_engineId, _channelId), "  %d bytes is allocated as RTP decrytption output => memory is now corrupted", kViEMaxMtu);
                return -1;
            }
            receivedPacket = _ptrSrtpBuffer;
            receivedPacketLength = decryptedLength;
        }
#endif
        if (_rtpDump)
        {
            _rtpDump->DumpPacket(receivedPacket,
                                 (WebRtc_UWord16) receivedPacketLength);
        }
    }
    return _rtpRtcp.IncomingPacket(receivedPacket, receivedPacketLength);
}

// ----------------------------------------------------------------------------
// InsertRTCPPacket
// ----------------------------------------------------------------------------

int ViEReceiver::InsertRTCPPacket(const WebRtc_Word8* rtcpPacket,
                                  int rtcpPacketLength)
{
    WebRtc_UWord8* receivedPacket = (WebRtc_UWord8*) rtcpPacket;
    int receivedPacketLength = rtcpPacketLength;
    {
        CriticalSectionScoped cs(_receiveCritsect);

        if (_ptrExternalDecryption)
        {
            int decryptedLength = 0;
            _ptrExternalDecryption->decrypt_rtcp(_channelId, receivedPacket,
                                                 _ptrDecryptionBuffer,
                                                 (int) receivedPacketLength,
                                                 (int*) &decryptedLength);
            if (decryptedLength <= 0)
            {
                WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideo, ViEId(_engineId,
                                                                 _channelId),
                           "RTP decryption failed");
                return -1;
            } else if (decryptedLength > kViEMaxMtu)
            {
                WEBRTC_TRACE(
                           webrtc::kTraceCritical,
                           webrtc::kTraceVideo,
                           ViEId(_engineId, _channelId),
                           "  %d bytes is allocated as RTP decrytption output => memory is now corrupted",
                           kViEMaxMtu);
                return -1;
            }
            receivedPacket = _ptrDecryptionBuffer;
            receivedPacketLength = decryptedLength;
        }
#ifdef WEBRTC_SRTP
        if (_ptrSrtcp)
        {
            int decryptedLength = 0;
            _ptrSrtcp->decrypt_rtcp(_channelId, receivedPacket, _ptrSrtcpBuffer, (int) receivedPacketLength, (int*) &decryptedLength);
            if (decryptedLength <= 0)
            {
                WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideo, ViEId(_engineId, _channelId), "RTP decryption failed");
                return -1;
            }
            else if (decryptedLength > kViEMaxMtu)
            {
                WEBRTC_TRACE(webrtc::kTraceCritical, webrtc::kTraceVideo, ViEId(_engineId, _channelId), "  %d bytes is allocated as RTP decrytption output => memory is now corrupted", kViEMaxMtu);
                return -1;
            }
            receivedPacket = _ptrSrtcpBuffer;
            receivedPacketLength = decryptedLength;
        }
#endif
        if (_rtpDump)
        {
            _rtpDump->DumpPacket(receivedPacket,
                                 (WebRtc_UWord16) receivedPacketLength);
        }
    }
    return _rtpRtcp.IncomingPacket(receivedPacket, receivedPacketLength);
}

// ----------------------------------------------------------------------------
// StartReceive
//
// Only used for external transport
// ----------------------------------------------------------------------------

void ViEReceiver::StartReceive()
{
    _receiving = true;
}

// ----------------------------------------------------------------------------
// StopReceive
//
// Only used for external transport
// ----------------------------------------------------------------------------

void ViEReceiver::StopReceive()
{
    _receiving = false;
}

// ----------------------------------------------------------------------------
// StartRTPDump
// ----------------------------------------------------------------------------

int ViEReceiver::StartRTPDump(const char fileNameUTF8[1024])
{
    CriticalSectionScoped cs(_receiveCritsect);
    if (_rtpDump)
    {
        // Restart it if it already exists and is started
        _rtpDump->Stop();
    } else
    {
        _rtpDump = RtpDump::CreateRtpDump();
        if (_rtpDump == NULL)
        {
            WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideo, ViEId(_engineId,
                                                             _channelId),
                       "%s: Failed to create RTP dump", __FUNCTION__);
            return -1;
        }
    }
    if (_rtpDump->Start(fileNameUTF8) != 0)
    {
        RtpDump::DestroyRtpDump(_rtpDump);
        _rtpDump = NULL;
        WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideo,
                   ViEId(_engineId, _channelId),
                   "%s: Failed to start RTP dump", __FUNCTION__);
        return -1;
    }
    return 0;
}

// ----------------------------------------------------------------------------
// StopRTPDump
// ----------------------------------------------------------------------------

int ViEReceiver::StopRTPDump()
{
    CriticalSectionScoped cs(_receiveCritsect);
    if (_rtpDump)
    {
        if (_rtpDump->IsActive())
        {
            _rtpDump->Stop();
        } else
        {
            WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideo, ViEId(_engineId,
                                                             _channelId),
                       "%s: Dump not active", __FUNCTION__);
        }
        RtpDump::DestroyRtpDump(_rtpDump);
        _rtpDump = NULL;
    } else
    {
        WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideo,
                   ViEId(_engineId, _channelId), "%s: RTP dump not started",
                   __FUNCTION__);
        return -1;
    }
    return 0;
}

// Implements RtpVideoFeedback
void ViEReceiver::OnReceivedIntraFrameRequest(const WebRtc_Word32 id,
                                              const WebRtc_UWord8 message)
{
    // Don't do anything, action trigged on default module
    return;
}

void ViEReceiver::OnNetworkChanged(const WebRtc_Word32 id,
                                   const WebRtc_UWord32 minBitrateBps,
                                   const WebRtc_UWord32 maxBitrateBps,
                                   const WebRtc_UWord8 fractionLost,
                                   const WebRtc_UWord16 roundTripTimeMs,
                                   const WebRtc_UWord16 bwEstimateKbitMin,
                                   const WebRtc_UWord16 bwEstimateKbitMax)
{
    // Called for default module
    return;
}
} // namespace webrtc
