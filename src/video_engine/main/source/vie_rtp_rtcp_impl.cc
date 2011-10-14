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
 * vie_rtp_rtcp_impl.cc
 */

#include "vie_rtp_rtcp_impl.h"

// Defines
#include "engine_configurations.h"
#include "vie_defines.h"

#include "vie_errors.h"
#include "file_wrapper.h"
#include "trace.h"
#include "vie_impl.h"
#include "vie_channel.h"
#include "vie_channel_manager.h"
#include "vie_encoder.h"

namespace webrtc
{

// ----------------------------------------------------------------------------
// GetInterface
// ----------------------------------------------------------------------------

ViERTP_RTCP* ViERTP_RTCP::GetInterface(VideoEngine* videoEngine)
{
#ifdef WEBRTC_VIDEO_ENGINE_RTP_RTCP_API
    if (videoEngine == NULL)
    {
        return NULL;
    }
    VideoEngineImpl* vieImpl = reinterpret_cast<VideoEngineImpl*> (videoEngine);
    ViERTP_RTCPImpl* vieRTPImpl = vieImpl;
    (*vieRTPImpl)++; // Increase ref count

    return vieRTPImpl;
#else
    return NULL;
#endif
}

// ----------------------------------------------------------------------------
// Release
//
// Releases the interface, i.e. reduces the reference counter. The number of
// remaining references is returned, -1 if released too many times.
// ----------------------------------------------------------------------------

int ViERTP_RTCPImpl::Release()
{
    WEBRTC_TRACE(webrtc::kTraceApiCall, webrtc::kTraceVideo, _instanceId,
                 "ViERTP_RTCP::Release()");
    (*this)--; // Decrease ref count

    WebRtc_Word32 refCount = GetCount();
    if (refCount < 0)
    {
        WEBRTC_TRACE(webrtc::kTraceWarning, webrtc::kTraceVideo, _instanceId,
                     "ViERTP_RTCP release too many times");
        SetLastError(kViEAPIDoesNotExist);
        return -1;
    }
    WEBRTC_TRACE(webrtc::kTraceInfo, webrtc::kTraceVideo, _instanceId,
                 "ViERTP_RTCP reference count: %d", refCount);
    return refCount;
}

// ----------------------------------------------------------------------------
// Constructor
// ----------------------------------------------------------------------------

ViERTP_RTCPImpl::ViERTP_RTCPImpl()
{
    WEBRTC_TRACE(webrtc::kTraceMemory, webrtc::kTraceVideo, _instanceId,
                 "ViERTP_RTCPImpl::ViERTP_RTCPImpl() Ctor");
}

// ----------------------------------------------------------------------------
// Destructor
// ----------------------------------------------------------------------------

ViERTP_RTCPImpl::~ViERTP_RTCPImpl()
{
    WEBRTC_TRACE(webrtc::kTraceMemory, webrtc::kTraceVideo, _instanceId,
                 "ViERTP_RTCPImpl::~ViERTP_RTCPImpl() Dtor");
}

// ============================================================================
// SSRC/CSRC
// ============================================================================

// ----------------------------------------------------------------------------
// SetLocalSSRC
//
// Sets the SSRC on the outgoing stream
// ----------------------------------------------------------------------------

int ViERTP_RTCPImpl::SetLocalSSRC(const int videoChannel,
                                  const unsigned int SSRC,
                                  const StreamType usage,
                                  const unsigned char simulcastIdx)
{
    WEBRTC_TRACE(webrtc::kTraceApiCall,
                 webrtc::kTraceVideo,
                 ViEId(_instanceId, videoChannel),
                 "%s(channel: %d, SSRC: %d)",
                 __FUNCTION__, videoChannel, SSRC);

    // Get the channel
    ViEChannelManagerScoped cs(_channelManager);
    ViEChannel* ptrViEChannel = cs.Channel(videoChannel);
    if (ptrViEChannel == NULL)
    {
        // The channel doesn't exists
        WEBRTC_TRACE(webrtc::kTraceError,
                     webrtc::kTraceVideo,
                     ViEId(_instanceId, videoChannel),
                     "%s: Channel %d doesn't exist",
                     __FUNCTION__, videoChannel);
        SetLastError(kViERtpRtcpInvalidChannelId);
        return -1;
    }
    if (ptrViEChannel->SetSSRC(SSRC, usage, simulcastIdx) != 0)
    {
        SetLastError(kViERtpRtcpUnknownError);
        return -1;
    }
    return 0;
}

// ----------------------------------------------------------------------------
// GetLocalSSRC
//
// Gets the SSRC of the outgoing stream
// ----------------------------------------------------------------------------

int ViERTP_RTCPImpl::GetLocalSSRC(const int videoChannel,
                                  unsigned int& SSRC) const
{
    WEBRTC_TRACE(webrtc::kTraceApiCall, webrtc::kTraceVideo,
                 ViEId(_instanceId, videoChannel), "%s(channel: %d, SSRC: %d)",
                 __FUNCTION__, videoChannel, SSRC);

    // Get the channel
    ViEChannelManagerScoped cs(_channelManager);
    ViEChannel* ptrViEChannel = cs.Channel(videoChannel);
    if (ptrViEChannel == NULL)
    {
        // The channel doesn't exists
        WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideo,
                     ViEId(_instanceId, videoChannel),
                     "%s: Channel %d doesn't exist", __FUNCTION__,
                     videoChannel);
        SetLastError(kViERtpRtcpInvalidChannelId);
        return -1;
    }

    if (ptrViEChannel->GetLocalSSRC((WebRtc_UWord32&) SSRC) != 0)
    {
        SetLastError(kViERtpRtcpUnknownError);
        return -1;
    }
    return 0;

}

int ViERTP_RTCPImpl::SetRemoteSSRCType(const int videoChannel,
                                       const StreamType usage,
                                       const unsigned int SSRC) const
{
    // TODO(pwestin) add support for RTX
    return -1;
}

