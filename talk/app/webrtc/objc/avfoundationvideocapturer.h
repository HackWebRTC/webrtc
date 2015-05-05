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

#ifndef TALK_APP_WEBRTC_OBJC_AVFOUNDATION_VIDEO_CAPTURER_H_
#define TALK_APP_WEBRTC_OBJC_AVFOUNDATION_VIDEO_CAPTURER_H_

#include "talk/media/base/videocapturer.h"
#include "webrtc/base/scoped_ptr.h"
#include "webrtc/video_frame.h"

#import <AVFoundation/AVFoundation.h>

@class RTCAVFoundationVideoCapturerInternal;

namespace webrtc {

class AVFoundationVideoCapturer : public cricket::VideoCapturer {
 public:
  AVFoundationVideoCapturer();
  ~AVFoundationVideoCapturer();

  cricket::CaptureState Start(const cricket::VideoFormat& format) override;
  void Stop() override;
  bool IsRunning() override;
  bool IsScreencast() const override {
    return false;
  }
  bool GetPreferredFourccs(std::vector<uint32>* fourccs) override {
    fourccs->push_back(cricket::FOURCC_NV12);
    return true;
  }

  // Returns the active capture session.
  AVCaptureSession* GetCaptureSession();

  // Switches the camera being used (either front or back).
  void SetUseBackCamera(bool useBackCamera);
  bool GetUseBackCamera() const;

  // Converts the sample buffer into a cricket::CapturedFrame and signals the
  // frame for capture.
  void CaptureSampleBuffer(CMSampleBufferRef sampleBuffer);

 private:
  // Used to signal frame capture on the thread that capturer was started on.
  void SignalFrameCapturedOnStartThread(const cricket::CapturedFrame* frame);

  RTCAVFoundationVideoCapturerInternal* _capturer;
  rtc::Thread* _startThread;  // Set in Start(), unset in Stop().
  uint64_t _startTime;
};  // AVFoundationVideoCapturer

}  // namespace webrtc

#endif  // TALK_APP_WEBRTC_OBJC_AVFOUNDATION_CAPTURER_H_
