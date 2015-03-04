/*
 * libjingle
 * Copyright 2013 Google Inc.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 *  1. Redistributions of source code must retain the above copyright notice,
 *     this list of conditions and the following disclaimer.
 *  2. Redistributions in binary form must reproduce the above copyright notice,
 *     this list of conditions and the following disclaimer in the documentation
 *     and/or other materials provided with the distribution.
 *  3. The name of the author may not be used to endorse or promote products
 *     derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO
 * EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef TALK_APP_WEBRTC_REMOTEVIDEOCAPTURER_H_
#define TALK_APP_WEBRTC_REMOTEVIDEOCAPTURER_H_

#include <vector>

#include "talk/app/webrtc/mediastreaminterface.h"
#include "talk/media/base/videocapturer.h"
#include "talk/media/base/videorenderer.h"

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
  bool GetPreferredFourccs(std::vector<uint32>* fourccs) override;
  bool GetBestCaptureFormat(const cricket::VideoFormat& desired,
                            cricket::VideoFormat* best_format) override;
  bool IsScreencast() const override;

 private:
  DISALLOW_COPY_AND_ASSIGN(RemoteVideoCapturer);
};

}  // namespace webrtc

#endif  // TALK_APP_WEBRTC_REMOTEVIDEOCAPTURER_H_
