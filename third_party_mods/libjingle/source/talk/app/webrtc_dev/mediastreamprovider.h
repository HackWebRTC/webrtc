/*
 * libjingle
 * Copyright 2011, Google Inc.
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

#ifndef TALK_APP_WEBRTC_DEV_MEDIASTREAMPROVIDER_H_
#define TALK_APP_WEBRTC_DEV_MEDIASTREAMPROVIDER_H_

#include "talk/app/webrtc_dev/mediastream.h"

namespace webrtc {

// Interface for setting media devices on a certain MediaTrack.
// This interface is called by classes in mediastreamhandler.h to
// set new devices.
class MediaProviderInterface {
 public:
  virtual void SetCaptureDevice(const std::string& name,
                                VideoCaptureModule* camera) = 0;
  virtual void SetLocalRenderer(const std::string& name,
                                cricket::VideoRenderer* renderer) = 0;
  virtual void SetRemoteRenderer(const std::string& name,
                                 cricket::VideoRenderer* renderer) = 0;
 protected:
  virtual ~MediaProviderInterface() {}
};

}  // namespace webrtc

#endif  // TALK_APP_WEBRTC_DEV_MEDIASTREAMPROVIDER_H_
