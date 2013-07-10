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

#include "talk/media/base/capturemanager.h"

#include <algorithm>

#include "talk/base/logging.h"
#include "talk/media/base/videocapturer.h"
#include "talk/media/base/videoprocessor.h"
#include "talk/media/base/videorenderer.h"

namespace cricket {

// CaptureManager helper class.
class VideoCapturerState {
 public:
  static const VideoFormatPod kDefaultCaptureFormat;

  static VideoCapturerState* Create(VideoCapturer* video_capturer);
  ~VideoCapturerState() {}

  void AddCaptureResolution(const VideoFormat& desired_format);
  bool RemoveCaptureResolution(const VideoFormat& format);
  VideoFormat GetHighestFormat(VideoCapturer* video_capturer) const;

  int IncCaptureStartRef();
  int DecCaptureStartRef();
  CaptureRenderAdapter* adapter() { return adapter_.get(); }
  VideoCapturer* GetVideoCapturer() { return adapter()->video_capturer(); }

  int start_count() const { return start_count_; }

 private:
  struct CaptureResolutionInfo {
    VideoFormat video_format;
    int format_ref_count;
  };
  typedef std::vector<CaptureResolutionInfo> CaptureFormats;

  explicit VideoCapturerState(CaptureRenderAdapter* adapter);

  talk_base::scoped_ptr<CaptureRenderAdapter> adapter_;

