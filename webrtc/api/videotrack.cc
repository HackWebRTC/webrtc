/*
 *  Copyright 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/api/videotrack.h"

#include <string>

namespace webrtc {

const char MediaStreamTrackInterface::kVideoKind[] = "video";

VideoTrack::VideoTrack(const std::string& label,
                       VideoSourceInterface* video_source)
    : MediaStreamTrack<VideoTrackInterface>(label),
      video_source_(video_source) {
  if (video_source_)
    video_source_->AddSink(&renderers_);
}

VideoTrack::~VideoTrack() {
  if (video_source_)
    video_source_->RemoveSink(&renderers_);
}

std::string VideoTrack::kind() const {
  return kVideoKind;
}

void VideoTrack::AddRenderer(VideoRendererInterface* renderer) {
  renderers_.AddRenderer(renderer);
}

void VideoTrack::RemoveRenderer(VideoRendererInterface* renderer) {
  renderers_.RemoveRenderer(renderer);
}

rtc::VideoSinkInterface<cricket::VideoFrame>* VideoTrack::GetSink() {
  return &renderers_;
}

bool VideoTrack::set_enabled(bool enable) {
  renderers_.SetEnabled(enable);
  return MediaStreamTrack<VideoTrackInterface>::set_enabled(enable);
}

rtc::scoped_refptr<VideoTrack> VideoTrack::Create(
    const std::string& id, VideoSourceInterface* source) {
  rtc::RefCountedObject<VideoTrack>* track =
      new rtc::RefCountedObject<VideoTrack>(id, source);
  return track;
}

}  // namespace webrtc
