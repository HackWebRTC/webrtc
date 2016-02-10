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
#include "webrtc/media/base/videorenderer.h"

namespace webrtc {

// VideoSourceInterface is a reference counted source used for VideoTracks.
// The same source can be used in multiple VideoTracks.
// The methods are only supposed to be called by the PeerConnection
// implementation.
class VideoSourceInterface : public MediaSourceInterface {
 public:
  // Get access to the source implementation of cricket::VideoCapturer.
  // This can be used for receiving frames and state notifications.
  // But it should not be used for starting or stopping capturing.
  virtual cricket::VideoCapturer* GetVideoCapturer() = 0;

  // Stop the video capturer.
  virtual void Stop() = 0;
  virtual void Restart() = 0;

  // Adds |output| to the source to receive frames.
  virtual void AddSink(
      rtc::VideoSinkInterface<cricket::VideoFrame>* output) = 0;
  virtual void RemoveSink(
      rtc::VideoSinkInterface<cricket::VideoFrame>* output) = 0;
  virtual const cricket::VideoOptions* options() const = 0;
  // TODO(nisse): Dummy implementation. Delete as soon as chrome's
  // MockVideoSource is updated.
  virtual cricket::VideoRenderer* FrameInput() { return nullptr; }

 protected:
  virtual ~VideoSourceInterface() {}
};

}  // namespace webrtc

#endif  // WEBRTC_API_VIDEOSOURCEINTERFACE_H_
