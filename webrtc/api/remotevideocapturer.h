/*
 *  Copyright 2013 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_API_REMOTEVIDEOCAPTURER_H_
#define WEBRTC_API_REMOTEVIDEOCAPTURER_H_

#include <vector>

#include "webrtc/api/mediastreaminterface.h"
#include "webrtc/media/base/videocapturer.h"
#include "webrtc/media/base/videorenderer.h"

namespace webrtc {

// RemoteVideoCapturer implements a simple cricket::VideoCapturer which
// gets decoded remote video frames from media channel.
// It's used as the remote video source's VideoCapturer so that the remote video
// can be used as a cricket::VideoCapturer and in that way a remote video stream
// can implement the MediaStreamSourceInterface.
class RemoteVideoCapturer : public cricket::VideoCapturer {
 public:
  RemoteVideoCapturer();
  virtual ~RemoteVideoCapturer();

  // cricket::VideoCapturer implementation.
  cricket::CaptureState Start(
      const cricket::VideoFormat& capture_format) override;
  void Stop() override;
  bool IsRunning() override;
  bool GetPreferredFourccs(std::vector<uint32_t>* fourccs) override;
  bool GetBestCaptureFormat(const cricket::VideoFormat& desired,
                            cricket::VideoFormat* best_format) override;
  bool IsScreencast() const override;

 private:
  RTC_DISALLOW_COPY_AND_ASSIGN(RemoteVideoCapturer);
};

}  // namespace webrtc

#endif  // WEBRTC_API_REMOTEVIDEOCAPTURER_H_
