/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

// The CaptureManager class manages VideoCapturers to make it possible to share
// the same VideoCapturers across multiple instances. E.g. if two instances of
// some class want to listen to same VideoCapturer they can't individually stop
// and start capturing as doing so will affect the other instance.
// The class employs reference counting on starting and stopping of capturing of
// frames such that if anyone is still listening it will not be stopped. The
// class also provides APIs for attaching VideoRenderers to a specific capturer
// such that the VideoRenderers are fed frames directly from the capturer.
// CaptureManager is Thread-unsafe. This means that none of its APIs may be
// called concurrently. Note that callbacks are called by the VideoCapturer's
// thread which is normally a separate unmarshalled thread and thus normally
// require lock protection.

#ifndef WEBRTC_MEDIA_BASE_CAPTUREMANAGER_H_
#define WEBRTC_MEDIA_BASE_CAPTUREMANAGER_H_

#include <map>
#include <vector>

#include "webrtc/base/sigslotrepeater.h"
#include "webrtc/base/thread_checker.h"
#include "webrtc/media/base/capturerenderadapter.h"
#include "webrtc/media/base/videocommon.h"

namespace cricket {

class VideoCapturer;
class VideoRenderer;
class VideoCapturerState;

class CaptureManager : public sigslot::has_slots<> {
 public:
  enum RestartOptions {
    kRequestRestart,
    kForceRestart
  };

  CaptureManager();
  virtual ~CaptureManager();

  virtual bool StartVideoCapture(VideoCapturer* video_capturer,
                                 const VideoFormat& desired_format);
  virtual bool StopVideoCapture(VideoCapturer* video_capturer,
                                const VideoFormat& format);

  // Possibly restarts the capturer. If |options| is set to kRequestRestart,
  // the CaptureManager chooses whether this request can be handled with the
  // current state or if a restart is actually needed. If |options| is set to
  // kForceRestart, the capturer is restarted.
  virtual bool RestartVideoCapture(VideoCapturer* video_capturer,
                                   const VideoFormat& previous_format,
                                   const VideoFormat& desired_format,
                                   RestartOptions options);

  virtual void AddVideoSink(VideoCapturer* video_capturer,
                            rtc::VideoSinkInterface<VideoFrame>* sink);
  virtual void RemoveVideoSink(VideoCapturer* video_capturer,
                               rtc::VideoSinkInterface<VideoFrame>* sink);

  sigslot::repeater2<VideoCapturer*, CaptureState> SignalCapturerStateChange;

 private:
  typedef std::map<VideoCapturer*, VideoCapturerState*> CaptureStates;

  bool IsCapturerRegistered(VideoCapturer* video_capturer) const;
  bool RegisterVideoCapturer(VideoCapturer* video_capturer);
  void UnregisterVideoCapturer(VideoCapturerState* capture_state);

  bool StartWithBestCaptureFormat(VideoCapturerState* capture_info,
                                  VideoCapturer* video_capturer);

  VideoCapturerState* GetCaptureState(VideoCapturer* video_capturer) const;
  CaptureRenderAdapter* GetAdapter(VideoCapturer* video_capturer) const;

  rtc::ThreadChecker thread_checker_;
  CaptureStates capture_states_;
};

}  // namespace cricket

#endif  // WEBRTC_MEDIA_BASE_CAPTUREMANAGER_H_