// ----------------------------------------------------------------------------
// GetRemoteSSRC 
//
// Gets the SSRC of the incoming stream
// ----------------------------------------------------------------------------

int ViERTP_RTCPImpl::GetRemoteSSRC(const int videoChannel,
                                   unsigned int& SSRC) const
{
    WEBRTC_TRACE(webrtc::kTraceApiCall, webrtc::kTraceVideo,
                 ViEId(_instanceId, videoChannel), "%s(channel: %d)",
                 __FUNCTION__, videoChannel, SSRC);

    // Get the channel
    ViEChannelManagerScoped cs(_channelManager);
    ViEChannel* ptrViEChannel = cs.Channel(videoChannel);
    if (ptrViEChannel == NULL)
    {
        // The channel doesn't exists
        WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideo,
                     ViEId(_instanceId, videoChannel),
                     "%s: Channel %d doesn't exist", __FUNCTION__,
                     videoChannel);
        SetLastError(kViERtpRtcpInvalidChannelId);
        return -1;
    }

    if (ptrViEChannel->GetRemoteSSRC((WebRtc_UWord32&) SSRC) != 0)
    {
        SetLastError(kViERtpRtcpUnknownError);
        return -1;
    }
    return 0;
}

// ----------------------------------------------------------------------------
// GetRemoteCSRCs
//
// Gets the CSRC of the incoming stream
// ----------------------------------------------------------------------------

int ViERTP_RTCPImpl::GetRemoteCSRCs(const int videoChannel,
                                    unsigned int CSRCs[kRtpCsrcSize]) const
{
    WEBRTC_TRACE(webrtc::kTraceApiCall, webrtc::kTraceVideo,
                 ViEId(_instanceId, videoChannel), "%s(channel: %d)",
                 __FUNCTION__, videoChannel);

    // Get the channel
    ViEChannelManagerScoped cs(_channelManager);
    ViEChannel* ptrViEChannel = cs.Channel(videoChannel);
    if (ptrViEChannel == NULL)
    {
        // The channel doesn't exists
        WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideo,
                     ViEId(_instanceId, videoChannel),
                     "%s: Channel %d doesn't exist", __FUNCTION__,
                     videoChannel);
        SetLastError(kViERtpRtcpInvalidChannelId);
        return -1;
    }

    if (ptrViEChannel->GetRemoteCSRC(CSRCs) != 0)
    {
        SetLastError(kViERtpRtcpUnknownError);
        return -1;
    }
    return 0;
}

// ----------------------------------------------------------------------------
// SetStartSequenceNumber
//
// Sets the starting sequence number, instead of a random number.
// ----------------------------------------------------------------------------

int ViERTP_RTCPImpl::SetStartSequenceNumber(const int videoChannel,
                                            unsigned short sequenceNumber)
{
    WEBRTC_TRACE(webrtc::kTraceApiCall, webrtc::kTraceVideo,
                 ViEId(_instanceId, videoChannel),
                 "%s(channel: %d, sequenceNumber: %u)", __FUNCTION__,
                 videoChannel, sequenceNumber);

    // Get the channel
    ViEChannelManagerScoped cs(_channelManager);
    ViEChannel* ptrViEChannel = cs.Channel(videoChannel);
    if (ptrViEChannel == NULL)
    {
        // The channel doesn't exists
        WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideo,
                     ViEId(_instanceId, videoChannel),
                     "%s: Channel %d doesn't exist", __FUNCTION__,
                     videoChannel);
        SetLastError(kViERtpRtcpInvalidChannelId);
        return -1;
    }
    if (ptrViEChannel->Sending())
    {
        WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideo,
                     ViEId(_instanceId, videoChannel),
                     "%s: Channel %d already sending.", __FUNCTION__,
                     videoChannel);
        SetLastError(kViERtpRtcpAlreadySending);
        return -1;
    }

    if (ptrViEChannel->SetStartSequenceNumber(sequenceNumber) != 0)
    {
        SetLastError(kViERtpRtcpUnknownError);
        return -1;
    }
    return 0;
}

// ============================================================================
// RTCP
// ============================================================================

// ----------------------------------------------------------------------------
// SetRTCPStatus
//
// Sets the RTCP status for the channel
// ----------------------------------------------------------------------------

int ViERTP_RTCPImpl::SetRTCPStatus(const int videoChannel,
                                   const ViERTCPMode rtcpMode)
{
    WEBRTC_TRACE(webrtc::kTraceApiCall, webrtc::kTraceVideo,
                 ViEId(_instanceId,videoChannel), "%s(channel: %d, mode: %d)",
                 __FUNCTION__, videoChannel, rtcpMode);

    // Get the channel
    ViEChannelManagerScoped cs(_channelManager);
    ViEChannel* ptrViEChannel = cs.Channel(videoChannel);
    if (ptrViEChannel == NULL)
    {
        // The channel doesn't exists
        WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideo,
                     ViEId(_instanceId, videoChannel),
                     "%s: Channel %d doesn't exist", __FUNCTION__,
                     videoChannel);
        SetLastError(kViERtpRtcpInvalidChannelId);
        return -1;
    }

    RTCPMethod moduleMode = ViERTCPModeToRTCPMethod(rtcpMode);

    if (ptrViEChannel->SetRTCPMode(moduleMode) != 0)
    {
        SetLastError(kViERtpRtcpUnknownError);
        return -1;
    }
    return 0;

}

// ----------------------------------------------------------------------------
// GetRTCPStatus
//
// Gets the RTCP status for the specified channel
// ----------------------------------------------------------------------------

