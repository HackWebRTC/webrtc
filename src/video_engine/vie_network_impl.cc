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
 * vie_network_impl.cpp
 */

#include "vie_network_impl.h"

// Defines
#include "engine_configurations.h"
#include "vie_defines.h"

// WebRTC include
#include "vie_errors.h"
#include "trace.h"
#include "vie_impl.h"
#include "vie_channel.h"
#include "vie_channel_manager.h"
#include "vie_encoder.h"

// System include
#include <stdio.h>

// Conditional system include
#if (defined(_WIN32) || defined(_WIN64))
#include <qos.h>
#endif

namespace webrtc
{

// ----------------------------------------------------------------------------
// GetInterface
// ----------------------------------------------------------------------------

ViENetwork* ViENetwork::GetInterface(VideoEngine* videoEngine)
{
#ifdef WEBRTC_VIDEO_ENGINE_NETWORK_API
    if (videoEngine == NULL)
    {
        return NULL;
    }
    VideoEngineImpl* vieImpl = reinterpret_cast<VideoEngineImpl*> (videoEngine);
    ViENetworkImpl* vieNetworkImpl = vieImpl;
    (*vieNetworkImpl)++; // Increase ref count

    return vieNetworkImpl;
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

int ViENetworkImpl::Release()
{
    WEBRTC_TRACE(webrtc::kTraceApiCall, webrtc::kTraceVideo, instance_id_,
                 "ViENetwork::Release()");
    (*this)--; // Decrease ref count

    WebRtc_Word32 refCount = GetCount();
    if (refCount < 0)
    {
        WEBRTC_TRACE(webrtc::kTraceWarning, webrtc::kTraceVideo, instance_id_,
                     "ViENetwork release too many times");
        SetLastError(kViEAPIDoesNotExist);
        return -1;
    }
    WEBRTC_TRACE(webrtc::kTraceInfo, webrtc::kTraceVideo, instance_id_,
                 "ViENetwork reference count: %d", refCount);
    return refCount;
}

// ----------------------------------------------------------------------------
// Constructor
// ----------------------------------------------------------------------------

ViENetworkImpl::ViENetworkImpl()
{
    WEBRTC_TRACE(webrtc::kTraceMemory, webrtc::kTraceVideo, instance_id_,
                 "ViENetworkImpl::ViENetworkImpl() Ctor");
}

// ----------------------------------------------------------------------------
// Destructor
// ----------------------------------------------------------------------------

ViENetworkImpl::~ViENetworkImpl()
{
    WEBRTC_TRACE(webrtc::kTraceMemory, webrtc::kTraceVideo, instance_id_,
                 "ViENetworkImpl::~ViENetworkImpl() Dtor");
}

// ============================================================================
// Receive functions
// ============================================================================

// ----------------------------------------------------------------------------
// SetLocalReceiver
//
// Initializes the receive socket
// ----------------------------------------------------------------------------
int ViENetworkImpl::SetLocalReceiver(const int videoChannel,
                                     const unsigned short rtpPort,
                                     const unsigned short rtcpPort,
                                     const char* ipAddress)
{
    WEBRTC_TRACE(webrtc::kTraceApiCall, webrtc::kTraceVideo,
                 ViEId(instance_id_, videoChannel),
                 "%s(channel: %d, rtpPort: %u, rtcpPort: %u, ipAddress: %s)",
                 __FUNCTION__, videoChannel, rtpPort, rtcpPort, ipAddress);
    if (!Initialized())
    {
        SetLastError(kViENotInitialized);
        WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideo, ViEId(instance_id_),
                     "%s - ViE instance %d not initialized", __FUNCTION__,
                     instance_id_);
        return -1;
    }

    // Get the channel
    ViEChannelManagerScoped cs(channel_manager_);
    ViEChannel* ptrViEChannel = cs.Channel(videoChannel);
    if (ptrViEChannel == NULL)
    {
        // The channel doesn't exists
        WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideo,
                     ViEId(instance_id_,videoChannel), "Channel doesn't exist");
        SetLastError(kViENetworkInvalidChannelId);
        return -1;
    }

    if (ptrViEChannel->Receiving())
    {
        SetLastError(kViENetworkAlreadyReceiving);
        return -1;
    }
    if (ptrViEChannel->SetLocalReceiver(rtpPort, rtcpPort, ipAddress) != 0)
    {
        SetLastError(kViENetworkUnknownError);
        return -1;
    }
    return 0;
}

