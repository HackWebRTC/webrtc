/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MODULES_UDP_TRANSPORT_SOURCE_UDP_TRANSPORT_IMPL_H_
#define WEBRTC_MODULES_UDP_TRANSPORT_SOURCE_UDP_TRANSPORT_IMPL_H_

#include "udp_transport.h"
#include "udp_socket_wrapper.h"

namespace webrtc {
class CriticalSectionWrapper;
class RWLockWrapper;
class UdpSocketManager;

class UdpTransportImpl : public UdpTransport
{
public:
    // Factory method. Constructor disabled.
    UdpTransportImpl(const WebRtc_Word32 id, WebRtc_UWord8& numSocketThreads);
    virtual ~UdpTransportImpl();

    // Module functions
    virtual WebRtc_Word32 ChangeUniqueId(const WebRtc_Word32 id);
    virtual WebRtc_Word32 TimeUntilNextProcess();
    virtual WebRtc_Word32 Process();

    // UdpTransport functions
    virtual WebRtc_Word32 InitializeSendSockets(
        const WebRtc_Word8* ipAddr,
        const WebRtc_UWord16 rtpPort,
        const WebRtc_UWord16 rtcpPort = 0);
    virtual WebRtc_Word32 InitializeReceiveSockets(
        UdpTransportData* const packetCallback,
        const WebRtc_UWord16 rtpPort,
        const WebRtc_Word8* ipAddr = NULL,
        const WebRtc_Word8* multicastIpAddr = NULL,
        const WebRtc_UWord16 rtcpPort = 0);
    virtual WebRtc_Word32 InitializeSourcePorts(
        const WebRtc_UWord16 rtpPort,
        const WebRtc_UWord16 rtcpPort = 0);
    virtual WebRtc_Word32 SourcePorts(WebRtc_UWord16& rtpPort,
                                      WebRtc_UWord16& rtcpPort) const;
    virtual WebRtc_Word32 ReceiveSocketInformation(
        WebRtc_Word8 ipAddr[kIpAddressVersion6Length],
        WebRtc_UWord16& rtpPort,
        WebRtc_UWord16& rtcpPort,
        WebRtc_Word8 multicastIpAddr[kIpAddressVersion6Length]) const;
    virtual WebRtc_Word32 SendSocketInformation(
        WebRtc_Word8 ipAddr[kIpAddressVersion6Length],
        WebRtc_UWord16& rtpPort,
        WebRtc_UWord16& rtcpPort) const;
    virtual WebRtc_Word32 RemoteSocketInformation(
        WebRtc_Word8 ipAddr[kIpAddressVersion6Length],
        WebRtc_UWord16& rtpPort,
        WebRtc_UWord16& rtcpPort) const;
    virtual WebRtc_Word32 SetQoS(const bool QoS,
                                 const WebRtc_Word32 serviceType,
                                 const WebRtc_UWord32 maxBitrate = 0,
                                 const WebRtc_Word32 overrideDSCP = 0,
                                 const bool audio = false);
    virtual WebRtc_Word32 QoS(bool& QoS, WebRtc_Word32& serviceType,
                              WebRtc_Word32& overrideDSCP) const;
    virtual WebRtc_Word32 SetToS(const WebRtc_Word32 DSCP,
                                 const bool useSetSockOpt = false);
    virtual WebRtc_Word32 ToS(WebRtc_Word32& DSCP,
                              bool& useSetSockOpt) const;
    virtual WebRtc_Word32 SetPCP(const WebRtc_Word32 PCP);
    virtual WebRtc_Word32 PCP(WebRtc_Word32& PCP) const;
    virtual WebRtc_Word32 EnableIpV6();
    virtual bool IpV6Enabled() const;
    virtual WebRtc_Word32 SetFilterIP(
        const WebRtc_Word8 filterIPAddress[kIpAddressVersion6Length]);
    virtual WebRtc_Word32 FilterIP(
        WebRtc_Word8 filterIPAddress[kIpAddressVersion6Length]) const;
    virtual WebRtc_Word32 SetFilterPorts(const WebRtc_UWord16 rtpFilterPort,
                                         const WebRtc_UWord16 rtcpFilterPort);
    virtual WebRtc_Word32 FilterPorts(WebRtc_UWord16& rtpFilterPort,
                                      WebRtc_UWord16& rtcpFilterPort) const;
    virtual WebRtc_Word32 StartReceiving(
        const WebRtc_UWord32 numberOfSocketBuffers);
    virtual WebRtc_Word32 StopReceiving();
    virtual bool Receiving() const;
    virtual bool SendSocketsInitialized() const;
    virtual bool SourcePortsInitialized() const;
    virtual bool ReceiveSocketsInitialized() const;
    virtual WebRtc_Word32 SendRaw(const WebRtc_Word8* data,
                                  WebRtc_UWord32 length, WebRtc_Word32 isRTCP,
                                  WebRtc_UWord16 portnr = 0,
                                  const WebRtc_Word8* ip = NULL);
    virtual WebRtc_Word32 SendRTPPacketTo(const WebRtc_Word8 *data,
                                          WebRtc_UWord32 length,
                                          const SocketAddress& to);
    virtual WebRtc_Word32 SendRTCPPacketTo(const WebRtc_Word8 *data,
                                           WebRtc_UWord32 length,
                                           const SocketAddress& to);
    virtual WebRtc_Word32 SendRTPPacketTo(const WebRtc_Word8 *data,
                                          WebRtc_UWord32 length,
                                          WebRtc_UWord16 rtpPort);
    virtual WebRtc_Word32 SendRTCPPacketTo(const WebRtc_Word8 *data,
                                           WebRtc_UWord32 length,
                                           WebRtc_UWord16 rtcpPort);
    // Transport functions
    virtual int SendPacket(int channel, const void* data, int length);
    virtual int SendRTCPPacket(int channel, const void* data, int length);

