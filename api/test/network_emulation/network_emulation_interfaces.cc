/*
 *  Copyright (c) 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#include "api/test/network_emulation/network_emulation_interfaces.h"

namespace webrtc {

namespace {
constexpr size_t kIPv4HeaderSize = 20;
constexpr size_t kIPv6HeaderSize = 40;
}  // namespace

EmulatedIpPacket::EmulatedIpPacket(const rtc::SocketAddress& from,
                                   const rtc::SocketAddress& to,
                                   rtc::CopyOnWriteBuffer data,
                                   Timestamp arrival_time)
    : from(from),
      to(to),
      data(data),
      ip_header_size((to.family() == AF_INET) ? kIPv4HeaderSize
                                              : kIPv6HeaderSize),
      arrival_time(arrival_time) {
  RTC_DCHECK(to.family() == AF_INET || to.family() == AF_INET6);
}

}  // namespace webrtc
