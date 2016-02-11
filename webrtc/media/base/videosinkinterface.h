/*
 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MEDIA_BASE_VIDEOSINKINTERFACE_H_
#define WEBRTC_MEDIA_BASE_VIDEOSINKINTERFACE_H_

namespace rtc {

template <typename VideoFrameT>
class VideoSinkInterface {
 public:
  virtual void OnFrame(const VideoFrameT& frame) = 0;

 protected:
  ~VideoSinkInterface() {}
};

}  // namespace rtc

#endif  // WEBRTC_MEDIA_BASE_VIDEOSINKINTERFACE_H_