int ViERTP_RTCPImpl::GetRTCPStatus(const int videoChannel,
                                   ViERTCPMode& rtcpMode)
{
    WEBRTC_TRACE(webrtc::kTraceApiCall, webrtc::kTraceVideo,
                 ViEId(_instanceId, videoChannel), "%s(channel: %d)",
                 __FUNCTION__, videoChannel, rtcpMode);

    // Get the channel
    ViEChannelManagerScoped cs(_channelManager);
    ViEChannel* ptrViEChannel = cs.Channel(videoChannel);
    if (ptrViEChannel == NULL)
    {
        // The channel doesn't exists
        WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideo,
                     ViEId(_instanceId, videoChannel),
                     "%s: Channel %d doesn't exist", __FUNCTION__,
                     videoChannel);
        SetLastError(kViERtpRtcpInvalidChannelId);
        return -1;
    }

    RTCPMethod moduleMode = kRtcpOff;
    if (ptrViEChannel->GetRTCPMode(moduleMode) != 0)
    {
        WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideo,
                     ViEId(_instanceId, videoChannel),
                     "%s: could not get current RTCP mode", __FUNCTION__);
        SetLastError(kViERtpRtcpUnknownError);
        return -1;
    }

    rtcpMode = RTCPMethodToViERTCPMode(moduleMode);
    return 0;
}

// ----------------------------------------------------------------------------
// SetRTCPCName
//
// Specifies what CName to use
// ----------------------------------------------------------------------------

int ViERTP_RTCPImpl::SetRTCPCName(const int videoChannel,
                                  const char rtcpCName[KMaxRTCPCNameLength])
{
    WEBRTC_TRACE(webrtc::kTraceApiCall, webrtc::kTraceVideo,
                 ViEId(_instanceId, videoChannel), "%s(channel: %d, name: %s)",
                 __FUNCTION__, videoChannel, rtcpCName);

    // Get the channel
    ViEChannelManagerScoped cs(_channelManager);
    ViEChannel* ptrViEChannel = cs.Channel(videoChannel);
    if (ptrViEChannel == NULL)
    {
        // The channel doesn't exists
        WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideo,
                     ViEId(_instanceId, videoChannel),
                     "%s: Channel %d doesn't exist", __FUNCTION__,
                     videoChannel);
        SetLastError(kViERtpRtcpInvalidChannelId);
        return -1;
    }

    if (ptrViEChannel->Sending())
    {
        WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideo,
                     ViEId(_instanceId, videoChannel),
                     "%s: Channel %d already sending.", __FUNCTION__,
                     videoChannel);
        SetLastError(kViERtpRtcpAlreadySending);
        return -1;
    }

    if (ptrViEChannel->SetRTCPCName(rtcpCName) != 0)
    {
        SetLastError(kViERtpRtcpUnknownError);
        return -1;
    }
    return 0;

}

// ----------------------------------------------------------------------------
// GetRTCPCName 
//
// Gets the set CName
// ----------------------------------------------------------------------------

int ViERTP_RTCPImpl::GetRTCPCName(const int videoChannel,
                                  char rtcpCName[KMaxRTCPCNameLength])
{
    WEBRTC_TRACE(webrtc::kTraceApiCall, webrtc::kTraceVideo,
                 ViEId(_instanceId, videoChannel), "%s(channel: %d)",
                 __FUNCTION__, videoChannel);

    // Get the channel
    ViEChannelManagerScoped cs(_channelManager);
    ViEChannel* ptrViEChannel = cs.Channel(videoChannel);
    if (ptrViEChannel == NULL)
    {
        // The channel doesn't exists
        WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideo,
                     ViEId(_instanceId, videoChannel),
                     "%s: Channel %d doesn't exist", __FUNCTION__,
                     videoChannel);
        SetLastError(kViERtpRtcpInvalidChannelId);
        return -1;
    }
    if (ptrViEChannel->GetRTCPCName(rtcpCName) != 0)
    {
        SetLastError(kViERtpRtcpUnknownError);
        return -1;
    }
    return 0;

}

// ----------------------------------------------------------------------------
// GetRemoteRTCPCName
//
// Gets the CName of for the incoming stream
// ----------------------------------------------------------------------------

int ViERTP_RTCPImpl::GetRemoteRTCPCName(const int videoChannel,
                                        char rtcpCName[KMaxRTCPCNameLength]) const
{
    WEBRTC_TRACE(webrtc::kTraceApiCall, webrtc::kTraceVideo,
                 ViEId(_instanceId, videoChannel), "%s(channel: %d)",
                 __FUNCTION__, videoChannel);

    // Get the channel
    ViEChannelManagerScoped cs(_channelManager);
    ViEChannel* ptrViEChannel = cs.Channel(videoChannel);
    if (ptrViEChannel == NULL)
    {
        // The channel doesn't exists
        WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideo,
                     ViEId(_instanceId, videoChannel),
                     "%s: Channel %d doesn't exist", __FUNCTION__,
                     videoChannel);
        SetLastError(kViERtpRtcpInvalidChannelId);
        return -1;
    }
    if (ptrViEChannel->GetRemoteRTCPCName(rtcpCName) != 0)
    {
        SetLastError(kViERtpRtcpUnknownError);
        return -1;
    }
    return 0;
}

// ----------------------------------------------------------------------------
//  SendApplicationDefinedRTCPPacket
//
//  From RFC 3550:
//
//  6.7 APP: Application-Defined RTCP Packet
//
//    0                   1                   2                   3
//    0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
//   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//   |V=2|P| subtype |   PT=APP=204  |             length            |
//   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//   |                           SSRC/CSRC                           |
//   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//   |                          name (ASCII)                         |
//   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//   |                   application-dependent data                ...
//   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//
//   The APP packet is intended for experimental use as new applications
//   and new features are developed, without requiring packet type value
//   registration.  APP packets with unrecognized names SHOULD be ignored.
//   After testing and if wider use is justified, it is RECOMMENDED that
//   each APP packet be redefined without the subtype and name fields and
//   registered with IANA using an RTCP packet type.
//
//   version (V), padding (P), length:
//     As described for the SR packet (see Section 6.4.1).
//
//   subtype: 5 bits
//      May be used as a subtype to allow a set of APP packets to be
//      defined under one unique name, or for any application-dependent
//      data.
//
//   packet type (PT): 8 bits
//      Contains the constant 204 to identify this as an RTCP APP packet.
//
//   name: 4 octets
//      A name chosen by the person defining the set of APP packets to be
//      unique with respect to other APP packets this application might
//      receive.  The application creator might choose to use the
//      application name, and then coordinate the allocation of subtype
//      values to others who want to define new packet types for the
//      application.  Alternatively, it is RECOMMENDED that others choose
//      a name based on the entity they represent, then coordinate the use
//      of the name within that entity.  The name is interpreted as a
//      sequence of four ASCII characters, with uppercase and lowercase
//      characters treated as distinct.
//
//   application-dependent data: variable length
//      Application-dependent data may or may not appear in an APP packet.
//      It is interpreted by the application and not RTP itself.  It MUST
//      be a multiple of 32 bits long.
// ----------------------------------------------------------------------------

