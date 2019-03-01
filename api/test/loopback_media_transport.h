/*
 *  Copyright 2018 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef API_TEST_LOOPBACK_MEDIA_TRANSPORT_H_
#define API_TEST_LOOPBACK_MEDIA_TRANSPORT_H_

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "absl/memory/memory.h"
#include "api/media_transport_interface.h"
#include "rtc_base/async_invoker.h"
#include "rtc_base/critical_section.h"
#include "rtc_base/thread.h"
#include "rtc_base/thread_checker.h"

namespace webrtc {

// Wrapper used to hand out unique_ptrs to loopback media
// transport without ownership changes to the underlying
// transport.
// It works in two modes:
// It can either wrap a factory, or it can wrap an existing interface.
// In the former mode, it delegates the work to the wrapped factory.
// In the latter mode, it always returns static instance of the transport
// interface.
//
// Example use:
// Factory wrap_static_interface = Wrapper(media_transport_interface);
// Factory wrap_factory = Wrapper(wrap_static_interface);
// The second factory may be created multiple times, and ownership may be passed
// to the client. The first factory counts the number of invocations of
// CreateMediaTransport();
class WrapperMediaTransportFactory : public MediaTransportFactory {
 public:
  explicit WrapperMediaTransportFactory(MediaTransportInterface* wrapped);
  explicit WrapperMediaTransportFactory(MediaTransportFactory* wrapped);

  RTCErrorOr<std::unique_ptr<MediaTransportInterface>> CreateMediaTransport(
      rtc::PacketTransportInternal* packet_transport,
      rtc::Thread* network_thread,
      const MediaTransportSettings& settings) override;

  RTCErrorOr<std::unique_ptr<MediaTransportInterface>> CreateMediaTransport(
      rtc::Thread* network_thread,
      const MediaTransportSettings& settings) override;

  std::string GetTransportName() const override;

  int created_transport_count() const;

 private:
  MediaTransportInterface* wrapped_;
  MediaTransportFactory* wrapped_factory_ = nullptr;
  int created_transport_count_ = 0;
};

// Contains two MediaTransportsInterfaces that are connected to each other.
// Currently supports audio only.
class MediaTransportPair {
 public:
  struct Stats {
    int sent_audio_frames = 0;
    int received_audio_frames = 0;
    int sent_video_frames = 0;
    int received_video_frames = 0;
  };

  explicit MediaTransportPair(rtc::Thread* thread);
  ~MediaTransportPair();

  // Ownership stays with MediaTransportPair
  MediaTransportInterface* first() { return &first_; }
  MediaTransportInterface* second() { return &second_; }

  std::unique_ptr<MediaTransportFactory> first_factory() {
    return absl::make_unique<WrapperMediaTransportFactory>(&first_factory_);
  }

  std::unique_ptr<MediaTransportFactory> second_factory() {
    return absl::make_unique<WrapperMediaTransportFactory>(&second_factory_);
  }

  void SetState(MediaTransportState state) {
    first_.SetState(state);
    second_.SetState(state);
  }

  void FlushAsyncInvokes() {
    first_.FlushAsyncInvokes();
    second_.FlushAsyncInvokes();
  }

  Stats FirstStats() { return first_.GetStats(); }
  Stats SecondStats() { return second_.GetStats(); }

  int first_factory_transport_count() const {
    return first_factory_.created_transport_count();
  }

  int second_factory_transport_count() const {
    return second_factory_.created_transport_count();
  }

 private:
  class LoopbackMediaTransport : public MediaTransportInterface {
   public:
    LoopbackMediaTransport(rtc::Thread* thread, LoopbackMediaTransport* other);

    ~LoopbackMediaTransport() override;

    RTCError SendAudioFrame(uint64_t channel_id,
                            MediaTransportEncodedAudioFrame frame) override;

    RTCError SendVideoFrame(
        uint64_t channel_id,
        const MediaTransportEncodedVideoFrame& frame) override;

    void SetKeyFrameRequestCallback(
        MediaTransportKeyFrameRequestCallback* callback) override;

    RTCError RequestKeyFrame(uint64_t channel_id) override;

    void SetReceiveAudioSink(MediaTransportAudioSinkInterface* sink) override;

    void SetReceiveVideoSink(MediaTransportVideoSinkInterface* sink) override;

    void AddTargetTransferRateObserver(
        TargetTransferRateObserver* observer) override;

    void RemoveTargetTransferRateObserver(
        TargetTransferRateObserver* observer) override;

    void AddRttObserver(MediaTransportRttObserver* observer) override;
    void RemoveRttObserver(MediaTransportRttObserver* observer) override;

    void SetMediaTransportStateCallback(
        MediaTransportStateCallback* callback) override;

    RTCError OpenChannel(int channel_id) override;

    RTCError SendData(int channel_id,
                      const SendDataParams& params,
                      const rtc::CopyOnWriteBuffer& buffer) override;

    RTCError CloseChannel(int channel_id) override;

    void SetDataSink(DataChannelSink* sink) override;

    void SetState(MediaTransportState state);

    void FlushAsyncInvokes();

    Stats GetStats();

    void SetAllocatedBitrateLimits(
        const MediaTransportAllocatedBitrateLimits& limits) override;

    absl::optional<std::string> GetTransportParametersOffer() const override;

   private:
    void OnData(uint64_t channel_id, MediaTransportEncodedAudioFrame frame);

    void OnData(uint64_t channel_id, MediaTransportEncodedVideoFrame frame);

    void OnData(int channel_id,
                DataMessageType type,
                const rtc::CopyOnWriteBuffer& buffer);

    void OnKeyFrameRequested(int channel_id);

    void OnRemoteCloseChannel(int channel_id);

    void OnStateChanged() RTC_RUN_ON(thread_);

    rtc::Thread* const thread_;
    rtc::CriticalSection sink_lock_;
    rtc::CriticalSection stats_lock_;

    MediaTransportAudioSinkInterface* audio_sink_ RTC_GUARDED_BY(sink_lock_) =
        nullptr;
    MediaTransportVideoSinkInterface* video_sink_ RTC_GUARDED_BY(sink_lock_) =
        nullptr;
    DataChannelSink* data_sink_ RTC_GUARDED_BY(sink_lock_) = nullptr;

    MediaTransportKeyFrameRequestCallback* key_frame_callback_
        RTC_GUARDED_BY(sink_lock_) = nullptr;

    MediaTransportStateCallback* state_callback_ RTC_GUARDED_BY(sink_lock_) =
        nullptr;

    std::vector<TargetTransferRateObserver*> target_transfer_rate_observers_
        RTC_GUARDED_BY(sink_lock_);
    std::vector<MediaTransportRttObserver*> rtt_observers_
        RTC_GUARDED_BY(sink_lock_);

    MediaTransportState state_ RTC_GUARDED_BY(thread_) =
        MediaTransportState::kPending;

    LoopbackMediaTransport* const other_;

    Stats stats_ RTC_GUARDED_BY(stats_lock_);

    rtc::AsyncInvoker invoker_;
  };

  LoopbackMediaTransport first_;
  LoopbackMediaTransport second_;
  WrapperMediaTransportFactory first_factory_;
  WrapperMediaTransportFactory second_factory_;
};

}  // namespace webrtc

#endif  // API_TEST_LOOPBACK_MEDIA_TRANSPORT_H_
