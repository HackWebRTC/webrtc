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
 * vie_network_impl.h
 */

#ifndef WEBRTC_VIDEO_ENGINE_MAIN_SOURCE_VIE_NETWORK_IMPL_H_
#define WEBRTC_VIDEO_ENGINE_MAIN_SOURCE_VIE_NETWORK_IMPL_H_

#include "typedefs.h"
#include "vie_defines.h"
#include "vie_network.h"
#include "vie_ref_count.h"
#include "vie_shared_data.h"

namespace webrtc
{

// ----------------------------------------------------------------------------
//	ViENetworkImpl
// ----------------------------------------------------------------------------

class ViENetworkImpl : public virtual ViESharedData,
                      public ViENetwork,
                      public ViERefCount
{
public:
    virtual int Release();

    // Receive functions
    virtual int SetLocalReceiver(const int videoChannel,
                                 const unsigned short rtpPort,
                                 const unsigned short rtcpPort,
                                 const char* ipAddress);

    virtual int GetLocalReceiver(const int videoChannel,
                                 unsigned short& rtpPort,
                                 unsigned short& rtcpPort, char* ipAddress);

    // Send functions
    virtual int SetSendDestination(const int videoChannel,
                                   const char* ipAddress,
                                   const unsigned short rtpPort,
                                   const unsigned short rtcpPort,
                                   const unsigned short sourceRtpPort,
                                   const unsigned short sourceRtcpPort);

    virtual int GetSendDestination(const int videoChannel, char* ipAddress,
                                   unsigned short& rtpPort,
                                   unsigned short& rtcpPort,
                                   unsigned short& sourceRtpPort,
                                   unsigned short& sourceRtcpPort);

    // External transport
    virtual int RegisterSendTransport(const int videoChannel,
                                      Transport& transport);

    virtual int DeregisterSendTransport(const int videoChannel);

    virtual int ReceivedRTPPacket(const int videoChannel, const void* data,
                                  const int length);

    virtual int ReceivedRTCPPacket(const int videoChannel, const void* data,
                                   const int length);

    // Get info
    virtual int GetSourceInfo(const int videoChannel, unsigned short& rtpPort,
                              unsigned short& rtcpPort, char* ipAddress,
                              unsigned int ipAddressLength);

    virtual int GetLocalIP(char ipAddress[64], bool ipv6);

    // IPv6
    virtual int EnableIPv6(int videoChannel);

    virtual bool IsIPv6Enabled(int videoChannel);

    // Source IP address and port filter
    virtual int SetSourceFilter(const int videoChannel,
                                const unsigned short rtpPort,
                                const unsigned short rtcpPort,
                                const char* ipAddress);

    virtual int GetSourceFilter(const int videoChannel,
                                unsigned short& rtpPort,
                                unsigned short& rtcpPort, char* ipAddress);

    // ToS
    virtual int SetSendToS(const int videoChannel, const int DSCP,
                           const bool useSetSockOpt);

    virtual int GetSendToS(const int videoChannel, int& DSCP,
                           bool& useSetSockOpt);

    // GQoS
    virtual int SetSendGQoS(const int videoChannel, const bool enable,
                            const int serviceType, const int overrideDSCP);

    virtual int GetSendGQoS(const int videoChannel, bool& enabled,
                            int& serviceType, int& overrideDSCP);

    // Network settings
    virtual int SetMTU(int videoChannel, unsigned int mtu);

    // Packet timout notification
    virtual int SetPacketTimeoutNotification(const int videoChannel,
                                             bool enable, int timeoutSeconds);

    // Periodic dead-or-alive reports
    virtual int RegisterObserver(const int videoChannel,
                                 ViENetworkObserver& observer);

    virtual int DeregisterObserver(const int videoChannel);

    virtual int
        SetPeriodicDeadOrAliveStatus(const int videoChannel, const bool enable,
                                     const unsigned int sampleTimeSeconds);

    // Send extra packet using the User Datagram Protocol (UDP)
    virtual int SendUDPPacket(const int videoChannel, const void* data,
                              const unsigned int length, int& transmittedBytes,
                              bool useRtcpSocket);

protected:
    ViENetworkImpl();
    virtual ~ViENetworkImpl();
};
} // namespace webrtc
#endif  // WEBRTC_VIDEO_ENGINE_MAIN_SOURCE_VIE_NETWORK_IMPL_H_
