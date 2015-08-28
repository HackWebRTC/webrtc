/*
 * libjingle
 * Copyright 2015 Google Inc.
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
#ifndef TALK_APP_WEBRTC_ANDROIDVIDEOCAPTURER_H_
#define TALK_APP_WEBRTC_ANDROIDVIDEOCAPTURER_H_

#include <string>
#include <vector>

#include "talk/media/base/videocapturer.h"
#include "webrtc/base/thread_checker.h"
#include "webrtc/common_video/interface/video_frame_buffer.h"

namespace webrtc {

class AndroidVideoCapturer;

class AndroidVideoCapturerDelegate : public rtc::RefCountInterface {
 public:
  virtual ~AndroidVideoCapturerDelegate() {}
  // Start capturing. The implementation of the delegate must call
  // AndroidVideoCapturer::OnCapturerStarted with the result of this request.
  virtual void Start(int width, int height, int framerate,
                     AndroidVideoCapturer* capturer) = 0;

  // Stops capturing.
  // The delegate may not call into AndroidVideoCapturer after this call.
  virtual void Stop() = 0;

  // Must returns a JSON string "{{width=xxx, height=xxx, framerate = xxx}}"
  virtual std::string GetSupportedFormats() = 0;
};

// Android implementation of cricket::VideoCapturer for use with WebRtc
// PeerConnection.
class AndroidVideoCapturer : public cricket::VideoCapturer {
 public:
  explicit AndroidVideoCapturer(
      const rtc::scoped_refptr<AndroidVideoCapturerDelegate>& delegate);
  virtual ~AndroidVideoCapturer();

  // Called from JNI when the capturer has been started.
  void OnCapturerStarted(bool success);

  // Called from JNI when a new frame has been captured.
  // Argument |buffer| is intentionally by value, for use with rtc::Bind.
  void OnIncomingFrame(rtc::scoped_refptr<webrtc::VideoFrameBuffer> buffer,
                       int rotation,
                       int64 time_stamp);

  // Called from JNI to request a new video format.
  void OnOutputFormatRequest(int width, int height, int fps);

  AndroidVideoCapturerDelegate* delegate() { return delegate_.get(); }

  // cricket::VideoCapturer implementation.
  bool GetBestCaptureFormat(const cricket::VideoFormat& desired,
                            cricket::VideoFormat* best_format) override;

 private:
  // cricket::VideoCapturer implementation.
  // Video frames will be delivered using
  // cricket::VideoCapturer::SignalFrameCaptured on the thread that calls Start.
  cricket::CaptureState Start(
      const cricket::VideoFormat& capture_format) override;
  void Stop() override;
  bool IsRunning() override;
  bool IsScreencast() const override { return false; }
  bool GetPreferredFourccs(std::vector<uint32>* fourccs) override;

  bool running_;
  rtc::scoped_refptr<AndroidVideoCapturerDelegate> delegate_;

  rtc::ThreadChecker thread_checker_;

  class FrameFactory;
  FrameFactory* frame_factory_;  // Owned by cricket::VideoCapturer.

  cricket::CaptureState current_state_;
};

}  // namespace webrtc

#endif  // TALK_APP_WEBRTC_ANDROIDVIDEOCAPTURER_H_
