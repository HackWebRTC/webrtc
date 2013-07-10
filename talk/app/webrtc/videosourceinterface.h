/*
 * libjingle
 * Copyright 2012, Google Inc.
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

#ifndef TALK_APP_WEBRTC_VIDEOSOURCEINTERFACE_H_
#define TALK_APP_WEBRTC_VIDEOSOURCEINTERFACE_H_

#include "talk/app/webrtc/mediastreaminterface.h"
#include "talk/media/base/mediachannel.h"

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
  // Adds |output| to the source to receive frames.
  virtual void AddSink(cricket::VideoRenderer* output) = 0;
  virtual void RemoveSink(cricket::VideoRenderer* output) = 0;
  virtual const cricket::VideoOptions* options() const = 0;

 protected:
  virtual ~VideoSourceInterface() {}
};

}  // namespace webrtc

#endif  // TALK_APP_WEBRTC_VIDEOSOURCEINTERFACE_H_
