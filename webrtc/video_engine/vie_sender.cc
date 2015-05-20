/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/video_engine/vie_sender.h"

#include "webrtc/modules/rtp_rtcp/source/rtp_sender.h"
#include "webrtc/system_wrappers/interface/critical_section_wrapper.h"
#include "webrtc/system_wrappers/interface/trace.h"

namespace webrtc {

ViESender::ViESender()
    : critsect_(CriticalSectionWrapper::CreateCriticalSection()),
      transport_(NULL) {
}

int ViESender::RegisterSendTransport(Transport* transport) {
  CriticalSectionScoped cs(critsect_.get());
  if (transport_) {
    return -1;
  }
  transport_ = transport;
  return 0;
}

int ViESender::DeregisterSendTransport() {
  CriticalSectionScoped cs(critsect_.get());
  if (transport_ == NULL) {
    return -1;
  }
  transport_ = NULL;
  return 0;
}

int ViESender::SendPacket(int id, const void* data, size_t len) {
  CriticalSectionScoped cs(critsect_.get());
  if (!transport_) {
    // No transport
    return -1;
  }
  return transport_->SendPacket(id, data, len);
}

int ViESender::SendRTCPPacket(int id, const void* data, size_t len) {
  CriticalSectionScoped cs(critsect_.get());
  if (!transport_) {
    return -1;
  }

  return transport_->SendRTCPPacket(id, data, len);
}

}  // namespace webrtc
