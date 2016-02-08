/*
 *  Copyright (c) 2004 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MEDIA_WEBRTC_WEBRTCVIDEOCAPTURER_H_
#define WEBRTC_MEDIA_WEBRTC_WEBRTCVIDEOCAPTURER_H_

#ifdef HAVE_WEBRTC_VIDEO

#include <string>
#include <vector>

#include "webrtc/base/asyncinvoker.h"
#include "webrtc/base/messagehandler.h"
#include "webrtc/base/scoped_ptr.h"
#include "webrtc/common_video/libyuv/include/webrtc_libyuv.h"
#include "webrtc/media/base/videocapturer.h"
#include "webrtc/media/webrtc/webrtcvideoframe.h"
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
#endif  // WEBRTC_MEDIA_WEBRTC_WEBRTCVIDEOCAPTURER_H_
