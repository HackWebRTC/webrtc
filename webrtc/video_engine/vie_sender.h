/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

// ViESender is responsible for sending packets to network.

#ifndef WEBRTC_VIDEO_ENGINE_VIE_SENDER_H_
#define WEBRTC_VIDEO_ENGINE_VIE_SENDER_H_

#include "webrtc/base/scoped_ptr.h"
#include "webrtc/common_types.h"
#include "webrtc/engine_configurations.h"
#include "webrtc/typedefs.h"
#include "webrtc/video_engine/vie_defines.h"

namespace webrtc {

class CriticalSectionWrapper;
class Transport;
class VideoCodingModule;

class ViESender: public Transport {
 public:
  ViESender();

  // Registers transport to use for sending RTP and RTCP.
  int RegisterSendTransport(Transport* transport);
  int DeregisterSendTransport();

  // Implements Transport.
  int SendPacket(int vie_id, const void* data, size_t len) override;
  int SendRTCPPacket(int vie_id, const void* data, size_t len) override;

 private:
  rtc::scoped_ptr<CriticalSectionWrapper> critsect_;

  Transport* transport_;
};

}  // namespace webrtc

#endif  // WEBRTC_VIDEO_ENGINE_VIE_SENDER_H_
