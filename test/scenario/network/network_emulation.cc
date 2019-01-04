/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "test/scenario/network/network_emulation.h"

#include <memory>

namespace webrtc {

EmulatedIpPacket::EmulatedIpPacket(const rtc::SocketAddress& from,
                                   const rtc::SocketAddress& to,
                                   uint64_t dest_endpoint_id,
                                   rtc::CopyOnWriteBuffer data,
                                   Timestamp arrival_time)
    : from(from),
      to(to),
      dest_endpoint_id(dest_endpoint_id),
      data(data),
      arrival_time(arrival_time) {}

EmulatedIpPacket::~EmulatedIpPacket() = default;

EmulatedIpPacket::EmulatedIpPacket(EmulatedIpPacket&&) = default;

}  // namespace webrtc