int ViERTP_RTCPImpl::SendApplicationDefinedRTCPPacket(
    const int videoChannel, const unsigned char subType, unsigned int name,
    const char* data, unsigned short dataLengthInBytes)
{
    WEBRTC_TRACE(webrtc::kTraceApiCall, webrtc::kTraceVideo,
                 ViEId(_instanceId, videoChannel),
                 "%s(channel: %d, subType: %c, name: %d, data: x, length: %u)",
                 __FUNCTION__, videoChannel, subType, name, dataLengthInBytes);

    // Get the channel
    ViEChannelManagerScoped cs(_channelManager);
    ViEChannel* ptrViEChannel = cs.Channel(videoChannel);
    if (ptrViEChannel == NULL)
    {
        // The channel doesn't exists
        WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideo,
                     ViEId(_instanceId, videoChannel),
                     "%s: Channel %d doesn't exist", __FUNCTION__,
                     videoChannel);
        SetLastError(kViERtpRtcpInvalidChannelId);
        return -1;
    }
    if (!ptrViEChannel->Sending())
    {
        WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideo,
                     ViEId(_instanceId, videoChannel),
                     "%s: Channel %d not sending", __FUNCTION__, videoChannel);
        SetLastError(kViERtpRtcpNotSending);
        return -1;
    }
    RTCPMethod method;
    if (ptrViEChannel->GetRTCPMode(method) != 0 || method == kRtcpOff)
    {
        WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideo,
                     ViEId(_instanceId, videoChannel),
                     "%s: RTCP disabled on channel %d.", __FUNCTION__,
                     videoChannel);
        SetLastError(kViERtpRtcpRtcpDisabled);
        return -1;
    }

    if (ptrViEChannel->SendApplicationDefinedRTCPPacket(
        subType, name, (const WebRtc_UWord8 *) data, dataLengthInBytes) != 0)
    {
        SetLastError(kViERtpRtcpUnknownError);
        return -1;
    }
    return 0;
}

// ----------------------------------------------------------------------------
// SetNACKStatus
//
// Enables NACK for the specified channel
// ----------------------------------------------------------------------------

int ViERTP_RTCPImpl::SetNACKStatus(const int videoChannel, const bool enable)
{
    WEBRTC_TRACE(webrtc::kTraceApiCall, webrtc::kTraceVideo,
                 ViEId(_instanceId, videoChannel),
                 "%s(channel: %d, enable: %d)", __FUNCTION__, videoChannel,
                 enable);

    // Get the channel
    ViEChannelManagerScoped cs(_channelManager);
    ViEChannel* ptrViEChannel = cs.Channel(videoChannel);
    if (ptrViEChannel == NULL)
    {
        // The channel doesn't exists
        WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideo,
                     ViEId(_instanceId, videoChannel),
                     "%s: Channel %d doesn't exist", __FUNCTION__,
                     videoChannel);
        SetLastError(kViERtpRtcpInvalidChannelId);
        return -1;
    }
    // Update the channel status
    if (ptrViEChannel->SetNACKStatus(enable) != 0)
    {
        WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideo,
                     ViEId(_instanceId, videoChannel),
                     "%s: failed for channel %d", __FUNCTION__, videoChannel);
        SetLastError(kViERtpRtcpUnknownError);
        return -1;
    }

    // Update the encoder
    ViEEncoder* ptrViEEncoder = cs.Encoder(videoChannel);
    if (ptrViEEncoder == NULL)
    {
        WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideo,
                     ViEId(_instanceId, videoChannel),
                     "%s: Could not get encoder for channel %d", __FUNCTION__,
                     videoChannel);
        SetLastError(kViERtpRtcpUnknownError);
        return -1;
    }
    ptrViEEncoder->UpdateProtectionMethod();

    return 0;
}

// ----------------------------------------------------------------------------
// SetFECStatus
//
// Enables/disables FEC and sets the payloadtypes
// ----------------------------------------------------------------------------

int ViERTP_RTCPImpl::SetFECStatus(const int videoChannel, const bool enable,
                                  const unsigned char payloadTypeRED,
                                  const unsigned char payloadTypeFEC)
{

    WEBRTC_TRACE(webrtc::kTraceApiCall, webrtc::kTraceVideo,
                 ViEId(_instanceId, videoChannel),
                 "%s(channel: %d, enable: %d, payloadTypeRED: %u, "
                 "payloadTypeFEC: %u)",
                 __FUNCTION__, videoChannel, enable, payloadTypeRED,
                 payloadTypeFEC);

    // Get the channel
    ViEChannelManagerScoped cs(_channelManager);
    ViEChannel* ptrViEChannel = cs.Channel(videoChannel);
    if (ptrViEChannel == NULL)
    {
        // The channel doesn't exists
        WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideo,
                     ViEId(_instanceId, videoChannel),
                     "%s: Channel %d doesn't exist", __FUNCTION__,
                     videoChannel);
        SetLastError(kViERtpRtcpInvalidChannelId);
        return -1;
    }

    // Update the channel status
    if (ptrViEChannel->SetFECStatus(enable, payloadTypeRED, payloadTypeFEC)
        != 0)
    {
        WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideo,
                     ViEId(_instanceId, videoChannel),
                     "%s: failed for channel %d", __FUNCTION__, videoChannel);
        SetLastError(kViERtpRtcpUnknownError);
        return -1;
    }

    // Update the encoder
    ViEEncoder* ptrViEEncoder = cs.Encoder(videoChannel);
    if (ptrViEEncoder == NULL)
    {
        WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideo,
                     ViEId(_instanceId, videoChannel),
                     "%s: Could not get encoder for channel %d", __FUNCTION__,
                     videoChannel);
        SetLastError(kViERtpRtcpUnknownError);
        return -1;
    }
    ptrViEEncoder->UpdateProtectionMethod();
    return 0;
}

