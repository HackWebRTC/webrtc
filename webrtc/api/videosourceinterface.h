/*
 *  Copyright 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_API_VIDEOSOURCEINTERFACE_H_
#define WEBRTC_API_VIDEOSOURCEINTERFACE_H_

#include "webrtc/api/mediastreaminterface.h"
#include "webrtc/media/base/mediachannel.h"
#include "webrtc/media/base/videosourceinterface.h"

namespace webrtc {

// VideoSourceInterface is a reference counted source used for VideoTracks.
// The same source can be used in multiple VideoTracks.
// The methods are only supposed to be called by the PeerConnection
// implementation.
class VideoSourceInterface :
    public MediaSourceInterface,
    public rtc::VideoSourceInterface<cricket::VideoFrame> {
 public:
  // Get access to the source implementation of cricket::VideoCapturer.
  // This can be used for receiving frames and state notifications.
  // But it should not be used for starting or stopping capturing.
  virtual cricket::VideoCapturer* GetVideoCapturer() = 0;

  virtual void Stop() = 0;
  virtual void Restart() = 0;

  virtual const cricket::VideoOptions* options() const = 0;

 protected:
  virtual ~VideoSourceInterface() {}
};

// TODO(perkj): Rename webrtc::VideoSourceInterface to
// webrtc::VideoTrackSourceInterface
using VideoTrackSourceInterface = VideoSourceInterface;

}  // namespace webrtc

#endif  // WEBRTC_API_VIDEOSOURCEINTERFACE_H_
