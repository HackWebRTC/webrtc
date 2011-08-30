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
#include "talk/app/webrtc_dev/local_stream_dev.h"

namespace webrtc {

class LocalVideoTrackImpl : public NotifierImpl<LocalVideoTrack> {
 public:
  LocalVideoTrackImpl() {}
  explicit LocalVideoTrackImpl(VideoDevice* video_device)
      : enabled_(true),
        kind_(kVideoTrackKind),
        video_device_(video_device) {
  }

  virtual void SetRenderer(VideoRenderer* renderer) {
    video_renderer_ = renderer;
    NotifierImpl<LocalVideoTrack>::FireOnChanged();
  }

  virtual scoped_refptr<VideoRenderer> GetRenderer() {
    return video_renderer_.get();
  }

  // Get the VideoCapture device associated with this track.
  virtual scoped_refptr<VideoDevice> GetVideoCapture() {
    return video_device_.get();
  }

  // Implement MediaStreamTrack
  virtual const std::string& kind() {
    return kind_;
  }

  virtual const std::string& label() {
    return video_device_->name();
  }

  virtual bool enabled() {
    return enabled_;
  }

  virtual bool set_enabled(bool enable) {
    bool fire_on_change = enable != enabled_;
    enabled_ = enable;
    if (fire_on_change)
      NotifierImpl<LocalVideoTrack>::FireOnChanged();
  }

 private:
  bool enabled_;
  std::string kind_;
  scoped_refptr<VideoDevice> video_device_;
  scoped_refptr<VideoRenderer> video_renderer_;
};

scoped_refptr<LocalVideoTrack> LocalVideoTrack::Create(
    VideoDevice* video_device) {
  RefCountImpl<LocalVideoTrackImpl>* track =
      new RefCountImpl<LocalVideoTrackImpl>(video_device);
  return track;
}

}  // namespace webrtc