// ----------------------------------------------------------------------------
// GetLocalReceiver
//
// Gets settings for an initialized receive socket
// ----------------------------------------------------------------------------
int ViENetworkImpl::GetLocalReceiver(const int videoChannel,
                                     unsigned short& rtpPort,
                                     unsigned short& rtcpPort, char* ipAddress)
{
    WEBRTC_TRACE(webrtc::kTraceApiCall, webrtc::kTraceVideo,
                 ViEId(instance_id_, videoChannel), "%s(channel: %d)",
                 __FUNCTION__, videoChannel);

    // Get the channel
    ViEChannelManagerScoped cs(channel_manager_);
    ViEChannel* ptrViEChannel = cs.Channel(videoChannel);
    if (ptrViEChannel == NULL)
    {
        // The channel doesn't exists
        WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideo,
                     ViEId(instance_id_, videoChannel), "Channel doesn't exist");
        SetLastError(kViENetworkInvalidChannelId);
        return -1;
    }
    if (ptrViEChannel->GetLocalReceiver(rtpPort, rtcpPort, ipAddress) != 0)
    {
        SetLastError(kViENetworkLocalReceiverNotSet);
        return -1;
    }
    return 0;
}

// ============================================================================
// Send functions
// ============================================================================

// ----------------------------------------------------------------------------
// SetSendDestination
//
// Initializes the send socket
// ----------------------------------------------------------------------------
int ViENetworkImpl::SetSendDestination(const int videoChannel,
                                       const char* ipAddress,
                                       const unsigned short rtpPort,
                                       const unsigned short rtcpPort,
                                       const unsigned short sourceRtpPort,
                                       const unsigned short sourceRtcpPort)
{
    WEBRTC_TRACE(webrtc::kTraceApiCall, webrtc::kTraceVideo,
                 ViEId(instance_id_, videoChannel),
                 "%s(channel: %d, ipAddress: %s, rtpPort: %u, rtcpPort: %u, "
                 "sourceRtpPort: %u, sourceRtcpPort: %u)",
                 __FUNCTION__, videoChannel, ipAddress, rtpPort, rtcpPort,
                 sourceRtpPort, sourceRtcpPort);
    if (!Initialized())
    {
        SetLastError(kViENotInitialized);
        WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideo, ViEId(instance_id_),
                     "%s - ViE instance %d not initialized", __FUNCTION__,
                     instance_id_);
        return -1;
    }

    // Get the channel
    ViEChannelManagerScoped cs(channel_manager_);
    ViEChannel* ptrViEChannel = cs.Channel(videoChannel);
    if (ptrViEChannel == NULL)
    {
        // The channel doesn't exists
        WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideo,
                     ViEId(instance_id_, videoChannel),
                     "%s Channel doesn't exist", __FUNCTION__);
        SetLastError(kViENetworkInvalidChannelId);
        return -1;
    }
    if (ptrViEChannel->Sending())
    {
        WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideo,
                     ViEId(instance_id_, videoChannel),
                     "%s Channel already sending.", __FUNCTION__);
        SetLastError(kViENetworkAlreadySending);
        return -1;
    }
    if (ptrViEChannel->SetSendDestination(ipAddress, rtpPort, rtcpPort,
                                          sourceRtpPort, sourceRtcpPort) != 0)
    {
        SetLastError(kViENetworkUnknownError);
        return -1;
    }

    return 0;
}

// ----------------------------------------------------------------------------
// GetSendDestination
//
// Gets settings for an initialized send socket
// ----------------------------------------------------------------------------
int ViENetworkImpl::GetSendDestination(const int videoChannel, char* ipAddress,
                                       unsigned short& rtpPort,
                                       unsigned short& rtcpPort,
                                       unsigned short& sourceRtpPort,
                                       unsigned short& sourceRtcpPort)
{
    WEBRTC_TRACE(webrtc::kTraceApiCall, webrtc::kTraceVideo,
                 ViEId(instance_id_, videoChannel), "%s(channel: %d)",
                 __FUNCTION__, videoChannel);

    // Get the channel
    ViEChannelManagerScoped cs(channel_manager_);
    ViEChannel* ptrViEChannel = cs.Channel(videoChannel);
    if (ptrViEChannel == NULL)
    {
        // The channel doesn't exists
        WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideo,
                     ViEId(instance_id_, videoChannel), "Channel doesn't exist");
        SetLastError(kViENetworkInvalidChannelId);
        return -1;
    }
    if (ptrViEChannel->GetSendDestination(ipAddress, rtpPort, rtcpPort,
                                          sourceRtpPort, sourceRtcpPort) != 0)
    {
        SetLastError(kViENetworkDestinationNotSet);
        return -1;
    }
    return 0;
}

// ============================================================================
// External transport
// ============================================================================

