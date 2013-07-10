/*
 * libjingle
 * Copyright 2012, Google Inc.
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

#ifndef TALK_APP_WEBRTC_LOCALVIDEOSOURCE_H_
#define TALK_APP_WEBRTC_LOCALVIDEOSOURCE_H_

#include "talk/app/webrtc/mediastreaminterface.h"
#include "talk/app/webrtc/notifier.h"
#include "talk/app/webrtc/videosourceinterface.h"
#include "talk/base/scoped_ptr.h"
#include "talk/base/sigslot.h"
#include "talk/media/base/videocapturer.h"
#include "talk/media/base/videocommon.h"

// LocalVideoSource implements VideoSourceInterface. It owns a
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

class LocalVideoSource : public Notifier<VideoSourceInterface>,
                         public sigslot::has_slots<> {
 public:
  // Creates an instance of LocalVideoSource.
  // LocalVideoSource take ownership of |capturer|.
  // |constraints| can be NULL and in that case the camera is opened using a
  // default resolution.
  static talk_base::scoped_refptr<LocalVideoSource> Create(
      cricket::ChannelManager* channel_manager,
      cricket::VideoCapturer* capturer,
      const webrtc::MediaConstraintsInterface* constraints);

  virtual SourceState state() const { return state_; }
  virtual const cricket::VideoOptions* options() const { return &options_; }

  virtual cricket::VideoCapturer* GetVideoCapturer() {
    return video_capturer_.get();
  }
  // |output| will be served video frames as long as the underlying capturer
  // is running video frames.
  virtual void AddSink(cricket::VideoRenderer* output);
  virtual void RemoveSink(cricket::VideoRenderer* output);

 protected:
  LocalVideoSource(cricket::ChannelManager* channel_manager,
                   cricket::VideoCapturer* capturer);
  ~LocalVideoSource();

 private:
  void Initialize(const webrtc::MediaConstraintsInterface* constraints);
  void OnStateChange(cricket::VideoCapturer* capturer,
                     cricket::CaptureState capture_state);
  void SetState(SourceState new_state);

  cricket::ChannelManager* channel_manager_;
  talk_base::scoped_ptr<cricket::VideoCapturer> video_capturer_;

  cricket::VideoFormat format_;
  cricket::VideoOptions options_;
  SourceState state_;
};

}  // namespace webrtc

#endif  // TALK_APP_WEBRTC_LOCALVIDEOSOURCE_H_
