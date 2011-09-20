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

#ifndef TALK_APP_WEBRTC_VIDEOTRACKIMPL_H_
#define TALK_APP_WEBRTC_VIDEOTRACKIMPL_H_

#include <string>

#include "talk/app/webrtc_dev/mediastream.h"
#include "talk/app/webrtc_dev/notifierimpl.h"
#include "talk/app/webrtc_dev/scoped_refptr.h"

#ifdef WEBRTC_RELATIVE_PATH
#include "modules/video_capture/main/interface/video_capture.h"
#else
#include "third_party/webrtc/files/include/video_capture.h"
#endif

namespace webrtc {

class VideoTrackImpl : public NotifierImpl<LocalVideoTrack> {
 public:
  static scoped_refptr<VideoTrack> Create(const std::string& label,
                                          uint32 ssrc);
  virtual scoped_refptr<VideoCaptureModule> GetVideoCapture();
  virtual void SetRenderer(VideoRenderer* renderer);
  scoped_refptr<VideoRenderer> GetRenderer();

  virtual const std::string& kind();
  virtual const std::string& label();
  virtual uint32 ssrc();
  virtual bool enabled();
  virtual bool set_enabled(bool enable);
  virtual bool set_ssrc(uint32 ssrc);

 protected:
  VideoTrackImpl(const std::string& label, uint32 ssrc);
  VideoTrackImpl(const std::string& label, VideoCaptureModule* video_device);

 private:
  bool enabled_;
  std::string kind_;
  std::string label_;
  uint32 ssrc_;
  scoped_refptr<VideoCaptureModule> video_device_;
  scoped_refptr<VideoRenderer> video_renderer_;
};

}  // namespace webrtc

#endif  // TALK_APP_WEBRTC_VIDEOTRACKIMPL_H_