// ----------------------------------------------------------------------------
// SetHybridNACKFECStatus
//
// Enables/disables hybrid NACK/FEC and sets the payloadtypes
// ----------------------------------------------------------------------------

int ViERTP_RTCPImpl::SetHybridNACKFECStatus(const int videoChannel,
                                            const bool enable,
                                            const unsigned char payloadTypeRED,
                                            const unsigned char payloadTypeFEC)
{
    WEBRTC_TRACE(webrtc::kTraceApiCall, webrtc::kTraceVideo,
                 ViEId(_instanceId, videoChannel),
                 "%s(channel: %d, enable: %d, payloadTypeRED: %u, "
                 "payloadTypeFEC: %u)",
                 __FUNCTION__, videoChannel, enable, payloadTypeRED,
                 payloadTypeFEC);

    // Get the channel
    ViEChannelManagerScoped cs(_channelManager);
    ViEChannel* ptrViEChannel = cs.Channel(videoChannel);
    if (ptrViEChannel == NULL)
    {
        // The channel doesn't exists
        WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideo,
                     ViEId(_instanceId, videoChannel),
                     "%s: Channel %d doesn't exist", __FUNCTION__,
                     videoChannel);
        SetLastError(kViERtpRtcpInvalidChannelId);
        return -1;
    }

    // Update the channel status with hybrid NACK FEC mode
    if (ptrViEChannel->SetHybridNACKFECStatus(enable, payloadTypeRED,
                                              payloadTypeFEC) != 0)
    {
        WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideo,
                     ViEId(_instanceId, videoChannel),
                     "%s: failed for channel %d", __FUNCTION__, videoChannel);
        SetLastError(kViERtpRtcpUnknownError);
        return -1;
    }

    // Update the encoder
    ViEEncoder* ptrViEEncoder = cs.Encoder(videoChannel);
    if (ptrViEEncoder == NULL)
    {
        WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideo,
                     ViEId(_instanceId, videoChannel),
                     "%s: Could not get encoder for channel %d", __FUNCTION__,
                     videoChannel);
        SetLastError(kViERtpRtcpUnknownError);
        return -1;
    }
    ptrViEEncoder->UpdateProtectionMethod();
    return 0;
}

// ----------------------------------------------------------------------------
// SetKeyFrameRequestMethod
//
// Sets the key frame request method to use
// ----------------------------------------------------------------------------

int ViERTP_RTCPImpl::SetKeyFrameRequestMethod(
    const int videoChannel, const ViEKeyFrameRequestMethod method)
{
    WEBRTC_TRACE(webrtc::kTraceApiCall, webrtc::kTraceVideo,
                 ViEId(_instanceId, videoChannel),
                 "%s(channel: %d, method: %d)", __FUNCTION__, videoChannel,
                 method);

    // Get the channel
    ViEChannelManagerScoped cs(_channelManager);
    ViEChannel* ptrViEChannel = cs.Channel(videoChannel);
    if (ptrViEChannel == NULL)
    {
        // The channel doesn't exists
        WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideo,
                     ViEId(_instanceId, videoChannel),
                     "%s: Channel %d doesn't exist", __FUNCTION__,
                     videoChannel);
        SetLastError(kViERtpRtcpInvalidChannelId);
        return -1;
    }

    KeyFrameRequestMethod moduleMethod = APIRequestToModuleRequest(method);
    if (ptrViEChannel->SetKeyFrameRequestMethod(moduleMethod) != 0)
    {
        SetLastError(kViERtpRtcpUnknownError);
        return -1;
    }
    return 0;
}

// ----------------------------------------------------------------------------
// SetTMMBRStatus
//
// Enables/disables TTMBR
// ----------------------------------------------------------------------------

int ViERTP_RTCPImpl::SetTMMBRStatus(const int videoChannel, const bool enable)
{
    WEBRTC_TRACE(webrtc::kTraceApiCall, webrtc::kTraceVideo,
                 ViEId(_instanceId, videoChannel),
                 "%s(channel: %d, enable: %d)", __FUNCTION__, videoChannel,
                 enable);

    // Get the channel
    ViEChannelManagerScoped cs(_channelManager);
    ViEChannel* ptrViEChannel = cs.Channel(videoChannel);
    if (ptrViEChannel == NULL)
    {
        // The channel doesn't exists
        WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideo,
                     ViEId(_instanceId, videoChannel),
                     "%s: Channel %d doesn't exist", __FUNCTION__,
                     videoChannel);
        SetLastError(kViERtpRtcpInvalidChannelId);
        return -1;
    }

    if (ptrViEChannel->EnableTMMBR(enable) != 0)
    {
        SetLastError(kViERtpRtcpUnknownError);
        return -1;
    }
    return 0;
}

// ============================================================================
// Statistics
// ============================================================================

// ----------------------------------------------------------------------------
// GetReceivedRTCPStatistics
//
// Gets statistics received in from remote side
// ----------------------------------------------------------------------------

