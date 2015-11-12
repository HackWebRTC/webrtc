/*
 * libjingle
 * Copyright 2004 Google Inc.
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

#ifndef TALK_MEDIA_WEBRTCVIDEOCAPTURER_H_
#define TALK_MEDIA_WEBRTCVIDEOCAPTURER_H_

#ifdef HAVE_WEBRTC_VIDEO

#include <string>
#include <vector>

#include "talk/media/base/videocapturer.h"
#include "talk/media/webrtc/webrtcvideoframe.h"
#include "webrtc/base/asyncinvoker.h"
#include "webrtc/base/messagehandler.h"
#include "webrtc/base/scoped_ptr.h"
#include "webrtc/common_video/libyuv/include/webrtc_libyuv.h"
#include "webrtc/modules/video_capture/video_capture.h"

namespace cricket {

// Factory to allow injection of a VCM impl into WebRtcVideoCapturer.
// DeviceInfos do not have a Release() and therefore need an explicit Destroy().
class WebRtcVcmFactoryInterface {
 public:
  virtual ~WebRtcVcmFactoryInterface() {}
  virtual webrtc::VideoCaptureModule* Create(
      int id, const char* device) = 0;
  virtual webrtc::VideoCaptureModule::DeviceInfo* CreateDeviceInfo(int id) = 0;
  virtual void DestroyDeviceInfo(
      webrtc::VideoCaptureModule::DeviceInfo* info) = 0;
};

// WebRTC-based implementation of VideoCapturer.
class WebRtcVideoCapturer : public VideoCapturer,
                            public webrtc::VideoCaptureDataCallback {
 public:
  WebRtcVideoCapturer();
  explicit WebRtcVideoCapturer(WebRtcVcmFactoryInterface* factory);
  virtual ~WebRtcVideoCapturer();

  bool Init(const Device& device);
  bool Init(webrtc::VideoCaptureModule* module);

  // Override virtual methods of the parent class VideoCapturer.
  virtual bool GetBestCaptureFormat(const VideoFormat& desired,
                                    VideoFormat* best_format);
  virtual CaptureState Start(const VideoFormat& capture_format);
  virtual void Stop();
  virtual bool IsRunning();
  virtual bool IsScreencast() const { return false; }
  virtual bool SetApplyRotation(bool enable);

 protected:
  // Override virtual methods of the parent class VideoCapturer.
  virtual bool GetPreferredFourccs(std::vector<uint32_t>* fourccs);

 private:
  // Callback when a frame is captured by camera.
  virtual void OnIncomingCapturedFrame(const int32_t id,
                                       const webrtc::VideoFrame& frame);
  virtual void OnCaptureDelayChanged(const int32_t id,
                                     const int32_t delay);

  // Used to signal captured frames on the same thread as invoked Start().
  // With WebRTC's current VideoCapturer implementations, this will mean a
  // thread hop, but in other implementations (e.g. Chrome) it will be called
  // directly from OnIncomingCapturedFrame.
  // TODO(tommi): Remove this workaround when we've updated the WebRTC capturers
  // to follow the same contract.
  void SignalFrameCapturedOnStartThread(const webrtc::VideoFrame& frame);

  rtc::scoped_ptr<WebRtcVcmFactoryInterface> factory_;
  webrtc::VideoCaptureModule* module_;
  int captured_frames_;
  std::vector<uint8_t> capture_buffer_;
  rtc::Thread* start_thread_;  // Set in Start(), unset in Stop();

  rtc::scoped_ptr<rtc::AsyncInvoker> async_invoker_;
};

struct WebRtcCapturedFrame : public CapturedFrame {
 public:
  WebRtcCapturedFrame(const webrtc::VideoFrame& frame,
                      void* buffer,
                      size_t length);
};

}  // namespace cricket

#endif  // HAVE_WEBRTC_VIDEO
#endif  // TALK_MEDIA_WEBRTCVIDEOCAPTURER_H_