// ----------------------------------------------------------------------------
// RegisterSendTransport
//
// Registers the customer implemented send transport
// ----------------------------------------------------------------------------
int ViENetworkImpl::RegisterSendTransport(const int videoChannel,
                                          Transport& transport)
{
    WEBRTC_TRACE(webrtc::kTraceApiCall, webrtc::kTraceVideo,
                 ViEId(instance_id_, videoChannel), "%s(channel: %d)",
                 __FUNCTION__, videoChannel);
    if (!Initialized())
    {
        SetLastError(kViENotInitialized);
        WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideo, ViEId(instance_id_),
                     "%s - ViE instance %d not initialized", __FUNCTION__,
                     instance_id_);
        return -1;
    }

    // Get the channel
    ViEChannelManagerScoped cs(channel_manager_);
    ViEChannel* ptrViEChannel = cs.Channel(videoChannel);
    if (ptrViEChannel == NULL)
    {
        // The channel doesn't exists
        WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideo,
                     ViEId(instance_id_, videoChannel),
                     "%s Channel doesn't exist", __FUNCTION__);
        SetLastError(kViENetworkInvalidChannelId);
        return -1;
    }
    if (ptrViEChannel->Sending())
    {
        WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideo,
                     ViEId(instance_id_, videoChannel),
                     "%s Channel already sending.", __FUNCTION__);
        SetLastError(kViENetworkAlreadySending);
        return -1;
    }
    if (ptrViEChannel->RegisterSendTransport(transport) != 0)
    {
        SetLastError(kViENetworkUnknownError);
        return -1;
    }
    return 0;
}

// ----------------------------------------------------------------------------
// DeregisterSendTransport
//
// Deregisters the send transport implementation from
// RegisterSendTransport
// ----------------------------------------------------------------------------
int ViENetworkImpl::DeregisterSendTransport(const int videoChannel)
{
    WEBRTC_TRACE(webrtc::kTraceApiCall, webrtc::kTraceVideo,
                 ViEId(instance_id_, videoChannel), "%s(channel: %d)",
                 __FUNCTION__, videoChannel);

    // Get the channel
    ViEChannelManagerScoped cs(channel_manager_);
    ViEChannel* ptrViEChannel = cs.Channel(videoChannel);
    if (ptrViEChannel == NULL)
    {
        // The channel doesn't exists
        WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideo,
                     ViEId(instance_id_, videoChannel),
                     "%s Channel doesn't exist", __FUNCTION__);
        SetLastError(kViENetworkInvalidChannelId);
        return -1;
    }
    if (ptrViEChannel->Sending())
    {
        WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideo,
                     ViEId(instance_id_, videoChannel),
                     "%s Channel already sending", __FUNCTION__);
        SetLastError(kViENetworkAlreadySending);
        return -1;
    }
    if (ptrViEChannel->DeregisterSendTransport() != 0)
    {
        SetLastError(kViENetworkUnknownError);
        return -1;
    }
    return 0;
}

// ----------------------------------------------------------------------------
// ReceivedRTPPacket
//
// Function for inserting a RTP packet received by a customer transport
// ----------------------------------------------------------------------------
int ViENetworkImpl::ReceivedRTPPacket(const int videoChannel, const void* data,
                                      const int length)
{
    WEBRTC_TRACE(webrtc::kTraceApiCall, webrtc::kTraceVideo,
                 ViEId(instance_id_, videoChannel),
                 "%s(channel: %d, data: -, length: %d)", __FUNCTION__,
                 videoChannel, length);
    if (!Initialized())
    {
        SetLastError(kViENotInitialized);
        WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideo, ViEId(instance_id_),
                     "%s - ViE instance %d not initialized", __FUNCTION__,
                     instance_id_);
        return -1;
    }

    // Get the channel
    ViEChannelManagerScoped cs(channel_manager_);
    ViEChannel* ptrViEChannel = cs.Channel(videoChannel);
    if (ptrViEChannel == NULL)
    {
        // The channel doesn't exists
        WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideo,
                     ViEId(instance_id_, videoChannel), "Channel doesn't exist");
        SetLastError(kViENetworkInvalidChannelId);
        return -1;
    }
    return ptrViEChannel->ReceivedRTPPacket(data, length);
}

// ----------------------------------------------------------------------------
// ReceivedRTCPPacket
//
// Function for inserting a RTCP packet received by a customer transport
// ----------------------------------------------------------------------------
int ViENetworkImpl::ReceivedRTCPPacket(const int videoChannel,
                                       const void* data, const int length)
{
    WEBRTC_TRACE(webrtc::kTraceApiCall, webrtc::kTraceVideo,
                 ViEId(instance_id_, videoChannel),
                 "%s(channel: %d, data: -, length: %d)", __FUNCTION__,
                 videoChannel, length);
    if (!Initialized())
    {
        SetLastError(kViENotInitialized);
        WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideo, ViEId(instance_id_),
                     "%s - ViE instance %d not initialized", __FUNCTION__,
                     instance_id_);
        return -1;
    }

    // Get the channel
    ViEChannelManagerScoped cs(channel_manager_);
    ViEChannel* ptrViEChannel = cs.Channel(videoChannel);
    if (ptrViEChannel == NULL)
    {
        // The channel doesn't exists
        WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideo,
                     ViEId(instance_id_, videoChannel), "Channel doesn't exist");
        SetLastError(kViENetworkInvalidChannelId);
        return -1;
    }
    return ptrViEChannel->ReceivedRTCPPacket(data, length);
}

// ============================================================================
// Get info
// ============================================================================