  int start_count_;
  CaptureFormats capture_formats_;
};

const VideoFormatPod VideoCapturerState::kDefaultCaptureFormat = {
  640, 360, FPS_TO_INTERVAL(30), FOURCC_ANY
};

VideoCapturerState::VideoCapturerState(CaptureRenderAdapter* adapter)
    : adapter_(adapter), start_count_(1) {}

VideoCapturerState* VideoCapturerState::Create(VideoCapturer* video_capturer) {
  CaptureRenderAdapter* adapter = CaptureRenderAdapter::Create(video_capturer);
  if (!adapter) {
    return NULL;
  }
  return new VideoCapturerState(adapter);
}

void VideoCapturerState::AddCaptureResolution(
    const VideoFormat& desired_format) {
  for (CaptureFormats::iterator iter = capture_formats_.begin();
       iter != capture_formats_.end(); ++iter) {
    if (desired_format == iter->video_format) {
      ++(iter->format_ref_count);
      return;
    }
  }
  CaptureResolutionInfo capture_resolution = { desired_format, 1 };
  capture_formats_.push_back(capture_resolution);
}

bool VideoCapturerState::RemoveCaptureResolution(const VideoFormat& format) {
  for (CaptureFormats::iterator iter = capture_formats_.begin();
       iter != capture_formats_.end(); ++iter) {
    if (format == iter->video_format) {
      --(iter->format_ref_count);
      if (iter->format_ref_count == 0) {
        capture_formats_.erase(iter);
      }
      return true;
    }
  }
  return false;
}

VideoFormat VideoCapturerState::GetHighestFormat(
    VideoCapturer* video_capturer) const {
  VideoFormat highest_format(0, 0, VideoFormat::FpsToInterval(1), FOURCC_ANY);
  if (capture_formats_.empty()) {
    VideoFormat default_format(kDefaultCaptureFormat);
    return default_format;
  }
  for (CaptureFormats::const_iterator iter = capture_formats_.begin();
       iter != capture_formats_.end(); ++iter) {
    if (iter->video_format.width > highest_format.width) {
      highest_format.width = iter->video_format.width;
    }
    if (iter->video_format.height > highest_format.height) {
      highest_format.height = iter->video_format.height;
    }
    if (iter->video_format.interval < highest_format.interval) {
      highest_format.interval = iter->video_format.interval;
    }
  }
  return highest_format;
}

int VideoCapturerState::IncCaptureStartRef() { return ++start_count_; }

int VideoCapturerState::DecCaptureStartRef() {
  if (start_count_ > 0) {
    // Start count may be 0 if a capturer was added but never started.
    --start_count_;
  }
  return start_count_;
}

CaptureManager::~CaptureManager() {
  while (!capture_states_.empty()) {
    // There may have been multiple calls to StartVideoCapture which means that
    // an equal number of calls to StopVideoCapture must be made. Note that
    // StopVideoCapture will remove the element from |capture_states_| when a
    // successfull stop has been made.
    UnregisterVideoCapturer(capture_states_.begin()->second);
  }
}

bool CaptureManager::StartVideoCapture(VideoCapturer* video_capturer,
                                       const VideoFormat& desired_format) {
  if (desired_format.width == 0 || desired_format.height == 0) {
    return false;
  }
  if (!video_capturer) {
    return false;
  }
  VideoCapturerState* capture_state = GetCaptureState(video_capturer);
  if (capture_state) {
    const int ref_count = capture_state->IncCaptureStartRef();
    if (ref_count < 1) {
      ASSERT(false);
    }
    // VideoCapturer has already been started. Don't start listening to
    // callbacks since that has already been done.
    capture_state->AddCaptureResolution(desired_format);
    return true;
  }
  if (!RegisterVideoCapturer(video_capturer)) {
    return false;
  }
  capture_state = GetCaptureState(video_capturer);
  ASSERT(capture_state != NULL);
  capture_state->AddCaptureResolution(desired_format);
  if (!StartWithBestCaptureFormat(capture_state, video_capturer)) {
    UnregisterVideoCapturer(capture_state);
    return false;
  }
  return true;
}

bool CaptureManager::StopVideoCapture(VideoCapturer* video_capturer,
                                      const VideoFormat& format) {
  VideoCapturerState* capture_state = GetCaptureState(video_capturer);
  if (!capture_state) {
    return false;
  }
  if (!capture_state->RemoveCaptureResolution(format)) {
    return false;
  }

  if (capture_state->DecCaptureStartRef() == 0) {
    // Unregistering cannot fail as capture_state is not NULL.
    UnregisterVideoCapturer(capture_state);
  }
  return true;
}

bool CaptureManager::RestartVideoCapture(
    VideoCapturer* video_capturer,
    const VideoFormat& previous_format,
    const VideoFormat& desired_format,
    CaptureManager::RestartOptions options) {
  if (!IsCapturerRegistered(video_capturer)) {
    LOG(LS_ERROR) << "RestartVideoCapture: video_capturer is not registered.";
    return false;
  }
  // Start the new format first. This keeps the capturer running.
  if (!StartVideoCapture(video_capturer, desired_format)) {
    LOG(LS_ERROR) << "RestartVideoCapture: unable to start video capture with "
        "desired_format=" << desired_format.ToString();
    return false;
  }
  // Stop the old format.
  if (!StopVideoCapture(video_capturer, previous_format)) {
    LOG(LS_ERROR) << "RestartVideoCapture: unable to stop video capture with "
        "previous_format=" << previous_format.ToString();
    // Undo the start request we just performed.
    StopVideoCapture(video_capturer, desired_format);
    return false;
  }

  switch (options) {
    case kForceRestart: {
      VideoCapturerState* capture_state = GetCaptureState(video_capturer);
      ASSERT(capture_state && capture_state->start_count() > 0);
      // Try a restart using the new best resolution.
      VideoFormat highest_asked_format =
          capture_state->GetHighestFormat(video_capturer);
      VideoFormat capture_format;
      if (video_capturer->GetBestCaptureFormat(highest_asked_format,
                                               &capture_format)) {
        if (!video_capturer->Restart(capture_format)) {
          LOG(LS_ERROR) << "RestartVideoCapture: Restart failed.";
        }
      } else {
        LOG(LS_WARNING)
            << "RestartVideoCapture: Couldn't find a best capture format for "
            << highest_asked_format.ToString();
      }
      break;
    }
    case kRequestRestart:
      // TODO(ryanpetrie): Support restart requests. Should this
      // to-be-implemented logic be used for {Start,Stop}VideoCapture as well?
      break;
    default:
      LOG(LS_ERROR) << "Unknown/unimplemented RestartOption";
      break;
  }
  return true;
}

bool CaptureManager::AddVideoRenderer(VideoCapturer* video_capturer,
                                      VideoRenderer* video_renderer) {
  if (!video_capturer || !video_renderer) {
    return false;
  }
  CaptureRenderAdapter* adapter = GetAdapter(video_capturer);
  if (!adapter) {
    return false;
  }
  return adapter->AddRenderer(video_renderer);
}

bool CaptureManager::RemoveVideoRenderer(VideoCapturer* video_capturer,
                                         VideoRenderer* video_renderer) {
  if (!video_capturer || !video_renderer) {
    return false;
  }
  CaptureRenderAdapter* adapter = GetAdapter(video_capturer);
  if (!adapter) {
    return false;
  }
  return adapter->RemoveRenderer(video_renderer);
}

bool CaptureManager::AddVideoProcessor(VideoCapturer* video_capturer,
                                       VideoProcessor* video_processor) {
  if (!video_capturer || !video_processor) {
    return false;
  }
  if (!IsCapturerRegistered(video_capturer)) {
    return false;
  }
  video_capturer->AddVideoProcessor(video_processor);
  return true;
}

bool CaptureManager::RemoveVideoProcessor(VideoCapturer* video_capturer,
                                          VideoProcessor* video_processor) {
  if (!video_capturer || !video_processor) {
    return false;
  }
  if (!IsCapturerRegistered(video_capturer)) {
    return false;
  }
  return video_capturer->RemoveVideoProcessor(video_processor);
}

bool CaptureManager::IsCapturerRegistered(VideoCapturer* video_capturer) const {
  return GetCaptureState(video_capturer) != NULL;
}

bool CaptureManager::RegisterVideoCapturer(VideoCapturer* video_capturer) {
  VideoCapturerState* capture_state =
      VideoCapturerState::Create(video_capturer);
  if (!capture_state) {
    return false;
  }
  capture_states_[video_capturer] = capture_state;
  SignalCapturerStateChange.repeat(video_capturer->SignalStateChange);
  return true;
}

void CaptureManager::UnregisterVideoCapturer(
    VideoCapturerState* capture_state) {
  VideoCapturer* video_capturer = capture_state->GetVideoCapturer();
  capture_states_.erase(video_capturer);
  delete capture_state;

  // When unregistering a VideoCapturer, the CaptureManager needs to unregister
  // from all state change callbacks from the VideoCapturer. E.g. to avoid
  // problems with multiple callbacks if registering the same VideoCapturer
  // multiple times. The VideoCapturer will update the capturer state. However,
  // this is done through Post-calls which means it may happen at any time. If
  // the CaptureManager no longer is listening to the VideoCapturer it will not
  // receive those callbacks. Here it is made sure that the the callback is
  // indeed sent by letting the ChannelManager do the signaling. The downside is
  // that the callback may happen before the VideoCapturer is stopped. However,
  // for the CaptureManager it doesn't matter as it will no longer receive any
  // frames from the VideoCapturer.
  SignalCapturerStateChange.stop(video_capturer->SignalStateChange);
  video_capturer->Stop();
  SignalCapturerStateChange(video_capturer, CS_STOPPED);
}

bool CaptureManager::StartWithBestCaptureFormat(
    VideoCapturerState* capture_state, VideoCapturer* video_capturer) {
  VideoFormat highest_asked_format =
      capture_state->GetHighestFormat(video_capturer);
  VideoFormat capture_format;
  if (!video_capturer->GetBestCaptureFormat(highest_asked_format,
                                            &capture_format)) {
    LOG(LS_WARNING) << "Unsupported format:"
                    << " width=" << highest_asked_format.width
                    << " height=" << highest_asked_format.height
                    << ". Supported formats are:";
    const std::vector<VideoFormat>* formats =
        video_capturer->GetSupportedFormats();
    ASSERT(formats != NULL);
    for (std::vector<VideoFormat>::const_iterator i = formats->begin();
         i != formats->end(); ++i) {
      const VideoFormat& format = *i;
      LOG(LS_WARNING) << "  " << GetFourccName(format.fourcc)
                      << ":" << format.width << "x" << format.height << "x"
                      << format.framerate();
    }
    return false;
  }
  return video_capturer->StartCapturing(capture_format);
}

VideoCapturerState* CaptureManager::GetCaptureState(
    VideoCapturer* video_capturer) const {
  CaptureStates::const_iterator iter = capture_states_.find(video_capturer);
  if (iter == capture_states_.end()) {
    return NULL;
  }
  return iter->second;
}

CaptureRenderAdapter* CaptureManager::GetAdapter(
    VideoCapturer* video_capturer) const {
  VideoCapturerState* capture_state = GetCaptureState(video_capturer);
  if (!capture_state) {
    return NULL;
  }
  return capture_state->adapter();
}

}  // namespace cricket
