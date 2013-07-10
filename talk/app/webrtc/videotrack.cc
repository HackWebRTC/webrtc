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
#include "talk/app/webrtc/videotrack.h"

#include <string>

#include "talk/media/webrtc/webrtcvideocapturer.h"

namespace webrtc {

static const char kVideoTrackKind[] = "video";

VideoTrack::VideoTrack(const std::string& label,
                       VideoSourceInterface* video_source)
    : MediaStreamTrack<VideoTrackInterface>(label),
      video_source_(video_source) {
  if (video_source_)
    video_source_->AddSink(FrameInput());
}

VideoTrack::~VideoTrack() {
  if (video_source_)
    video_source_->RemoveSink(FrameInput());
}

std::string VideoTrack::kind() const {
  return kVideoTrackKind;
}

void VideoTrack::AddRenderer(VideoRendererInterface* renderer) {
  renderers_.AddRenderer(renderer);
}

void VideoTrack::RemoveRenderer(VideoRendererInterface* renderer) {
  renderers_.RemoveRenderer(renderer);
}

cricket::VideoRenderer* VideoTrack::FrameInput() {
  return &renderers_;
}

bool VideoTrack::set_enabled(bool enable) {
  renderers_.SetEnabled(enable);
  return MediaStreamTrack<VideoTrackInterface>::set_enabled(enable);
}

talk_base::scoped_refptr<VideoTrack> VideoTrack::Create(
    const std::string& id, VideoSourceInterface* source) {
  talk_base::RefCountedObject<VideoTrack>* track =
      new talk_base::RefCountedObject<VideoTrack>(id, source);
  return track;
}

}  // namespace webrtc