// ----------------------------------------------------------------------------
// GetSourceInfo
//
// Retreives informatino about the remote side sockets
// ----------------------------------------------------------------------------
int ViENetworkImpl::GetSourceInfo(const int videoChannel,
                                  unsigned short& rtpPort,
                                  unsigned short& rtcpPort, char* ipAddress,
                                  unsigned int ipAddressLength)
{
    WEBRTC_TRACE(webrtc::kTraceApiCall, webrtc::kTraceVideo,
                 ViEId(instance_id_, videoChannel), "%s(channel: %d)",
                 __FUNCTION__, videoChannel);

    // Get the channel
    ViEChannelManagerScoped cs(channel_manager_);
    ViEChannel* ptrViEChannel = cs.Channel(videoChannel);
    if (ptrViEChannel == NULL)
    {
        // The channel doesn't exists
        WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideo,
                     ViEId(instance_id_, videoChannel), "Channel doesn't exist");
        SetLastError(kViENetworkInvalidChannelId);
        return -1;
    }
    if (ptrViEChannel->GetSourceInfo(rtpPort, rtcpPort, ipAddress,
                                     ipAddressLength) != 0)
    {
        SetLastError(kViENetworkUnknownError);
        return -1;
    }
    return 0;
}

// ----------------------------------------------------------------------------
// GetLocalIP
//
// Gets the local ip address
// ----------------------------------------------------------------------------
int ViENetworkImpl::GetLocalIP(char ipAddress[64], bool ipv6)
{
    WEBRTC_TRACE(webrtc::kTraceApiCall, webrtc::kTraceVideo, ViEId(instance_id_),
                 "%s( ipAddress, ipV6: %d)", __FUNCTION__, ipv6);

#ifndef WEBRTC_EXTERNAL_TRANSPORT
    if (!Initialized())
    {
        SetLastError(kViENotInitialized);
        WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideo, ViEId(instance_id_),
                     "%s - ViE instance %d not initialized", __FUNCTION__,
                     instance_id_);
        return -1;
    }

    if (ipAddress == NULL)
    {
        WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideo, ViEId(instance_id_),
                     "%s: No argument", __FUNCTION__);
        SetLastError(kViENetworkInvalidArgument);
        return -1;
    }

    WebRtc_UWord8 numSocketThreads = 1;
    UdpTransport* ptrSocketTransport =
        UdpTransport::Create(
            ViEModuleId(instance_id_,-1),numSocketThreads);

    if (ptrSocketTransport == NULL)
    {
        WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideo, ViEId(instance_id_),
                     "%s: Could not create socket module", __FUNCTION__);
        SetLastError(kViENetworkUnknownError);
        return -1;
    }

    WebRtc_Word8 localIpAddress[64];
    if (ipv6)
    {
        WebRtc_UWord8 localIP[16];
        if (ptrSocketTransport->LocalHostAddressIPV6(localIP) != 0)
        {
            WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideo, ViEId(instance_id_),
                         "%s: Could not get local IP", __FUNCTION__);
            SetLastError(kViENetworkUnknownError);
            return -1;
        }
        // Convert 128-bit address to character string (a:b:c:d:e:f:g:h)
        sprintf(localIpAddress,
                "%.2x%.2x:%.2x%.2x:%.2x%.2x:%.2x%.2x:%.2x%.2x:%.2x%.2x:%.2x%.2x:%.2x%.2x",
                localIP[0], localIP[1], localIP[2], localIP[3], localIP[4],
                localIP[5], localIP[6], localIP[7], localIP[8], localIP[9],
                localIP[10], localIP[11], localIP[12], localIP[13],
                localIP[14], localIP[15]);
    }
    else
    {
        WebRtc_UWord32 localIP = 0;
        if (ptrSocketTransport->LocalHostAddress(localIP) != 0)
        {
            WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideo, ViEId(instance_id_),
                         "%s: Could not get local IP", __FUNCTION__);
            SetLastError(kViENetworkUnknownError);
            return -1;
        }
        // Convert 32-bit address to character string (x.y.z.w)
        sprintf(localIpAddress, "%d.%d.%d.%d", (int) ((localIP >> 24) & 0x0ff),
                (int) ((localIP >> 16) & 0x0ff),
                (int) ((localIP >> 8) & 0x0ff), (int) (localIP & 0x0ff));
    }
    strcpy(ipAddress, localIpAddress);
    UdpTransport::Destroy(
        ptrSocketTransport);
    WEBRTC_TRACE(webrtc::kTraceStateInfo, webrtc::kTraceVideo, ViEId(instance_id_),
                 "%s: local ip = %s", __FUNCTION__, localIpAddress);

    return 0;
#else
    WEBRTC_TRACE(webrtc::kTraceStateInfo, webrtc::kTraceVideo, ViEId(instance_id_),
        "%s: not available for external transport", __FUNCTION__);

    return -1;
#endif
}

// ============================================================================
// IPv6
// ============================================================================