int ViERTP_RTCPImpl::GetReceivedRTCPStatistics(
    const int videoChannel, unsigned short& fractionLost,
    unsigned int& cumulativeLost, unsigned int& extendedMax,
    unsigned int& jitter, int& rttMs) const
{
    WEBRTC_TRACE(webrtc::kTraceApiCall, webrtc::kTraceVideo,
                 ViEId(_instanceId, videoChannel), "%s(channel: %d)",
                 __FUNCTION__, videoChannel);

    // Get the channel
    ViEChannelManagerScoped cs(_channelManager);
    ViEChannel* ptrViEChannel = cs.Channel(videoChannel);
    if (ptrViEChannel == NULL)
    {
        // The channel doesn't exists
        WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideo,
                     ViEId(_instanceId, videoChannel),
                     "%s: Channel %d doesn't exist", __FUNCTION__,
                     videoChannel);
        SetLastError(kViERtpRtcpInvalidChannelId);
        return -1;
    }

    if (ptrViEChannel->GetReceivedRtcpStatistics(
        (WebRtc_UWord16&) fractionLost, (WebRtc_UWord32&) cumulativeLost,
        (WebRtc_UWord32&) extendedMax, (WebRtc_UWord32&) jitter,
        (WebRtc_Word32&) rttMs) != 0)
    {
        SetLastError(kViERtpRtcpUnknownError);
        return -1;
    }
    return 0;
}

// ----------------------------------------------------------------------------
// GetSentRTCPStatistics
//
// Gets statistics sent in RTCP to the remote side
// ----------------------------------------------------------------------------

int ViERTP_RTCPImpl::GetSentRTCPStatistics(const int videoChannel,
                                           unsigned short& fractionLost,
                                           unsigned int& cumulativeLost,
                                           unsigned int& extendedMax,
                                           unsigned int& jitter,
                                           int& rttMs) const
{
    WEBRTC_TRACE(webrtc::kTraceApiCall, webrtc::kTraceVideo,
                 ViEId(_instanceId, videoChannel), "%s(channel: %d)",
                 __FUNCTION__, videoChannel);

    // Get the channel
    ViEChannelManagerScoped cs(_channelManager);
    ViEChannel* ptrViEChannel = cs.Channel(videoChannel);
    if (ptrViEChannel == NULL)
    {
        // The channel doesn't exists
        WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideo,
                     ViEId(_instanceId, videoChannel),
                     "%s: Channel %d doesn't exist", __FUNCTION__,
                     videoChannel);
        SetLastError(kViERtpRtcpInvalidChannelId);
        return -1;
    }

    if (ptrViEChannel->GetSendRtcpStatistics((WebRtc_UWord16&) fractionLost,
                                             (WebRtc_UWord32&) cumulativeLost,
                                             (WebRtc_UWord32&) extendedMax,
                                             (WebRtc_UWord32&) jitter,
                                             (WebRtc_Word32&) rttMs) != 0)
    {
        SetLastError(kViERtpRtcpUnknownError);
        return -1;
    }
    return 0;
}

// ----------------------------------------------------------------------------
// GetRTCPStatistics
//
// Gets statistics about sent/received rtp packets
// ----------------------------------------------------------------------------

int ViERTP_RTCPImpl::GetRTPStatistics(const int videoChannel,
                                      unsigned int& bytesSent,
                                      unsigned int& packetsSent,
                                      unsigned int& bytesReceived,
                                      unsigned int& packetsReceived) const
{
    WEBRTC_TRACE(webrtc::kTraceApiCall, webrtc::kTraceVideo,
                 ViEId(_instanceId, videoChannel), "%s(channel: %d)",
                 __FUNCTION__, videoChannel);

    // Get the channel
    ViEChannelManagerScoped cs(_channelManager);
    ViEChannel* ptrViEChannel = cs.Channel(videoChannel);
    if (ptrViEChannel == NULL)
    {
        // The channel doesn't exists
        WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideo,
                     ViEId(_instanceId, videoChannel),
                     "%s: Channel %d doesn't exist", __FUNCTION__,
                     videoChannel);
        SetLastError(kViERtpRtcpInvalidChannelId);
        return -1;
    }

    if (ptrViEChannel->GetRtpStatistics((WebRtc_UWord32&) bytesSent,
                                        (WebRtc_UWord32&) packetsSent,
                                        (WebRtc_UWord32&) bytesReceived,
                                        (WebRtc_UWord32&) packetsReceived) != 0)
    {
        SetLastError(kViERtpRtcpUnknownError);
        return -1;
    }
    return 0;
}

// The function gets bandwidth usage statistics from the sent RTP streams.
int ViERTP_RTCPImpl::GetBandwidthUsage(const int videoChannel,
                                       unsigned int& totalBitrateSent,
                                       unsigned int& fecBitrateSent,
                                       unsigned int& nackBitrateSent) const {
  WEBRTC_TRACE(webrtc::kTraceApiCall, webrtc::kTraceVideo,
               ViEId(_instanceId, videoChannel), "%s(channel: %d)",
               __FUNCTION__, videoChannel);

  ViEChannelManagerScoped cs(_channelManager);
  ViEChannel* ptrViEChannel = cs.Channel(videoChannel);
  if (ptrViEChannel == NULL) {
    // The channel doesn't exists
    WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideo,
                 ViEId(_instanceId, videoChannel),
                 "%s: Channel %d doesn't exist", __FUNCTION__,
                 videoChannel);
    SetLastError(kViERtpRtcpInvalidChannelId);
    return -1;
  }

  ptrViEChannel->GetBandwidthUsage(
      static_cast<WebRtc_UWord32&>(totalBitrateSent),
      static_cast<WebRtc_UWord32&>(fecBitrateSent),
      static_cast<WebRtc_UWord32&>(nackBitrateSent));
  return 0;
}

// ============================================================================
// Keep alive
// ============================================================================

// ----------------------------------------------------------------------------
// SetRTPKeepAliveStatus
//
// Enable/disable RTP keepaliv packets on a non-sending channel
// ----------------------------------------------------------------------------

