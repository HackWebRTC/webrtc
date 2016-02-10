/*
 *  Copyright 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_API_VIDEOSOURCE_H_
#define WEBRTC_API_VIDEOSOURCE_H_

#include <list>

#include "webrtc/api/mediastreaminterface.h"
#include "webrtc/api/notifier.h"
#include "webrtc/api/videosourceinterface.h"
#include "webrtc/api/videotrackrenderers.h"
#include "webrtc/base/scoped_ptr.h"
#include "webrtc/base/sigslot.h"
#include "webrtc/media/base/videosinkinterface.h"
#include "webrtc/media/base/videocapturer.h"
#include "webrtc/media/base/videocommon.h"

// VideoSource implements VideoSourceInterface. It owns a
// cricket::VideoCapturer and make sure the camera is started at a resolution
// that honors the constraints.
// The state is set depending on the result of starting the capturer.
// If the constraint can't be met or the capturer fails to start, the state
// transition to kEnded, otherwise it transitions to kLive.

namespace cricket {

class ChannelManager;

}  // namespace cricket

namespace webrtc {

class MediaConstraintsInterface;

class VideoSource : public Notifier<VideoSourceInterface>,
                    public sigslot::has_slots<> {
 public:
  // Creates an instance of VideoSource.
  // VideoSource take ownership of |capturer|.
  // |constraints| can be NULL and in that case the camera is opened using a
  // default resolution.
  static rtc::scoped_refptr<VideoSource> Create(
      cricket::ChannelManager* channel_manager,
      cricket::VideoCapturer* capturer,
      const webrtc::MediaConstraintsInterface* constraints,
      bool remote);

  SourceState state() const override { return state_; }
  bool remote() const override { return remote_; }

  virtual const cricket::VideoOptions* options() const { return &options_; }

  virtual cricket::VideoCapturer* GetVideoCapturer() {
    return video_capturer_.get();
  }

  void Stop() override;
  void Restart() override;

  // |output| will be served video frames as long as the underlying capturer
  // is running video frames.
  virtual void AddSink(rtc::VideoSinkInterface<cricket::VideoFrame>* output);
  virtual void RemoveSink(rtc::VideoSinkInterface<cricket::VideoFrame>* output);

 protected:
  VideoSource(cricket::ChannelManager* channel_manager,
              cricket::VideoCapturer* capturer,
              bool remote);
  virtual ~VideoSource();
  void Initialize(const webrtc::MediaConstraintsInterface* constraints);

 private:
  void OnStateChange(cricket::VideoCapturer* capturer,
                     cricket::CaptureState capture_state);
  void SetState(SourceState new_state);

  cricket::ChannelManager* channel_manager_;
  rtc::scoped_ptr<cricket::VideoCapturer> video_capturer_;
  rtc::scoped_ptr<cricket::VideoRenderer> frame_input_;

  std::list<rtc::VideoSinkInterface<cricket::VideoFrame>*> sinks_;

  cricket::VideoFormat format_;
  cricket::VideoOptions options_;
  SourceState state_;
  const bool remote_;
};

}  // namespace webrtc

#endif  // WEBRTC_API_VIDEOSOURCE_H_
