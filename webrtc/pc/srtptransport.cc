/*
 *  Copyright 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/pc/srtptransport.h"

#include <string>

#include "webrtc/media/base/rtputils.h"
#include "webrtc/pc/rtptransport.h"
#include "webrtc/pc/srtpsession.h"
#include "webrtc/rtc_base/asyncpacketsocket.h"
#include "webrtc/rtc_base/copyonwritebuffer.h"
#include "webrtc/rtc_base/ptr_util.h"
#include "webrtc/rtc_base/trace_event.h"

namespace webrtc {

SrtpTransport::SrtpTransport(bool rtcp_mux_enabled,
                             const std::string& content_name)
    : content_name_(content_name),
      rtp_transport_(rtc::MakeUnique<RtpTransport>(rtcp_mux_enabled)) {
  ConnectToRtpTransport();
}

SrtpTransport::SrtpTransport(std::unique_ptr<RtpTransportInternal> transport,
                             const std::string& content_name)
    : content_name_(content_name), rtp_transport_(std::move(transport)) {
  ConnectToRtpTransport();
}

void SrtpTransport::ConnectToRtpTransport() {
  rtp_transport_->SignalPacketReceived.connect(
      this, &SrtpTransport::OnPacketReceived);
  rtp_transport_->SignalReadyToSend.connect(this,
                                            &SrtpTransport::OnReadyToSend);
}

bool SrtpTransport::SendPacket(bool rtcp,
                               rtc::CopyOnWriteBuffer* packet,
                               const rtc::PacketOptions& options,
                               int flags) {
  // TODO(zstein): Protect packet.

  return rtp_transport_->SendPacket(rtcp, packet, options, flags);
}

void SrtpTransport::OnPacketReceived(bool rtcp,
                                     rtc::CopyOnWriteBuffer* packet,
                                     const rtc::PacketTime& packet_time) {
  // TODO(zstein): Unprotect packet.

  SignalPacketReceived(rtcp, packet, packet_time);
}

}  // namespace webrtc