int ViERTP_RTCPImpl::SetRTPKeepAliveStatus(
    const int videoChannel, bool enable, const char unknownPayloadType,
    const unsigned int deltaTransmitTimeSeconds)
{
    WEBRTC_TRACE(webrtc::kTraceApiCall, webrtc::kTraceVideo,
                 ViEId(_instanceId, videoChannel),
                 "%s(channel: %d, enable: %d, unknownPayloadType: %d, "
                 "deltaTransmitTimeMS: %ul)",
                 __FUNCTION__, videoChannel, enable, (int) unknownPayloadType,
                 deltaTransmitTimeSeconds);

    // Get the channel
    ViEChannelManagerScoped cs(_channelManager);
    ViEChannel* ptrViEChannel = cs.Channel(videoChannel);
    if (ptrViEChannel == NULL)
    {
        // The channel doesn't exists
        WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideo,
                     ViEId(_instanceId, videoChannel),
                     "%s: Channel %d doesn't exist", __FUNCTION__,
                     videoChannel);
        SetLastError(kViERtpRtcpInvalidChannelId);
        return -1;
    }
    WebRtc_UWord16 deltaTransmitTimeMs = 1000 * deltaTransmitTimeSeconds;
    if (ptrViEChannel->SetKeepAliveStatus(enable, unknownPayloadType,
                                          deltaTransmitTimeMs) != 0)
    {
        SetLastError(kViERtpRtcpUnknownError);
        return -1;
    }
    return 0;
}

// ----------------------------------------------------------------------------
// GetRTPKeepAliveStatus
// ----------------------------------------------------------------------------

int ViERTP_RTCPImpl::GetRTPKeepAliveStatus(
    const int videoChannel, bool& enabled, char& unknownPayloadType,
    unsigned int& deltaTransmitTimeSeconds)
{
    WEBRTC_TRACE(webrtc::kTraceApiCall, webrtc::kTraceVideo,
                 ViEId(_instanceId, videoChannel), "%s(channel: %d)",
                 __FUNCTION__, videoChannel);

    // Get the channel
    ViEChannelManagerScoped cs(_channelManager);
    ViEChannel* ptrViEChannel = cs.Channel(videoChannel);
    if (ptrViEChannel == NULL)
    {
        // The channel doesn't exists
        WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideo,
                     ViEId(_instanceId, videoChannel),
                     "%s: Channel %d doesn't exist", __FUNCTION__,
                     videoChannel);
        SetLastError(kViERtpRtcpInvalidChannelId);
        return -1;
    }

    WebRtc_UWord16 deltaTimeMs = 0;
    int retVal = ptrViEChannel->GetKeepAliveStatus(enabled, unknownPayloadType,
                                                   deltaTimeMs);
    deltaTransmitTimeSeconds = deltaTimeMs / 1000;
    if (retVal != 0)
    {
        SetLastError(kViERtpRtcpUnknownError);
    }
    return retVal;
}

// ============================================================================
// Dump RTP stream, for debug purpose
// ============================================================================

// ----------------------------------------------------------------------------
// StartRTPDump
//
// SAves all incoming/outgoing packets to a file
// ----------------------------------------------------------------------------

int ViERTP_RTCPImpl::StartRTPDump(const int videoChannel,
                                  const char fileNameUTF8[1024],
                                  RTPDirections direction)
{
    WEBRTC_TRACE(webrtc::kTraceApiCall, webrtc::kTraceVideo,
                 ViEId(_instanceId, videoChannel),
                 "%s(channel: %d, fileName: %s, direction: %d)", __FUNCTION__,
                 videoChannel, fileNameUTF8, direction);

    assert(FileWrapper::kMaxFileNameSize == 1024);

    // Get the channel
    ViEChannelManagerScoped cs(_channelManager);
    ViEChannel* ptrViEChannel = cs.Channel(videoChannel);
    if (ptrViEChannel == NULL)
    {
        // The channel doesn't exists
        WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideo,
                     ViEId(_instanceId, videoChannel),
                     "%s: Channel %d doesn't exist", __FUNCTION__,
                     videoChannel);
        SetLastError(kViERtpRtcpInvalidChannelId);
        return -1;
    }

    if (ptrViEChannel->StartRTPDump(fileNameUTF8, direction) != 0)
    {
        SetLastError(kViERtpRtcpUnknownError);
        return -1;
    }
    return 0;
}

// ----------------------------------------------------------------------------
// StopRTPDump
//
// Stops the RTP dump
// ----------------------------------------------------------------------------

int ViERTP_RTCPImpl::StopRTPDump(const int videoChannel,
                                 RTPDirections direction)
{
    WEBRTC_TRACE(webrtc::kTraceApiCall, webrtc::kTraceVideo,
                 ViEId(_instanceId, videoChannel),
                 "%s(channel: %d, direction: %d)", __FUNCTION__, videoChannel,
                 direction);

    // Get the channel
    ViEChannelManagerScoped cs(_channelManager);
    ViEChannel* ptrViEChannel = cs.Channel(videoChannel);
    if (ptrViEChannel == NULL)
    {
        // The channel doesn't exists
        WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideo,
                     ViEId(_instanceId, videoChannel),
                     "%s: Channel %d doesn't exist", __FUNCTION__,
                     videoChannel);
        SetLastError(kViERtpRtcpInvalidChannelId);
        return -1;
    }

    if (ptrViEChannel->StopRTPDump(direction) != 0)
    {
        SetLastError(kViERtpRtcpUnknownError);
        return -1;
    }
    return 0;
}

// ============================================================================
// Callback
// ============================================================================

// ----------------------------------------------------------------------------
// RegisterRTPObserver
//
// 
// ----------------------------------------------------------------------------

int ViERTP_RTCPImpl::RegisterRTPObserver(const int videoChannel,
                                         ViERTPObserver& observer)
{
    WEBRTC_TRACE(webrtc::kTraceApiCall, webrtc::kTraceVideo,
                 ViEId(_instanceId, videoChannel), "%s(channel: %d)",
                 __FUNCTION__, videoChannel);

    // Get the channel
    ViEChannelManagerScoped cs(_channelManager);
    ViEChannel* ptrViEChannel = cs.Channel(videoChannel);
    if (ptrViEChannel == NULL)
    {
        // The channel doesn't exists
        WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideo,
                     ViEId(_instanceId, videoChannel),
                     "%s: Channel %d doesn't exist", __FUNCTION__,
                     videoChannel);
        SetLastError(kViERtpRtcpInvalidChannelId);
        return -1;
    }

    if (ptrViEChannel->RegisterRtpObserver(&observer) != 0)
    {
        SetLastError(kViERtpRtcpObserverAlreadyRegistered);
        return -1;
    }
    return 0;
}

