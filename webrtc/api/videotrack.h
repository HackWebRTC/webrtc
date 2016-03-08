/*
 *  Copyright 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_API_VIDEOTRACK_H_
#define WEBRTC_API_VIDEOTRACK_H_

#include <string>

#include "webrtc/api/mediastreamtrack.h"
#include "webrtc/api/videosourceinterface.h"
#include "webrtc/api/videotrackrenderers.h"
#include "webrtc/base/scoped_ref_ptr.h"

namespace webrtc {

class VideoTrack : public MediaStreamTrack<VideoTrackInterface> {
 public:
  static rtc::scoped_refptr<VideoTrack> Create(
      const std::string& label,
      VideoTrackSourceInterface* source);

  void AddOrUpdateSink(rtc::VideoSinkInterface<cricket::VideoFrame>* sink,
                       const rtc::VideoSinkWants& wants) override;
  void RemoveSink(rtc::VideoSinkInterface<cricket::VideoFrame>* sink) override;

  virtual VideoTrackSourceInterface* GetSource() const {
    return video_source_.get();
  }
  rtc::VideoSinkInterface<cricket::VideoFrame>* GetSink() override;
  virtual bool set_enabled(bool enable);
  virtual std::string kind() const;

 protected:
  VideoTrack(const std::string& id, VideoTrackSourceInterface* video_source);
  ~VideoTrack();

 private:
  VideoTrackRenderers renderers_;
  rtc::scoped_refptr<VideoTrackSourceInterface> video_source_;
};

}  // namespace webrtc

#endif  // WEBRTC_API_VIDEOTRACK_H_