// ----------------------------------------------------------------------------
// EnableIPv6
//
// Enables IPv6
// ----------------------------------------------------------------------------
int ViENetworkImpl::EnableIPv6(int videoChannel)
{
    WEBRTC_TRACE(webrtc::kTraceApiCall, webrtc::kTraceVideo,
                 ViEId(instance_id_, videoChannel), "%s(channel: %d)",
                 __FUNCTION__, videoChannel);

    // Get the channel
    ViEChannelManagerScoped cs(channel_manager_);
    ViEChannel* ptrViEChannel = cs.Channel(videoChannel);
    if (ptrViEChannel == NULL)
    {
        // The channel doesn't exists
        WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideo,
                     ViEId(instance_id_, videoChannel), "Channel doesn't exist");
        SetLastError(kViENetworkInvalidChannelId);
        return -1;
    }
    if (ptrViEChannel->EnableIPv6() != 0)
    {
        SetLastError(kViENetworkUnknownError);
        return -1;
    }
    return 0;
}

// ----------------------------------------------------------------------------
// IsIPv6Enabled
//
// Returns true if IPv6 is enabled, false otherwise.
// ----------------------------------------------------------------------------
bool ViENetworkImpl::IsIPv6Enabled(int videoChannel)
{
    WEBRTC_TRACE(webrtc::kTraceApiCall, webrtc::kTraceVideo,
                 ViEId(instance_id_, videoChannel), "%s(channel: %d)",
                 __FUNCTION__, videoChannel);

    // Get the channel
    ViEChannelManagerScoped cs(channel_manager_);
    ViEChannel* ptrViEChannel = cs.Channel(videoChannel);
    if (ptrViEChannel == NULL)
    {
        // The channel doesn't exists
        WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideo,
                     ViEId(instance_id_, videoChannel), "Channel doesn't exist");
        SetLastError(kViENetworkInvalidChannelId);
        return false;
    }
    return ptrViEChannel->IsIPv6Enabled();
}

// ============================================================================
// Source IP address and port filter
// ============================================================================

// ----------------------------------------------------------------------------
// SetSourceFilter
//
// Sets filter source port and IP address. Packets from all other sources
// will be discarded.
// ----------------------------------------------------------------------------
int ViENetworkImpl::SetSourceFilter(const int videoChannel,
                                    const unsigned short rtpPort,
                                    const unsigned short rtcpPort,
                                    const char* ipAddress)
{
    WEBRTC_TRACE(webrtc::kTraceApiCall, webrtc::kTraceVideo,
                 ViEId(instance_id_, videoChannel),
                 "%s(channel: %d, rtpPort: %u, rtcpPort: %u, ipAddress: %s)",
                 __FUNCTION__, videoChannel, rtpPort, rtcpPort, ipAddress);

    // Get the channel
    ViEChannelManagerScoped cs(channel_manager_);
    ViEChannel* ptrViEChannel = cs.Channel(videoChannel);
    if (ptrViEChannel == NULL)
    {
        // The channel doesn't exists
        WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideo,
                     ViEId(instance_id_, videoChannel), "Channel doesn't exist");
        SetLastError(kViENetworkInvalidChannelId);
        return -1;
    }
    if (ptrViEChannel->SetSourceFilter(rtpPort, rtcpPort, ipAddress) != 0)
    {
        SetLastError(kViENetworkUnknownError);
        return -1;
    }
    return 0;
}

// ----------------------------------------------------------------------------
// GetSourceFilter
//
// Gets vaules set by SetSourceFilter
// ----------------------------------------------------------------------------
int ViENetworkImpl::GetSourceFilter(const int videoChannel,
                                    unsigned short& rtpPort,
                                    unsigned short& rtcpPort, char* ipAddress)
{
    WEBRTC_TRACE(webrtc::kTraceApiCall, webrtc::kTraceVideo,
                 ViEId(instance_id_, videoChannel), "%s(channel: %d)",
                 __FUNCTION__, videoChannel);

    // Get the channel
    ViEChannelManagerScoped cs(channel_manager_);
    ViEChannel* ptrViEChannel = cs.Channel(videoChannel);
    if (ptrViEChannel == NULL)
    {
        // The channel doesn't exists
        WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideo,
                     ViEId(instance_id_, videoChannel), "Channel doesn't exist");
        SetLastError(kViENetworkInvalidChannelId);
        return -1;
    }
    if (ptrViEChannel->GetSourceFilter(rtpPort, rtcpPort, ipAddress) != 0)
    {
        SetLastError(kViENetworkUnknownError);
        return -1;
    }
    return 0;
}

// ============================================================================
// ToS
// ============================================================================

