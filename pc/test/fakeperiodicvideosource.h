/*
 *  Copyright 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef PC_TEST_FAKEPERIODICVIDEOSOURCE_H_
#define PC_TEST_FAKEPERIODICVIDEOSOURCE_H_

#include "api/videosourceinterface.h"
#include "media/base/fakeframesource.h"
#include "media/base/videobroadcaster.h"
#include "rtc_base/task_queue.h"

namespace webrtc {

class FakePeriodicVideoSource final
    : public rtc::VideoSourceInterface<VideoFrame> {
 public:
  static constexpr int kFrameIntervalMs = 33;
  static constexpr int kWidth = 640;
  static constexpr int kHeight = 480;

  FakePeriodicVideoSource() {
    thread_checker_.DetachFromThread();
    task_queue_.PostTask(rtc::MakeUnique<FrameTask>(&broadcaster_));
  }

  void RemoveSink(rtc::VideoSinkInterface<webrtc::VideoFrame>* sink) override {
    RTC_DCHECK(thread_checker_.CalledOnValidThread());
    broadcaster_.RemoveSink(sink);
  }

  void AddOrUpdateSink(rtc::VideoSinkInterface<webrtc::VideoFrame>* sink,
                       const rtc::VideoSinkWants& wants) override {
    RTC_DCHECK(thread_checker_.CalledOnValidThread());
    broadcaster_.AddOrUpdateSink(sink, wants);
  }

 private:
  class FrameTask : public rtc::QueuedTask {
   public:
    explicit FrameTask(rtc::VideoSinkInterface<VideoFrame>* sink)
        : frame_source_(kWidth,
                        kHeight,
                        kFrameIntervalMs * rtc::kNumMicrosecsPerMillisec),
          sink_(sink) {}

    bool Run() override {
      sink_->OnFrame(frame_source_.GetFrame());
      rtc::TaskQueue::Current()->PostDelayedTask(rtc::WrapUnique(this),
                                                 kFrameIntervalMs);
      return false;
    }
    cricket::FakeFrameSource frame_source_;
    rtc::VideoSinkInterface<VideoFrame>* sink_;
  };

  void OnFrame(const webrtc::VideoFrame& frame) { broadcaster_.OnFrame(frame); }
  rtc::ThreadChecker thread_checker_;

  rtc::VideoBroadcaster broadcaster_;

  // Last member, depend on detruction order.
  rtc::TaskQueue task_queue_{"FakePeriodicVideoTrackSource"};
};

}  // namespace webrtc

#endif  // PC_TEST_FAKEPERIODICVIDEOSOURCE_H_
