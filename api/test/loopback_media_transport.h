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

#include <utility>

#include "api/media_transport_interface.h"
#include "rtc_base/asyncinvoker.h"
#include "rtc_base/criticalsection.h"
#include "rtc_base/thread.h"

namespace webrtc {

// Contains two MediaTransportsInterfaces that are connected to each other.
// Currently supports audio only.
class MediaTransportPair {
 public:
  explicit MediaTransportPair(rtc::Thread* thread)
      : first_(thread, &second_), second_(thread, &first_) {}

  // Ownership stays with MediaTransportPair
  MediaTransportInterface* first() { return &first_; }
  MediaTransportInterface* second() { return &second_; }

  void FlushAsyncInvokes() {
    first_.FlushAsyncInvokes();
    second_.FlushAsyncInvokes();
  }

 private:
  class LoopbackMediaTransport : public MediaTransportInterface {
   public:
    LoopbackMediaTransport(rtc::Thread* thread, LoopbackMediaTransport* other)
        : thread_(thread), other_(other) {}

    ~LoopbackMediaTransport() {
      rtc::CritScope lock(&sink_lock_);
      RTC_CHECK(sink_ == nullptr);
      RTC_CHECK(data_sink_ == nullptr);
    }

    RTCError SendAudioFrame(uint64_t channel_id,
                            MediaTransportEncodedAudioFrame frame) override {
      invoker_.AsyncInvoke<void>(RTC_FROM_HERE, thread_,
                                 [this, channel_id, frame] {
                                   other_->OnData(channel_id, std::move(frame));
                                 });
      return RTCError::OK();
    };

    RTCError SendVideoFrame(
        uint64_t channel_id,
        const MediaTransportEncodedVideoFrame& frame) override {
      return RTCError::OK();
    }

    RTCError RequestKeyFrame(uint64_t channel_id) override {
      return RTCError::OK();
    }

    void SetReceiveAudioSink(MediaTransportAudioSinkInterface* sink) override {
      rtc::CritScope lock(&sink_lock_);
      if (sink) {
        RTC_CHECK(sink_ == nullptr);
      }
      sink_ = sink;
    }

    void SetReceiveVideoSink(MediaTransportVideoSinkInterface* sink) override {}

    void SetTargetTransferRateObserver(
        webrtc::TargetTransferRateObserver* observer) override {}

    void SetMediaTransportStateCallback(
        MediaTransportStateCallback* callback) override {}

    RTCError SendData(int channel_id,
                      const SendDataParams& params,
                      const rtc::CopyOnWriteBuffer& buffer) override {
      invoker_.AsyncInvoke<void>(
          RTC_FROM_HERE, thread_, [this, channel_id, params, buffer] {
            other_->OnData(channel_id, params.type, buffer);
          });
      return RTCError::OK();
    }

    RTCError CloseChannel(int channel_id) override {
      invoker_.AsyncInvoke<void>(RTC_FROM_HERE, thread_, [this, channel_id] {
        other_->OnRemoteCloseChannel(channel_id);
        rtc::CritScope lock(&sink_lock_);
        if (data_sink_) {
          data_sink_->OnChannelClosed(channel_id);
        }
      });
      return RTCError::OK();
    }

    void SetDataSink(DataChannelSink* sink) override {
      rtc::CritScope lock(&sink_lock_);
      data_sink_ = sink;
    }

    void FlushAsyncInvokes() { invoker_.Flush(thread_); }

   private:
    void OnData(uint64_t channel_id, MediaTransportEncodedAudioFrame frame) {
      rtc::CritScope lock(&sink_lock_);
      if (sink_) {
        sink_->OnData(channel_id, frame);
      }
    }

    void OnData(int channel_id,
                DataMessageType type,
                const rtc::CopyOnWriteBuffer& buffer) {
      rtc::CritScope lock(&sink_lock_);
      if (data_sink_) {
        data_sink_->OnDataReceived(channel_id, type, buffer);
      }
    }

    void OnRemoteCloseChannel(int channel_id) {
      rtc::CritScope lock(&sink_lock_);
      if (data_sink_) {
        data_sink_->OnChannelClosing(channel_id);
        data_sink_->OnChannelClosed(channel_id);
      }
    }

    rtc::Thread* const thread_;
    rtc::CriticalSection sink_lock_;

    MediaTransportAudioSinkInterface* sink_ RTC_GUARDED_BY(sink_lock_) =
        nullptr;
    DataChannelSink* data_sink_ RTC_GUARDED_BY(sink_lock_) = nullptr;
    LoopbackMediaTransport* const other_;

    rtc::AsyncInvoker invoker_;
  };

  LoopbackMediaTransport first_;
  LoopbackMediaTransport second_;
};

}  // namespace webrtc

#endif  // API_TEST_LOOPBACK_MEDIA_TRANSPORT_H_
