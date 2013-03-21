/*
 *  Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_TEST_CHANNEL_TRANSPORT_INCLUDE_CHANNEL_TRANSPORT_H_
#define WEBRTC_TEST_CHANNEL_TRANSPORT_INCLUDE_CHANNEL_TRANSPORT_H_

#include "webrtc/test/channel_transport/udp_transport.h"

namespace webrtc {

class ViENetwork;
class VoENetwork;

namespace test {

// Helper class for VoiceEngine tests.
class VoiceChannelTransport : public UdpTransportData {
 public:
  VoiceChannelTransport(VoENetwork* voe_network, int channel);

  virtual ~VoiceChannelTransport();

  // Start implementation of UdpTransportData.
  void IncomingRTPPacket(const WebRtc_Word8* incoming_rtp_packet,
                         const WebRtc_Word32 packet_length,
                         const char* /*from_ip*/,
                         const WebRtc_UWord16 /*from_port*/);

  void IncomingRTCPPacket(const WebRtc_Word8* incoming_rtcp_packet,
                          const WebRtc_Word32 packet_length,
                          const char* /*from_ip*/,
                          const WebRtc_UWord16 /*from_port*/);
  // End implementation of UdpTransportData.

  // Specifies the ports to receive RTP packets on.
  int SetLocalReceiver(WebRtc_UWord16 rtp_port);

  // Specifies the destination port and IP address for a specified channel.  
  int SetSendDestination(const char* ip_address, WebRtc_UWord16 rtp_port);

 private:
  int channel_;
  VoENetwork* voe_network_;
  UdpTransport* socket_transport_;
};

// Helper class for VideoEngine tests.
class VideoChannelTransport : public UdpTransportData {
 public:
  VideoChannelTransport(ViENetwork* vie_network, int channel);

  virtual  ~VideoChannelTransport();

  // Start implementation of UdpTransportData.
  void IncomingRTPPacket(const WebRtc_Word8* incoming_rtp_packet,
                         const WebRtc_Word32 packet_length,
                         const char* /*from_ip*/,
                         const WebRtc_UWord16 /*from_port*/);

  void IncomingRTCPPacket(const WebRtc_Word8* incoming_rtcp_packet,
                          const WebRtc_Word32 packet_length,
                          const char* /*from_ip*/,
                          const WebRtc_UWord16 /*from_port*/);
  // End implementation of UdpTransportData.

  // Specifies the ports to receive RTP packets on.
  int SetLocalReceiver(WebRtc_UWord16 rtp_port);

  // Specifies the destination port and IP address for a specified channel.  
  int SetSendDestination(const char* ip_address, WebRtc_UWord16 rtp_port);

 private:
  int channel_;
  ViENetwork* vie_network_;
  UdpTransport* socket_transport_;
};

}  // namespace test
}  // namespace webrtc

#endif  // WEBRTC_TEST_CHANNEL_TRANSPORT_INCLUDE_CHANNEL_TRANSPORT_H_

