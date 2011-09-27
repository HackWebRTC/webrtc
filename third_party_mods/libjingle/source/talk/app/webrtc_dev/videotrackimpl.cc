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
#include "talk/app/webrtc_dev/videotrackimpl.h"

#include <string>

namespace webrtc {

VideoTrackImpl::VideoTrackImpl(const std::string& label, uint32 ssrc)
    : enabled_(true),
      kind_(kVideoTrackKind),
      label_(label),
      ssrc_(ssrc),
      state_(kInitializing),
      video_device_(NULL) {
}

VideoTrackImpl::VideoTrackImpl(const std::string& label,
                               VideoCaptureModule* video_device)
    : enabled_(true),
      kind_(kVideoTrackKind),
      label_(label),
      ssrc_(0),
      state_(kInitializing),
      video_device_(video_device) {
}

void VideoTrackImpl::SetRenderer(VideoRenderer* renderer) {
  video_renderer_ = renderer;
  NotifierImpl<LocalVideoTrack>::FireOnChanged();
}

scoped_refptr<VideoRenderer> VideoTrackImpl::GetRenderer() {
  return video_renderer_;
}

  // Get the VideoCapture device associated with this track.
scoped_refptr<VideoCaptureModule> VideoTrackImpl::GetVideoCapture() {
  return video_device_.get();
}

const std::string& VideoTrackImpl::kind() {
  return kind_;
}

const std::string& VideoTrackImpl::label() {
  return label_;
}

bool VideoTrackImpl::enabled() {
    return enabled_;
}

bool VideoTrackImpl::set_enabled(bool enable) {
  bool fire_on_change = enable != enabled_;
  enabled_ = enable;
  if (fire_on_change)
    NotifierImpl<LocalVideoTrack>::FireOnChanged();
}

uint32 VideoTrackImpl::ssrc() {
  return ssrc_;
}

bool VideoTrackImpl::set_ssrc(uint32 ssrc) {
  ASSERT(ssrc_ == 0);
  ASSERT(ssrc != 0);
  if (ssrc_ != 0)
    return false;
  ssrc_ = ssrc;
  NotifierImpl<LocalVideoTrack>::FireOnChanged();
  return true;
}

MediaStreamTrack::TrackState VideoTrackImpl::state() {
  return state_;
}

bool VideoTrackImpl::set_state(TrackState new_state) {
  bool fire_on_change = state_ != new_state;
  state_ = new_state;
  if (fire_on_change)
    NotifierImpl<LocalVideoTrack>::FireOnChanged();
  return true;
}

scoped_refptr<VideoTrack> VideoTrackImpl::Create(const std::string& label,
                                                     uint32 ssrc) {
  RefCountImpl<VideoTrackImpl>* track =
      new RefCountImpl<VideoTrackImpl>(label, ssrc);
  return track;
}

scoped_refptr<LocalVideoTrack> CreateLocalVideoTrack(
    const std::string& label,
    VideoCaptureModule* video_device) {
  RefCountImpl<VideoTrackImpl>* track =
      new RefCountImpl<VideoTrackImpl>(label, video_device);
  return track;
}

}  // namespace webrtc
