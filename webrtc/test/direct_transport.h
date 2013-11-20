/*
 *  Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#ifndef WEBRTC_VIDEO_ENGINE_TEST_COMMON_DIRECT_TRANSPORT_H_
#define WEBRTC_VIDEO_ENGINE_TEST_COMMON_DIRECT_TRANSPORT_H_

#include <assert.h>

#include <deque>

#include "webrtc/system_wrappers/interface/critical_section_wrapper.h"
#include "webrtc/system_wrappers/interface/event_wrapper.h"
#include "webrtc/system_wrappers/interface/scoped_ptr.h"
#include "webrtc/system_wrappers/interface/thread_wrapper.h"
#include "webrtc/transport.h"

namespace webrtc {

class Clock;
class PacketReceiver;

namespace test {

class DirectTransport : public newapi::Transport {
 public:
  DirectTransport();
  explicit DirectTransport(int delay_ms);
  ~DirectTransport();

  virtual void StopSending();
  virtual void SetReceiver(PacketReceiver* receiver);

  virtual bool SendRtp(const uint8_t* data, size_t length) OVERRIDE;
  virtual bool SendRtcp(const uint8_t* data, size_t length) OVERRIDE;

 private:
  struct Packet {
    Packet();
    Packet(const uint8_t* data, size_t length, int64_t delivery_time_ms);

    uint8_t data[1500];
    size_t length;
    int64_t delivery_time_ms;
  };

  void QueuePacket(const uint8_t* data,
                   size_t length,
                   int64_t delivery_time_ms);

  static bool NetworkProcess(void* transport);
  bool SendPackets();

  scoped_ptr<CriticalSectionWrapper> lock_;
  scoped_ptr<EventWrapper> packet_event_;
  scoped_ptr<ThreadWrapper> thread_;
  Clock* clock_;

  bool shutting_down_;

  std::deque<Packet> packet_queue_;
  PacketReceiver* receiver_;
  // TODO(stefan): Replace this with FakeNetworkPipe.
  const int delay_ms_;
};
}  // namespace test
}  // namespace webrtc

#endif  // WEBRTC_VIDEO_ENGINE_TEST_COMMON_DIRECT_TRANSPORT_H_