// ----------------------------------------------------------------------------
// SetToS
//
// Sets values for ToS
// ----------------------------------------------------------------------------
int ViENetworkImpl::SetSendToS(const int videoChannel, const int DSCP,
                               const bool useSetSockOpt = false)
{
    WEBRTC_TRACE(webrtc::kTraceApiCall, webrtc::kTraceVideo,
                 ViEId(instance_id_, videoChannel),
                 "%s(channel: %d, DSCP: %d, useSetSockOpt: %d)", __FUNCTION__,
                 videoChannel, DSCP, useSetSockOpt);

    // Get the channel
    ViEChannelManagerScoped cs(channel_manager_);
    ViEChannel* ptrViEChannel = cs.Channel(videoChannel);
    if (ptrViEChannel == NULL)
    {
        // The channel doesn't exists
        WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideo,
                     ViEId(instance_id_, videoChannel), "Channel doesn't exist");
        SetLastError(kViENetworkInvalidChannelId);
        return -1;
    }

#if defined(WEBRTC_LINUX) || defined(WEBRTC_MAC)
    WEBRTC_TRACE(kTraceInfo, kTraceVideo, ViEId(instance_id_, videoChannel),
                 "   force useSetSockopt=true since there is no alternative"
                 " implementation");
    if (ptrViEChannel->SetToS(DSCP, true) != 0)
#else
    if (ptrViEChannel->SetToS(DSCP, useSetSockOpt) != 0)
#endif
    {
        SetLastError(kViENetworkUnknownError);
        return -1;
    }
    return 0;
}

// ----------------------------------------------------------------------------
// GetToS
//
// Gets values set by SetToS
// ----------------------------------------------------------------------------
int ViENetworkImpl::GetSendToS(const int videoChannel, int& DSCP,
                               bool& useSetSockOpt)
{
    WEBRTC_TRACE(webrtc::kTraceApiCall, webrtc::kTraceVideo,
                 ViEId(instance_id_, videoChannel), "%s(channel: %d)",
                 __FUNCTION__, videoChannel);

    // Get the channel
    ViEChannelManagerScoped cs(channel_manager_);
    ViEChannel* ptrViEChannel = cs.Channel(videoChannel);
    if (ptrViEChannel == NULL)
    {
        // The channel doesn't exists
        WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideo,
                     ViEId(instance_id_, videoChannel), "Channel doesn't exist");
        SetLastError(kViENetworkInvalidChannelId);
        return -1;
    }
    if (ptrViEChannel->GetToS((WebRtc_Word32&) DSCP, useSetSockOpt) != 0)
    {
        SetLastError(kViENetworkUnknownError);
        return -1;
    }
    return 0;
}

// ============================================================================
// GQoS
// ============================================================================

// ----------------------------------------------------------------------------
// SetSendGQoS
//
// Sets settings for GQoS. Must be called after SetSendCodec to get correct
// bitrate setting.
// ----------------------------------------------------------------------------
int ViENetworkImpl::SetSendGQoS(const int videoChannel, const bool enable,
                                const int serviceType, const int overrideDSCP)
{
    WEBRTC_TRACE(webrtc::kTraceApiCall, webrtc::kTraceVideo,
                 ViEId(instance_id_, videoChannel),
                 "%s(channel: %d, enable: %d, serviceType: %d, "
                 "overrideDSCP: %d)",
                 __FUNCTION__, videoChannel, enable, serviceType, overrideDSCP);
    if (!Initialized())
    {
        SetLastError(kViENotInitialized);
        WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideo, ViEId(instance_id_),
                     "%s - ViE instance %d not initialized", __FUNCTION__,
                     instance_id_);
        return -1;
    }

#if (defined(_WIN32) || defined(_WIN64))
    // Sanity check. We might crash if testing and relying on an OS socket error
    if (enable
        && (serviceType != SERVICETYPE_BESTEFFORT)
        && (serviceType != SERVICETYPE_CONTROLLEDLOAD)
        && (serviceType != SERVICETYPE_GUARANTEED) &&
        (serviceType != SERVICETYPE_QUALITATIVE))
    {
        WEBRTC_TRACE(webrtc::kTraceApiCall, webrtc::kTraceVideo,
                     ViEId(instance_id_, videoChannel),
                     "%s: service type %d not supported", __FUNCTION__,
                     videoChannel, serviceType);
        SetLastError(kViENetworkServiceTypeNotSupported);
        return -1;
    }

    // Get the channel
    ViEChannelManagerScoped cs(channel_manager_);
    ViEChannel* ptrViEChannel = cs.Channel(videoChannel);
    if (ptrViEChannel == NULL)
    {
        // The channel doesn't exists
        WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideo,
                     ViEId(instance_id_, videoChannel), "Channel doesn't exist");
        SetLastError(kViENetworkInvalidChannelId);
        return -1;
    }
    ViEEncoder* ptrVieEncoder = cs.Encoder(videoChannel);
    if (ptrVieEncoder == NULL)
    {
        // The channel doesn't exists
        WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideo,
                     ViEId(instance_id_, videoChannel), "Channel doesn't exist");
        SetLastError(kViENetworkInvalidChannelId);
        return -1;
    }
    VideoCodec videoCodec;
    if (ptrVieEncoder->GetEncoder(videoCodec) != 0)
    {
        // Could not get
        WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideo,
                     ViEId(instance_id_, videoChannel),
                     "%s: Could not get max bitrate for the channel",
                     __FUNCTION__);
        SetLastError(kViENetworkSendCodecNotSet);
        return -1;
    }
    if(ptrViEChannel->SetSendGQoS(enable, serviceType, videoCodec.maxBitrate,
                                  overrideDSCP)!=0)
    {
        SetLastError(kViENetworkUnknownError);
        return -1;
    }
    return 0;