// ----------------------------------------------------------------------------
// DeregisterRTPObserver
//
// Deregisters a set observer
// ----------------------------------------------------------------------------

int ViERTP_RTCPImpl::DeregisterRTPObserver(const int videoChannel)
{
    WEBRTC_TRACE(webrtc::kTraceApiCall, webrtc::kTraceVideo,
                 ViEId(_instanceId, videoChannel), "%s(channel: %d)",
                 __FUNCTION__, videoChannel);

    // Get the channel
    ViEChannelManagerScoped cs(_channelManager);
    ViEChannel* ptrViEChannel = cs.Channel(videoChannel);
    if (ptrViEChannel == NULL)
    {
        // The channel doesn't exists
        WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideo,
                     ViEId(_instanceId, videoChannel),
                     "%s: Channel %d doesn't exist", __FUNCTION__,
                     videoChannel);
        SetLastError(kViERtpRtcpInvalidChannelId);
        return -1;
    }

    if (ptrViEChannel->RegisterRtpObserver(NULL) != 0)
    {
        SetLastError(kViERtpRtcpObserverNotRegistered);
        return -1;
    }
    return 0;
}
// ----------------------------------------------------------------------------
// RegisterRTCPObserver
// ----------------------------------------------------------------------------

int ViERTP_RTCPImpl::RegisterRTCPObserver(const int videoChannel,
                                          ViERTCPObserver& observer)
{
    WEBRTC_TRACE(webrtc::kTraceApiCall, webrtc::kTraceVideo,
                 ViEId(_instanceId, videoChannel), "%s(channel: %d)",
                 __FUNCTION__, videoChannel);

    // Get the channel
    ViEChannelManagerScoped cs(_channelManager);
    ViEChannel* ptrViEChannel = cs.Channel(videoChannel);
    if (ptrViEChannel == NULL)
    {
        // The channel doesn't exists
        WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideo,
                     ViEId(_instanceId, videoChannel),
                     "%s: Channel %d doesn't exist", __FUNCTION__,
                     videoChannel);
        SetLastError(kViERtpRtcpInvalidChannelId);
        return -1;
    }

    if (ptrViEChannel->RegisterRtcpObserver(&observer) != 0)
    {
        SetLastError(kViERtpRtcpObserverAlreadyRegistered);
        return -1;
    }
    return 0;
}

// ----------------------------------------------------------------------------
// DeregisterRTCPObserver
// ----------------------------------------------------------------------------

int ViERTP_RTCPImpl::DeregisterRTCPObserver(const int videoChannel)
{
    WEBRTC_TRACE(webrtc::kTraceApiCall, webrtc::kTraceVideo,
                 ViEId(_instanceId, videoChannel), "%s(channel: %d)",
                 __FUNCTION__, videoChannel);

    // Get the channel
    ViEChannelManagerScoped cs(_channelManager);
    ViEChannel* ptrViEChannel = cs.Channel(videoChannel);
    if (ptrViEChannel == NULL)
    {
        // The channel doesn't exists
        WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideo,
                     ViEId(_instanceId, videoChannel),
                     "%s: Channel %d doesn't exist", __FUNCTION__,
                     videoChannel);
        SetLastError(kViERtpRtcpInvalidChannelId);
        return -1;

    }

    if (ptrViEChannel->RegisterRtcpObserver(NULL) != 0)
    {
        SetLastError(kViERtpRtcpObserverNotRegistered);
        return -1;
    }
    return 0;
}

// ============================================================================
// Prsivate methods
// ============================================================================

// ----------------------------------------------------------------------------
// ViERTCPModeToRTCPMethod
//
// Help method for converting API mode to Module mode
// ----------------------------------------------------------------------------

RTCPMethod ViERTP_RTCPImpl::ViERTCPModeToRTCPMethod(ViERTCPMode apiMode)
{
    switch (apiMode)
    {
        case kRtcpNone:
            return kRtcpOff;

        case kRtcpCompound_RFC4585:
            return kRtcpCompound;

        case kRtcpNonCompound_RFC5506:
            return kRtcpNonCompound;

        default:
            assert(false);
            return kRtcpOff;
    }
}

// ----------------------------------------------------------------------------
// RTCPMethodToViERTCPMode
//
// Help method for converting API mode to Module mode
// ----------------------------------------------------------------------------

ViERTCPMode ViERTP_RTCPImpl::RTCPMethodToViERTCPMode(RTCPMethod moduleMethod)
{
    switch (moduleMethod)
    {
        case kRtcpOff:
            return kRtcpNone;

        case kRtcpCompound:
            return kRtcpCompound_RFC4585;

        case kRtcpNonCompound:
            return kRtcpNonCompound_RFC5506;

        default:
            assert(false);
            return kRtcpNone;
    }
}

KeyFrameRequestMethod ViERTP_RTCPImpl::APIRequestToModuleRequest(
    ViEKeyFrameRequestMethod apiMethod)
{
    switch (apiMethod)
    {
        case kViEKeyFrameRequestNone:
            return kKeyFrameReqFirRtp;

        case kViEKeyFrameRequestPliRtcp:
            return kKeyFrameReqPliRtcp;

        case kViEKeyFrameRequestFirRtp:
            return kKeyFrameReqFirRtp;

        case kViEKeyFrameRequestFirRtcp:
            return kKeyFrameReqFirRtcp;

        default:
            assert(false);
            return kKeyFrameReqFirRtp;
    }
}
} // namespace webrtc
