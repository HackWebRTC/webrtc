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
 * vie_sender.cc
 */

#include "vie_sender.h"

#include "critical_section_wrapper.h"
#include "rtp_rtcp.h"
#ifdef WEBRTC_SRTP
#include "SrtpModule.h"
#endif
#include "rtp_dump.h"
#include "trace.h"

namespace webrtc {

// ----------------------------------------------------------------------------
// Constructor
// ----------------------------------------------------------------------------

ViESender::ViESender(int engineId, int channelId,
                     RtpRtcp& rtpRtcpModule)
    : _engineId(engineId), _channelId(channelId),
      _sendCritsect(*CriticalSectionWrapper::CreateCriticalSection()),
      _rtpRtcp(rtpRtcpModule),
#ifdef WEBRTC_SRTP
      _ptrSrtp(NULL),
      _ptrSrtcp(NULL),
#endif
      _ptrExternalEncryption(NULL), _ptrSrtpBuffer(NULL),
      _ptrSrtcpBuffer(NULL), _ptrEncryptionBuffer(NULL), _ptrTransport(NULL),
      _rtpDump(NULL)
{
}

// ----------------------------------------------------------------------------
// Destructor
// ----------------------------------------------------------------------------
ViESender::~ViESender()
{
    delete &_sendCritsect;

    if (_ptrSrtpBuffer)
    {
        delete[] _ptrSrtpBuffer;
        _ptrSrtpBuffer = NULL;
    }
    if (_ptrSrtcpBuffer)
    {
        delete[] _ptrSrtcpBuffer;
        _ptrSrtcpBuffer = NULL;
    }
    if (_ptrEncryptionBuffer)
    {
        delete[] _ptrEncryptionBuffer;
        _ptrEncryptionBuffer = NULL;
    }
    if (_rtpDump)
    {
        _rtpDump->Stop();
        RtpDump::DestroyRtpDump(_rtpDump);
        _rtpDump = NULL;
    }
}

// ----------------------------------------------------------------------------
// RegisterExternalEncryption
// ----------------------------------------------------------------------------

int ViESender::RegisterExternalEncryption(Encryption* encryption)
{
    CriticalSectionScoped cs(_sendCritsect);
    if (_ptrExternalEncryption)
    {
        return -1;
    }
    _ptrEncryptionBuffer = new WebRtc_UWord8[kViEMaxMtu];
    if (_ptrEncryptionBuffer == NULL)
    {
        return -1;
    }
    _ptrExternalEncryption = encryption;
    return 0;
}

// ----------------------------------------------------------------------------
// DeregisterExternalEncryption
// ----------------------------------------------------------------------------

int ViESender::DeregisterExternalEncryption()
{
    CriticalSectionScoped cs(_sendCritsect);
    if (_ptrExternalEncryption == NULL)
    {
        return -1;
    }
    if (_ptrEncryptionBuffer)
    {
        delete _ptrEncryptionBuffer;
        _ptrEncryptionBuffer = NULL;
    }
    _ptrExternalEncryption = NULL;
    return 0;
}

// ----------------------------------------------------------------------------
// RegisterSendTransport
// ----------------------------------------------------------------------------

int ViESender::RegisterSendTransport(Transport* transport)
{
    CriticalSectionScoped cs(_sendCritsect);
    if (_ptrTransport)
    {
        return -1;
    }
    _ptrTransport = transport;
    return 0;
}

// ----------------------------------------------------------------------------
// DeregisterSendTransport
// ----------------------------------------------------------------------------

int ViESender::DeregisterSendTransport()
{
    CriticalSectionScoped cs(_sendCritsect);
    if (_ptrTransport == NULL)
    {
        return -1;
    }
    _ptrTransport = NULL;
    return 0;
}

#ifdef WEBRTC_SRTP
// ----------------------------------------------------------------------------
// RegisterSRTPModule
// ----------------------------------------------------------------------------

int ViESender::RegisterSRTPModule(SrtpModule* srtpModule)
{
    CriticalSectionScoped cs(_sendCritsect);
    if (_ptrSrtp ||
        srtpModule == NULL)
    {
        return -1;
    }
    _ptrSrtpBuffer = new WebRtc_UWord8[KMaxPacketSize];
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

int ViESender::DeregisterSRTPModule()
{
    CriticalSectionScoped cs(_sendCritsect);
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

int ViESender::RegisterSRTCPModule(SrtpModule* srtcpModule)
{
    CriticalSectionScoped cs(_sendCritsect);
    if (_ptrSrtcp ||
        srtcpModule == NULL)
    {
        return -1;
    }
    _ptrSrtcpBuffer = new WebRtc_UWord8[KMaxPacketSize];
    if (_ptrSrtcpBuffer == NULL)
    {
        return -1;
    }
    _ptrSrtcp = srtcpModule;

    return 0;
}

// ----------------------------------------------------------------------------
// DeregisterSRTCPModule
// ----------------------------------------------------------------------------

int ViESender::DeregisterSRTCPModule()
{
    CriticalSectionScoped cs(_sendCritsect);
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
// StartRTPDump
// ----------------------------------------------------------------------------

int ViESender::StartRTPDump(const char fileNameUTF8[1024])
{
    CriticalSectionScoped cs(_sendCritsect);
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

int ViESender::StopRTPDump()
{
    CriticalSectionScoped cs(_sendCritsect);
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

// ----------------------------------------------------------------------------
// SendPacket
// ----------------------------------------------------------------------------
int ViESender::SendPacket(int vieId, const void *data, int len)
{
    CriticalSectionScoped cs(_sendCritsect);
    if (!_ptrTransport)
    {
        // No transport
        return -1;
    }

    assert(ChannelId(vieId) == _channelId);

    // Prepare for possible encryption and sending
    WebRtc_UWord8* sendPacket = (WebRtc_UWord8*) data;
    int sendPacketLength = len;

    if (_rtpDump)
    {
        _rtpDump->DumpPacket(sendPacket, sendPacketLength);
    }
#ifdef WEBRTC_SRTP
    if (_ptrSrtp)
    {
        _ptrSrtp->encrypt(_channelId, sendPacket, _ptrSrtpBuffer, sendPacketLength, (int*) &sendPacketLength);
        if (sendPacketLength <= 0)
        {
            WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideo, ViEId(_engineId, _channelId), "RTP encryption failed for channel");
            return -1;
        }
        else if (sendPacketLength > KMaxPacketSize)
        {
            WEBRTC_TRACE(webrtc::kTraceCritical, webrtc::kTraceVideo, ViEId(_engineId, _channelId),
                "  %d bytes is allocated as RTP output => memory is now corrupted", KMaxPacketSize);
            return -1;
        }
        sendPacket = _ptrSrtpBuffer;
    }
#endif
    if (_ptrExternalEncryption)
    {
        _ptrExternalEncryption->encrypt(_channelId, sendPacket,
                                        _ptrEncryptionBuffer, sendPacketLength,
                                        (int*) &sendPacketLength);
        sendPacket = _ptrEncryptionBuffer;
    }

    return _ptrTransport->SendPacket(_channelId, sendPacket, sendPacketLength);
}
// ----------------------------------------------------------------------------
// SendRTCPPacket
// ----------------------------------------------------------------------------

int ViESender::SendRTCPPacket(int vieId, const void *data, int len)
{
    CriticalSectionScoped cs(_sendCritsect);

    if (!_ptrTransport)
    {
        // No transport
        return -1;
    }

    assert(ChannelId(vieId) == _channelId);

    // Prepare for possible encryption and sending
    WebRtc_UWord8* sendPacket = (WebRtc_UWord8*) data;
    int sendPacketLength = len;

    if (_rtpDump)
    {
        _rtpDump->DumpPacket(sendPacket, sendPacketLength);
    }
#ifdef WEBRTC_SRTP
    if (_ptrSrtcp)
    {
        _ptrSrtcp->encrypt_rtcp(_channelId, sendPacket, _ptrSrtcpBuffer, sendPacketLength, (int*) &sendPacketLength);
        if (sendPacketLength <= 0)
        {
            WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideo, ViEId(_engineId, _channelId), "RTCP encryption failed for channel");
            return -1;
        }
        else if (sendPacketLength > KMaxPacketSize)
        {
            WEBRTC_TRACE(webrtc::kTraceCritical, webrtc::kTraceVideo, ViEId(_engineId, _channelId), "  %d bytes is allocated as RTCP output => memory is now corrupted", KMaxPacketSize);
            return -1;
        }
        sendPacket = _ptrSrtcpBuffer;
    }
#endif
    if (_ptrExternalEncryption)
    {
        _ptrExternalEncryption->encrypt_rtcp(_channelId, sendPacket,
                                             _ptrEncryptionBuffer,
                                             sendPacketLength,
                                             (int*) &sendPacketLength);
        sendPacket = _ptrEncryptionBuffer;
    }

    return _ptrTransport->SendRTCPPacket(_channelId, sendPacket,
                                         sendPacketLength);
}
} // namespace webrtc
