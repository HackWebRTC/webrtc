/*
 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MEDIA_BASE_VIDEOSOURCEINTERFACE_H_
#define WEBRTC_MEDIA_BASE_VIDEOSOURCEINTERFACE_H_

#include "webrtc/media/base/videosinkinterface.h"
#include "webrtc/base/callback.h"

namespace rtc {

// VideoSinkWants is used for notifying the source of properties a video frame
// should have when it is delivered to a certain sink.
struct VideoSinkWants {
  bool operator==(const VideoSinkWants& rh) const {
    return rotation_applied == rh.rotation_applied;
  }
  bool operator!=(const VideoSinkWants& rh) const { return !operator==(rh); }

  // Tells the source whether the sink wants frames with rotation applied.
  // By default, the rotation is applied by the source.
  bool rotation_applied = true;
};

template <typename VideoFrameT>
class VideoSourceInterface {
 public:
  virtual void AddOrUpdateSink(VideoSinkInterface<VideoFrameT>* sink,
                               const VideoSinkWants& wants) = 0;
  // RemoveSink must guarantee that at the time the method returns,
  // there is no current and no future calls to VideoSinkInterface::OnFrame.
  virtual void RemoveSink(VideoSinkInterface<VideoFrameT>* sink) = 0;

 protected:
  virtual ~VideoSourceInterface() {}
};

}  // namespace rtc
#endif  // WEBRTC_MEDIA_BASE_VIDEOSOURCEINTERFACE_H_
