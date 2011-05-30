/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_VIDEO_ENGINE_MAIN_INTERFACE_VIE_NETWORK_H_
#define WEBRTC_VIDEO_ENGINE_MAIN_INTERFACE_VIE_NETWORK_H_

// This sub-API supports the following functionalities:
//  - Configuring send and receive addresses.
//  - External transport support.
//  - Port and address filters.
//  - Windows GQoS functions and ToS functions.
//  - Packet timeout notification.
//  - Dead‐or‐Alive connection observations.


#include "common_types.h"

namespace webrtc
{
class VideoEngine;
class Transport;

// ----------------------------------------------------------------------------
//	ViENetworkObserver
// ----------------------------------------------------------------------------

// This enumerator describes VideoEngine packet timeout states.
enum ViEPacketTimeout
{
    NoPacket = 0,
    PacketReceived = 1
};

// This class declares an abstract interface for a user defined observer. It is
// up to the VideoEngine user to implement a derived class which implements the
// observer class. The observer is registered using RegisterObserver() and
// deregistered using DeregisterObserver().
class WEBRTC_DLLEXPORT ViENetworkObserver
{
public:
    // This method will be called periodically delivering a dead‐or‐alive
    // decision for a specified channel.
    virtual void OnPeriodicDeadOrAlive(const int videoChannel,
                                       const bool alive) = 0;

    // This method is called once if a packet timeout occurred.
    virtual void PacketTimeout(const int videoChannel,
                               const ViEPacketTimeout timeout) = 0;
protected:
    virtual ~ViENetworkObserver() {};
};

// ----------------------------------------------------------------------------
//	ViENetwork
// ----------------------------------------------------------------------------

class WEBRTC_DLLEXPORT ViENetwork
{
public:
    // Default values
    enum
    {
        KDefaultSampleTimeSeconds = 2
    };

    // Factory for the ViENetwork sub‐API and increases an internal reference
    // counter if successful. Returns NULL if the API is not supported or if
    // construction fails.
    static ViENetwork* GetInterface(VideoEngine* videoEngine);

    // Releases the ViENetwork sub-API and decreases an internal reference
    // counter.Returns the new reference count. This value should be zero
    // for all sub-API:s before the VideoEngine object can be safely deleted.
    virtual int Release() = 0;

    // Specifies the ports to receive RTP packets on. It is also possible to set
    // port for RTCP and local IP address.
    virtual int SetLocalReceiver(const int videoChannel,
                                 const unsigned short rtpPort,
                                 const unsigned short rtcpPort = 0,
                                 const char* ipAddress = NULL) = 0;

    // Gets the local receiver ports and address for a specified channel.
    virtual int GetLocalReceiver(const int videoChannel,
                                 unsigned short& rtpPort,
                                 unsigned short& rtcpPort, char* ipAddress) = 0;

    // Specifies the destination port and IP address for a specified channel.
    virtual int SetSendDestination(const int videoChannel,
                                   const char* ipAddress,
                                   const unsigned short rtpPort,
                                   const unsigned short rtcpPort = 0,
                                   const unsigned short sourceRtpPort = 0,
                                   const unsigned short sourceRtcpPort = 0) = 0;

    // Get the destination port and address for a specified channel.
    virtual int GetSendDestination(const int videoChannel, char* ipAddress,
                                   unsigned short& rtpPort,
                                   unsigned short& rtcpPort,
                                   unsigned short& sourceRtpPort,
                                   unsigned short& sourceRtcpPort) = 0;

    // This function registers a user implementation of Transport to use for
    // sending RTP and RTCP packets on this channel.
    virtual int RegisterSendTransport(const int videoChannel,
                                      Transport& transport) = 0;

    // This function deregisters a used Transport for a specified channel.
    virtual int DeregisterSendTransport(const int videoChannel) = 0;

    // When using external transport for a channel, received RTP packets should
    // be passed to VideoEngine using this function. The input should contain
    // the RTP header and payload.
    virtual int ReceivedRTPPacket(const int videoChannel, const void* data,
                                  const int length) = 0;

