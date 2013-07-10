/*
 * libjingle
 * Copyright 2012 Google Inc.
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

// The CaptureManager class manages VideoCapturers to make it possible to share
// the same VideoCapturers across multiple instances. E.g. if two instances of
// some class want to listen to same VideoCapturer they can't individually stop
// and start capturing as doing so will affect the other instance.
// The class employs reference counting on starting and stopping of capturing of
// frames such that if anyone is still listening it will not be stopped. The
// class also provides APIs for attaching VideoRenderers to a specific capturer
// such that the VideoRenderers are fed frames directly from the capturer. In
// addition, these frames can be altered before being sent to the capturers by
// way of VideoProcessors.
// CaptureManager is Thread-unsafe. This means that none of its APIs may be
// called concurrently. Note that callbacks are called by the VideoCapturer's
// thread which is normally a separate unmarshalled thread and thus normally
// require lock protection.

#ifndef TALK_MEDIA_BASE_CAPTUREMANAGER_H_
#define TALK_MEDIA_BASE_CAPTUREMANAGER_H_

#include <map>
#include <vector>

#include "talk/base/sigslotrepeater.h"
#include "talk/media/base/capturerenderadapter.h"
#include "talk/media/base/videocommon.h"

namespace cricket {

class VideoCapturer;
class VideoProcessor;
class VideoRenderer;
class VideoCapturerState;

class CaptureManager : public sigslot::has_slots<> {
 public:
  enum RestartOptions {
    kRequestRestart,
    kForceRestart
  };

  CaptureManager() {}
  virtual ~CaptureManager();

  bool StartVideoCapture(VideoCapturer* video_capturer,
                         const VideoFormat& desired_format);
  bool StopVideoCapture(VideoCapturer* video_capturer,
                        const VideoFormat& format);

  // Possibly restarts the capturer. If |options| is set to kRequestRestart,
  // the CaptureManager chooses whether this request can be handled with the
  // current state or if a restart is actually needed. If |options| is set to
  // kForceRestart, the capturer is restarted.
  bool RestartVideoCapture(VideoCapturer* video_capturer,
                           const VideoFormat& previous_format,
                           const VideoFormat& desired_format,
                           RestartOptions options);

  virtual bool AddVideoRenderer(VideoCapturer* video_capturer,
                                VideoRenderer* video_renderer);
  virtual bool RemoveVideoRenderer(VideoCapturer* video_capturer,
                                   VideoRenderer* video_renderer);

  virtual bool AddVideoProcessor(VideoCapturer* video_capturer,
                                 VideoProcessor* video_processor);
  virtual bool RemoveVideoProcessor(VideoCapturer* video_capturer,
                                    VideoProcessor* video_processor);

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

  CaptureStates capture_states_;
};

}  // namespace cricket

#endif  // TALK_MEDIA_BASE_CAPTUREMANAGER_H_
