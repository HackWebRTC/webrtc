/*
 *  Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#ifndef WEBRTC_TEST_DIRECT_TRANSPORT_H_
#define WEBRTC_TEST_DIRECT_TRANSPORT_H_

#include <assert.h>

#include <memory>

#include "webrtc/api/call/transport.h"
#include "webrtc/call/call.h"
#include "webrtc/rtc_base/sequenced_task_checker.h"
#include "webrtc/test/fake_network_pipe.h"
#include "webrtc/test/single_threaded_task_queue.h"

namespace webrtc {

class Clock;
class PacketReceiver;

namespace test {

class DirectTransport : public Transport {
 public:
  RTC_DEPRECATED DirectTransport(
      Call* send_call,
      const std::map<uint8_t, MediaType>& payload_type_map);
  RTC_DEPRECATED DirectTransport(
      const FakeNetworkPipe::Config& config,
      Call* send_call,
      const std::map<uint8_t, MediaType>& payload_type_map);
  RTC_DEPRECATED DirectTransport(
      const FakeNetworkPipe::Config& config,
      Call* send_call,
      std::unique_ptr<Demuxer> demuxer);

  // This deprecated variant always uses MediaType::VIDEO.
  RTC_DEPRECATED explicit DirectTransport(Call* send_call)
      : DirectTransport(
            FakeNetworkPipe::Config(),
            send_call,
            std::unique_ptr<Demuxer>(new ForceDemuxer(MediaType::VIDEO))) {}

  DirectTransport(SingleThreadedTaskQueueForTesting* task_queue,
                  Call* send_call,
                  const std::map<uint8_t, MediaType>& payload_type_map);

  DirectTransport(SingleThreadedTaskQueueForTesting* task_queue,
                  const FakeNetworkPipe::Config& config,
                  Call* send_call,
                  const std::map<uint8_t, MediaType>& payload_type_map);

  DirectTransport(SingleThreadedTaskQueueForTesting* task_queue,
                  const FakeNetworkPipe::Config& config,
                  Call* send_call,
                  std::unique_ptr<Demuxer> demuxer);

  ~DirectTransport() override;

  void SetConfig(const FakeNetworkPipe::Config& config);

  RTC_DEPRECATED void StopSending();

  // TODO(holmer): Look into moving this to the constructor.
  virtual void SetReceiver(PacketReceiver* receiver);

  bool SendRtp(const uint8_t* data,
               size_t length,
               const PacketOptions& options) override;
  bool SendRtcp(const uint8_t* data, size_t length) override;

  int GetAverageDelayMs();

 private:
  // TODO(minyue): remove when the deprecated ctors of DirectTransport that
  // create ForceDemuxer are removed.
  class ForceDemuxer : public Demuxer {
   public:
    explicit ForceDemuxer(MediaType media_type);
    void SetReceiver(PacketReceiver* receiver) override;
    void DeliverPacket(const NetworkPacket* packet,
                       const PacketTime& packet_time) override;

   private:
    const MediaType media_type_;
    PacketReceiver* packet_receiver_;
    RTC_DISALLOW_COPY_AND_ASSIGN(ForceDemuxer);
  };

  void SendPackets();

  Call* const send_call_;
  Clock* const clock_;

  // TODO(eladalon): Make |task_queue_| const.
  // https://bugs.chromium.org/p/webrtc/issues/detail?id=8125
  SingleThreadedTaskQueueForTesting* task_queue_;
  SingleThreadedTaskQueueForTesting::TaskId next_scheduled_task_;

  FakeNetworkPipe fake_network_;

  rtc::SequencedTaskChecker sequence_checker_;

  // TODO(eladalon): https://bugs.chromium.org/p/webrtc/issues/detail?id=8125
  // Deprecated versions of the ctor don't get the task queue passed in from
  // outside. We'll create one locally for them. This is deprecated, and will
  // be removed as soon as the need for those ctors is removed.
  std::unique_ptr<SingleThreadedTaskQueueForTesting> deprecated_task_queue_;
};
}  // namespace test
}  // namespace webrtc

#endif  // WEBRTC_TEST_DIRECT_TRANSPORT_H_
