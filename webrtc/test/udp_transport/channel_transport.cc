/*
 *  Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/test/udp_transport/include/channel_transport.h"

#include "webrtc/test/udp_transport/udp_transport.h"
#include "webrtc/video_engine/include/vie_network.h"
#include "webrtc/voice_engine/include/voe_network.h"

namespace webrtc {

VoiceChannelTransport::VoiceChannelTransport(VoENetwork* voe_network,
                                             int channel)
    : channel_(channel),
      voe_network_(voe_network) {
  WebRtc_UWord8 socket_threads = 1;
  socket_transport_ = UdpTransport::Create(channel, socket_threads);
  voe_network_->RegisterExternalTransport(channel, *socket_transport_);
}

VoiceChannelTransport::~VoiceChannelTransport() {
  voe_network_->DeRegisterExternalTransport(channel_);
  UdpTransport::Destroy(socket_transport_);
}

void VoiceChannelTransport::IncomingRTPPacket(
    const WebRtc_Word8* incoming_rtp_packet,
    const WebRtc_Word32 packet_length,
    const char* /*from_ip*/,
    const WebRtc_UWord16 /*from_port*/) {
  voe_network_->ReceivedRTPPacket(channel_, incoming_rtp_packet, packet_length);
}

void VoiceChannelTransport::IncomingRTCPPacket(
    const WebRtc_Word8* incoming_rtcp_packet,
    const WebRtc_Word32 packet_length,
    const char* /*from_ip*/,
    const WebRtc_UWord16 /*from_port*/) {
  voe_network_->ReceivedRTCPPacket(channel_, incoming_rtcp_packet,
                                   packet_length);
}

int VoiceChannelTransport::SetLocalReceiver(WebRtc_UWord16 rtp_port) {
  return socket_transport_->InitializeReceiveSockets(this, rtp_port);
}

int VoiceChannelTransport::SetSendDestination(const char* ip_address,
                                              WebRtc_UWord16 rtp_port) {
  return socket_transport_->InitializeSendSockets(ip_address, rtp_port);
}


VideoChannelTransport::VideoChannelTransport(ViENetwork* vie_network,
                                             int channel)
    : channel_(channel),
      vie_network_(vie_network) {
  WebRtc_UWord8 socket_threads = 1;
  socket_transport_ = UdpTransport::Create(channel, socket_threads);
  vie_network_->RegisterSendTransport(channel, *socket_transport_);
}
  
VideoChannelTransport::~VideoChannelTransport() {
  vie_network_->DeregisterSendTransport(channel_);
  UdpTransport::Destroy(socket_transport_);
}

void VideoChannelTransport::IncomingRTPPacket(
    const WebRtc_Word8* incoming_rtp_packet,
    const WebRtc_Word32 packet_length,
    const char* /*from_ip*/,
    const WebRtc_UWord16 /*from_port*/) {
  vie_network_->ReceivedRTPPacket(channel_, incoming_rtp_packet, packet_length);
}

void VideoChannelTransport::IncomingRTCPPacket(
    const WebRtc_Word8* incoming_rtcp_packet,
    const WebRtc_Word32 packet_length,
    const char* /*from_ip*/,
    const WebRtc_UWord16 /*from_port*/) {
  vie_network_->ReceivedRTCPPacket(channel_, incoming_rtcp_packet,
                                   packet_length);
}

int VideoChannelTransport::SetLocalReceiver(WebRtc_UWord16 rtp_port) {
  return socket_transport_->InitializeReceiveSockets(this, rtp_port);
}

int VideoChannelTransport::SetSendDestination(const char* ip_address,
                                              WebRtc_UWord16 rtp_port) {
  return socket_transport_->InitializeSendSockets(ip_address, rtp_port);
}

} // namespace webrtc
