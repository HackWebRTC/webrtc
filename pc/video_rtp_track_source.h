/*
 *  Copyright 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef PC_VIDEO_RTP_TRACK_SOURCE_H_
#define PC_VIDEO_RTP_TRACK_SOURCE_H_

#include "media/base/video_broadcaster.h"
#include "pc/video_track_source.h"

namespace webrtc {

// Video track source in use by VideoRtpReceiver
class VideoRtpTrackSource : public VideoTrackSource {
 public:
  VideoRtpTrackSource();

  // VideoTrackSource
  rtc::VideoSourceInterface<VideoFrame>* source() override;
  rtc::VideoSinkInterface<VideoFrame>* sink();

 private:
  // |broadcaster_| is needed since the decoder can only handle one sink.
  // It might be better if the decoder can handle multiple sinks and consider
  // the VideoSinkWants.
  rtc::VideoBroadcaster broadcaster_;

  RTC_DISALLOW_COPY_AND_ASSIGN(VideoRtpTrackSource);
};

}  // namespace webrtc

#endif  // PC_VIDEO_RTP_TRACK_SOURCE_H_