    // UdpTransport functions continue.
    virtual WebRtc_Word32 SetSendIP(const WebRtc_Word8* ipaddr);
    virtual WebRtc_Word32 SetSendPorts(const WebRtc_UWord16 rtpPort,
                                       const WebRtc_UWord16 rtcpPort = 0);

    virtual ErrorCode LastError() const;

    virtual WebRtc_Word32 IPAddressCached(const SocketAddress& address,
                                          WebRtc_Word8* ip,
                                          WebRtc_UWord32& ipSize,
                                          WebRtc_UWord16& sourcePort);

    WebRtc_Word32 Id() const {return _id;}
protected:
    // IncomingSocketCallback signature functions for receiving callbacks from
    // UdpSocketWrapper.
    static void IncomingRTPCallback(CallbackObj obj,
                                    const WebRtc_Word8* rtpPacket,
                                    WebRtc_Word32 rtpPacketLength,
                                    const SocketAddress* from);
    static void IncomingRTCPCallback(CallbackObj obj,
                                     const WebRtc_Word8* rtcpPacket,
                                     WebRtc_Word32 rtcpPacketLength,
                                     const SocketAddress* from);

    void CloseSendSockets();
    void CloseReceiveSockets();

    // Update _remoteRTPAddr according to _destPort and _destIP
    void BuildRemoteRTPAddr();
    // Update _remoteRTCPAddr according to _destPortRTCP and _destIP
    void BuildRemoteRTCPAddr();

    void BuildSockaddrIn(WebRtc_UWord16 portnr, const WebRtc_Word8* ip,
                         SocketAddress& remoteAddr) const;

    ErrorCode BindLocalRTPSocket();
    ErrorCode BindLocalRTCPSocket();

    ErrorCode BindRTPSendSocket();
    ErrorCode BindRTCPSendSocket();

    void IncomingRTPFunction(const WebRtc_Word8* rtpPacket,
                             WebRtc_Word32 rtpPacketLength,
                             const SocketAddress* from);
    void IncomingRTCPFunction(const WebRtc_Word8* rtcpPacket,
                              WebRtc_Word32 rtcpPacketLength,
                              const SocketAddress* from);

    bool FilterIPAddress(const SocketAddress* fromAddress);

    bool SetSockOptUsed();

    WebRtc_Word32 EnableQoS(WebRtc_Word32 serviceType, bool audio,
                            WebRtc_UWord32 maxBitrate,
                            WebRtc_Word32 overrideDSCP);

    WebRtc_Word32 DisableQoS();

private:
    void GetCachedAddress(WebRtc_Word8* ip, WebRtc_UWord32& ipSize,
                          WebRtc_UWord16& sourcePort);

    WebRtc_Word32 _id;
    // Protects the sockets from being re-configured while receiving packets.
    CriticalSectionWrapper* _crit;
    CriticalSectionWrapper* _critFilter;
    // _packetCallback's critical section.
    CriticalSectionWrapper* _critPacketCallback;
    UdpSocketManager* _mgr;
    ErrorCode _lastError;

    // Remote RTP and RTCP ports.
    WebRtc_UWord16 _destPort;
    WebRtc_UWord16 _destPortRTCP;

    // Local RTP and RTCP ports.
    WebRtc_UWord16 _localPort;
    WebRtc_UWord16 _localPortRTCP;

    // Local port number when the local port for receiving and local port number
    // for sending are not the same.
    WebRtc_UWord16 _srcPort;
    WebRtc_UWord16 _srcPortRTCP;

    // Remote port from which last received packet was sent.
    WebRtc_UWord16 _fromPort;
    WebRtc_UWord16 _fromPortRTCP;

    WebRtc_Word8 _fromIP[kIpAddressVersion6Length];
    WebRtc_Word8 _destIP[kIpAddressVersion6Length];
    WebRtc_Word8 _localIP[kIpAddressVersion6Length];
    WebRtc_Word8 _localMulticastIP[kIpAddressVersion6Length];

    UdpSocketWrapper* _ptrRtpSocket;
    UdpSocketWrapper* _ptrRtcpSocket;

    // Local port when the local port for receiving and local port for sending
    // are not the same.
    UdpSocketWrapper* _ptrSendRtpSocket;
    UdpSocketWrapper* _ptrSendRtcpSocket;

    SocketAddress _remoteRTPAddr;
    SocketAddress _remoteRTCPAddr;

    SocketAddress _localRTPAddr;
    SocketAddress _localRTCPAddr;

    WebRtc_Word32 _tos;
    bool _inCallbackMode;
    bool _receiving;
    bool _useSetSockOpt;
    bool _qos;
    WebRtc_Word32 _pcp;
    mutable bool _IpV6EnabledRead;
    bool _ipV6Enabled;
    WebRtc_Word32 _serviceType;
    WebRtc_Word32 _overrideDSCP;
    WebRtc_UWord32 _maxBitrate;

    // Cache used by GetCachedAddress(..).
    RWLockWrapper* _cachLock;
    SocketAddress _previousAddress;
    WebRtc_Word8 _previousIP[kIpAddressVersion6Length];
    WebRtc_UWord32 _previousIPSize;
    WebRtc_UWord16 _previousSourcePort;

    SocketAddress _filterIPAddress;
    WebRtc_UWord16 _rtpFilterPort;
    WebRtc_UWord16 _rtcpFilterPort;

    UdpTransportData* _packetCallback;
};
} // namespace webrtc

#endif // WEBRTC_MODULES_UDP_TRANSPORT_SOURCE_UDP_TRANSPORT_IMPL_H_
