/*
 *  Copyright 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_API_OBJC_AVFOUNDATION_VIDEO_CAPTURER_H_
#define WEBRTC_API_OBJC_AVFOUNDATION_VIDEO_CAPTURER_H_

#include "webrtc/base/scoped_ptr.h"
#include "webrtc/media/base/videocapturer.h"
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
  bool GetPreferredFourccs(std::vector<uint32_t> *fourccs) override {
    fourccs->push_back(cricket::FOURCC_NV12);
    return true;
  }

  /** Returns the active capture session. */
  AVCaptureSession* GetCaptureSession();

  /** Switches the camera being used (either front or back). */
  void SetUseBackCamera(bool useBackCamera);
  bool GetUseBackCamera() const;

  /**
   * Converts the sample buffer into a cricket::CapturedFrame and signals the
   * frame for capture.
   */
  void CaptureSampleBuffer(CMSampleBufferRef sampleBuffer);

 private:
  /**
   * Used to signal frame capture on the thread that capturer was started on.
   */
  void SignalFrameCapturedOnStartThread(const cricket::CapturedFrame *frame);

  RTCAVFoundationVideoCapturerInternal *_capturer;
  rtc::Thread *_startThread;  // Set in Start(), unset in Stop().
};  // AVFoundationVideoCapturer

}  // namespace webrtc

#endif  // TALK_APP_WEBRTC_OBJC_AVFOUNDATION_CAPTURER_H_
