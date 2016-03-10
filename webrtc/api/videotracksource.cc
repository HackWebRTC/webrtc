/*
 *  Copyright 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/api/videotracksource.h"

#include <string>

#include "webrtc/base/bind.h"

namespace webrtc {

VideoTrackSource::VideoTrackSource(
    rtc::VideoSourceInterface<cricket::VideoFrame>* source,
    rtc::Thread* worker_thread,
    bool remote)
    : source_(source),
      worker_thread_(worker_thread),
      state_(kInitializing),
      remote_(remote) {}

void VideoTrackSource::SetState(SourceState new_state) {
  if (state_ != new_state) {
    state_ = new_state;
    FireOnChanged();
  }
}

void VideoTrackSource::OnSourceDestroyed() {
  source_ = nullptr;
}

void VideoTrackSource::AddOrUpdateSink(
    rtc::VideoSinkInterface<cricket::VideoFrame>* sink,
    const rtc::VideoSinkWants& wants) {
  if (!source_) {
    return;
  }
  worker_thread_->Invoke<void>(rtc::Bind(
      &rtc::VideoSourceInterface<cricket::VideoFrame>::AddOrUpdateSink, source_,
      sink, wants));
}

void VideoTrackSource::RemoveSink(
    rtc::VideoSinkInterface<cricket::VideoFrame>* sink) {
  if (!source_) {
    return;
  }
  worker_thread_->Invoke<void>(
      rtc::Bind(&rtc::VideoSourceInterface<cricket::VideoFrame>::RemoveSink,
                source_, sink));
}

}  //  namespace webrtc