    // When using external transport for a channel, received RTCP packets should
    // be passed to VideoEngine using this function.
    virtual int ReceivedRTCPPacket(const int videoChannel, const void* data,
                                   const int length) = 0;

    // Gets the source ports and IP address of the incoming stream for a
    // specified channel.
    virtual int GetSourceInfo(const int videoChannel, unsigned short& rtpPort,
                              unsigned short& rtcpPort, char* ipAddress,
                              unsigned int ipAddressLength) = 0;

    // Gets the local IP address, in string format.
    virtual int GetLocalIP(char ipAddress[64], bool ipv6 = false) = 0;

    // Enables IPv6, instead of IPv4, for a specified channel.
    virtual int EnableIPv6(int videoChannel) = 0;

    // The function returns true if IPv6 is enabled, false otherwise.
    virtual bool IsIPv6Enabled(int videoChannel) = 0;

    // Enables a port and IP address filtering for incoming packets on a
    // specific channel.
    virtual int SetSourceFilter(const int videoChannel,
                                const unsigned short rtpPort,
                                const unsigned short rtcpPort = 0,
                                const char* ipAddress = NULL) = 0;

    // Gets current port and IP address filter for a specified channel.
    virtual int GetSourceFilter(const int videoChannel, unsigned short& rtpPort,
                                unsigned short& rtcpPort, char* ipAddress) = 0;

    // This function sets the six‐bit Differentiated Services Code Point (DSCP)
    // in the IP header of the outgoing stream for a specific channel.
    // Windows and Linux only.
    virtual int SetSendToS(const int videoChannel, const int DSCP,
                           const bool useSetSockOpt = false) = 0;

    // Retrieves the six‐bit Differentiated Services Code Point (DSCP) in the IP
    // header of the outgoing stream for a specific channel.
    virtual int GetSendToS(const int videoChannel, int& DSCP,
                           bool& useSetSockOpt) = 0;

    // This function sets the Generic Quality of Service (GQoS) service level.
    // The Windows operating system then maps to a Differentiated Services Code
    // Point (DSCP) and to an 802.1p setting. Windows only.
    virtual int SetSendGQoS(const int videoChannel, const bool enable,
                            const int serviceType,
                            const int overrideDSCP = 0) = 0;

    // This function retrieves the currently set GQoS service level for a
    // specific channel.
    virtual int GetSendGQoS(const int videoChannel, bool& enabled,
                            int& serviceType, int& overrideDSCP) = 0;

    // This function sets the Maximum Transition Unit (MTU) for a channel. The
    // RTP packet will be packetized based on this MTU to optimize performance
    // over the network.
    virtual int SetMTU(int videoChannel, unsigned int mtu) = 0;

    // This function enables or disables warning reports if packets have not
    // been received for a specified time interval.
    virtual int SetPacketTimeoutNotification(const int videoChannel,
                                             bool enable,
                                             int timeoutSeconds) = 0;

    // Registers an instance of a user implementation of the ViENetwork
    // observer.
    virtual int RegisterObserver(const int videoChannel,
                                 ViENetworkObserver& observer) = 0;

    // Removes a registered instance of ViENetworkObserver.
    virtual int DeregisterObserver(const int videoChannel) = 0;

    // This function enables or disables the periodic dead‐or‐alive callback
    // functionality for a specified channel.
    virtual int SetPeriodicDeadOrAliveStatus(
        const int videoChannel, const bool enable,
        const unsigned int sampleTimeSeconds = KDefaultSampleTimeSeconds) = 0;

    // This function handles sending a raw UDP data packet over an existing RTP
    // or RTCP socket.
    virtual int SendUDPPacket(const int videoChannel, const void* data,
                              const unsigned int length, int& transmittedBytes,
                              bool useRtcpSocket = false) = 0;

protected:
    ViENetwork() {};
    virtual ~ViENetwork() {};
};
} // namespace webrtc
#endif  // WEBRTC_VIDEO_ENGINE_MAIN_INTERFACE_VIE_NETWORK_H_