#else
    WEBRTC_TRACE(webrtc::kTraceApiCall, webrtc::kTraceVideo,
                 ViEId(instance_id_, videoChannel), "%s: Not supported",
                 __FUNCTION__);
    SetLastError(kViENetworkNotSupported);
    return -1;
#endif
}

// ----------------------------------------------------------------------------
// GetSendGQoS
//
// Gets the settigns set by SetSendGQoS
// ----------------------------------------------------------------------------
int ViENetworkImpl::GetSendGQoS(const int videoChannel, bool& enabled,
                                int& serviceType, int& overrideDSCP)
{
    WEBRTC_TRACE(webrtc::kTraceApiCall, webrtc::kTraceVideo,
                 ViEId(instance_id_, videoChannel), "%s(channel: %d)",
                 __FUNCTION__, videoChannel);

    // Get the channel
    ViEChannelManagerScoped cs(channel_manager_);
    ViEChannel* ptrViEChannel = cs.Channel(videoChannel);
    if (ptrViEChannel == NULL)
    {
        // The channel doesn't exists
        WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideo,
                     ViEId(instance_id_, videoChannel), "Channel doesn't exist");
        SetLastError(kViENetworkInvalidChannelId);
        return -1;
    }
    if (ptrViEChannel->GetSendGQoS(enabled, (WebRtc_Word32&) serviceType,
                                   (WebRtc_Word32&) overrideDSCP) != 0)
    {
        SetLastError(kViENetworkUnknownError);
        return -1;
    }
    return 0;

}

// ============================================================================
// Network settings
// ============================================================================

// ----------------------------------------------------------------------------
// SetMTU
// ----------------------------------------------------------------------------
int ViENetworkImpl::SetMTU(int videoChannel, unsigned int mtu)
{
    WEBRTC_TRACE(webrtc::kTraceApiCall, webrtc::kTraceVideo,
                 ViEId(instance_id_, videoChannel), "%s(channel: %d, mtu: %u)",
                 __FUNCTION__, videoChannel, mtu);

    // Get the channel
    ViEChannelManagerScoped cs(channel_manager_);
    ViEChannel* ptrViEChannel = cs.Channel(videoChannel);
    if (ptrViEChannel == NULL)
    {
        // The channel doesn't exists
        WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideo,
                     ViEId(instance_id_, videoChannel), "Channel doesn't exist");
        SetLastError(kViENetworkInvalidChannelId);
        return -1;
    }
    if (ptrViEChannel->SetMTU(mtu) != 0)
    {
        SetLastError(kViENetworkUnknownError);
        return -1;
    }
    return 0;

}

// ============================================================================
// Packet timout notification
// ============================================================================

// ----------------------------------------------------------------------------
// SetPacketTimeoutNotification
//
// Sets the time for packet timout notifications
// ----------------------------------------------------------------------------
int ViENetworkImpl::SetPacketTimeoutNotification(const int videoChannel,
                                                 bool enable,
                                                 int timeoutSeconds)
{
    WEBRTC_TRACE(webrtc::kTraceApiCall, webrtc::kTraceVideo,
                 ViEId(instance_id_, videoChannel),
                 "%s(channel: %d, enable: %d, timeoutSeconds: %u)",
                 __FUNCTION__, videoChannel, enable, timeoutSeconds);

    // Get the channel
    ViEChannelManagerScoped cs(channel_manager_);
    ViEChannel* ptrViEChannel = cs.Channel(videoChannel);
    if (ptrViEChannel == NULL)
    {
        // The channel doesn't exists
        WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideo,
                     ViEId(instance_id_, videoChannel), "Channel doesn't exist");
        SetLastError(kViENetworkInvalidChannelId);
        return -1;
    }
    if (ptrViEChannel->SetPacketTimeoutNotification(enable, timeoutSeconds)
        != 0)
    {
        SetLastError(kViENetworkUnknownError);
        return -1;
    }
    return 0;
}

// ============================================================================
// Periodic dead-or-alive reports
// ============================================================================

