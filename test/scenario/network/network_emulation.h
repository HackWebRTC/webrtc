/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef TEST_SCENARIO_NETWORK_NETWORK_EMULATION_H_
#define TEST_SCENARIO_NETWORK_NETWORK_EMULATION_H_

#include <cstdint>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "absl/types/optional.h"
#include "api/units/timestamp.h"
#include "rtc_base/asyncsocket.h"
#include "rtc_base/copyonwritebuffer.h"
#include "rtc_base/socketaddress.h"
#include "rtc_base/thread.h"

namespace webrtc {

struct EmulatedIpPacket {
 public:
  EmulatedIpPacket(const rtc::SocketAddress& from,
                   const rtc::SocketAddress& to,
                   uint64_t dest_endpoint_id,
                   rtc::CopyOnWriteBuffer data,
                   Timestamp arrival_time);

  ~EmulatedIpPacket();
  // This object is not copyable or assignable.
  EmulatedIpPacket(const EmulatedIpPacket&) = delete;
  EmulatedIpPacket& operator=(const EmulatedIpPacket&) = delete;
  // This object is only moveable.
  EmulatedIpPacket(EmulatedIpPacket&&);
  EmulatedIpPacket& operator=(EmulatedIpPacket&&);

  size_t size() const { return data.size(); }
  const uint8_t* cdata() const { return data.cdata(); }

  rtc::SocketAddress from;
  rtc::SocketAddress to;
  uint64_t dest_endpoint_id;
  rtc::CopyOnWriteBuffer data;
  Timestamp arrival_time;
};

class EmulatedNetworkReceiverInterface {
 public:
  virtual ~EmulatedNetworkReceiverInterface() = default;

  virtual void OnPacketReceived(EmulatedIpPacket packet) = 0;
};

}  // namespace webrtc

#endif  // TEST_SCENARIO_NETWORK_NETWORK_EMULATION_H_
