/*
 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MEDIA_BASE_VIDEOBROADCASTER_H_
#define WEBRTC_MEDIA_BASE_VIDEOBROADCASTER_H_

#include <utility>
#include <vector>

#include "webrtc/base/thread_checker.h"
#include "webrtc/media/base/videoframe.h"
#include "webrtc/media/base/videosinkinterface.h"
#include "webrtc/media/base/videosourceinterface.h"

namespace rtc {

class VideoBroadcaster : public VideoSourceInterface<cricket::VideoFrame>,
                         public VideoSinkInterface<cricket::VideoFrame> {
 public:
  VideoBroadcaster();
  void AddOrUpdateSink(VideoSinkInterface<cricket::VideoFrame>* sink,
                       const VideoSinkWants& wants) override;
  void RemoveSink(VideoSinkInterface<cricket::VideoFrame>* sink) override;

  // Returns true if the next frame will be delivered to at least one sink.
  bool frame_wanted() const;

  // Returns VideoSinkWants a source is requested to fulfill. They are
  // aggregated by all VideoSinkWants from all sinks.
  VideoSinkWants wants() const;

  void OnFrame(const cricket::VideoFrame& frame) override;

 protected:
  struct SinkPair {
    SinkPair(VideoSinkInterface<cricket::VideoFrame>* sink,
             VideoSinkWants wants)
        : sink(sink), wants(wants) {}
    VideoSinkInterface<cricket::VideoFrame>* sink;
    VideoSinkWants wants;
  };
  SinkPair* FindSinkPair(const VideoSinkInterface<cricket::VideoFrame>* sink);
  void UpdateWants();

  ThreadChecker thread_checker_;

  VideoSinkWants current_wants_;
  std::vector<SinkPair> sinks_;
};

}  // namespace rtc

#endif  // WEBRTC_MEDIA_BASE_VIDEOBROADCASTER_H_
