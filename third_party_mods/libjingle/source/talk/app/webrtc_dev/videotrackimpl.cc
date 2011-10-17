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

static const char kVideoTrackKind[] = "video";

VideoTrack::VideoTrack(const std::string& label, uint32 ssrc)
    : MediaTrack<LocalVideoTrackInterface>(label, ssrc),
      video_device_(NULL) {
}

VideoTrack::VideoTrack(const std::string& label,
                       VideoCaptureModule* video_device)
    : MediaTrack<LocalVideoTrackInterface>(label, 0),
      video_device_(video_device) {
}

void VideoTrack::SetRenderer(VideoRendererWrapperInterface* renderer) {
  video_renderer_ = renderer;
  NotifierImpl<LocalVideoTrackInterface>::FireOnChanged();
}

VideoRendererWrapperInterface* VideoTrack::GetRenderer() {
  return video_renderer_.get();
}

  // Get the VideoCapture device associated with this track.
VideoCaptureModule* VideoTrack::GetVideoCapture() {
  return video_device_.get();
}

const char* VideoTrack::kind() const {
  return kVideoTrackKind;
}

scoped_refptr<VideoTrack> VideoTrack::CreateRemote(
    const std::string& label,
    uint32 ssrc) {
  talk_base::RefCountImpl<VideoTrack>* track =
      new talk_base::RefCountImpl<VideoTrack>(label, ssrc);
  return track;
}

scoped_refptr<VideoTrack> VideoTrack::CreateLocal(
    const std::string& label,
    VideoCaptureModule* video_device) {
  talk_base::RefCountImpl<VideoTrack>* track =
      new talk_base::RefCountImpl<VideoTrack>(label, video_device);
  return track;
}

}  // namespace webrtc
