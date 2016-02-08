/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

// This file contains the class CaptureRenderAdapter. The class connects a
// VideoCapturer to any number of VideoRenders such that the former feeds the
// latter.
// CaptureRenderAdapter is Thread-unsafe. This means that none of its APIs may
// be called concurrently.

#ifndef WEBRTC_MEDIA_BASE_CAPTURERENDERADAPTER_H_
#define WEBRTC_MEDIA_BASE_CAPTURERENDERADAPTER_H_

#include <vector>

#include "webrtc/base/criticalsection.h"
#include "webrtc/base/sigslot.h"
#include "webrtc/media/base/videocapturer.h"
#include "webrtc/media/base/videosinkinterface.h"

namespace cricket {

class VideoCapturer;
class VideoProcessor;

class CaptureRenderAdapter : public sigslot::has_slots<> {
 public:
  static CaptureRenderAdapter* Create(VideoCapturer* video_capturer);
  ~CaptureRenderAdapter();

  void AddSink(rtc::VideoSinkInterface<VideoFrame>* sink);
  void RemoveSink(rtc::VideoSinkInterface<VideoFrame>* sink);

  VideoCapturer* video_capturer() { return video_capturer_; }
 private:

  explicit CaptureRenderAdapter(VideoCapturer* video_capturer);
  void Init();

  // Callback for frames received from the capturer.
  void OnVideoFrame(VideoCapturer* capturer, const VideoFrame* video_frame);

  // Just pointers since ownership is not handed over to this class.
  std::vector<rtc::VideoSinkInterface<VideoFrame>*> sinks_;
  VideoCapturer* video_capturer_;
  // Critical section synchronizing the capture thread.
  rtc::CriticalSection capture_crit_;
};

}  // namespace cricket

#endif  // WEBRTC_MEDIA_BASE_CAPTURERENDERADAPTER_H_