// ----------------------------------------------------------------------------
// RegisterObserver
// ----------------------------------------------------------------------------
int ViENetworkImpl::RegisterObserver(const int videoChannel,
                                     ViENetworkObserver& observer)
{
    WEBRTC_TRACE(webrtc::kTraceApiCall, webrtc::kTraceVideo,
                 ViEId(instance_id_, videoChannel), "%s(channel: %d)",
                 __FUNCTION__, videoChannel);

    // Get the channel
    ViEChannelManagerScoped cs(channel_manager_);
    ViEChannel* ptrViEChannel = cs.Channel(videoChannel);
    if (ptrViEChannel == NULL)
    {
        // The channel doesn't exists
        WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideo,
                     ViEId(instance_id_, videoChannel), "Channel doesn't exist");
        SetLastError(kViENetworkInvalidChannelId);
        return -1;
    }
    if (ptrViEChannel->RegisterNetworkObserver(&observer) != 0)
    {
        SetLastError(kViENetworkObserverAlreadyRegistered);
        return -1;
    }
    return 0;
}

// ----------------------------------------------------------------------------
// DeregisterObserver
// ----------------------------------------------------------------------------
int ViENetworkImpl::DeregisterObserver(const int videoChannel)
{
    WEBRTC_TRACE(webrtc::kTraceApiCall, webrtc::kTraceVideo,
                 ViEId(instance_id_, videoChannel), "%s(channel: %d)",
                 __FUNCTION__, videoChannel);

    // Get the channel
    ViEChannelManagerScoped cs(channel_manager_);
    ViEChannel* ptrViEChannel = cs.Channel(videoChannel);
    if (ptrViEChannel == NULL)
    {
        // The channel doesn't exists
        WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideo,
                     ViEId(instance_id_, videoChannel), "Channel doesn't exist");
        SetLastError(kViENetworkInvalidChannelId);
        return -1;
    }
    if (!ptrViEChannel->NetworkObserverRegistered())
    {
        SetLastError(kViENetworkObserverNotRegistered);
        return -1;
    }
    return ptrViEChannel->RegisterNetworkObserver(NULL);
}

// ----------------------------------------------------------------------------
// SetPeriodicDeadOrAliveStatus
//
// Enables/disables the dead-or-alive callback
// ----------------------------------------------------------------------------
int ViENetworkImpl::SetPeriodicDeadOrAliveStatus(const int videoChannel,
                                                 bool enable,
                                                 unsigned int sampleTimeSeconds)
{
    WEBRTC_TRACE(webrtc::kTraceApiCall, webrtc::kTraceVideo,
                 ViEId(instance_id_, videoChannel),
                 "%s(channel: %d, enable: %d, sampleTimeSeconds: %ul)",
                 __FUNCTION__, videoChannel, enable, sampleTimeSeconds);

    // Get the channel
    ViEChannelManagerScoped cs(channel_manager_);
    ViEChannel* ptrViEChannel = cs.Channel(videoChannel);
    if (ptrViEChannel == NULL)
    {
        // The channel doesn't exists
        WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideo,
                     ViEId(instance_id_, videoChannel), "Channel doesn't exist");
        SetLastError(kViENetworkInvalidChannelId);
        return -1;
    }
    if (!ptrViEChannel->NetworkObserverRegistered())
    {
        SetLastError(kViENetworkObserverNotRegistered);
        return -1;
    }

    if (ptrViEChannel->SetPeriodicDeadOrAliveStatus(enable, sampleTimeSeconds)
        != 0)
    {
        SetLastError(kViENetworkUnknownError);
        return -1;
    }
    return 0;
}

// ============================================================================
// Send packet using the User Datagram Protocol (UDP)
// ============================================================================

// ----------------------------------------------------------------------------
// SendUDPPacket
//
// Send an extra UDP packet
// ----------------------------------------------------------------------------
int ViENetworkImpl::SendUDPPacket(const int videoChannel, const void* data,
                                  const unsigned int length,
                                  int& transmittedBytes,
                                  bool useRtcpSocket = false)
{
    WEBRTC_TRACE(webrtc::kTraceApiCall, webrtc::kTraceVideo,
                 ViEId(instance_id_, videoChannel),
                 "%s(channel: %d, data: -, length: %d, transmitterBytes: -, "
                 "useRtcpSocket: %d)", __FUNCTION__, videoChannel, length,
                 useRtcpSocket);
    if (!Initialized())
    {
        SetLastError(kViENotInitialized);
        WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideo, ViEId(instance_id_),
                     "%s - ViE instance %d not initialized", __FUNCTION__,
                     instance_id_);
        return -1;
    }

    // Get the channel
    ViEChannelManagerScoped cs(channel_manager_);
    ViEChannel* ptrViEChannel = cs.Channel(videoChannel);
    if (ptrViEChannel == NULL)
    {
        // The channel doesn't exists
        WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideo,
                     ViEId(instance_id_, videoChannel), "Channel doesn't exist");
        SetLastError(kViENetworkInvalidChannelId);
        return -1;
    }
    if (ptrViEChannel->SendUDPPacket((const WebRtc_Word8*) data, length,
                                     (WebRtc_Word32&) transmittedBytes,
                                     useRtcpSocket) < 0)
    {
        SetLastError(kViENetworkUnknownError);
        return -1;
    }
    return 0;
}
} // namespace webrtc
